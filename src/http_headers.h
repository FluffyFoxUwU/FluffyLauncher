#ifndef _headers_1667633530_FluffyLauncher_http_headers
#define _headers_1667633530_FluffyLauncher_http_headers

#include <stdbool.h>
#include <stdarg.h>

#include "hashmap.h"
#include "vec.h"
#include "buffer.h"

struct http_headers {
  HASHMAP(char, vec_str_t) headers;
};

enum http_headers_serialize_type {
  // Serialize to normal HTTP headers
  // the usual one which delimited by \r\n
  HTTP_HEADER_NORMAL
};

[[nodiscard]]
struct http_headers* http_headers_new();
void http_headers_free(struct http_headers* self);

[[nodiscard]]
int http_headers_add(struct http_headers* self, const char* name, const char* content);

[[nodiscard]]
int http_headers_addf(struct http_headers* self, const char* name, const char* contentFmt, ...);

[[nodiscard]]
int http_headers_addf_va(struct http_headers* self, const char* name, const char* contentFmt, va_list list);

[[nodiscard]]
int http_headers_set(struct http_headers* self, const char* name, const char* content);

[[nodiscard]]
int http_headers_setf(struct http_headers* self, const char* name, const char* contentFmt, ...);

[[nodiscard]]
int http_headers_setf_va(struct http_headers* self, const char* name, const char* contentFmt, va_list list);

// Return non-empty header or NULL if not found or empty
vec_str_t* http_headers_get(struct http_headers* self, const char* name);

// Return serialized header
[[nodiscard]]
buffer_t* http_headers_serialize(struct http_headers* self, enum http_headers_serialize_type type);

#endif

