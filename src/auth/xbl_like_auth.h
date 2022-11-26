#ifndef _headers_1669031806_FluffyLauncher_xbl_like_auth
#define _headers_1669031806_FluffyLauncher_xbl_like_auth

#include <stdint.h>

struct xbl_like_auth_result {
  const char* token;
  const char* userhash;
};

enum xbl_error {
  XBL_LIKE_NO_XBOX_ACOUNT = 2148916233,
  XBL_LIKE_BANNED_OR_UNAVAILABLE = 2148916235,
  XBL_LIKE_REQUIRE_ADULT_VERIFY = 2148916236,
  XBL_LIKE_REQUIRE_ADULT_VERIFY2 = 2148916237,
  XBL_LIKE_UNDERAGE = 2148916238
};

int xbl_like_auth(const char* hostname, const char* location, const char* requestBody, struct xbl_like_auth_result* result);

#endif

