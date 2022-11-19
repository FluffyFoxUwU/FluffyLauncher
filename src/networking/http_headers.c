#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "http_headers.h"
#include "list.h"
#include "util.h"
#include "hashmap.h"
#include "http_headers_serializer/normal.h"
#include "bug.h"
#include "vec.h"

struct http_headers* http_headers_new() {
  struct http_headers* self = malloc(sizeof(*self));
  if (!self)
    return NULL;
  
  hashmap_init(&self->headers, hashmap_hash_string, strcmp);
  self->headers.map_base.key_dup = (void* (*)(const void*)) strdup;
  self->headers.map_base.key_free = free;
  
  self->insertOrder = list_new();
  if (!self->insertOrder)
    goto failure;
  self->insertOrder->free = free;
    
  return self;

failure:
  http_headers_free(self);
  return NULL;
}

void http_headers_free(struct http_headers* self) {
  if (!self)
    return;
  
  vec_str_t* data;
  hashmap_foreach_data(data, &self->headers) {
    int i = 0;
    char* content;
    vec_foreach(data, content, i)
      free(content);
    
    vec_deinit(data);
    free(data);
  }
  
  hashmap_cleanup(&self->headers);
  
  list_destroy(self->insertOrder);
  free(self);
}

// `name` expect internal copy of name string
static vec_str_t* getHeaderAutoAlloc(struct http_headers* self, const char* name) {
  vec_str_t* res = NULL;
  vec_str_t* headers = hashmap_get(&self->headers, name);
  if (headers) {
    res = headers;
    goto lookup_hit;
  }
  
  char* copyOfName = strdup(name);
  if (!copyOfName)
    goto name_copy_error;
  
  list_node_t* node = list_node_new((char*) copyOfName);
  if (!node)
    goto node_alloc_failure;
  
  headers = malloc(sizeof(*headers));
  if (!headers)
    goto header_alloc_failure;
  vec_init(headers); 
  
  if (hashmap_put(&self->headers, name, headers) < 0)
    goto header_add_failure;
  list_rpush(self->insertOrder, node);
  
  res = headers;  
header_add_failure:
  if (!res)
    vec_deinit(headers);
header_alloc_failure:
node_alloc_failure:
name_copy_error:
lookup_hit:
  return res;
}

vec_str_t* http_headers_get(struct http_headers* self, const char* name) {
  vec_str_t* list = hashmap_get(&self->headers, name);
  if (list && list->length == 0)
    return NULL;
  return list;
}

int http_headers_addf(struct http_headers* self, const char* name, const char* contentFmt, ...) {
  va_list list;
  va_start(list, contentFmt);
  int res = http_headers_addf_va(self, name, contentFmt, list);
  va_end(list);
  return res;
}

