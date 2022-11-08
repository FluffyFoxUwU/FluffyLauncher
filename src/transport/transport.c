#include "transport.h"

void transport_base_init(struct transport* self, int timeoutMilis) {
  *self = (struct transport) {};
  self->timeoutMilis = timeoutMilis;
}
