#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "xbl_like_auth.h"
#include "networking/easy.h"
#include "networking/transport/transport.h"
#include "xbl_like_auth.h"
#include "networking/http_request.h"
#include "util/util.h"
#include "parser/json/json.h"
#include "networking/http_response.h"
#include "logging/logging.h"

static void xbl_like_auth_free(struct xbl_like_auth_result* self) {
  if (!self)
    return;
  
  free((char*) self->userhash);
  free((char*) self->token);
}

static int processResult200(struct xbl_like_auth_result* self, struct json_node* root) {
  int res = 0;
  if (root->type != JSON_OBJECT) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  struct json_node* token;
  struct json_node* displayClaims;
  if (json_get_member(root, "Token", &token) < 0 ||
      json_get_member(root, "DisplayClaims", &displayClaims) ||
       token->type != JSON_STRING ||
       displayClaims->type != JSON_OBJECT) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  struct json_node* xui;
  if (json_get_member(displayClaims, "xui", &xui) < 0 || xui->type != JSON_ARRAY) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  if (JSON_ARRAY(xui)->array.length < 1) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  struct json_node* first = JSON_ARRAY(xui)->array.data[0];
  if (first->type != JSON_OBJECT) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  if (JSON_ARRAY(xui)->array.length > 2)
    pr_warn("There are more than one entry in $.DisplayClaims.xui array");
  
  struct json_node* userhash;
  if (json_get_member(first, "uhs", &userhash) < 0 || userhash->type != JSON_STRING) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  self->token = strdup(buffer_string(JSON_STRING(token)->string));
  self->userhash = strdup(buffer_string(JSON_STRING(userhash)->string));
  if (!self->token || !self->userhash) {
    res = -ENOMEM;
    goto out_of_memory;
  }
out_of_memory:
invalid_response:
  return res;
}

static int processResult401(struct xbl_like_auth_result* self, struct json_node* root) {
  int res = 0;
  if (root->type != JSON_OBJECT) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  struct json_node* xerr;
  if (json_get_member(root, "XErr", &xerr) < 0 || xerr->type != JSON_NUMBER) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  if (JSON_NUMBER(xerr)->number < 0 || JSON_NUMBER(xerr)->number > (double) UINT64_MAX)  {
    res = -EINVAL;
    goto invalid_response;
  }
  
  uint64_t xerrInteger = JSON_NUMBER(xerr)->number;
  switch (xerrInteger) {
    case XBL_LIKE_UNDERAGE:
      pr_alert("Unable to authenticate: Your account is underage unless added to family by an adult");
      break;
    case XBL_LIKE_NO_XBOX_ACOUNT:
      pr_alert("Unable to authenticate: There is no XBox Live please create one");
      break;
    case XBL_LIKE_BANNED_OR_UNAVAILABLE:
      pr_alert("Unable to authenticate: XBox Live unavailable/banned in your country");
      break;
    case XBL_LIKE_REQUIRE_ADULT_VERIFY:
    case XBL_LIKE_REQUIRE_ADULT_VERIFY2:
      pr_alert("Unable to authenticate: Adult verification needed (South Korea)");
      break;
    default:
      pr_alert("Unable to authenticate: Unknown XBox error: %" PRIu64, xerrInteger);
      break;
  }
invalid_response:
  return res;
}

int xbl_like_auth(const char* hostname, const char* location, const char* requestBody, struct xbl_like_auth_result* result) {
  int res = 0;
  struct xbl_like_auth_result self = (struct xbl_like_auth_result) {};
  
  struct easy_http_headers headers[] = {
    {"Accept", "application/json"},
    {"Content-Type", "application/json"},
    {NULL, NULL}
  };
  
  struct json_node* responseJson;
  res = networking_easy_do_json_http_rpc(&responseJson, 
                                         true,
                                         HTTP_POST, 
                                         hostname, 
                                         location, 
                                         headers,
                                         "%s",
                                         requestBody);
  if (res < 0)
    goto request_error;
  
  if (res == 200)
    res = processResult200(&self, responseJson);
  else if (res == 401)
    res = processResult401(&self, responseJson); 
  else if (res >= 0)
    pr_critical("Unexpected XBL like server responded with %d", res);
  json_free(responseJson);
  
  if (res < 0)
    pr_critical("Error processing XBL like server response: %d", res);
request_error: 
  if (result && res >= 0)
    *result = self;  
  else
    xbl_like_auth_free(&self);
  return res;
}



