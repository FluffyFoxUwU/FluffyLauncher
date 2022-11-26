#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "xbl_like_auth.h"
#include "xsts_auth.h"
#include "util/util.h"

int xsts_auth(const char* xblToken, struct xsts_auth_result** result) {
  int res = 0;
  struct xsts_auth_result* self = malloc(sizeof(*self));
  if (!self)
    return -ENOMEM;
  
  char* requestBody = NULL;
  util_asprintf(&requestBody, 
"{ \
  \"Properties\": { \
    \"SandboxId\": \"RETAIL\", \
    \"UserTokens\": [ \
      \"%s\" \
    ] \
  }, \
  \"RelyingParty\": \"rp://api.minecraftservices.com/\", \
  \"TokenType\": \"JWT\" \
}", xblToken);

  if (!requestBody) {
    res = -ENOMEM;
    goto request_body_creation_error;
  }
  
  struct xbl_like_auth_result xblLikeAuthResult = {};
  res = xbl_like_auth("xsts.auth.xboxlive.com", "/xsts/authorize", requestBody, &xblLikeAuthResult);
  if (res < 0)
    goto xbl_like_auth_error;
  
  self->token = xblLikeAuthResult.token;
  self->userhash = xblLikeAuthResult.userhash;
xbl_like_auth_error:
  free(requestBody);
request_body_creation_error:
  if (result && res >= 0)
    *result = self;  
  else
    xsts_free(self);
  return res;
}

void xsts_free(struct xsts_auth_result* self) {
  if (!self)
    return;
  
  free((char*) self->userhash);
  free((char*) self->token);
  free(self);
}
