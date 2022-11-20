#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <threads.h>

#include "util/circular_buffer.h"
#include "logging.h"
#include "bug.h"

#define BUFFER_SIZE (512 * 1024 * 1024)

circular_buffer_static_init(circularBuffer, BUFFER_SIZE);
static pthread_mutex_t loggingLock = PTHREAD_MUTEX_INITIALIZER;

// For emergency usage if cant allocate buffer for each thread
static char emergencyBuffer[BUFFER_SIZE - 8]; 

static bool validLogLevels[256] = {
  ['0'] = true,
  ['1'] = true,
  ['2'] = true,
  ['3'] = true,
  ['4'] = true,
  ['5'] = true,
  ['6'] = true,
  ['7'] = true
};

static enum log_level logLevelLookup[256] = {
  ['0'] = LOG_LEVEL_EMERGENCY,
  ['1'] = LOG_LEVEL_ALERT,
  ['2'] = LOG_LEVEL_CRITICAL,
  ['3'] = LOG_LEVEL_ERROR,
  ['4'] = LOG_LEVEL_WARNING,
  ['5'] = LOG_LEVEL_NOTICE,
  ['6'] = LOG_LEVEL_INFO,
  ['7'] = LOG_LEVEL_DEBUG
};

void logging_release_critical() {
  pthread_mutex_unlock(&loggingLock);
}

static void recordNoLock(const char* msg) {
  struct log_entry entry = {
    .timestampInMs = ((float) clock()) / ((float) CLOCKS_PER_SEC),
    .messageLen = strlen(msg) - 1,
    .message = NULL
  };
  
  BUG_ON(validLogLevels[(int) msg[0]] == false);
  entry.logLevel = logLevelLookup[(int) msg[0]];
  
  int res = circular_buffer_write(&circularBuffer, &entry, sizeof(entry));
  BUG_ON(res < 0);
  
  // The +1 are for skipping log level header
  res = circular_buffer_write(&circularBuffer, msg + 1, entry.messageLen);
  BUG_ON(res < 0); 
}

void logging_read_log(struct log_entry* entryPtr) {
  struct log_entry entry;
  
  int res = circular_buffer_read(&circularBuffer, &entry, sizeof(entry));
  BUG_ON(res < 0 || entry.messageLen + 1 > sizeof(emergencyBuffer));
  
  pthread_mutex_lock(&loggingLock);
  res = circular_buffer_read(&circularBuffer, emergencyBuffer, entry.messageLen);
  BUG_ON(res < 0);
  
  emergencyBuffer[entry.messageLen] = '\0';
  entry.message = emergencyBuffer;
  
  if (entryPtr)
    *entryPtr = entry;
}

void printk_va(const char* fmt, va_list args) {
  va_list copy;
  va_copy(copy, args);
  size_t len = vsnprintf(NULL, 0, fmt, copy);
  va_end(copy);
  
  BUG_ON(len + 1 > sizeof(emergencyBuffer));
  
  pthread_mutex_lock(&loggingLock);
  vsnprintf(emergencyBuffer, len + 1, fmt, args);
  recordNoLock(emergencyBuffer);
  pthread_mutex_unlock(&loggingLock);
}

void printk(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  printk_va(fmt, args);
  va_end(args);
}

bool logging_has_more_entry() {
  pthread_mutex_lock(&loggingLock);
  bool res = circular_buffer_get_usage(&circularBuffer) > 0;
  pthread_mutex_unlock(&loggingLock);
  return res;
}
