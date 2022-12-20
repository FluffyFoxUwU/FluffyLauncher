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

static const struct json_schema apiErrorSchema = {
  .entries = {
    JSON_SCHEMA_ENTRY("$.path", JSON_STRING, struct minecraft_api_error_raw, path),
    JSON_SCHEMA_ENTRY("$.errorType", JSON_STRING, struct minecraft_api_error_raw, errorType),
    JSON_SCHEMA_ENTRY("$.errorMessage", JSON_STRING, struct minecraft_api_error_raw, errorMessage),
    JSON_SCHEMA_ENTRY("$.developerMessage", JSON_STRING, struct minecraft_api_error_raw, developerMessage),
    {}
  }
};

int minecraft_api_response_parse_profile(struct json_node* root, struct minecraft_api_profile_raw* profile) {
  return json_schema_load(&apiProfileSchema, root, profile);
}

int minecraft_api_response_parse_error(struct json_node* root, struct minecraft_api_error_raw* error) {
  return json_schema_load(&apiErrorSchema, root, error);
}

