#ifndef _headers_1669806346_FluffyLauncher_abstract_json
#define _headers_1669806346_FluffyLauncher_abstract_json

#include <stddef.h>
#include <stdbool.h>

#include "buffer.h"
#include "hashmap.h"
#include "vec.h"
#include "util/util.h"

// JSON (also can handle NUL bytes in keys or values)
// So that i can swap JSON encoder/decoder implementations
// seperately

enum json_type {
  JSON_NULL,
  JSON_STRING,
  JSON_NUMBER,
  JSON_ARRAY,
  JSON_OBJECT,
  JSON_BOOLEAN
};

struct json_node {
  struct json_node* parent;
  enum json_type type; 
};

struct json_boolean {
  struct json_node node;
  bool boolean;
};

struct json_number {
  struct json_node node;
  double number;
};

struct json_string {
  struct json_node node;
  buffer_t* string;
};

struct json_object {
  struct json_node node;
  HASHMAP(buffer_t, struct json_node) members;
};

struct json_array {
  struct json_node node;
  vec_t(struct json_node*) array;
};

struct json_null {
  struct json_node node;
};

struct json_boolean* json_new_boolean(bool val);
struct json_number* json_new_double(double val);
struct json_string* json_new_string(buffer_t* val);
struct json_object* json_new_object();
struct json_array* json_new_array();
struct json_null* json_new_null();

void json_free(struct json_node* self);

// 0 on sucess
// Errors:
// -EINVAL: Incorrect type
// -ENODATA: Not found

// These copies the key
int json_set_member(struct json_node* self, const char* key, struct json_node* node);
int json_set_member_buffer(struct json_node* self, buffer_t* key, struct json_node* node);

int json_get_member(struct json_node* self, const char* key, struct json_node** node);
int json_get_member_buffer(struct json_node* self, buffer_t* key, struct json_node** node);

#define JSON_OBJECT(x) container_of(x, struct json_object, node)
#define JSON_ARRAY(x) container_of(x, struct json_array, node)
#define JSON_NUMBER(x) container_of(x, struct json_number, node)
#define JSON_STRING(x) container_of(x, struct json_string, node)
#define JSON_NULL(x) container_of(x, struct json_null, node)
#define JSON_BOOLEAN(x) container_of(x, struct json_boolean, node)

#endif

