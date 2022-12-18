#include "schema.h"
#include "parser/json/json.h"
#include "util/json_schema_loader.h"

static const struct json_schema apiProfileSchema = {
  .entries = {
    JSON_SCHEMA_ENTRY("$.id", JSON_STRING, struct minecraft_api_profile_raw, id),
    JSON_SCHEMA_ENTRY("$.name", JSON_STRING, struct minecraft_api_profile_raw, name),
    {}
  }
};

int minecraft_api_response_parse_profile(struct json_node* root, struct minecraft_api_profile_raw* profile) {
  return json_schema_load(&apiProfileSchema, root, profile);
}

