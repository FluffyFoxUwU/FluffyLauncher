#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <unistd.h>

#include "bug.h"
#include "networking/http_request.h"
#include "networking/http_response.h"
#include "networking/transport/transport.h"
#include "stage2.h"
#include "auth/microsoft_auth.h"
#include "auth/microsoft_auth/stage1.h"
#include "parser/sjson.h"
#include "util/util.h"
#include "logging/logging.h"
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

static int process(struct microsoft_auth_stage2* self, char* body, size_t bodyLen) {
  int res = 0;
  sjson_context* sjson = sjson_create_context(0, 0, NULL);
  sjson_node* root = sjson_decode(sjson, body);
  if (!root) {
    pr_critical("Failure parsing Microsoft server response body for token");
    res = -EINVAL;
    goto invalid_response;
  }
  
  if (root->tag != SJSON_OBJECT) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  sjson_node* error = sjson_find_member(root, "error");
  if (error && error->tag != SJSON_STRING) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  if (error) {
    const char* errorStr = error->string_;
    if (strcmp(errorStr, "authorization_pending") == 0) {
      res = 0;
      goto token_not_ready;
    } else if (strcmp(errorStr, "authorization_declined") == 0) {
      res = -EPERM;
      goto token_not_ready;
    } else if (strcmp(errorStr, "expired_token") == 0) {
      res = -ETIMEDOUT;
      goto token_not_ready;
    }
    
    BUG_ON(strcmp(errorStr, "b ad_verification_code") == 0);
  }
  
  // Mandatory
  sjson_node* tokenType = sjson_find_member(root, "token_type");
  sjson_node* expiresIn = sjson_find_member(root, "expires_in");
  sjson_node* accessToken = sjson_find_member(root, "access_token");
  if (!tokenType || !expiresIn || !accessToken ||
      tokenType->tag != SJSON_STRING ||
      expiresIn->tag != SJSON_NUMBER ||
      accessToken->tag != SJSON_STRING ||
      expiresIn->number_ < 0 || expiresIn->number_ >= (double) UINT64_MAX) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  if (strcmp(tokenType->string_, "Bearer") != 0)
    pr_error("Returned token type is not Bearer. Proceeding anyway");
  
  // Optional
  sjson_node* refreshToken = sjson_find_member(root, "refresh_token");
  if (refreshToken && refreshToken->tag != SJSON_STRING) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  self->result->expiresTimestamp = ((uint64_t) time(NULL)) + ((uint64_t) expiresIn->number_);
  self->result->refreshToken = refreshToken ? strdup(refreshToken->string_) : NULL;
  self->result->accessToken = strdup(accessToken->string_);
  
  if (!self->result->accessToken || (refreshToken && !self->result->refreshToken)) {
    res = -ENOMEM;
    goto out_of_memory;
  }

  res = 1;
out_of_memory:
token_not_ready:  
invalid_response:
  sjson_destroy_context(sjson);
  return res;
}

static int tryGetToken(struct microsoft_auth_stage2* self, struct http_request* pollReq) {
  int res = 0;
  struct transport* connection;
  if ((res = networking_easy_new_connection(true, self->arg->hostname, self->arg->port, &connection)) < 0)
    goto error_new_connection;
  
  if ((res = http_request_send(pollReq, connection)) < 0)
    goto send_request_error;
  
  struct http_response* response = http_response_new();
  if (!response) {
    res = -ENOMEM;
    goto cannot_allocate_response;
  }
  
  char* responseBody = NULL;
  size_t responseBodyLen = 0;
  FILE* responseBodyFile = open_memstream(&responseBody, &responseBodyLen);
  if (!responseBodyFile) {
    res = -ENOMEM;
    goto fail_open_memfd;
  }
  
  res = http_response_recv(response, connection, responseBodyFile);
  fclose(responseBodyFile);
  if (res < 0)
    goto receive_error;
  
  if (res != 200 && res != 400) {
    pr_error("Authentication server responded with %d (200 or 400 was expected)", res);
    res = -EFAULT;
    goto receive_error;
  }
  
  // Process poll response
  res = process(self, responseBody, responseBodyLen);
receive_error:
  free(responseBody);
fail_open_memfd:
  http_response_free(response);
cannot_allocate_response:
send_request_error:
  connection->close(connection);
error_new_connection:
  return res;
}

