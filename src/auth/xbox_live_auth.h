#ifndef _headers_1668950532_FluffyLauncher_xbox_live_auth
#define _headers_1668950532_FluffyLauncher_xbox_live_auth

struct xbox_live_auth_result {
  const char* token;
  const char* userhash;
};

int xbox_live_auth(const char* microsoftToken, struct xbox_live_auth_result** result);
void xbox_live_free(struct xbox_live_auth_result* self);

#endif

