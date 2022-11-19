#ifndef _headers_1667913027_FluffyLauncher_transport_ssl
#define _headers_1667913027_FluffyLauncher_transport_ssl

#include <stdbool.h>

#include "transport.h"

struct transport_socket;
struct transport_ssl {
  struct transport super;
  struct transport* transportLayer;
  
  // Abstract detail away as ssl.h drag too many stuff than i would like
  struct transport_ssl_private* priv;
};

// Each protocol version sorted by well version
// and each protocol sorted by its strength (like 
// SSL now insecure than TLS relatively)
enum ssl_version {
  TRANSPORT_SSL_UNKNOWN,
  TRANSPORT_SSL_ANY, 
  TRANSPORT_SSL_V3 = TRANSPORT_SSL_ANY,
  TRANSPORT_SSL_MAX = TRANSPORT_SSL_V3,
  
  TRANSPORT_TLS_ANY,
  TRANSPORT_TLS_V1_0 = TRANSPORT_TLS_ANY,
  TRANSPORT_TLS_V1_1,
  TRANSPORT_TLS_V1_2,
  TRANSPORT_TLS_V1_3,
  TRANSPORT_TLS_MAX = TRANSPORT_TLS_V1_3,
};

#define TRANSPORT_SSL_VERSIONS \
  X(TRANSPORT_SSL_V3, "SSLv3") \
   \
  X(TRANSPORT_TLS_V1_0, "TLSv1") \
  X(TRANSPORT_TLS_V1_1, "TLSv1.1") \
  X(TRANSPORT_TLS_V1_2, "TLSv1.2") \
  X(TRANSPORT_TLS_V1_3, "TLSv1.3") \

[[nodiscard]]
int transport_ssl_init();
void transport_ssl_cleanup();

[[nodiscard]]
struct transport_ssl* transport_ssl_new(struct transport* socket, bool verify);

// Errors:
// -ENOTSUP: Minimum SSL/TLS version requirement not met
// -EFAULT: Error doing handshake
// -EINVAL: Invalid min version
int transport_ssl_connect(struct transport_ssl* self, enum ssl_version minimumVersion);

bool transport_ssl_verify_host(struct transport_ssl* self);

bool transport_ssl_get_handshake_state(struct transport_ssl* self);
enum ssl_version transport_ssl_get_version(struct transport_ssl* self);

void transport_ssl_free(struct transport_ssl* self);

// Methods for `struct transport`
int transport_ssl_write(struct transport_ssl* self, const void* data, size_t len);
int transport_ssl_read(struct transport_ssl* self, void* result, size_t len, size_t* szRead); 
void transport_ssl_free(struct transport_ssl* self);

#endif

