#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "bug.h"
#include "buffer.h"
#include "http_headers.h"
#include "transport/transport.h"
#include "http_request.h"
#include "hashmap.h"
#include "util/util.h"
#include "vec.h"

struct http_request* http_request_new() {
  struct http_request* self = malloc(sizeof(*self));
  if (!self)
    return NULL;
  
  *self = (struct http_request) {};
  self->headers = http_headers_new();
  self->isMethodSet = false;
  self->canFreeLocation = false;
  if (!self->headers)
    goto headers_alloc_fail;
  
  return self;

headers_alloc_fail:
  http_request_free(self);
  return NULL;
}

void http_request_free(struct http_request* self) {
  if (!self)
    return;
  
  http_headers_free(self->headers);
  if (self->canFreeLocation)
    free((char*) self->location);
  free(self);
}

int http_request_set_location_formatted(struct http_request* self, const char* urlFmt, ...) {
  va_list list;
  va_start(list, urlFmt);
  int res = http_request_set_location_formatted_va(self, urlFmt, list);
  va_end(list);
  return res;
}

void http_request_set_location(struct http_request* self, const char* location) {
  if (self->canFreeLocation)
    free((char*) self->location);
  
  self->location = location;
  self->canFreeLocation = false;
}

static bool isValidLocation(const char* location) {
  // https://0mg.github.io/tools/uri/
  static char lookup[256] = {
    ['t'] = true,  ['h'] = true,  ['e'] = true,  ['l'] = true,  ['a'] = true,  ['z'] = true,  ['y'] = true,  ['q'] = true, 
    ['u'] = true,  ['i'] = true,  ['c'] = true,  ['k'] = true,  ['f'] = true,  ['o'] = true,  ['x'] = true,  ['j'] = true, 
    ['m'] = true,  ['p'] = true,  ['s'] = true,  ['v'] = true,  ['r'] = true,  ['b'] = true,  ['w'] = true,  ['n'] = true, 
    ['d'] = true,  ['g'] = true,  ['T'] = true,  ['H'] = true,  ['E'] = true,  ['L'] = true,  ['A'] = true,  ['Z'] = true, 
    ['Y'] = true,  ['Q'] = true,  ['U'] = true,  ['I'] = true,  ['C'] = true,  ['K'] = true,  ['F'] = true,  ['O'] = true, 
    ['X'] = true,  ['J'] = true,  ['M'] = true,  ['P'] = true,  ['S'] = true,  ['V'] = true,  ['R'] = true,  ['B'] = true, 
    ['W'] = true,  ['N'] = true,  ['D'] = true,  ['G'] = true,  ['0'] = true,  ['1'] = true,  ['2'] = true,  ['3'] = true, 
    ['4'] = true,  ['5'] = true,  ['6'] = true,  ['7'] = true,  ['8'] = true,  ['9'] = true,  ['-'] = true,  ['.'] = true, 
    ['_'] = true,  ['~'] = true,  ['!'] = true,  ['$'] = true,  ['&'] = true,  ['('] = true,  [')'] = true,  ['*'] = true, 
    ['+'] = true,  [','] = true,  [';'] = true,  ['='] = true,  [':'] = true,  ['@'] = true,  ['/'] = true,  ['?'] = true,
    ['\''] = true
  };
  
  // Must be absolute path
  if (location[0] != '/')
    return -EINVAL;
  
  while (*location != '\0') { 
    if (!lookup[(int) *location])
      return false;
    
    location++;
  }
  
  return true;
}

int http_request_set_location_formatted_va(struct http_request* self, const char* urlFmt, va_list list) {
  util_asprintf((char**) &self->location, urlFmt, list);
  if (!self->location)
    return -ENOMEM;
  
  if (!isValidLocation(self->location)) {
    free((char*) self->location);
    self->location = NULL;
    return -EINVAL;
  }
  
  self->canFreeLocation = true;
  return 0;
}

void http_request_set_method(struct http_request* self, enum http_method method) {
  self->method = method;
  self->isMethodSet = true; 
}

static const char* methodAsString(enum http_method method) {
  switch (method) {
    case HTTP_GET:
      return "GET";
    case HTTP_POST:
      return "POST";
    case HTTP_DELETE:
      return "DELETE";
    case HTTP_PATCH:
      return "PATCH";
    case HTTP_PUT:
      return "PUT";
  }
  
  BUG();
}

static int sendRequest(struct http_request* self, struct transport* transport, const char* httpVer) {
  int res = 0;
  buffer_t* requestPayload = buffer_new();
  if (!requestPayload) {
    res = -ENOMEM;
    goto request_buffer_alloc_failure;
  }
  
  buffer_t* requestHeader = http_headers_serialize(self->headers, HTTP_HEADER_NORMAL);
  if (!requestHeader) {
    res = -ENOMEM;
    goto serialize_request_header_failure;
  }
  
  res = buffer_appendf(requestPayload, "%s %s %s\r\n%s\r\n", methodAsString(self->method), self->location, httpVer, buffer_string(requestHeader));
  buffer_free(requestHeader);
  if (res < 0) {
    res = -ENOMEM;
    goto payload_creation_failure;
  }
  
  // Sending request
  res = transport->write(transport, buffer_string(requestPayload), buffer_length(requestPayload));
  buffer_free(requestPayload);
  if (res < 0) 
    goto write_request_payload_failure;
  
  // Sending request body if exist
  if (self->requestData) 
    if ((res = transport->write(transport, self->requestData, self->requestDataLen)) < 0) 
      goto write_request_payload_failure; 
write_request_payload_failure:
payload_creation_failure:
serialize_request_header_failure:
request_buffer_alloc_failure:
  return res;
}



int http_request_send(struct http_request* self, struct transport* transport) {
  if (self->location == NULL || !self->isMethodSet) 
    return -EINVAL;
  
  int res = 0;
  // Sending request
  if ((res = sendRequest(self, transport, HTTP_PROTOCOL_VERSION)) < 0)
    goto send_request_failure; 

send_request_failure:
  return res;
}





