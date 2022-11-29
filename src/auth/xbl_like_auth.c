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
#include "parser/sjson.h"
#include "networking/http_response.h"
#include "logging/logging.h"

static void xbl_like_auth_free(struct xbl_like_auth_result* self) {
  if (!self)
    return;
  
  free((char*) self->userhash);
  free((char*) self->token);
}

static int processResult200(struct xbl_like_auth_result* self, struct sjson_node* root) {
  int res = 0;
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
  return res;
}

static int processResult401(struct xbl_like_auth_result* self, sjson_node* root) {
  int res = 0;
  if (root->tag != SJSON_OBJECT) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  sjson_node* xerr = sjson_find_member(root, "XErr");
  if (!xerr || xerr->tag != SJSON_NUMBER) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  if (xerr->number_ < 0 || xerr->number_ > (double) UINT64_MAX)  {
    res = -EINVAL;
    goto invalid_response;
  }
  
  uint64_t xerrInteger = xerr->number_;
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
  sjson_context* jsonCtx;
  sjson_node* responseJson;
  res = networking_easy_do_json_http_rpc(&jsonCtx,
                                         &responseJson, 
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
  sjson_destroy_context(jsonCtx);
  
  if (res < 0)
    pr_critical("Error processing XBL like server response: %d", res);
request_error: 
  if (result && res >= 0)
    *result = self;  
  else
    xbl_like_auth_free(&self);
  return res;
}



