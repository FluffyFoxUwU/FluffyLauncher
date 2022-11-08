#ifndef _headers_1667641752_FluffyLauncher_transport_socket
#define _headers_1667641752_FluffyLauncher_transport_socket

#include <stdint.h>
#include "transport.h"
#include "networking.h"

struct transport_socket {
  struct transport super;
  struct ip_address connectAddr;
  int fd;
};

struct transport_socket* transport_socket_new(int timeoutMilis);
int transport_socket_connect(struct transport_socket* self, struct ip_address* addr);
void transport_socket_free(struct transport_socket* self);

int transport_socket_write(struct transport_socket* self, const void* data, size_t len);
int transport_socket_read(struct transport_socket* self, void* result, size_t len, size_t* szRead); 
void transport_socket_close(struct transport_socket* self);

#endif

