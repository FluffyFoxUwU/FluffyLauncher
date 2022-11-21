#include <errno.h>
#include <string.h>
#include <limits.h>

#include "parser/sjson.h"
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

static int processResponse(struct microsoft_auth_stage1* stage1, char* body, size_t bodyLen) {
  int res = 0;
  sjson_context* sjson = sjson_create_context(0, 0, NULL);
  sjson_node* root = sjson_decode(sjson, body);
  if (!root) {
    pr_critical("Failure parsing Microsoft server response body for device code");
    res = -EINVAL;
    goto invalid_response;
  }
  
  if (root->tag != SJSON_OBJECT) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  sjson_node* deviceCode = sjson_find_member(root, "device_code");
  sjson_node* userCode = sjson_find_member(root, "user_code");
  sjson_node* verificationUrl = sjson_find_member(root, "verification_uri");
  sjson_node* expireIn = sjson_find_member(root, "expires_in");
  sjson_node* interval = sjson_find_member(root, "interval");
  if (!deviceCode || !userCode || !verificationUrl || !expireIn || !interval ||
      deviceCode->tag != SJSON_STRING ||
      userCode->tag != SJSON_STRING ||
      verificationUrl->tag != SJSON_STRING ||
      expireIn->tag != SJSON_NUMBER ||
      interval->tag != SJSON_NUMBER) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  // `interval` cant be zero as it would mean dont wait between polling
  // which obvious why cant be zero
  if (interval->number_ <= 0 || interval->number_ > (double) INT_MAX ||
     expireIn->number_ < 0 || expireIn->number_ > (double) UINT64_MAX) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  stage1->pollInterval = interval->number_;
  stage1->expireTimestamp = ((uint64_t) time(NULL)) + ((uint64_t) expireIn->number_);
  stage1->deviceCode = strdup(deviceCode->string_);
  stage1->userCode = strdup(userCode->string_);
  stage1->verificationURL = strdup(verificationUrl->string_);
  
  if (!stage1->userCode || !stage1->verificationURL || !stage1->deviceCode) {
    res = -ENOMEM;
    goto out_of_memory;
  }

out_of_memory:
invalid_response:
  sjson_destroy_context(sjson);
  return res;
} 

int microsoft_auth_stage1_run(struct microsoft_auth_stage1* self) {
  int res = 0;
  struct http_response* response = http_response_new();
  if (!response) {
    res = -ENOMEM;
    goto response_creation_error;
  }
  
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
  
  struct http_request* req = networking_easy_new_http(NULL, 
                                                      HTTP_POST, 
                                                      self->arg->hostname, location, 
                                                      requestBody, requestBodyLen, 
                                                      "application/x-www-form-urlencoded", 
                                                      "application/json");
  if (!req) {
    res = -ENOMEM;
    goto request_creation_error;
  }
  
  char* responseBody = NULL;
  size_t responseBodyLen = 0;
  FILE* responseBodyFile = open_memstream(&responseBody, &responseBodyLen);
  if (responseBodyFile == NULL) {
    pr_critical("Failed to open memstream");
    goto memstream_open_failed;
  }
  
  // pr_notice("Getting devicecode at %s:%d/%s", self->arg->hostname, self->arg->port, req->location);
  // Do actual request
  struct transport* connection;
  if ((res = networking_easy_new_connection(true, self->arg->hostname, self->arg->port, &connection)) < 0) 
    goto create_connection_error; 
  
  res = http_request_send(req, connection);
  if (res < 0)
    goto http_request_send_error;
  
  res = http_response_recv(response, connection, responseBodyFile);
  if (res < 0 || res != 200) {
    pr_critical("Error getting devicecode. Server responseded with %d", res);
    goto http_request_recv_error;
  }
  
  fflush(responseBodyFile);
  res = processResponse(self, responseBody, responseBodyLen);
  
  if (res < 0) {
    pr_critical("Processing Microsoft authentication server response failed: %d", res);
    goto process_response_failed;
  }

process_response_failed:
http_request_recv_error:
http_request_send_error:
  connection->close(connection);
create_connection_error: 
  fclose(responseBodyFile);
  free(responseBody);
memstream_open_failed:
  free(requestBody);
  http_request_free(req);
request_creation_error:
request_body_creation_error:
  free(location);
location_creation_error:
  http_response_free(response);
response_creation_error:
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
