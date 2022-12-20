#ifndef _headers_1669520876_FluffyLauncher_api
#define _headers_1669520876_FluffyLauncher_api

#include "parser/json/json.h"

struct minecraft_api_profile {
  const char* uuid;
  const char* username;
};

enum minecraft_api_error_code {
  MINECRAFT_API_OK,
  MINECRAFT_API_NETWORK_ERROR,
  MINECRAFT_API_PARSE_SERVER_ERROR,
  MINECRAFT_API_GENERIC_ERROR,
  MINECRAFT_API_NOT_FOUND
};

struct minecraft_api_error {
  const char* path;
  const char* error;
  const char* errorType;
  const char* errorMessage;
  const char* developerMessage;
  
  // errorNum is set for MINECRAFT_API_NETWORK_ERROR
  // otherwise undefined
  int errorNum;
};

struct minecraft_api {
  const char* token;
  const char* authorizationValue;
  
  union {
    struct minecraft_api_profile profile;
  } callResult;
  
  struct easy_http_headers* requestHeaders;
  
  // JSON of last request (remain valid until next API call)
  bool hasErrorOccured;
  struct json_node* lastJSON;
  struct minecraft_api_error lastError;
};

struct minecraft_api* minecraft_api_new(const char* token);
void minecraft_api_free(struct minecraft_api* self);

// API calls

// Result in self->profile (valid until next API call)
// Errors: 
// MINECRAFT_API_NOT_FOUND: Profile cant be found 
enum minecraft_api_error_code minecraft_api_get_profile(struct minecraft_api* self);

bool minecraft_api_have_own_minecraft(struct minecraft_api* self);

#endif

