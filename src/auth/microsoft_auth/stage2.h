#ifndef _headers_1668920890_FluffyLauncher_stage2
#define _headers_1668920890_FluffyLauncher_stage2

#include <time.h>

struct microsoft_auth_result;
struct microsoft_auth_arg;
struct microsoft_auth_stage1;

struct microsoft_auth_stage2 {
  struct microsoft_auth_result* result;
  struct microsoft_auth_arg* arg;
  struct microsoft_auth_stage1* stage1;
  
  // Stage2 result
  time_t expireIn;
  const char* token;
  const char* refreshToken;
};

struct microsoft_auth_stage2* microsoft_auth_stage2_new(struct microsoft_auth_result* result, struct microsoft_auth_arg* arg, struct microsoft_auth_stage1* stag1);
void microsoft_auth_stage2_free(struct microsoft_auth_stage2 *self);

// Errors:
// -EPERM: User declined authorization request 
// -ETIMEDOUT: Device code expired out and user havent authenticate yet
// -EFAULT: Catch it all error
int microsoft_auth_stage2_run(struct microsoft_auth_stage2* self);

#endif

