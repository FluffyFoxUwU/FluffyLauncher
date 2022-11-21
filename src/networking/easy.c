#include <errno.h>

#include "easy.h"
#include "http_headers.h"
#include "http_request.h"
#include "networking.h"
#include "transport/transport.h"
#include "transport/transport_socket.h"
#include "transport/transport_ssl.h"
#include "logging/logging.h"
#include "util/util.h"

int networking_easy_new_connection(bool isSecure, const char* hostname, uint16_t port, struct transport** result) {
  int res = 0;
  struct ip_address ip;
  struct transport* transportResult = NULL;
  if ((res = networking_resolve(hostname, &ip, NETWORKING_RESOLVE_PREFER_IPV4)) < 0)
    goto resolve_error;
  
  ip.port = port;
  
  struct transport_socket* socketTransport = transport_socket_new(1000);
  if (socketTransport == NULL) {
    res = -ENOMEM;
    goto socket_transport_creation_error;
  }
  transportResult = &socketTransport->super;
  
  if ((res = transport_socket_connect(socketTransport, &ip)) < 0) 
    goto connect_error;
  
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
  
  if ((res = transport_ssl_connect(sslTransport, TRANSPORT_TLS_ANY)) < 0) 
    goto ssl_connect_error;
  
  if (transport_ssl_verify_host(sslTransport) == false) {
    res = -EFAULT;
    goto verify_host_error;
  }

verify_host_error:
ssl_connect_error: 
ssl_transport_creation_error:
connect_error:
  if (res < 0) {
    transportResult->close(transportResult);
    transportResult = NULL;
  }
socket_transport_creation_error:
resolve_error:
  if (res < 0) 
    pr_error("Cant connect to %s:%d (Reason: %d)", hostname, port, res);
ssl_not_needed:
  *result = transportResult;
  return res;
}

struct http_request* networking_easy_new_http(int* status, enum http_method method, const char* hostname, const char* location, const void* requestBody, size_t requestBodySize, const char* requestBodyType, const char* accept) {
  struct http_request* req = http_request_new();
  int res = 0;
  
  req->requestData = requestBody;
  req->requestDataLen = requestBodySize;

  http_request_set_location(req, location);
  http_request_set_method(req, HTTP_POST);
  
  if (requestBody) {
    char* requestLengthString = NULL;
    util_asprintf(&requestLengthString, "%zu", requestBodySize);
    if (requestLengthString == NULL)
      goto request_preparation_error;
    
    res = http_headers_set(req->headers, "Content-Length", requestLengthString);
    free(requestLengthString);
    
    if (res < 0)
      goto request_preparation_error;
  }
  
  if (accept && (res = http_headers_set(req->headers, "Accept", accept) < 0)) 
    goto request_preparation_error;
  
  if (hostname && (res = http_headers_set(req->headers, "Host", hostname) < 0)) 
    goto request_preparation_error;
  
  if (requestBodyType && (res = http_headers_set(req->headers, "Content-Type", requestBodyType) < 0)) 
    goto request_preparation_error;
  
request_preparation_error:
  if (res < 0) {
    http_request_free(req);
    req = NULL;
  }
  
  if (status)
    *status = res;
  return req;
}
