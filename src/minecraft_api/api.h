#ifndef _headers_1669520876_FluffyLauncher_api
#define _headers_1669520876_FluffyLauncher_api

#include "parser/json/json.h"

struct minecraft_api_profile {
  const char* uuid;
  const char* username;
};

struct minecraft_api {
  const char* token;
  const char* authorizationValue;

  // JSON of last request (remain valid until next call)
  struct json_node* lastJSON;
  
  struct minecraft_api_profile profile;
  struct easy_http_headers* requestHeaders;
};

struct minecraft_api* minecraft_api_new(const char* token);
void minecraft_api_free(struct minecraft_api* self);

// Result in self->profile (valid until next API call)
int minecraft_api_get_profile(struct minecraft_api* self);

#endif

