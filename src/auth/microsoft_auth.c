#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "logging/logging.h"
#include "microsoft_auth.h" 
#include "microsoft_auth/stage1.h"
#include "microsoft_auth/stage2.h"

int microsoft_auth(struct microsoft_auth_result** resultPtr, struct microsoft_auth_arg* arg) {
  int res = 0;
  struct microsoft_auth_result* result = malloc(sizeof(*result));
  if (!result)
    return -ENOMEM;
  *result = (struct microsoft_auth_result) {};
  
  struct microsoft_auth_stage1* stage1 = NULL;
  if (!arg->refreshToken) {
    pr_info("Refresh token not present. Restarting authentication flow from stage 1");
    stage1 = microsoft_auth_stage1_new(result, arg);
    if (!stage1) {
      res = -ENOMEM;
      goto stage1_failure;
    }

    if ((res = microsoft_auth_stage1_run(stage1)) < 0)
      goto stage1_failure;
  } else {
    pr_info("Refresh token present. Skipping stage 1");
  }

  struct microsoft_auth_stage2* stage2 = microsoft_auth_stage2_new(result, arg, stage1);
  if (!stage2) {
    res = -ENOMEM;
    goto stage2_failure;
  }

  if ((res = microsoft_auth_stage2_run(stage2)) < 0)
    goto stage2_failure;

stage2_failure:
  microsoft_auth_stage2_free(stage2);
stage1_failure:
  microsoft_auth_stage1_free(stage1);
  if (res < 0) {
    microsoft_auth_free(result);
    result = NULL;
  }
  *resultPtr = result;
  return res;
}

void microsoft_auth_free(struct microsoft_auth_result* authResult) {
  if (!authResult)
    return;
  
  free((char*) authResult->errorMessage);
  free((char*) authResult->accessToken);
  free((char*) authResult->refreshToken);
  free((char*) authResult->idToken);
  free(authResult);
}
