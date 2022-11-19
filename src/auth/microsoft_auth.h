#ifndef _headers_1667618789_FluffyLauncher_microsoft_auth
#define _headers_1667618789_FluffyLauncher_microsoft_auth

#include <stdint.h>
#include <time.h>

enum microsoft_auth_token_type {
  MICROSOFT_AUTH_TOKEN_BEARER
};

enum microsoft_auth_protocol {
  MICROSOFT_AUTH_HTTP,
  MICROSOFT_AUTH_HTTPS
};

struct microsoft_auth_result {
  enum microsoft_auth_token_type tokenType;
  const char* scope;
  time_t expiresIn; // In seconds
  const char* accessToken; 
  const char* idToken;
  const char* refreshToken; 
  
  // Only if microsoft_auth returned -EFAULT
  const char* errorMessage;
};

struct microsoft_auth_arg {
  const char* scope;
  const char* tenant;
  const char* clientID;
  const char* hostname;
  
  uint16_t port;
  enum microsoft_auth_protocol protocol;
};

int microsoft_auth(struct microsoft_auth_result** result, struct microsoft_auth_arg* arg);
void microsoft_auth_free(struct microsoft_auth_result* authResult);

#endif

