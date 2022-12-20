#ifndef _headers_1671350005_FluffyLauncher_schema
#define _headers_1671350005_FluffyLauncher_schema

#include "buffer.h"
#include "parser/json/json.h"

struct minecraft_api_profile_raw {
  buffer_t* id;
  buffer_t* name;
};

struct minecraft_api_error_raw {
  buffer_t* path;
  buffer_t* errorType;
  buffer_t* error;
  buffer_t* errorMessage;
  buffer_t* developerMessage;
};

int minecraft_api_response_parse_profile(struct json_node* root, struct minecraft_api_profile_raw* profile);
int minecraft_api_response_parse_error(struct json_node* root, struct minecraft_api_error_raw* error);

#endif

