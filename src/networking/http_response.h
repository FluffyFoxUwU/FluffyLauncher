#ifndef _headers_1668922041_FluffyLauncher_http_response
#define _headers_1668922041_FluffyLauncher_http_response

#include <stddef.h>
#include <stdio.h>

struct transport;

struct http_response {
  int status;
  const char* description;
  size_t writtenSize;
  
  struct http_headers* headers;
};

struct http_response* http_response_new();
void http_response_free(struct http_response* self);

// Wait for request result
// Return http status code on success
// Errors:
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
int http_response_recv(struct http_response* self, struct transport* transport, FILE* writeTo);

#endif

