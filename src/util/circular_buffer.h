#ifndef _headers_1668855949_FluffyLauncher_circular_buffer
#define _headers_1668855949_FluffyLauncher_circular_buffer

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>

// Thread-safe circular buffer
// Can be staticly initialized

struct circular_buffer {
  bool isStaticInit;
  
  pthread_mutex_t lock;
  pthread_cond_t lockCond;
  bool lockInited;
  bool lockCondInited;
  
  void* buffer;
  
  uintptr_t writeHead;
  uintptr_t readHead;
  
  size_t bufferSize; 
};

#define circular_buffer_static_init(name, size) \
  static char name ## ___data[size] = {0}; \
  static struct circular_buffer name = { \
    .isStaticInit = true, \
    .lock = PTHREAD_MUTEX_INITIALIZER, \
    .lockCond = PTHREAD_COND_INITIALIZER, \
    .buffer = name ## ___data, \
    .bufferSize = size \
  };

struct circular_buffer* circular_buffer_new(size_t bufferSize);
void circular_buffer_free(struct circular_buffer* self);

// The `size` is used to check buffer access overflows 
// Errors:
// -EOVERFLOW: Circular buffer doesnt fit
int circular_buffer_read(struct circular_buffer* self, void* result, size_t size);
int circular_buffer_write(struct circular_buffer* self, const void* data, size_t size);

bool circular_buffer_can_write(struct circular_buffer* self, size_t size);
bool circular_buffer_can_read(struct circular_buffer* self, size_t size);

bool circular_buffer_is_full(struct circular_buffer* self);
bool circular_buffer_is_empty(struct circular_buffer* self);
size_t circular_buffer_get_usage(struct circular_buffer* self);

// Polls every 1 ms and check
// Return miliseconds left or -ETIMEDOUT on timeout
// could return 0 incase its empty/full at same ms when time out happens
int circular_buffer_wait_until_empty(struct circular_buffer* self, int maxMs);
int circular_buffer_wait_until_full(struct circular_buffer* self, int maxMs);

#endif

