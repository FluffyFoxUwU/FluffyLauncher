#include "xbl_like_auth.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "networking/easy.h"
#include "networking/transport/transport.h"
#include "xbl_like_auth.h"
#include "networking/http_request.h"
#include "util/util.h"
#include "parser/sjson.h"
#include "networking/http_response.h"
#include "logging/logging.h"

static void xbl_like_auth_free(struct xbl_like_auth_result* self) {
  if (!self)
    return;
  
  free((char*) self->userhash);
  free((char*) self->token);
  free(self);
}

static int processResult200(struct xbl_like_auth_result* self, char* body, size_t bodyLen) {
  int res = 0;
  sjson_context* sjson = sjson_create_context(0, 0, NULL);
  sjson_node* root = sjson_decode(sjson, body);
  if (!root) {
    pr_critical("Failure parsing XBL like server response body for token");
    res = -EINVAL;
    goto invalid_response;
  }
  
  if (root->tag != SJSON_OBJECT) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  sjson_node* token = sjson_find_member(root, "Token");
  sjson_node* displayClaims = sjson_find_member(root, "DisplayClaims");
  if (!token || !displayClaims ||
       token->tag != SJSON_STRING ||
       displayClaims->tag != SJSON_OBJECT) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  sjson_node* xui = sjson_find_member(displayClaims, "xui");
  if (!xui || xui->tag != SJSON_ARRAY) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  sjson_node* first = sjson_first_child(xui);
  if (!first || first->tag != SJSON_OBJECT) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  if (first->next)
    pr_warn("There are more than one entry in $.DisplayClaims.xui array");
  
  sjson_node* userhash = sjson_find_member(first, "uhs");
  if (!userhash || userhash->tag != SJSON_STRING) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  self->token = strdup(token->string_);
  self->userhash = strdup(userhash->string_);
  if (!self->token || !self->userhash) {
    res = -ENOMEM;
    goto out_of_memory;
  }
out_of_memory:
invalid_response:
  sjson_destroy_context(sjson);
  return res;
}

static int processResult401(struct xbl_like_auth_result* self, char* body, size_t bodyLen) {
  int res = 0;
  sjson_context* sjson = sjson_create_context(0, 0, NULL);
  sjson_node* root = sjson_decode(sjson, body);
  if (!root) {
    pr_critical("Failure parsing XBL like server response body for token");
    res = -EINVAL;
    goto invalid_response;
  }
  
  if (root->tag != SJSON_OBJECT) {
    res = -EINVAL;
    goto invalid_response;
  }
  
invalid_response:
  sjson_destroy_context(sjson);
  return res;
}

int xbl_like_auth(const char* hostname, const char* location, const char* requestBody, struct xbl_like_auth_result* result) {
  int res = 0;
  struct xbl_like_auth_result self = (struct xbl_like_auth_result) {};
  
  struct http_response* response = http_response_new();
  if (!response) {
    res = -ENOMEM;
    goto response_creation_failure;
  }

  struct http_request* req = networking_easy_new_http(&res, 
                                                      HTTP_POST, 
                                                      hostname, 
                                                      location, 
                                                      requestBody, strlen(requestBody), 
                                                      "application/json", 
                                                      "application/json");
  if (res < 0)
    goto request_creation_failure;
  
  char* responseBody = NULL;
  size_t responseBodyLen = 0;
  FILE* memfd = open_memstream(&responseBody, &responseBodyLen);
  if (!memfd) {
    res = -ENOMEM;
    goto memfd_create_error;
  }
  
  struct transport* connection;
  if ((res = networking_easy_new_connection(true, hostname, 443, &connection)) < 0)
    goto connection_creation_failure;
  if ((res = http_request_send(req, connection)) < 0)
    goto request_send_failure;
  
  res = http_response_recv(response, connection, memfd);
  fflush(memfd);
  if (res < 0)
    goto response_recv_failure;
  
  if (res == 200)
    res = processResult200(&self, responseBody, responseBodyLen);
  else if (res == 401)
    res = processResult401(&self, responseBody, responseBodyLen); 
  else if (res >= 0)
    pr_critical("Unexpected XBL like server responded with %d", res);
  
  if (res < 0)
    pr_critical("Error processing XBL like server response: %d", res);
response_recv_failure:
request_send_failure:
  connection->close(connection);
  fclose(memfd);
  free(responseBody);
  http_request_free(req);
memfd_create_error:
request_creation_failure:
connection_creation_failure:
  http_response_free(response);
response_creation_failure:
  if (result)
    *result = self;  
  else
    xbl_like_auth_free(&self);
  return res;
}



