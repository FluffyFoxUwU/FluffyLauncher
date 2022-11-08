#include <stdbool.h>
#include <stdio.h>

#include "buffer.h"
#include "hashmap.h"
#include "http_headers.h"
#include "http_headers_serializer/normal.h"
#include "vec.h"

buffer_t* http_headers_serialize_normal(struct http_headers* self) {
  buffer_t* buffer = buffer_new();
  if (!buffer)
    goto buffer_alloc_failure;
  
  const char* name = NULL;
  vec_str_t* list = NULL;
  hashmap_foreach(name, list, &self->headers) {
    int i = 0;
    const char* str = NULL;
    vec_foreach(list, str, i) 
      buffer_appendf(buffer, "%s: %s\r\n", name, str); 
  }
buffer_alloc_failure:
  return buffer;
}
