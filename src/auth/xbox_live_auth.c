#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "networking/easy.h"
#include "networking/transport/transport.h"
#include "xbox_live_auth.h"
#include "networking/http_request.h"
#include "util/util.h"
#include "parser/sjson.h"
#include "networking/http_response.h"
#include "logging/logging.h"

static int process(struct xbox_live_auth_result* self, char* body, size_t bodyLen) {
  int res = 0;
  sjson_context* sjson = sjson_create_context(0, 0, NULL);
  sjson_node* root = sjson_decode(sjson, body);
  if (!root) {
    pr_critical("Failure parsing XBL server response body for token");
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

int xbox_live_auth(const char* microsoftToken, struct xbox_live_auth_result** result) {
  int res = 0;
  struct xbox_live_auth_result* self = malloc(sizeof(*self));
  if (!self)
    return -ENOMEM;
  
  *self = (struct xbox_live_auth_result) {};
  
  struct http_response* response = http_response_new();
  if (!response) {
    res = -ENOMEM;
    goto response_creation_failure;
  }
  
  char* requestBody = NULL;
  size_t requestBodyLen = util_asprintf(&requestBody, 
"{ \
  'Properties': { \
    'AuthMethod': 'RPS',\
    'SiteName': 'user.auth.xboxlive.com',\
    'RpsTicket': 'd=%s'\
  }, \
  'RelyingParty': 'http://auth.xboxlive.com', \
  'TokenType': 'JWT' \
}", microsoftToken);

  if (!requestBody) {
    res = -ENOMEM;
    goto request_body_creation_error;
  }

  struct http_request* req = networking_easy_new_http(&res, 
                                                      HTTP_POST, 
                                                      "user.auth.xboxlive.com", 
                                                      "/user/authenticate", 
                                                      requestBody, requestBodyLen, 
                                                      "application/json", 
                                                      "application/json");
  if (res < 0)
    goto request_creation_failure;
  
  struct transport* connection;
  res = networking_easy_new_connection(true, "user.auth.xboxlive.com", 443, &connection);
  if (res < 0)
    goto connection_creation_failure;
  if ((res = http_request_send(req, connection)) < 0)
    goto request_send_failure;
  
  char* responseBody = NULL;
  size_t responseBodyLen = 0;
  FILE* memfd = open_memstream(&responseBody, &responseBodyLen);
  
  res = http_response_recv(response, connection, memfd);
  fclose(memfd);
  if (res < 0)
    goto response_recv_failure;

  res = process(self, responseBody, responseBodyLen);
  free(responseBody);
response_recv_failure:
request_send_failure:
  connection->close(connection);
connection_creation_failure:
  free(requestBody);
request_creation_failure:
  http_request_free(req);
request_body_creation_error:
  http_response_free(response);
response_creation_failure:
  if (result)
    *result = self;  
  else
    xbox_live_free(self);
  return res;
}

void xbox_live_free(struct xbox_live_auth_result* self) {
  if (!self)
    return;
  
  free((char*) self->userhash);
  free((char*) self->token);
  free(self);
}
