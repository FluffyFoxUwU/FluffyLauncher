#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "minecraft_auth.h"
#include "networking/easy.h"
#include "networking/http_response.h"
#include "networking/http_request.h"
#include "parser/sjson.h"
#include "logging/logging.h"
#include "util/util.h"

static int processResult200(struct minecraft_auth_result* self, char* body, size_t bodyLen) {
  int res = 0;
  sjson_context* sjson = sjson_create_context(0, 0, NULL);
  sjson_node* root = sjson_decode(sjson, body);
  // puts(body);
  if (!root) {
    int raise(int);
    //raise(5);
    pr_critical("Failure parsing Minecraft services API server response body for token");
    res = -EINVAL;
    goto invalid_response;
  }
  
  if (root->tag != SJSON_OBJECT) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  sjson_node* token = sjson_find_member(root, "access_token");
  sjson_node* expireIn = sjson_find_member(root, "expires_in");
  if (!token || !expireIn ||
      token->tag != SJSON_STRING ||
      expireIn->tag != SJSON_NUMBER) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  if (expireIn->number_ < 0 || expireIn->number_ > (double) UINT64_MAX) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  self->expireTimestamp = ((uint64_t) time(NULL)) + ((uint64_t) expireIn->number_);
  self->token = strdup(token->string_);
  if (!self->token) {
    res = -ENOMEM;
    goto out_of_memory;
  }
  
out_of_memory:
invalid_response:
  sjson_destroy_context(sjson);
  return res;
}

int minecraft_auth(const char* userhash, const char* xstsToken, struct minecraft_auth_result** result) {
  int res = 0;
  char* body = NULL;
  struct minecraft_auth_result* self = malloc(sizeof(*self));
  
  size_t bodySize = util_asprintf(&body, "{\"identityToken\": \"XBL3.0 x=%s;%s\"}", userhash, xstsToken);
  if (!body) {
    res = -ENOMEM;
    goto error_creating_body;
  }
  
  struct http_request* req = networking_easy_new_http(&res, HTTP_POST, "api.minecraftservices.com", "/authentication/login_with_xbox", body, bodySize, "application/json", "application/json");
  if (!req) {
    res = -ENOMEM;
    goto error_creating_request;
  }
  
  struct transport* connection = NULL;
  if ((res = networking_easy_new_connection(true, "api.minecraftservices.com", 443, &connection)) < 0)
    goto connect_error;
  
  if ((res = http_request_send(req, connection)) < 0)
    goto request_send_error;
  
  char* responseBody = NULL;
  size_t responseBodyLen;
  FILE* memfd = open_memstream(&responseBody, &responseBodyLen);
  if (!memfd) {
    res = -ENOMEM;
    goto memfd_create_error;
  }
  
  if ((res = http_response_recv(NULL, connection, memfd)) < 0)
    goto response_recv_error;
  
  fclose(memfd);
  
  if (res == 200) {
    res = processResult200(self, responseBody, responseBodyLen);
  } else {
    res = -EFAULT;
    goto response_recv_error;
  }
  
  if (res < 0)
    pr_critical("Error processing Minecraft services API response: %d", res);
response_recv_error:
  free(responseBody);
memfd_create_error:
request_send_error:
  connection->close(connection);
connect_error:
  http_request_free(req);
error_creating_request:
  free(body);
error_creating_body:
  if (result && res > 0)
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
