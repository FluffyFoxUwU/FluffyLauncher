#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "xbl_like_auth.h"
#include "xbox_live_auth.h"
#include "util/util.h"

int xbox_live_auth(const char* microsoftToken, struct xbox_live_auth_result** result) {
  int res = 0;
  struct xbox_live_auth_result* self = malloc(sizeof(*self));
  if (!self)
    return -ENOMEM;
  *self = (struct xbox_live_auth_result) {};
  
  char* requestBody = NULL;
  util_asprintf(&requestBody, 
"{ \
  \"Properties\": { \
    \"AuthMethod\": \"RPS\",\
    \"SiteName\": \"user.auth.xboxlive.com\",\
    \"RpsTicket\": \"d=%s\"\
  }, \
  \"RelyingParty\": \"http://auth.xboxlive.com\", \
  \"TokenType\": \"JWT\" \
}", microsoftToken);

  if (!requestBody) {
    res = -ENOMEM;
    goto request_body_creation_error;
  }
  
  struct xbl_like_auth_result xblLikeAuthResult = {};
  res = xbl_like_auth("user.auth.xboxlive.com", "/user/authenticate", requestBody, &xblLikeAuthResult);
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
