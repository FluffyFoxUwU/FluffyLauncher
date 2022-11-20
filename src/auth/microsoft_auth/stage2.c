#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <inttypes.h>

#include "networking/http_request.h"
#include "stage2.h"
#include "auth/microsoft_auth.h"
#include "auth/microsoft_auth/stage1.h"
#include "util/util.h"
#include "networking/easy.h"

struct microsoft_auth_stage2* microsoft_auth_stage2_new(struct microsoft_auth_result* result, struct microsoft_auth_arg* arg, struct microsoft_auth_stage1* stage1) {
  struct microsoft_auth_stage2* self = malloc(sizeof(*self));
  if (!self)
    goto failure;
  *self = (struct microsoft_auth_stage2) {};
  self->arg = arg;
  self->result = result;
  self->stage1 = stage1;
  return self;

failure:
  microsoft_auth_stage2_free(self);
  return NULL;
}

void microsoft_auth_stage2_free(struct microsoft_auth_stage2 *self) {
  if (!self)
    return;
  
  free(self);
}

int microsoft_auth_stage2_run(struct microsoft_auth_stage2* self) {
  int res = 0;
  
  char* location = NULL;
  util_asprintf(&location, "/%s/oauth2/v2.0/token", self->arg->tenant);
  if (!location) {
    res = -ENOMEM;
    goto location_creation_error;
  }
  
  char* requestBody = NULL;
  size_t requestBodyLen = util_asprintf(&requestBody, "/%s/oauth2/v2.0/token", self->arg->tenant);
  if (!requestBody) {
    res = -ENOMEM;
    goto request_body_creation_error;
  }
  
  struct http_request* pollRequest = networking_easy_new_http(&res, HTTP_POST, self->arg->hostname, location, requestBody, requestBodyLen, "application/x-www-form-urlencoded", "application/json");  
  if (!pollRequest) 
    goto poll_request_creation_error;
  
  uint64_t timeout = ((uint64_t) time(NULL)) + self->stage1->expireIn;
  
  printf("Device code: %s\n", self->stage1->deviceCode);
  printf("User code: %s\n", self->stage1->userCode);
  printf("Verification URL: %s\n", self->stage1->verificationURL);
  printf("Expire in: %" PRIu64 "\n", self->stage1->expireIn);
  printf("Poll interval: %" PRIu64 "\n", self->stage1->pollInterval);
  
  http_request_free(pollRequest);
poll_request_creation_error:
  free(requestBody);
request_body_creation_error:
  free(location);
location_creation_error:
  return res;
}
