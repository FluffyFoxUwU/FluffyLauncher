#include <pthread.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <threads.h>
#include <string.h>
#include <time.h>

#include "buffer.h"
#include "hashmap.h"
#include "bug.h"
#include "hashmap_base.h"
#include "util.h"
#include "logging/logging.h"

size_t util_vasprintf(char** buffer, const char* fmt, va_list args) {
  va_list copy;
  va_copy(copy, args);
  size_t size = vsnprintf(NULL, 0, fmt, copy);
  va_end(copy);

  *buffer = malloc(size + 1);
  if (*buffer == NULL)
    return -ENOMEM;

  return vsnprintf(*buffer, size + 1, fmt, args);
}

size_t util_asprintf(char** buffer, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  size_t ret = util_vasprintf(buffer, fmt, args);
  va_end(args);
  return ret;
}

static pthread_rwlock_t accessLock = PTHREAD_RWLOCK_INITIALIZER;

static atomic_bool inited = false;
static HASHMAP(pthread_t, const char) threadNames;
static thread_local bool isManaged = false;

static size_t hash(const pthread_t* key) {
  return hashmap_hash_default(key, sizeof(pthread_t));
}

static int cmp(const pthread_t* a, const pthread_t* b) {
  return pthread_equal(*a, *b) != 0 ? 0 : -1;
}

static void* dupFunc(const void* key) {
  pthread_t* newOne = malloc(sizeof(*newOne));
  *newOne = *((pthread_t*) key);
  return newOne;
}

int util_init() {
  hashmap_init(&threadNames, hash, cmp);
  threadNames.map_base.key_dup = dupFunc;
  threadNames.map_base.key_free = free;
  atomic_store(&inited, true);
  isManaged = true;
  return 0;
}

struct thread_exec_data {
  void* (*routine)(void *);
  void* arg;
};

static void* customRoutine(void* _args) {
  struct thread_exec_data* exec = _args;
  pthread_t currentThread = pthread_self();
  isManaged = true;
  
  void* res = exec->routine(exec->arg);
  
  pthread_rwlock_rdlock(&accessLock);
  const char* existing = hashmap_get(&threadNames, &currentThread);
  if (existing) {
    hashmap_remove(&threadNames, &currentThread);
    free((char*) existing);
  }
  pthread_rwlock_unlock(&accessLock);
  
  isManaged = false;
  free(exec);
  return res;
}

int util_thread_create(pthread_t* newthread, pthread_attr_t* attr, void* (*routine)(void *), void* arg) {
  int res = 0;
  struct thread_exec_data* data = malloc(sizeof(*data));
  if (!data)  
    return -ENOMEM;
  
  data->arg = arg;
  data->routine = routine;
  res = -pthread_create(newthread, attr, customRoutine, data);
  if (res < 0)
    free(data);
  return res;
}

void util_set_thread_name(pthread_t thread, const char* name) {
  if (atomic_load(&inited) == false) {
    pr_warn("%s: Called before utility initialized! (Will result in noop)", __func__);
    return;
  }
  
  if (isManaged == false) {
    pr_warn("%s: Called on unmanaged thread (Expect small memory leak)", __func__);
    return;
  }
  
  pthread_rwlock_wrlock(&accessLock);
  
  const char* existing = hashmap_get(&threadNames, &thread);
  if (existing) {
    hashmap_remove(&threadNames, &thread);
    free((char*) existing);
  }
  
  const char* clone = strdup(name);
  if (!clone) {
    pr_warn("%s: Out of memory thread name not recorded", __func__);
    goto name_clone_failed;
  }
  
  if (hashmap_put(&threadNames, &thread, clone) < 0) {
    free((char*) clone);
    pr_warn("%s: Out of memory thread name not recorded", __func__);
    goto insert_thread_failed;
  }
  
insert_thread_failed:
name_clone_failed:
  pthread_rwlock_unlock(&accessLock);
  return;
}

const char* util_get_thread_name(pthread_t thread) {
  if (atomic_load(&inited) == false)
    return NULL;
  
  pthread_rwlock_rdlock(&accessLock);
  const char* existing = hashmap_get(&threadNames, &thread);
  pthread_rwlock_unlock(&accessLock);
  return existing;
}

void util_cleanup() {}

// Portable strcasecmp
int util_strcasecmp(const char* a, const char* b) {
  while (tolower(*a) == tolower(*b) && *a && *b) {
    a++;
    b++;
  }
  
  return (*(a - 1) > *(b - 1)) - (*(a - 1) < *(b - 1));
}

size_t util_hash_buffer(const buffer_t* buff) {
  return hashmap_hash_string(buff->data);
}

int util_compare_buffer(const buffer_t* a, const buffer_t* b) {
  return strcmp(a->data, b->data);
}

buffer_t* util_clone_buffer(const buffer_t* buff) {
  return buffer_new_with_copy(buff->data);
}

void util_msleep(uint32_t milisecs) {
  util_usleep(milisecs * 1000);
}

void util_usleep(uint32_t microsecs) {
  util_nanosleep(microsecs * 1000);
}

void util_nanosleep(uint64_t nanosecs) {
  struct timespec rem = {};
  struct timespec req = {
    .tv_nsec = nanosecs % 1000000000,
    .tv_sec = nanosecs / 1000000000
  };
  
  int res = 0;
  while ((res = nanosleep(&req, &rem)) == -EINTR) {
    req = rem;
    rem = (struct timespec) {};
  }
  BUG_ON(res == -EINVAL);
}

double util_get_realtime() {
  struct timespec timespec;
  clock_gettime(CLOCK_REALTIME, &timespec);
  return (double) timespec.tv_sec + ((double) timespec.tv_nsec / (double) 1000000000);
}
