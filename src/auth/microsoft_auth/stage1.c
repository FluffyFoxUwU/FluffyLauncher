#include <errno.h>

#include "stage1.h"
#include "networking/http_request.h"
#include "networking/http_headers.h"
#include "util/util.h"
#include "bug.h"
#include "logging/logging.h"
#include "networking/easy.h"

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
  return res;
} 

int microsoft_auth_stage1_run(struct microsoft_auth_stage1* self) {
  int res = 0;
  struct http_request* req = http_new_request();
  if (!req) {
    res = -ENOMEM;
    goto request_creation_error;
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
  
  char* requestBodyLenString = NULL;
  util_asprintf(&requestBodyLenString, "%zu", requestBodyLen);
  if (!requestBodyLenString) {
    res = -ENOMEM;
    goto request_body_len_error;
  }
  
  http_set_location(req, location);
  http_set_method(req, HTTP_POST);
  
  if ((res = http_headers_set(req->headers, "Content-Type", "application/x-www-form-urlencoded")) < 0 ||
      (res = http_headers_set(req->headers, "Content-Length", requestBodyLenString)) < 0 ||
      (res = http_headers_set(req->headers, "Accept", "application/json")) < 0 ||
      (res = http_headers_set(req->headers, "Host", self->arg->hostname)) < 0) {
    BUG_ON(res == -EINVAL);
    res = -ENOMEM;
    goto request_preparation_error;
  }
  
  req->requestData = requestBody;
  req->requestDataLen = requestBodyLen;
  
  char* responseBody = NULL;
  size_t responseBodyLen = 0;
  FILE* responseBodyFile = open_memstream(&responseBody, &responseBodyLen);
  if (responseBodyFile == NULL) {
    pr_error("Failed to open memstream");
    goto memstream_open_failed;
  }
  
  pr_notice("Getting devicecode at %s:%d/%s", self->arg->hostname, self->arg->port, req->location);
  // Do actual request
  struct transport* connection;
  if ((res = networking_easy_new_connection(true, self->arg->hostname, self->arg->port, &connection)) < 0) 
    goto create_connection_error; 
  
  http_set_transport(req, connection);
  res = http_send(req);
  if (res < 0)
    goto http_request_send_error;
  
  res = http_recv(req, NULL, responseBodyFile);
  if (res < 0 || res != 200) {
    pr_error("Error getting devicecode. Server responseded with %d", res);
    goto http_request_recv_error;
  }
  
  pr_notice("Success! Response: %d", res);
  
  pr_notice("Processing response body...");
  fflush(responseBodyFile);
  res = processResponse(self, responseBody, responseBodyLen);
  
  if (res < 0) {
    pr_error("Processing response failed: %d", res);
    goto process_response_failed;
  }
  pr_notice("Done!");

process_response_failed:
http_request_recv_error:
http_request_send_error:
  connection->close(connection);
create_connection_error: 
  fclose(responseBodyFile);
  free(responseBody);
memstream_open_failed:
request_preparation_error:
  free(requestBodyLenString);
request_body_len_error:
  free(requestBody);
request_body_creation_error:
  free(location);
location_creation_error:
request_creation_error:
  http_free_request(req);
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
