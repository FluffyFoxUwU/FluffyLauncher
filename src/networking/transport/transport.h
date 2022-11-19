#ifndef _headers_1667641607_FluffyLauncher_transport_base
#define _headers_1667641607_FluffyLauncher_transport_base

#include <stddef.h>

struct transport {
  int timeoutMilis;

  int (*write)(struct transport* self, const void* data, size_t len);
  int (*read)(struct transport* self, void* result, size_t len, size_t* szRead); 
  void (*close)(struct transport* self); 
 
  // Override this to avoid creation of emulated fds
  int (*get_sockfd)(struct transport* self);
  
  struct transport_private* private;
};

[[nodiscard]]
int transport_base_init(struct transport* self, int timeoutMilis);
void transport_base_close(struct transport* self);

#endif

