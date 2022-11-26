#ifndef _headers_1669113525_FluffyLauncher_minecraft_auth
#define _headers_1669113525_FluffyLauncher_minecraft_auth

#include <stdint.h>

struct minecraft_auth_result {
  const char* token;
  uint64_t expireTimestamp;
};

int minecraft_auth(const char* userhash, const char* xstsToken, struct minecraft_auth_result** result);
void minecraft_auth_free(struct minecraft_auth_result* self);

#endif

