#include <errno.h>

#include "easy.h"
#include "networking.h"
#include "transport/transport.h"
#include "transport/transport_socket.h"
#include "transport/transport_ssl.h"

int networking_easy_new_connection(bool isSecure, const char* hostname, uint16_t port, struct transport** result) {
  int res = 0;
  struct ip_address ip;
  struct transport* transportResult = NULL;
  int resolveRes = networking_resolve(hostname, &ip, NETWORKING_RESOLVE_PREFER_IPV4);
  if (resolveRes < 0)
    return -EFAULT;
  
  ip.port = port;
  
  struct transport_socket* socketTransport = transport_socket_new(1000);
  if (socketTransport == NULL) {
    res = -ENOMEM;
    goto socket_transport_creation_error;
  }
  transportResult = &socketTransport->super;
  
  if (transport_socket_connect(socketTransport, &ip) < 0) {
    res = -EFAULT;
    goto connect_error;
  }
  
  if (!isSecure) {
    transportResult = &socketTransport->super;
    goto ssl_not_needed;
  }
  
  struct transport_ssl* sslTransport = transport_ssl_new(&socketTransport->super, true);
  if (sslTransport == NULL) {
    res = -ENOMEM;
    goto ssl_transport_creation_error;
  }
  transportResult = &sslTransport->super;
  
  if (transport_ssl_connect(sslTransport, TRANSPORT_TLS_ANY) < 0) {
    res = -EFAULT;
    goto ssl_connect_error;
  }
  
  if (transport_ssl_verify_host(sslTransport) == false) {
    res = -EFAULT;
    goto verify_host_error;
  }

verify_host_error:
ssl_connect_error: 
ssl_transport_creation_error:
connect_error:
  if (res < 0)
    transportResult->close(transportResult);
socket_transport_creation_error:
ssl_not_needed:
  *result = transportResult;
  return res;
}
