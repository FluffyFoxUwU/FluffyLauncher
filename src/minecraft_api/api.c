#include <Block.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "api.h"
#include "buffer.h"
#include "networking/easy.h"
#include "networking/http_request.h"
#include "parser/json/json.h"
#include "util/util.h"
#include "logging/logging.h"
#include "schema.h"
#include "config.h"

static void cleanLastCall(struct minecraft_api* self) {
  if (self->lastJSON)
    json_free(self->lastJSON);
}

struct minecraft_api* minecraft_api_new(const char* token) {
  struct minecraft_api* self = malloc(sizeof(*self));
  if (!self)
    return NULL;
  *self = (struct minecraft_api) {};
  
  self->token = strdup(token);
  if (!self->token)
    goto failure;
  
  util_asprintf((char**) &self->authorizationValue, "Bearer %s", self->token);
  if (!self->authorizationValue)
    goto failure;
  
  struct easy_http_headers headers[] = {
    {"Authorization", self->authorizationValue},
    {NULL, NULL}
  };
  
  if ((self->requestHeaders = malloc(sizeof(headers))) == NULL)
    goto failure;
  
  memcpy(self->requestHeaders, headers, sizeof(headers));
  return self;

failure:
  minecraft_api_free(self);
  return NULL;
}

// True if error reported else false
static bool testAndReportError(struct minecraft_api* self) {
  struct minecraft_api_error_raw err = {};
  if (minecraft_api_response_parse_error(self->lastJSON, &err) < 0)
    return false;
  
  self->lastError.path = buffer_string(err.path);
  self->lastError.error = buffer_string(err.error);
  self->lastError.errorMessage = buffer_string(err.errorMessage);
  self->lastError.errorType = buffer_string(err.errorType);
  self->lastError.developerMessage = buffer_string(err.developerMessage);
  
  pr_error("Minecraft API call error '%s': %s", self->lastError.path, self->lastError.developerMessage);
  return true;
}

enum minecraft_api_error_code minecraft_api_get_profile(struct minecraft_api* self) {
  cleanLastCall(self);
  
  if ((self->lastError.errorNum = networking_easy_do_json_http_rpc(&self->lastJSON, true, HTTP_GET, CONFIG_MINECRAFT_API_HOSTNAME, "/minecraft/profile", self->requestHeaders, "")) < 0) 
    return MINECRAFT_API_NETWORK_ERROR; 
  
  if (self->lastJSON == NULL)
    return MINECRAFT_API_PARSE_SERVER_ERROR;
  
  if (testAndReportError(self) == true)
    return strcmp(self->lastError.error, "NOT_FOUND") ? MINECRAFT_API_NOT_FOUND : MINECRAFT_API_GENERIC_ERROR;

  struct minecraft_api_profile_raw profile = {};
  
  if (minecraft_api_response_parse_profile(self->lastJSON, &profile) < 0)
    return MINECRAFT_API_PARSE_SERVER_ERROR;
  
  self->callResult.profile.username = buffer_string(profile.name);
  self->callResult.profile.uuid = buffer_string(profile.id);
  return 0;
}

bool minecraft_api_have_own_minecraft(struct minecraft_api* self) {
  if (minecraft_api_get_profile(self) == 0)
    return true;
  return false;
}

void minecraft_api_free(struct minecraft_api* self) {
  if (!self)
    return;
  
  cleanLastCall(self);
  
  free(self->requestHeaders);
  free((char*) self->authorizationValue);
  free((char*) self->token);
  free(self);
}
