#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "bug.h"
#include "transport.h"
#include "transport_socket.h"
#include "util.h"

#define SELF(ptr) container_of(ptr, struct transport_socket, super)

static int impl_write(struct transport* _self, const void* data, size_t len);
static int impl_read(struct transport* _self, void* result, size_t len, size_t* szRead); 
static void impl_close(struct transport* _self);

struct transport_socket* transport_socket_new(int timeoutMilis) {
  struct transport_socket* self = malloc(sizeof(*self));
  if (!self)
    return NULL;
  
  transport_base_init(&self->super, timeoutMilis);
  self->super.close = impl_close;
  self->super.read = impl_read;
  self->super.write = impl_write;
  
  self->fd = -1;
  return self;
}

void transport_socket_free(struct transport_socket* self) {
  if (!self)
    return;
  
  // Keep trying to close if interrupted
  while (close(self->fd) == -1 && errno == -EINTR)
    ;
  free(self);
}

static int commonPerformIO(ssize_t (*action)(int,void*,size_t,int), int fd, void* data, size_t len, size_t* szProcessed) {
  int res = 0;
  size_t processedSize = 0;
    
  while (len > 0) {
    ssize_t processedCount = action(fd, data, len, 0);
    if (processedCount < 0) {
      // Retry if interrupted
      if (errno == EINTR)
        continue;
      
      res = -errno;
      break;
    }
    
    if (processedCount == 0) {
      res = -ENODATA;
      break;
    }
    
    len -= processedCount;
    processedSize += processedCount;
    data = &((char*) data)[processedCount];
  }
  
  switch (res) {
    case -ENETDOWN:
    case -ENETUNREACH:
    case -ENODATA:
    case -ECONNRESET:
    case -ETIMEDOUT:
      break;
    
    default:
      if (res < 0)
        res = -EFAULT;
      break;
  }
  
  if (szProcessed)
    *szProcessed = processedSize;
  
  return res;
}

int transport_socket_write(struct transport_socket* self, const void* data, size_t len) {
  if (self->fd < 0)
    return -EINVAL;
  return commonPerformIO((void*) send, self->fd, (void*) data, len, NULL);
}

int transport_socket_read(struct transport_socket* self, void* result, size_t len, size_t* szRead) {
  if (self->fd < 0)
    return -EINVAL;
  return commonPerformIO(recv, self->fd, result, len, szRead);
}

int transport_socket_connect(struct transport_socket* self, struct ip_address* addr) {
  self->connectAddr = *addr;
  int res = 0;
  int socketFd = networking_connect(addr, NETWORKING_TCP);
  if (socketFd < 0) 
    return -errno;
  
  self->fd = socketFd;
  return res;
}

// Implementations 
static int impl_write(struct transport* _self, const void* data, size_t len) {
  return transport_socket_write(SELF(_self), data, len);
}

static int impl_read(struct transport* _self, void* result, size_t len, size_t* szRead) {
  return transport_socket_read(SELF(_self), result, len, szRead);
}

static void impl_close(struct transport* _self) {
  transport_socket_free(SELF(_self));
}

