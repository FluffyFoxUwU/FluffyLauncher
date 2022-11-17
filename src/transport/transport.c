#include <errno.h>
#include "transport.h"

struct transport_private {
  int emulatedSockFD;
};

// TODO: Implement this and create socket faker
// by creating socket pair and have I/O threads
// to call appropriate method
static int impl_get_sockfd(struct transport* self) {
  return -ENOSYS;
}

int transport_base_init(struct transport* self, int timeoutMilis) {
  *self = (struct transport) {};
  self->timeoutMilis = timeoutMilis;
  self->get_sockfd = impl_get_sockfd;
  
  return 0;
}

void transport_base_close(struct transport* self) {
  
}

