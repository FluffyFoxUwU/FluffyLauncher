#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util.h"
#include "circular_buffer.h"
#include "bug.h"

struct circular_buffer* circular_buffer_new(size_t bufferSize) {
  struct circular_buffer* self = malloc(sizeof(*self));
  if (!self)
    return NULL;
  *self = (struct circular_buffer) {
    .bufferSize = bufferSize 
  };
  self->buffer = malloc(bufferSize);
  if (!self->buffer)
    goto failure;
  
  return self;

failure:
  circular_buffer_free(self);
  return NULL;
}

void circular_buffer_free(struct circular_buffer* self) {
  if (!self)
    return;
  
  BUG_ON(self->isStaticInit == true);
  
  int res = 0;
  if (self->lockInited) {
    res = pthread_mutex_destroy(&self->lock);
    BUG_ON(res < 0);
  }
  
  if (self->lockCondInited) {
    res = pthread_cond_destroy(&self->lockCond);
    BUG_ON(res < 0);
  }
  
  free(self->buffer);
  free(self);
}

int circular_buffer_read(struct circular_buffer* self, void* result, size_t size) {
  if (size > self->bufferSize - 1)
    return -EOVERFLOW;
  
  pthread_mutex_lock(&self->lock);
  while (!circular_buffer_can_read(self, size))
    pthread_cond_wait(&self->lockCond, &self->lock);
  
  uintptr_t readEnd = self->readHead + size;
  if (readEnd >= self->bufferSize) {
    // [readHead, readHead + size] crossing boundry of bufferSize
    // so do two copy which is
    // [readHead, bufferSize) and
    // [0, readEnd % bufferSize]
    memcpy(result, self->buffer + self->readHead, self->bufferSize - self->readHead);
    memcpy(result, self->buffer, (readEnd % self->bufferSize) + 1);
  } else {
    // Directly do single memcpy call as the region
    // not wrapping back into the front
    // [readHead, readHead + size] not crossing boundry of bufferSize
    memcpy(result, self->buffer + self->readHead, size);
  }
  
  self->readHead += size;
  self->readHead %= self->bufferSize;
  pthread_mutex_unlock(&self->lock);
  
  pthread_cond_broadcast(&self->lockCond);
  return 0;
}

int circular_buffer_write(struct circular_buffer* self, const void* data, size_t size) {
  if (size > self->bufferSize - 4)
    return -EOVERFLOW;
  
  pthread_mutex_lock(&self->lock);
  while (!circular_buffer_can_write(self, size))
    pthread_cond_wait(&self->lockCond, &self->lock); 
  
  uintptr_t writeEnd = self->writeHead + size;
  if (writeEnd >= self->bufferSize) {
    // [writeHead, writeHead + size] crossing boundry of bufferSize
    // so do two copy which is
    // [writeHead, bufferSize) and
    // [0, writeEnd % bufferSize]
    memcpy(self->buffer + self->writeHead, data, self->bufferSize - self->writeHead);
    memcpy(self->buffer, data, (writeEnd % self->bufferSize) + 1);
  } else {
    // Directly do single memcpy call as the region
    // not wrapping back into the front
    // [readHead, readHead + size] not crossing boundry of bufferSize
    memcpy(self->buffer + self->writeHead, data, size);
  }
  
  self->writeHead += size;
  self->writeHead %= self->bufferSize;
  pthread_mutex_unlock(&self->lock);
  
  pthread_cond_broadcast(&self->lockCond);
  return 0;
}

static int waitStub(struct circular_buffer* self, int timeout, bool (*checkFunc)(struct circular_buffer*, size_t)) {
  int currentMs = timeout;

  pthread_mutex_lock(&self->lock);
  while (checkFunc(self, 1) && (currentMs > 0 || timeout == 0)) {
    pthread_cond_timedwait(&self->lockCond, &self->lock, util_milisec_to_timespec_ptr(1));
    util_msleep(1);
    if (timeout > 0)
      currentMs--;
  }
  
  if (checkFunc(self, 1))
    currentMs = -ETIMEDOUT;
  
  pthread_mutex_unlock(&self->lock);
  return currentMs;
}

int circular_buffer_wait_until_empty(struct circular_buffer* self, int timeout) {
  return waitStub(self, timeout, circular_buffer_can_read);
}

int circular_buffer_wait_until_full(struct circular_buffer* self, int timeout) {
  return waitStub(self, timeout, circular_buffer_can_write);
}

size_t circular_buffer_get_usage(struct circular_buffer* self) {
  return (self->bufferSize + self->writeHead - self->readHead) % self->bufferSize;
}

bool circular_buffer_is_full(struct circular_buffer* self) {
  return circular_buffer_get_usage(self) >= self->bufferSize - 1;
}

bool circular_buffer_can_write(struct circular_buffer* self, size_t size) {
  return circular_buffer_get_usage(self) <= self->bufferSize - size;
}

bool circular_buffer_can_read(struct circular_buffer* self, size_t size) {
  return circular_buffer_get_usage(self) >= size;
}

bool circular_buffer_is_empty(struct circular_buffer* self) {
  return circular_buffer_get_usage(self) == 0;
}
