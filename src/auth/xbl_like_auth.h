#ifndef _headers_1669031806_FluffyLauncher_xbl_like_auth
#define _headers_1669031806_FluffyLauncher_xbl_like_auth

#include <stdint.h>

struct xbl_like_auth_result {
  const char* token;
  const char* userhash;
};

int xbl_like_auth(const char* hostname, const char* location, const char* requestBody, struct xbl_like_auth_result* result);

#endif

