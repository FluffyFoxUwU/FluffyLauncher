#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "buffer.h"
#include "parser/json/json.h"
#include "stage1.h"
#include "networking/http_request.h"
#include "networking/http_response.h"
#include "networking/http_headers.h"
#include "util/json_schema_loader.h"
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

struct stage1_response {
  double interval;
  double expiresIn;
  buffer_t* verificationURL;
  buffer_t* userCode;
  buffer_t* deviceCode;
};

static struct json_schema stage1ResponseSchema = {
  .entries = {
    JSON_SCHEMA_ENTRY("$.interval", JSON_NUMBER, struct stage1_response, interval),
    JSON_SCHEMA_ENTRY("$.expires_in", JSON_NUMBER, struct stage1_response, expiresIn),
    JSON_SCHEMA_ENTRY("$.verification_uri", JSON_STRING, struct stage1_response, verificationURL),
    JSON_SCHEMA_ENTRY("$.user_code", JSON_STRING, struct stage1_response, userCode),
    JSON_SCHEMA_ENTRY("$.device_code", JSON_STRING, struct stage1_response, deviceCode),
    {}
  }
};

static int processResponse(struct microsoft_auth_stage1* stage1, struct json_node* root) {
  int res = 0;
  struct stage1_response response = {};
  if ((res = json_schema_load(&stage1ResponseSchema, root, &response)) < 0)
    goto invalid_response;
  
  // `interval` cant be zero as it would mean dont wait between polling
  // which obvious why cant be zero
  if (response.interval <= 0 || response.interval > (double) INT_MAX ||
     response.expiresIn < 0 || response.expiresIn > (double) UINT64_MAX) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  stage1->pollInterval = response.interval;
  stage1->expireTimestamp = ((uint64_t) time(NULL)) + ((uint64_t) response.expiresIn);
  stage1->deviceCode = strdup(buffer_string(response.deviceCode));
  stage1->userCode = strdup(buffer_string(response.userCode));
  stage1->verificationURL = strdup(buffer_string(response.verificationURL));
  
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
                                         "client_id=%s&scope=%s", self->arg->clientID, self->arg->scope);
  if (res < 0)
    goto request_error;
  
  if (responseJson == NULL) {
    res = -EINVAL;
    goto no_content;
  }
  
  // pr_notice("Getting devicecode at %s:%d/%s", self->arg->hostname, self->arg->port, req->location);
  if ((res = processResponse(self, responseJson)) < 0) {
    pr_critical("Processing Microsoft authentication server response failed: %d", res);
    goto process_response_failed;
  }

no_content:
process_response_failed:
  json_free(responseJson);
request_error:
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
