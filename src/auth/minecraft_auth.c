#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "buffer.h"
#include "minecraft_auth.h"
#include "networking/easy.h"
#include "networking/http_response.h"
#include "networking/http_request.h"
#include "parser/json/json.h"
#include "logging/logging.h"
#include "util/util.h"

static int processResult200(struct minecraft_auth_result* self, struct json_node* root) {
  int res = 0;
  
  if (root->type != JSON_OBJECT) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  struct json_node* token;
  struct json_node* expireIn;
  if (json_get_member(root, "expires_in", &expireIn) < 0 ||
      json_get_member(root, "access_token", &token) < 0 ||
      token->type != JSON_STRING ||
      expireIn->type != JSON_NUMBER) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  if (JSON_NUMBER(expireIn)->number < 0 || JSON_NUMBER(expireIn)->number > (double) UINT64_MAX) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  self->expireTimestamp = ((uint64_t) time(NULL)) + ((uint64_t) JSON_NUMBER(expireIn)->number);
  self->token = strdup(buffer_string(JSON_STRING(token)->string));
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
                                         "api.minecraftservices.com", 
                                         "/authentication/login_with_xbox",
                                         headers,
                                         "{\"identityToken\": \"XBL3.0 x=%s;%s\"}", userhash, xstsToken);
  if (res < 0)
    goto request_error;
  
  if (res == 200) 
    res = processResult200(self, responseJSON);
  json_free(responseJSON);
  
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
