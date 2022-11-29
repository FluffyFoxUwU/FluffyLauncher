#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#include "bug.h"
#include "easy.h"
#include "http_headers.h"
#include "http_request.h"
#include "networking.h"
#include "networking/http_request.h"
#include "networking/http_response.h"
#include "parser/sjson.h"
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

int networking_easy_new_http_va(struct http_request** requestPtr, enum http_method method, const char* hostname, const char* location, struct easy_http_headers* headers, const char* requestBodyFormat, va_list args) {
  struct http_request* req = http_request_new();
  int res = 0;
  
  req->requestData = NULL;
  req->requestDataLen = 0;
  if (requestBodyFormat) {
    char* requestData = NULL;
    size_t requestDataLen = util_vasprintf(&requestData, requestBodyFormat, args);
    req->requestDataLen = requestDataLen;
    req->requestData = requestData;
    
    if (!requestData)
      goto request_body_alloc_error;
    
    char* requestLengthString = NULL;
    util_asprintf(&requestLengthString, "%zu", req->requestDataLen);
    if (requestLengthString == NULL)
      goto request_preparation_error;
    
    res = http_headers_set(req->headers, "Content-Length", requestLengthString);
    free(requestLengthString);
    
    if (res < 0) 
      goto request_preparation_error;
  }
  
  http_request_set_location(req, location);
  http_request_set_method(req, method);
  
  if (hostname && (res = http_headers_set(req->headers, "Host", hostname) < 0)) 
    goto request_preparation_error;

  for (int i = 0; headers[i].key && headers[i].value; i++) {
    const char* key = headers[i].key;
    const char* value = headers[i].value;
    
    if ((res = http_headers_set(req->headers, key, value) < 0)) 
      goto request_preparation_error;
  }

request_preparation_error:
  if (res < 0) {
    free((char*) req->requestData);
    req->requestData = NULL;
    req->requestDataLen = 0;
  }
request_body_alloc_error:
  if (!requestPtr || res < 0) {
    free((char*) req->requestData);
    http_request_free(req);
    req = NULL;
  }
  
  if (requestPtr)
    *requestPtr = req;
  return res;
}

int networking_easy_new_http(struct http_request** result,
                             enum http_method method, 
                             const char* hostname, 
                             const char* location, 
                             struct easy_http_headers* headers,
                             const char* requestBodyFormat,
                             ...) {
  va_list args;
  va_start(args, requestBodyFormat);
  int res = networking_easy_new_http_va(result, method, hostname, location, headers, requestBodyFormat, args);
  va_end(args);
  return res;
}

int networking_easy_do_json_http_rpc(struct sjson_context** jsonCtx, 
                                     struct sjson_node** root, 
                                     bool isSecure,
                                     enum http_method method, 
                                     const char* hostname, 
                                     const char* location, 
                                     struct easy_http_headers* headers,
                                     const char* requestBodyFormat,
                                     ...) {
  va_list args;
  va_start(args, requestBodyFormat);
  int res = networking_easy_do_json_http_rpc_va(jsonCtx, root, isSecure, method, hostname, location, headers, requestBodyFormat, args);
  va_end(args);
  return res;
}

int networking_easy_do_http(void** response, 
                            size_t* responseLength, 
                            bool isSecure,
                            enum http_method method, 
                            const char* hostname, 
                            const char* location, 
                            struct easy_http_headers* headers,
                            const char* requestBodyFormat,
                            ...) {
  va_list args;
  va_start(args, requestBodyFormat);
  int res = networking_easy_do_http_va(response, responseLength, isSecure, method, hostname, location, headers, requestBodyFormat, args);
  va_end(args);
  return res;
}

int networking_easy_do_http_va(void** responseBody, 
                            size_t* responseBodyLength, 
                            bool isSecure,
                            enum http_method method, 
                            const char* hostname, 
                            const char* location, 
                            struct easy_http_headers* headers,
                            const char* requestBodyFormat,
                            va_list args) {
  int res = 0;
  struct http_request* req;
  if ((res = networking_easy_new_http_va(&req, method, hostname, location, headers, requestBodyFormat, args)) < 0)
    goto create_request_error;
  
  FILE* memfd = open_memstream((char**) responseBody, responseBodyLength);
  if (!memfd) {
    res = -ENOMEM;
    goto memfd_open_error;
  }
  
  struct transport* connection;
  if ((res = networking_easy_new_connection(isSecure, hostname, isSecure ? 443 : 80, &connection)) < 0)
    goto connect_error;
  
  if ((res = http_request_send(req, connection)) < 0)
    goto send_error;
  if ((res = http_response_recv(NULL, connection, memfd)) < 0)
    goto receive_error;

receive_error:
send_error:
  connection->close(connection);
connect_error:
  fclose(memfd);
memfd_open_error:
  free((char*) req->requestData);
  http_request_free(req);
create_request_error:
  return res;
}

int networking_easy_do_json_http_rpc_va(struct sjson_context** jsonCtxPtr, 
                                     struct sjson_node** rootPtr, 
                                     bool isSecure,
                                     enum http_method method, 
                                     const char* hostname, 
                                     const char* location, 
                                     struct easy_http_headers* headers,
                                     const char* requestBodyFormat,
                                     va_list args) {
  void* responseBody = NULL;
  size_t responseBodyLen = 0;
  sjson_context* ctx = NULL;
  sjson_node* root = NULL;
  int res = 0;
  if ((res = networking_easy_do_http_va(&responseBody, &responseBodyLen, isSecure, method, hostname, location, headers, requestBodyFormat, args)) < 0)
    goto request_error;

  ctx = sjson_create_context(0, 0, NULL);
  if (!ctx) {
    res = -ENOMEM;
    goto fail_to_create_context;
  }
  
  root = sjson_decode(ctx, responseBody);
  if (!root) {
    pr_error("Failed parsing response from %s://%s/%s", isSecure ? "https" : "http", hostname, location);
    res = -EFAULT;
    goto fail_parsing;
  }

fail_parsing:
fail_to_create_context:
  free(responseBody);
request_error:
  if (jsonCtxPtr) {
    *jsonCtxPtr = ctx;
    *rootPtr = root;
  } else {
    sjson_destroy_context(ctx);
  }  
  return res;
}