static int devicodeAuth(struct microsoft_auth_stage2* self) {
  int res = 0;
  char* location = NULL;
  util_asprintf(&location, "/%s/oauth2/v2.0/token", self->arg->tenant);
  if (!location) {
    res = -ENOMEM;
    goto location_creation_error;
  }
  
  char* requestBody = NULL;
  size_t requestBodyLen = util_asprintf(&requestBody, "grant_type=urn:ietf:params:oauth:grant-type:device_code&client_id=%s&device_code=%s", self->arg->clientID, self->stage1->deviceCode);
  if (!requestBody) {
    res = -ENOMEM;
    goto request_body_creation_error;
  }
  
  struct http_request* pollRequest = networking_easy_new_http(&res, HTTP_POST, self->arg->hostname, location, requestBody, requestBodyLen, "application/x-www-form-urlencoded", "application/json");  
  if (!pollRequest) 
    goto poll_request_creation_error;
  
  pr_alert("Open %s in your browser and provide %s code to authenticate", self->stage1->verificationURL, self->stage1->userCode);
  
  while (time(NULL) < self->stage1->expireTimestamp) {
    if ((res = tryGetToken(self, pollRequest)) < 0)
      goto poll_error;
    
    // Sucessfully get the token
    if (res == 1) {
      pr_info("Obtained token!");
      res = 0;
      break;
    } else if (res == 0) {
      pr_info("Token not obtained. Retrying after %d seconds", self->stage1->pollInterval);
    }
    sleep(self->stage1->pollInterval);
  }

poll_error:
  http_request_free(pollRequest);
poll_request_creation_error:
  free(requestBody);
request_body_creation_error:
  free(location);
location_creation_error:
  if (res == -ETIMEDOUT || time(NULL) >= self->stage1->expireTimestamp)
    pr_critical("Cannot obtain token, code expired! Please try again later");
  else if (res == -EPERM)
    pr_critical("Access declined cannot log in");
  return res;
}

static int refreshTokenAuth(struct microsoft_auth_stage2* self) {
  int res = 0;
  char* location = NULL;
  util_asprintf(&location, "/%s/oauth2/v2.0/token", self->arg->tenant);
  if (!location) {
    res = -ENOMEM;
    goto location_creation_error;
  }
  
  char* requestBody = NULL;
  size_t requestBodyLen = util_asprintf(&requestBody, "grant_type=refresh_token&client_id=%s&refresh_token=%s", self->arg->clientID, self->arg->refreshToken);
  if (!requestBody) {
    res = -ENOMEM;
    goto request_body_creation_error;
  }
  
  struct http_request* pollRequest = networking_easy_new_http(&res, HTTP_POST, self->arg->hostname, location, requestBody, requestBodyLen, "application/x-www-form-urlencoded", "application/json");  
  if (!pollRequest) 
    goto refresh_request_creation_error;
  
  pr_info("Authenticating via refresh token...");
  res = tryGetToken(self, pollRequest);
  
  http_request_free(pollRequest);
refresh_request_creation_error:
  free(requestBody);
request_body_creation_error:
  free(location);
location_creation_error:
  return res;
}

int microsoft_auth_stage2_run(struct microsoft_auth_stage2* self) {
  // Try using refresh token if any
  if (self->arg->refreshToken) {
    BUG_ON(self->stage1);
    int res = refreshTokenAuth(self);
    if (res >= 0)
      return res;
  }
  
  return devicodeAuth(self);
}
