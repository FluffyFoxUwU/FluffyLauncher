#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include "buffer.h"
#include "bug.h"
#include "hashmap.h"
#include "hashmap_base.h"
#include "json.h"
#include "vec.h"
#include "util/util.h"

struct json_object* json_new_object() {
  struct json_object* self = malloc(sizeof(*self));
  if (!self)
    return NULL;
  *self = (struct json_object) {};
  self->node.type = JSON_OBJECT;
  
  hashmap_init(&self->members, util_hash_buffer, util_compare_buffer);
  json_set_managed(&self->node);
  return self; 
}

void json_set_unmanaged(struct json_node* node, json_finalizer_func finalizer, void* udata) {
  node->isUnmananged = true;
  node->udata = udata;
  node->finalizer = finalizer;
  
  if (node->type == JSON_OBJECT)
    hashmap_set_key_alloc_funcs(&JSON_OBJECT(node)->members, NULL, NULL);
}

void json_set_managed(struct json_node* node) {
  node->isUnmananged = false;
  node->finalizer = NULL;
  
  if (node->type == JSON_OBJECT)
    hashmap_set_key_alloc_funcs(&JSON_OBJECT(node)->members, util_clone_buffer, buffer_free);
}

struct json_array* json_new_array() {
  struct json_array* self = malloc(sizeof(*self));
  if (!self)
    return NULL;
  *self = (struct json_array) {};
  self->node.type = JSON_ARRAY;
  
  vec_init(&self->array);
  json_set_managed(&self->node);
  return self;
}

#define trivial_constructor(ret, name, tag, dataType, dataField) \
ret* name(dataType val) { \
  ret* self = malloc(sizeof(*self)); \
  if (!self) \
    return NULL; \
  *self = (ret) {}; \
  self->node.type = tag; \
  self->dataField = val; \
  json_set_managed(&self->node); \
  return self; \
}
#define trivial_deconstructor(T, name) \
static void name(T* val) { \
  free(val); \
}

trivial_constructor(struct json_boolean, json_new_boolean, JSON_BOOLEAN, bool, boolean);
trivial_constructor(struct json_number, json_new_number, JSON_NUMBER, double, number);
trivial_deconstructor(struct json_boolean, freeBoolean);
trivial_deconstructor(struct json_number, freeNumber);

trivial_constructor(struct json_string, json_new_string, JSON_STRING, buffer_t*, string);
static void freeString(struct json_string* val) { 
  if (!val->node.isUnmananged)
    buffer_free(val->string);
  free(val); 
}

struct json_null* json_new_null() {
  struct json_null* self = malloc(sizeof(*self));
  if (!self)
    return NULL;
  *self = (struct json_null) {};
  self->node.type = JSON_NULL;
  return self;
}
trivial_deconstructor(struct json_null, freeNull);

#undef trivial_constructor

static void freeArray(struct json_array* array) {
  int i;
  struct json_node* item;
  vec_foreach(&array->array, item, i)
    json_free(item);
  vec_deinit(&array->array);
  free(array);
}

static void freeObject(struct json_object* array) {
  const buffer_t* key;
  struct json_node* value;
  hashmap_foreach(key, value, &array->members) 
    json_free(value);
  
  hashmap_cleanup(&array->members);
  free(array);
}

void json_free(struct json_node* self) {
  if (!self)
    return;
  
  if (self->isUnmananged && self->finalizer)
    self->finalizer(self, self->udata);
  
  switch (self->type) {
    case JSON_ARRAY:
      freeArray(JSON_ARRAY(self));
      break;
    case JSON_OBJECT:
      freeObject(JSON_OBJECT(self));
      break;
    case JSON_STRING:
      freeString(JSON_STRING(self));
      break;
    case JSON_BOOLEAN:
      freeBoolean(JSON_BOOLEAN(self));
      break;
    case JSON_NULL:
      freeNull(JSON_NULL(self));
      break;
    case JSON_NUMBER:
      freeNumber(JSON_NUMBER(self));
      break;
  }
}

int json_set_member(struct json_node* self, const char* key, struct json_node* node) {
  buffer_t* keyBuffer;
  if (self->isUnmananged) {
    keyBuffer = buffer_new_with_string((char*) key);
    keyBuffer->alloc = NULL;
  } else {
    keyBuffer = buffer_new_with_copy((char*) key);
  }
  
  if (!keyBuffer)
    return -ENOMEM;
  
  int res = json_set_member_buffer(self, keyBuffer, node);
  buffer_free(keyBuffer);
  return res;
}

int json_set_member_buffer(struct json_node* self, buffer_t* key, struct json_node* node) {
  if (self->type != JSON_OBJECT)
    return -EINVAL;
  
  // Unconditional remove if it fail it fails (normally mean nothing exist)
  hashmap_remove(&JSON_OBJECT(self)->members, key);
  int res = json_set_member_buffer_no_overwrite(self, key, node);
  BUG_ON(res == -EEXIST);
  return res;
}

int json_set_member_no_overwrite(struct json_node* self, const char* key, struct json_node* node) {
  buffer_t* keyBuffer;
  if (self->isUnmananged) {
    keyBuffer = buffer_new_with_string((char*) key);
    keyBuffer->alloc = NULL;
  } else {
    keyBuffer = buffer_new_with_copy((char*) key);
  }
  
  int res = json_set_member_buffer_no_overwrite(self, keyBuffer, node);
  buffer_free(keyBuffer);
  return res;
}

int json_set_member_buffer_no_overwrite(struct json_node* self, buffer_t* key, struct json_node* node) {
  if (self->type != JSON_OBJECT)
    return -EINVAL;
  
  int res = hashmap_put(&JSON_OBJECT(self)->members, key, node);
  if (res == -EADDRNOTAVAIL || res == -ENODATA)
    res = -ENODATA;
  
  return res;
}

int json_get_member(struct json_node* self, const char* key, struct json_node** node) {
  if (self->type != JSON_OBJECT)
    return -EINVAL;
  
  buffer_t* buff = buffer_new_with_string((char*) key);
  if (!buff)
    return -ENOMEM;
  buff->alloc = NULL;
  
  int res = json_get_member_buffer(self, buff, node);
  buffer_free(buff);
  return res;
}

int json_get_member_buffer(struct json_node* self, buffer_t* key, struct json_node** nodePtr) {
  if (self->type != JSON_OBJECT)
    return -EINVAL;
  
  struct json_node* node = hashmap_get(&JSON_OBJECT(self)->members, key);
  if (!node)
    return -ENODATA;
  
  if (nodePtr)
    *nodePtr = node;
  return 0;
}
