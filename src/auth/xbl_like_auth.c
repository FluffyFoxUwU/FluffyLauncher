#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "xbl_like_auth.h"
#include "buffer.h"
#include "networking/easy.h"
#include "networking/transport/transport.h"
#include "util/json_schema_loader.h"
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

struct xbl_like_200_response {
  buffer_t* token;
  buffer_t* userhash;
  struct json_array* displayclaims_xui;
};

static const struct json_schema xblLike200ResponseSchema = {
  .entries = {
    JSON_SCHEMA_ENTRY("$.Token", JSON_STRING, struct xbl_like_200_response, token),
    JSON_SCHEMA_ENTRY("$.DisplayClaims.xui[0].uhs", JSON_STRING, struct xbl_like_200_response, userhash),
    JSON_SCHEMA_ENTRY("$.DisplayClaims.xui", JSON_ARRAY, struct xbl_like_200_response, displayclaims_xui),
    {}
  }
};

static int processResult200(struct xbl_like_auth_result* self, struct json_node* root) {
  int res = 0;
  struct xbl_like_200_response response = {};
  if (root == NULL) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  if ((res = json_schema_load(&xblLike200ResponseSchema, root, &response)) < 0)
    goto invalid_response;
  
  if (response.displayclaims_xui->array.length >= 2)
    pr_warn("There are more than one entry in $.DisplayClaims.xui array");
  
  self->token = strdup(buffer_string(response.token));
  self->userhash = strdup(buffer_string(response.userhash));
  if (!self->token || !self->userhash) {
    res = -ENOMEM;
    goto out_of_memory;
  }
out_of_memory:
invalid_response:
  return res;
}

struct xbl_like_401_response {
  double xerr;
};

static const struct json_schema xblLike401ResponseSchema = {
  .entries = {
    JSON_SCHEMA_ENTRY("$.XErr", JSON_NUMBER, struct xbl_like_401_response, xerr),
    {}
  }
};

static int processResult401(struct xbl_like_auth_result* self, struct json_node* root) {
  int res = 0;
  if (root == NULL) {
    res = -EINVAL;
    goto invalid_response;
  }
  
  struct xbl_like_401_response response = {};
  if ((res = json_schema_load(&xblLike401ResponseSchema, root, &response)) < 0)
    goto invalid_response;
  
  if (response.xerr < 0 || response.xerr > (double) UINT64_MAX)  {
    res = -EINVAL;
    goto invalid_response;
  }
  
  switch ((uint64_t) response.xerr) {
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
      pr_alert("Unable to authenticate: Unknown XBox error: %" PRIu64, (uint64_t) response.xerr);
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



