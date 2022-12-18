#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <threads.h>

#include "buffer.h"
#include "util/circular_buffer.h"
#include "logging.h"
#include "bug.h"
#include "util/util.h"

// Need to replace BUG as at very extraordinary when ALL star aligns
// BUG() in here can infinitely recurse or deadlock
// tldr this replacement is for extraordinary cases
#undef BUG
#define BUG() do { \
 fprintf(stderr, "BUG in logging subsystem: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __func__); \
 abort(); \
} while(0)

#define BUFFER_SIZE (2 * 1024 * 1024)
#define PRINTK_BUFFER_SIZE 512 * 1024

circular_buffer_static_init(circularBuffer, BUFFER_SIZE);

// Read buffer (only ever get as large as printk buffer)
static char readBuffer[PRINTK_BUFFER_SIZE]; 

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

static void recordNoLock(const char* msg) {
  const char* threadName = util_get_thread_name(pthread_self());
  size_t msgLen = strlen(msg) - 1;
  size_t threadNameLen = strlen(threadName);
  struct log_entry entry = {
    .cpuTimestampInMs = ((float) clock()) / ((float) CLOCKS_PER_SEC),
    .realtime = util_get_realtime(),
    .messageLen = msgLen + threadNameLen + 3,
  };
  
  BUG_ON(validLogLevels[(int) msg[0]] == false);
  entry.logLevel = logLevelLookup[(int) msg[0]];
  
  // Skip log header UwU
  msg++;
  
  int res = circular_buffer_write(&circularBuffer, &entry, sizeof(entry));
  BUG_ON(res < 0);
  
  res = circular_buffer_write(&circularBuffer, "[", 1);
  BUG_ON(res < 0);
  
  res = circular_buffer_write(&circularBuffer, threadName, threadNameLen);
  BUG_ON(res < 0);
  
  res = circular_buffer_write(&circularBuffer, "] ", 2);
  BUG_ON(res < 0);
  
  res = circular_buffer_write(&circularBuffer, msg, msgLen);
  BUG_ON(res < 0); 
}

void logging_read_log(struct log_entry* entryPtr) {
  struct log_entry entry;
  
  int res = circular_buffer_read(&circularBuffer, &entry, sizeof(entry));
  BUG_ON(res < 0 || entry.messageLen + 1 >= sizeof(readBuffer));
  
  res = circular_buffer_read(&circularBuffer, readBuffer, entry.messageLen);
  BUG_ON(res < 0);
  
  readBuffer[entry.messageLen] = '\0';
  entry.message = readBuffer;
  
  if (entryPtr)
    *entryPtr = entry;
}

static pthread_mutex_t loggingLock = PTHREAD_MUTEX_INITIALIZER;
void printk_va(const char* fmt, va_list args) {
  // You may UwU-ify `fmt` here
  // to UwU-ify every log entries

  static thread_local char localPrintkBuffer[PRINTK_BUFFER_SIZE]; 
  size_t bytesWritten = vsnprintf(localPrintkBuffer, sizeof(localPrintkBuffer), fmt, args);
  
  pthread_mutex_lock(&loggingLock);
  recordNoLock(localPrintkBuffer);
  if (bytesWritten >= sizeof(localPrintkBuffer))
    recordNoLock(LOG_ALERT pr_fmt("ALERT", "Some log entry is truncated!"));
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

bool logging_flush() {
  // Wait 5 secs before declaring something with logging subsystem is deadlocked
  // or unresponsive
  return circular_buffer_wait_until_empty(&circularBuffer, 5000) >= 0;
}
