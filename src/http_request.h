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

struct http_response {
  int status;
  const char* description;
  size_t writtenSize;
  
  struct http_headers* headers;
};

[[nodiscard]]
struct http_request* http_new_request();
void http_free_request(struct http_request* self);

// Free response allocated by http_exec
void http_free_response(struct http_response* self);

// Return http response code on success
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
int http_exec(struct http_request* self, struct transport* transport, struct http_response** response, FILE* writeTo);

void http_set_method(struct http_request* self, enum http_method method);
void http_set_location(struct http_request* self, const char* location);

[[nodiscard]]
int http_set_location_formatted(struct http_request* self, const char* location, ...);
[[nodiscard]]
int http_set_location_formatted_va(struct http_request* self, const char* location, va_list list);

#endif


