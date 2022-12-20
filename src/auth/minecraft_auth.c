#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "buffer.h"
#include "bug.h"
#include "minecraft_auth.h"
#include "config.h"
#include "networking/easy.h"
#include "networking/http_response.h"
#include "networking/http_request.h"
#include "parser/json/json.h"
#include "logging/logging.h"
#include "util/json_schema_loader.h"
#include "util/util.h"

struct minecraft_auth_response {
  double expiresIn;
  buffer_t* token;
};

static const struct json_schema minecraftAuthResponseSchema = {
  .entries = {
    JSON_SCHEMA_ENTRY("$.expires_in", JSON_NUMBER, struct minecraft_auth_response, expiresIn),
    JSON_SCHEMA_ENTRY("$.access_token", JSON_STRING, struct minecraft_auth_response, token),
    {}
  }
};

static int processResult200(struct minecraft_auth_result* self, struct json_node* root) {
  int res = 0;
  struct minecraft_auth_response response = {};
  if ((res = json_schema_load(&minecraftAuthResponseSchema, root, &response)) < 0)
    goto invalid_response;
  
  if (response.expiresIn < 0 || response.expiresIn > (double) UINT64_MAX) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  self->expireTimestamp = ((uint64_t) time(NULL)) + ((uint64_t) response.expiresIn);
  self->token = strdup(buffer_string(response.token));
  if (!self->token) {
    res = -ENOMEM;
    goto out_of_memory;
  }
  
out_of_memory:
invalid_response:
  return res;
}

int minecraft_auth(const char* userhash, const char* xstsToken, struct minecraft_auth_result** result) {
  int res = 0;
  struct minecraft_auth_result* self = malloc(sizeof(*self));
  *self = (struct minecraft_auth_result) {};
  
  struct json_node* responseJSON;
  struct easy_http_headers headers[] = {
    {"Accept", "application/json"},
    {"Content-Type", "application/json"},
    {NULL, NULL}
  };
  res = networking_easy_do_json_http_rpc(&responseJSON,
                                         true,
                                         HTTP_POST, 
                                         CONFIG_MINECRAFT_API_HOSTNAME, 
                                         "/authentication/login_with_xbox",
                                         headers,
                                         "{\"identityToken\": \"XBL3.0 x=%s;%s\"}", userhash, xstsToken);
  if (res < 0)
    goto request_error;
  
  if (responseJSON == NULL) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  if (res == 200) 
    res = processResult200(self, responseJSON);
  json_free(responseJSON);
  
invalid_response:
  if (res < 0)
    pr_critical("Error processing Minecraft services API response: %d", res);
request_error:
  if (result && res >= 0)
    *result = self;
  else
    minecraft_auth_free(self);
  return res;
}

void minecraft_auth_free(struct minecraft_auth_result* self) {
  if (!self)
    return;
  
  free((char*) self->token);
  free(self);
}
