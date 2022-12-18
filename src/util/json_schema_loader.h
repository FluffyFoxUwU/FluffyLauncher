#ifndef _headers_1671174868_FluffyLauncher_json_schema_processor
#define _headers_1671174868_FluffyLauncher_json_schema_processor

#include <stddef.h>

#include "buffer.h"
#include "parser/json/json.h"
#include "util/util.h"

/*
Type mappings for non nulls
JSON_STRING  : buffer_t*
JSON_NUMBER  : double
JSON_ARRAY   : struct json_array*
JSON_OBJECT  : struct json_object*
JSON_BOOLEAN : bool
*/

struct json_schema_entry {
  const char* path;
  enum json_type type;
  
  size_t fieldOffset;
  size_t fieldSize;
};

struct json_schema {
  int evenIfYouThinkThisUselessTryRemoveThisAndSeeWhatHappenUwU;
  struct json_schema_entry entries[];
};

#define JSON_SCHEMA_ENTRY(path, type, structure, member) {path, type, offsetof(structure, member), FIELD_SIZEOF(structure, member)}

// Pass NULL to result to just verify
// Return 0 on sucess, 1 if json doesnt match schema
// negative on error
int json_schema_load(const struct json_schema* schema, struct json_node* json, void* result);

/*
Example usage:
struct xbl_auth_response {
  buffer_t* token;
  buffer_t* uhs;
};

struct json_node* responseJSON;
struct xbl_auth_response response;

// $ has special meaning which is current node
// due design indexing multi dimension array must be $.array[0].$[1]
// instead $.array[0][1] (which is treated as if "{'array[0]': [0, 1]}" given
// and a warning generated for this as its most likely not intended)
// and $.$.$.UwU refers to $.UwU 
static struct json_schema xblLikeAuthResponseSchema = {
  .entries = {
    JSON_SCHEMA_ENTRY("$.Token", JSON_STRING, struct xbl_auth_response, token),
    JSON_SCHEMA_ENTRY("$.DisplayClaims.xui[0].uhs", JSON_STRING, struct xbl_auth_response, uhs),
    {}
  }
};

if (json_schema_load(&xblLikeAuthResponseSchema, responseJSON, &response) == -EINVAL) {
  // JSON doesnt match schema error or report
  // ...
} else {
  // Use the data
}
*/

#endif

