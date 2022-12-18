#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <unistd.h>

#include "buffer.h"
#include "bug.h"
#include "networking/http_request.h"
#include "networking/http_response.h"
#include "networking/transport/transport.h"
#include "panic.h"
#include "util/json_schema_loader.h"
#include "parser/json/decoder.h"
#include "stage2.h"
#include "auth/microsoft_auth.h"
#include "auth/microsoft_auth/stage1.h"
#include "parser/json/json.h"
#include "util/util.h"
#include "logging/logging.h"
#include "networking/easy.h"

// TODO: Modify this file to use networking_easy_do_json_http_rpc for abstraction

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

struct stage2_response {
  double expireIn;
  buffer_t* token;
  buffer_t* tokenType;
  buffer_t* refreshToken;
};

static const struct json_schema stage2ResponseSchema = {
  .entries = {
    JSON_SCHEMA_ENTRY("$.access_token", JSON_STRING, struct stage2_response, token),
    JSON_SCHEMA_ENTRY("$.token_type", JSON_STRING, struct stage2_response, tokenType),
    JSON_SCHEMA_ENTRY("$.refresh_token", JSON_STRING, struct stage2_response, refreshToken),
    JSON_SCHEMA_ENTRY("$.expires_in", JSON_NUMBER, struct stage2_response, expireIn),
    {}
  }
};

static int process(struct microsoft_auth_stage2* self, char* body, size_t bodyLen) {
  int res = 0;
  struct json_node* root = NULL;
  if (json_decode_default(&root, body, bodyLen) < 0) {
    pr_critical("Failure parsing Microsoft server response body for token");
    res = -EINVAL;
    goto invalid_response;
  }
  
  if (root->type != JSON_OBJECT) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  struct json_node* error = NULL;
  int getRes = json_get_member(root, "error", &error);
  if (getRes != -ENODATA && (getRes < 0 || error->type != JSON_STRING)) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  if (error) {
    const char* errorStr = buffer_string(JSON_STRING(error)->string);
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
    
    if (strcmp(errorStr, "bad_verification_code") == 0)
      panic("Bad verification code shouldnt happen");
    panic("Unknown error: %s", errorStr);
  }
  
  // Mandatory
  struct stage2_response response = {};
  if ((res = json_schema_load(&stage2ResponseSchema, root, &response) < 0))
    goto invalid_response;
  
  if (response.expireIn < 0 || response.expireIn >= (double) UINT64_MAX) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  self->result->expiresTimestamp = ((uint64_t) time(NULL)) + ((uint64_t) response.expireIn);
  self->result->refreshToken = strdup(buffer_string(response.refreshToken));
  self->result->accessToken = strdup(buffer_string(response.token));
  
  if (!self->result->accessToken || !self->result->refreshToken) {
    res = -ENOMEM;
    goto out_of_memory;
  }

  res = 1;
out_of_memory:
token_not_ready:  
invalid_response: 
  json_free(root);
  return res;
}

static int tryGetToken(struct microsoft_auth_stage2* self, struct http_request* pollReq) {
  int res = 0;
  struct transport* connection;
  if ((res = networking_easy_new_connection(true, self->arg->hostname, self->arg->port, &connection)) < 0)
    goto error_new_connection;
  
  if ((res = http_request_send(pollReq, connection)) < 0)
    goto send_request_error;
  
  char* responseBody = NULL;
  size_t responseBodyLen = 0;
  FILE* responseBodyFile = open_memstream(&responseBody, &responseBodyLen);
  if (!responseBodyFile) {
    res = -ENOMEM;
    goto fail_open_memfd;
  }
  
  res = http_response_recv(NULL, connection, responseBodyFile);
  fclose(responseBodyFile);
  if (res < 0)
    goto receive_error;
  
  if (res != 200 && res != 400) {
    pr_critical("Authentication server responded with %d (200 or 400 was expected)", res);
    res = -EFAULT;
    goto receive_error;
  }
  
  // Process poll response
  res = process(self, responseBody, responseBodyLen);
  if (res < 0)
    pr_critical("Error processing Microsoft authentication server response: %d", res);
receive_error:
  free(responseBody);
fail_open_memfd:
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
  
  struct http_request* pollRequest;
  struct easy_http_headers headers[] = {
    {"Accept", "application/json"},
    {"Content-Type", "application/x-www-form-urlencoded"},
    {NULL, NULL}
  };
  
  res = networking_easy_new_http(&pollRequest, HTTP_POST, self->arg->hostname, location, headers, "grant_type=urn:ietf:params:oauth:grant-type:device_code&client_id=%s&device_code=%s", self->arg->clientID, self->stage1->deviceCode);  
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
  free((char*) pollRequest->requestData);
  http_request_free(pollRequest);
poll_request_creation_error:
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
  
  struct http_request* pollRequest;
  struct easy_http_headers headers[] = {
    {"Accept", "application/json"},
    {"Content-Type", "application/x-www-form-urlencoded"},
    {NULL, NULL}
  };
  res = networking_easy_new_http(&pollRequest, HTTP_POST, self->arg->hostname, location, headers, "grant_type=refresh_token&client_id=%s&refresh_token=%s", self->arg->clientID, self->arg->refreshToken);  
  if (res < 0) 
    goto refresh_request_creation_error;
  
  pr_info("Authenticating via refresh token...");
  res = tryGetToken(self, pollRequest);
  
  free((char*) pollRequest->requestData);
  http_request_free(pollRequest);
refresh_request_creation_error:
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
  
  if (!self->stage1)
    return -EINVAL;
  
  return devicodeAuth(self);
}
