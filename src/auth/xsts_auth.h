#ifndef _headers_1669033884_FluffyLauncher_xsts_auth
#define _headers_1669033884_FluffyLauncher_xsts_auth

struct xsts_auth_result {
  const char* token;
  const char* userhash;
};

int xsts_auth(const char* xblToken, struct xsts_auth_result** result);
void xsts_free(struct xsts_auth_result* self);

#endif

