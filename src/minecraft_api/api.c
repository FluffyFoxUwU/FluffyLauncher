#include <Block.h>
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

int minecraft_api_get_profile(struct minecraft_api* self) {
  cleanLastCall(self);
  int res = 0;
  
  if ((res = networking_easy_do_json_http_rpc(&self->lastJSON, true, HTTP_GET, CONFIG_MINECRAFT_API_HOSTNAME, "/minecraft/profile", self->requestHeaders, "")) < 0)
    goto send_request_error;

  if (self->lastJSON == NULL) {
    res = -EFAULT;
    goto invalid_response;
  }

  struct minecraft_api_profile_raw profile = {};
  
  if ((res = minecraft_api_response_parse_profile(self->lastJSON, &profile)) < 0)
    goto invalid_response;
  
  self->callResult.profile.username = buffer_string(profile.name);
  self->callResult.profile.uuid = buffer_string(profile.id);
invalid_response:
send_request_error:
  return res;
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
