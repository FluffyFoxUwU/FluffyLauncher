#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <threads.h>

#include "buffer.h"
#include "bug.h"
#include "cjson.h"
#include "parser/json/json.h"
#include "submodules/cJSON/cJSON.h"
#include "util/util.h"
#include "config.h"
#include "vec.h"

struct cjson_data {  
  cJSON* json;
  vec_t(buffer_t*) buffers;
};

static void finalizer(struct json_node* node, void* _self) {
  struct cjson_data* self = _self;
  
  int i = 0;
  buffer_t* v = NULL;
  vec_foreach(&self->buffers, v, i)
    buffer_free(v);
  
  vec_deinit(&self->buffers);
  cJSON_Delete(self->json);
  free(self);
}

struct decoder_entry {
  cJSON* currentCJSONNode;
  struct json_node* currentNode;
  cJSON* currentCJSONChild; 
};

static buffer_t* newBuffer(struct cjson_data* self, const char* str) {
  BUG_ON(!str);
  
  buffer_t* buff = buffer_new_with_string((char*) str);
  if (!buff)
    return NULL;
  
  buff->alloc = NULL;
  if (vec_push(&self->buffers, buff) < 0) {
    buffer_free(buff);
    return NULL;
  }
  return buff;
}

int json_decode_cjson(struct json_node** root, const char* data, size_t len) {
  int res = 0;
  const char* end;
  cJSON* json = cJSON_ParseWithLengthOpts(data, len, &end, false);
  if (!json)
    return -EINVAL;
  
  struct cjson_data* self = malloc(sizeof(*self));
  if (!self) {
    res = -ENOMEM;
    goto alloc_self_failure;
  }
  *self = (struct cjson_data) {
    .json = json
  };
  vec_init(&self->buffers);
  
  int sp = 0;
  static thread_local struct decoder_entry stack[CONFIG_JSON_NEST_MAX + 1];
  stack[sp++] = (struct decoder_entry) {
    .currentCJSONNode = json
  };
  
  #define checkStack(delta) do { \
    BUG_ON(sp + delta < 0); \
    if (sp + delta >= ARRAY_SIZE(stack)) { \
      res = -EOVERFLOW; \
      goto stack_overflow_or_underflow; \
    } \
  } while (0)
  
  while (sp > 0) {
    struct decoder_entry* cur = &stack[sp - 1];
    buffer_t* buff;
    bool alreadyInited = cur->currentNode != NULL;
    BUG_ON(cur->currentCJSONNode->type == cJSON_Invalid);
    
    switch (cur->currentCJSONNode->type) {
      case cJSON_Object:
        if (!alreadyInited) {
          cur->currentNode = &json_new_object()->node;
        } else {
          // Add the child to the result
          checkStack(+1);
          if ((buff = newBuffer(self, stack[sp].currentCJSONNode->string)) == NULL) {
            res = -ENOMEM;
            goto alloc_buff_failure;
          }
          
          if ((res = json_set_member_buffer_no_overwrite(cur->currentNode, buff, stack[sp].currentNode)) < 0) {
            json_free(stack[sp].currentNode);
            buffer_free(buff);
            
            BUG_ON(stack[sp].currentNode == NULL);
            BUG_ON(res == -EINVAL);
            res = -ENOMEM;
            goto add_child_failure;
          }
        }
        goto advance_to_next_child;
      case cJSON_Array:
        if (!alreadyInited) {
          cur->currentNode = &json_new_array()->node;
        } else {
          // Add the child to the result
          checkStack(+1);
          
          if (vec_push(&JSON_ARRAY(cur->currentNode)->array, stack[sp].currentNode) < 0) {
            json_free(stack[sp].currentNode);
            buffer_free(buff);
            
            res = -ENOMEM;
            goto add_child_failure;
          }
        }
        goto advance_to_next_child;
advance_to_next_child:
        if (!alreadyInited) 
          cur->currentCJSONChild = cur->currentCJSONNode->child;
        else 
          cur->currentCJSONChild = cur->currentCJSONChild->next;
        
        if (cur->currentCJSONChild) {
          checkStack(+1);
          stack[sp++] = (struct decoder_entry) {
            .currentCJSONNode = cur->currentCJSONChild,
          };
          
          goto continue_no_pop;
        }
        break;
      case cJSON_Number:
        cur->currentNode = &json_new_number(cJSON_GetNumberValue(cur->currentCJSONNode))->node;
        break;
      case cJSON_NULL:
        cur->currentNode = &json_new_null()->node;
        break;
      case cJSON_True:
      case cJSON_False:
        cur->currentNode = &json_new_boolean(cur->currentCJSONNode->type == cJSON_True)->node;
        break;
      case cJSON_String:
        if ((buff = newBuffer(self, cJSON_GetStringValue(cur->currentCJSONNode))) == NULL) {
          res = -ENOMEM;
          goto alloc_buff_failure;
        }
        cur->currentNode = &json_new_string(buff)->node;
        break; 
      default:
        BUG();
    }
    
    checkStack(-1);
    sp--;

continue_no_pop:
    json_set_unmanaged(cur->currentNode, NULL, NULL);
  }
  
  // Install custom finalizer on JSON root
  json_set_unmanaged(stack[0].currentNode, finalizer, self);
  if (root)
    *root = stack[0].currentNode;
  else
    json_free(stack[0].currentNode); 
alloc_buff_failure:
add_child_failure:
stack_overflow_or_underflow:
alloc_self_failure:
  if (res < 0) 
    json_free(stack[0].currentNode);
  return res;
}
