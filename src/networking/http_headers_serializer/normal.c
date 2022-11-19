#include <stdbool.h>
#include <stdio.h>

#include "buffer.h"
#include "hashmap.h"
#include "http_headers.h"
#include "http_headers_serializer/normal.h"
#include "list.h"
#include "vec.h"

buffer_t* http_headers_serialize_normal(struct http_headers* self) {
  buffer_t* buffer = buffer_new();
  if (!buffer)
    goto buffer_alloc_failure;
  
  list_node_t* current = self->insertOrder->head;
  while (current) {
    const char* name = current->val;
    vec_str_t* list = hashmap_get(&self->headers, name);
    int i = 0;
    const char* str = NULL;
    
    vec_foreach(list, str, i) 
      buffer_appendf(buffer, "%s: %s\r\n", name, str);
    
    current = current->next;
  }
buffer_alloc_failure:
  return buffer;
}
