#include <errno.h>
#include <string.h>
#include <limits.h>

#include "parser/json/json.h"
#include "stage1.h"
#include "networking/http_request.h"
#include "networking/http_response.h"
#include "networking/http_headers.h"
#include "util/util.h"
#include "bug.h"
#include "logging/logging.h"
#include "networking/easy.h"
#include "auth/microsoft_auth.h"

struct microsoft_auth_stage1* microsoft_auth_stage1_new(struct microsoft_auth_result* result, struct microsoft_auth_arg* arg) {
  struct microsoft_auth_stage1* self = malloc(sizeof(*self));
  if (!self)
    goto failure;
  
  *self = (struct microsoft_auth_stage1) {};
  self->arg = arg;
  self->result = result;
  return self;
failure:
  microsoft_auth_stage1_free(self);
  return NULL;
}

static int processResponse(struct microsoft_auth_stage1* stage1, struct json_node* root) {
  int res = 0;
  if (root->type != JSON_OBJECT) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  struct json_node* deviceCode;
  struct json_node* userCode;
  struct json_node* verificationUrl;
  struct json_node* expireIn;
  struct json_node* interval;
  if (json_get_member(root, "interval", &interval) < 0 ||
      json_get_member(root, "expires_in", &expireIn) < 0 ||
      json_get_member(root, "verification_uri", &verificationUrl) < 0 ||
      json_get_member(root, "user_code", &userCode) < 0 ||
      json_get_member(root, "device_code", &deviceCode) < 0 ||
      deviceCode->type != JSON_STRING ||
      userCode->type != JSON_STRING ||
      verificationUrl->type != JSON_STRING ||
      expireIn->type != JSON_NUMBER ||
      interval->type != JSON_NUMBER) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  // `interval` cant be zero as it would mean dont wait between polling
  // which obvious why cant be zero
  if (JSON_NUMBER(interval)->number <= 0 || JSON_NUMBER(interval)->number > (double) INT_MAX ||
     JSON_NUMBER(expireIn)->number < 0 || JSON_NUMBER(expireIn)->number > (double) UINT64_MAX) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  stage1->pollInterval = JSON_NUMBER(interval)->number;
  stage1->expireTimestamp = ((uint64_t) time(NULL)) + ((uint64_t) JSON_NUMBER(expireIn)->number);
  stage1->deviceCode = strdup(buffer_string(JSON_STRING(deviceCode)->string));
  stage1->userCode = strdup(buffer_string(JSON_STRING(userCode)->string));
  stage1->verificationURL = strdup(buffer_string(JSON_STRING(verificationUrl)->string));
  
  if (!stage1->userCode || !stage1->verificationURL || !stage1->deviceCode) {
    res = -ENOMEM;
    goto out_of_memory;
  }

out_of_memory:
invalid_response:
  return res;
} 

int microsoft_auth_stage1_run(struct microsoft_auth_stage1* self) {
  int res = 0;
  
  char* location = NULL;
  util_asprintf(&location, "/%s/oauth2/v2.0/devicecode", self->arg->tenant);
  if (!location) {
    res = -ENOMEM;
    goto location_creation_error;
  }
  
  char* requestBody = NULL;
  size_t requestBodyLen = util_asprintf(&requestBody, "client_id=%s&scope=%s", self->arg->clientID, self->arg->scope);
  if (!requestBody) {
    res = -ENOMEM;
    goto request_body_creation_error;
  }
  
  struct json_node* responseJson;
  struct easy_http_headers headers[] = {
    {"Content-Type", "application/x-www-form-urlencoded"}, 
    {"Accept", "application/json"},
    {NULL, NULL}
  };
  res = networking_easy_do_json_http_rpc(&responseJson,
                                         true, 
                                         HTTP_POST, 
                                         self->arg->hostname, location, 
                                         headers,
                                         requestBody, requestBodyLen, 
                                         "client_id=%s&scope=%s", self->arg->clientID, self->arg->scope);
  if (res < 0)
    goto request_error;
  
  // pr_notice("Getting devicecode at %s:%d/%s", self->arg->hostname, self->arg->port, req->location);
  if ((res = processResponse(self, responseJson)) < 0) {
    pr_critical("Processing Microsoft authentication server response failed: %d", res);
    goto process_response_failed;
  }
  json_free(responseJson);

process_response_failed:
request_error:
request_body_creation_error:
  free(location);
location_creation_error:
  return res;
}

void microsoft_auth_stage1_free(struct microsoft_auth_stage1* self) {
  if (!self)
    return;
  
  free((char*) self->deviceCode);
  free((char*) self->userCode);
  free((char*) self->verificationURL);
  free(self);
}