static bool isValidHeaderNameAndContent(const char* name, const char* content) {
  // https://developers.cloudflare.com/rules/transform/request-header-modification/reference/header-format/
  static bool validNameCharLookup[256] = {
    ['-'] = true, ['_'] = true, ['q'] = true, ['u'] = true, ['i'] = true, ['c'] = true, ['k'] = true, ['l'] = true,
    ['a'] = true, ['z'] = true, ['y'] = true, ['f'] = true, ['o'] = true, ['x'] = true, ['j'] = true, ['p'] = true,
    ['s'] = true, ['v'] = true, ['e'] = true, ['r'] = true, ['b'] = true, ['w'] = true, ['n'] = true, ['d'] = true,
    ['g'] = true, ['Q'] = true, ['U'] = true, ['I'] = true, ['C'] = true, ['K'] = true, ['L'] = true, ['A'] = true,
    ['Z'] = true, ['Y'] = true, ['F'] = true, ['O'] = true, ['X'] = true, ['J'] = true, ['M'] = true, ['P'] = true,
    ['S'] = true, ['E'] = true, ['R'] = true, ['B'] = true, ['W'] = true, ['N'] = true, ['D'] = true, ['G'] = true,
    ['0'] = true, ['1'] = true, ['2'] = true, ['3'] = true, ['4'] = true, ['5'] = true, ['7'] = true, ['8'] = true,
    ['9'] = true, ['t'] = true, ['T'] = true, ['h'] = true, ['H'] = true, ['m'] = true, ['6'] = true, ['V'] = true
  };
  
  // https://developers.cloudflare.com/rules/transform/request-header-modification/reference/header-format/
  static bool validContentCharLookup[256] = {
    [' '] = true, [':'] = true, [';'] = true, ['.'] = true, [','] = true, ['/'] = true, ['"'] = true, ['?'] = true,
    ['!'] = true, ['('] = true, [')'] = true, ['{'] = true, ['}'] = true, ['['] = true, [']'] = true, ['@'] = true,
    ['<'] = true, ['>'] = true, ['='] = true, ['-'] = true, ['+'] = true, ['*'] = true, ['#'] = true, ['$'] = true,
    ['&'] = true, ['`'] = true, ['|'] = true, ['~'] = true, ['^'] = true, ['%'] = true, ['_'] = true, ['q'] = true,
    ['u'] = true, ['i'] = true, ['c'] = true, ['k'] = true, ['l'] = true, ['a'] = true, ['z'] = true, ['y'] = true,
    ['f'] = true, ['o'] = true, ['x'] = true, ['j'] = true, ['p'] = true, ['s'] = true, ['v'] = true, ['e'] = true,
    ['r'] = true, ['b'] = true, ['w'] = true, ['n'] = true, ['d'] = true, ['g'] = true, ['Q'] = true, ['U'] = true,
    ['I'] = true, ['C'] = true, ['K'] = true, ['L'] = true, ['A'] = true, ['Z'] = true, ['Y'] = true, ['F'] = true,
    ['O'] = true, ['X'] = true, ['J'] = true, ['M'] = true, ['P'] = true, ['S'] = true, ['E'] = true, ['R'] = true,
    ['B'] = true, ['W'] = true, ['N'] = true, ['D'] = true, ['G'] = true, ['0'] = true, ['1'] = true, ['2'] = true,
    ['3'] = true, ['4'] = true, ['5'] = true, ['7'] = true, ['8'] = true, ['9'] = true, ['t'] = true, ['T'] = true,
    ['h'] = true, ['H'] = true, ['\\'] = true, ['\''] = true, ['m'] = true, ['6'] = true, ['V'] = true
  };
  
  while (*name != '\0') { 
    if (!validNameCharLookup[(int) *name])
      return false;
    
    name++;
  }
  
  while (*content != '\0') { 
    if (!validContentCharLookup[(int) *content])
      return false;
    
    content++;
  }
  return true;
}

int http_headers_add(struct http_headers* self, const char* name, const char* content) {
  int res = 0;
  if (!isValidHeaderNameAndContent(name, content)) {
    res = -EINVAL;
    goto header_verification_failure;
  }
  
  char* copy = strdup(content);
  if (copy == NULL)
    return -ENOMEM;
  
  vec_str_t* headerList = getHeaderAutoAlloc(self, name);
  if (headerList == NULL) { 
    res = -ENOMEM;
    goto header_get_failure;
  }
  
  if (vec_push(headerList, copy) < 0) {
    res = -ENOMEM;
    goto add_header_failure;
  }

add_header_failure:
header_get_failure:
header_verification_failure:
  return res;
}

int http_headers_addf_va(struct http_headers* self, const char* name, const char* contentFmt, va_list list) {
  int res = 0; 
  char* content;
  util_asprintf(&content, contentFmt, list);
  if (!content)
    return -ENOMEM;
  
  res = http_headers_add(self, name, content);
  free(content);
  return res;
}

int http_headers_setf(struct http_headers* self, const char* name, const char* contentFmt, ...) {
  va_list list;
  va_start(list, contentFmt);
  int res = http_headers_setf_va(self, name, contentFmt, list);
  va_end(list);
  return res;
}

static void clearHeaderFor(struct http_headers* self, const char* name) {
  vec_str_t* headerList = getHeaderAutoAlloc(self, name);
  if (!headerList)
    return;
  
  int i = 0;
  const char* content = NULL;
  vec_foreach(headerList, content, i)
    free((char*) content);
  
  vec_clear(headerList);
  return;
}

int http_headers_setf_va(struct http_headers* self, const char* name, const char* contentFmt, va_list list) {
  clearHeaderFor(self, name); 
  return http_headers_addf_va(self, name, contentFmt, list);
}

int http_headers_set(struct http_headers* self, const char* name, const char* contentData) {
  clearHeaderFor(self, name);
  return http_headers_add(self, name, contentData);
}

buffer_t* http_headers_serialize(struct http_headers* self, enum http_headers_serialize_type type) {
  switch (type) {
    case HTTP_HEADER_NORMAL:
      return http_headers_serialize_normal(self);
    default:
      BUG();
  }
}
