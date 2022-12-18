#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>

#include "buffer.h"
#include "json_schema_loader.h"
#include "parser/json/json.h"
#include "logging/logging.h"
#include "bug.h"
#include "panic.h"
#include "util/util.h"
#include "util/checks.h"
#include "vec.h"

// Checking to remind if anyone changes types
// and forgot to modify this file
check_struct_field(struct json_number, number, double);
check_struct_field(struct json_string, string, buffer_t*);
check_struct_field(struct json_boolean, boolean, bool);
check_struct_field(struct json_array, array.data[0], struct json_node*);
check_struct_field(struct json_object, members.map_types->t_data, struct json_node*);
check_struct_field(struct json_object, members.map_types->t_key, const buffer_t*);

// This can do panics as normally schema's be hardcoded
//
// *lastPtr contain pointer to '['
static int32_t getArrayIndexFromDescriptor(char* descriptor, char** lastPtr) {
  int64_t res = 0;
  size_t len = strlen(descriptor);
  char* current = &descriptor[len - 1];
  
  if (*current != ']')
    return -EINVAL;
  current--;
  
  int64_t digitShift = 1;
  int64_t digitIndex = 0;
  while (*current != '[') {
    if (current == descriptor) 
      panic("Unmatched [] pair!");
    
    if (*current < '0' || *current > '9')  
      panic("Invalid array index!");
    
    int currentDigit = *current - '0';
    // 10 is digits in 32-bit signed limit
    if (digitIndex > 10)
      panic("Array index too large!");
    
    res += currentDigit * digitShift;
    digitIndex++;
    digitShift *= 10;
    
    current--;
  }
  
  if (res > INT32_MAX) 
    panic("Array index too large!");
  
  if (lastPtr)
    *lastPtr = current;
  return (int32_t) res;
}

static int processPath(struct json_node* json, const char* _path, struct json_node** result) {
  int res = 0;
  char* path = strdup(_path);
  if (!path) {
    res = -ENOMEM;
    goto failed_clone_path;
  }
  
  // Traverse the path
  struct json_node* currentNode = json;
  char* strtokLastPtr = NULL;
  char* field = strtok_r(path, ".", &strtokLastPtr);
  if (strcmp(field, "$") != 0)
    panic("'$' expected for beginning of path!");
  
  void raise(int); 
  do {
    if (strcmp(field, "$") == 0)
      continue;
    
    char* last = NULL;
    int32_t arrayIndex = getArrayIndexFromDescriptor(field, &last);
    
    // Neat trick to convert arr[0] to arr\00] so that
    // next code sees arr instead to get the array field then
    // we can access it as array
    if (arrayIndex >= 0)
      *last = '\0';
    
    if (strcmp(field, "$") != 0) {
      if (currentNode->type != JSON_OBJECT) {
        res = -EINVAL;
        goto failed_load;
      }
      
      struct json_node* node;
      res = json_get_member(currentNode, field, &node);
      if (res < 0) {
        res = -EINVAL;
        goto failed_load;
      }
      currentNode = node;
    }
    
    if (arrayIndex >= 0) {
      if (currentNode->type != JSON_ARRAY) {
        res = -EINVAL;
        goto failed_load;
      }
      
      struct json_array* arr = JSON_ARRAY(currentNode);
      if (arrayIndex >= arr->array.length) {
        res = -EINVAL;
        goto failed_load;
      }
      currentNode = arr->array.data[arrayIndex];
    }
    
    if (currentNode == NULL) {
      res = -EINVAL;
      goto failed_load;
    }
  } while ((field = strtok_r(NULL, ".", &strtokLastPtr)));
  
  if (result)
    *result = currentNode;

failed_load:
  free(path);
failed_clone_path:
  return res;
}

// Can panic as schemas are normally hardcoded not dynamicly generated
// therefore this can panic
int json_schema_load(const struct json_schema* schema, struct json_node* json, void* result) {
  int res = 0;
  
  for (int i = 0; schema->entries[i].path; i++) {
    const struct json_schema_entry* current = &schema->entries[i];
    const char* path = current->path;
    
    struct json_node* node = NULL;
    if ((res = processPath(json, path, &node)) < 0)
      return res;
    
    if (node->type != current->type)
      return -EINVAL;
    
    void* writeAt = result + current->fieldOffset;
    switch (current->type) {
      case JSON_STRING:
        BUG_ON(current->fieldSize != sizeof(buffer_t*));
        *(buffer_t**) writeAt = JSON_STRING(node)->string;
        break;
      case JSON_NUMBER:
        BUG_ON(current->fieldSize != sizeof(double));
        *(double*) writeAt = JSON_NUMBER(node)->number;
        break;
      case JSON_BOOLEAN:
        BUG_ON(current->fieldSize != sizeof(bool));
        *(bool*) writeAt = JSON_BOOLEAN(node)->boolean;
        break;
      case JSON_ARRAY:
        BUG_ON(current->fieldSize != sizeof(struct json_array*));
        *(struct json_array**) writeAt = JSON_ARRAY(node);
        break;
      case JSON_OBJECT:
        BUG_ON(current->fieldSize != sizeof(struct json_object*));
        *(struct json_object**) writeAt = JSON_OBJECT(node);
        break;
      default:
        panic("Invalid type in schema!");
    }
  }
  return res;
}
