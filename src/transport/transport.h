#ifndef _headers_1667641607_FluffyLauncher_transport_base
#define _headers_1667641607_FluffyLauncher_transport_base

#include "buffer.h"

struct transport {
  int timeoutMilis;

  int (*write)(struct transport* self, const void* data, size_t len);
  int (*read)(struct transport* self, void* result, size_t len, size_t* szRead); 
  void (*close)(struct transport* self); 
};

void transport_base_init(struct transport* self, int timeoutMilis);

#endif

