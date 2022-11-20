#ifndef _headers_1668919817_FluffyLauncher_stage1
#define _headers_1668919817_FluffyLauncher_stage1

#include <stdint.h>

struct microsoft_auth_result;
struct microsoft_auth_arg;

struct microsoft_auth_stage1 {
  struct microsoft_auth_result* result;
  struct microsoft_auth_arg* arg;
  
  // Stage 1 result
  const char* deviceCode;
  const char* userCode;
  const char* verificationURL;
  
  uint64_t expireIn;
  uint64_t pollInterval;
};

struct microsoft_auth_stage1* microsoft_auth_stage1_new(struct microsoft_auth_result* result, struct microsoft_auth_arg* arg);
int microsoft_auth_stage1_run(struct microsoft_auth_stage1* self);
void microsoft_auth_stage1_free(struct microsoft_auth_stage1* self);

#endif

