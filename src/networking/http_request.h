#ifndef _headers_1667620472_FluffyLauncher_http_request_builder
#define _headers_1667620472_FluffyLauncher_http_request_builder

// HTTP/1.0 request builder and runner

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "vec.h"
#include "hashmap.h"

#define HTTP_PROTOCOL_VERSION_MAJOR_ONLY "HTTP/1"
#define HTTP_PROTOCOL_VERSION "HTTP/1.1"

struct http_headers;
struct transport;

enum http_method {
  HTTP_GET,
  HTTP_POST,
  HTTP_PUT,
  HTTP_PATCH,
  HTTP_DELETE
};

struct http_request {
  bool isMethodSet;
  enum http_method method;
  
  bool canFreeLocation;
  const char* location;
  
  struct http_headers* headers;
  
  size_t requestDataLen;
  void* requestData;
};

[[nodiscard]]
struct http_request* http_request_new();
void http_request_free(struct http_request* self);

// Send request
// Return 0 on success
// Negative on error:
// -ENOMEM: Not enough memory
// -ECONNREFUSED: Connection refused
// -EIO: I/O Error 
// -ETIMEDOUT: Timed out
// -ENETUNREACH: Network unreachable
// -ECONNRESET: Connection reset
// -EFAULT: Malformed server response
// -EINVAL: Invalid state
// -ENOTSUP: Server transfer encoding unsupported
[[nodiscard]]
int http_request_send(struct http_request* self, struct transport* transport);

void http_request_set_method(struct http_request* self, enum http_method method);
void http_request_set_location(struct http_request* self, const char* location);

[[nodiscard]]
int http_request_set_location_formatted(struct http_request* self, const char* location, ...);
[[nodiscard]]
int http_request_set_location_formatted_va(struct http_request* self, const char* location, va_list list);

#endif


