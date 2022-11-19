#ifndef _headers_1667641752_FluffyLauncher_transport_socket
#define _headers_1667641752_FluffyLauncher_transport_socket

#include <stdint.h>
#include "transport.h"
#include "networking/networking.h"

struct transport_socket {
  struct transport super;
  struct ip_address connectAddr;
  int fd;
};

[[nodiscard]]
struct transport_socket* transport_socket_new(int timeoutMilis);
void transport_socket_free(struct transport_socket* self);

[[nodiscard]]
int transport_socket_connect(struct transport_socket* self, struct ip_address* addr);

#endif

