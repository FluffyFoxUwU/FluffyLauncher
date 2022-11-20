#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "microsoft_auth.h" 
#include "microsoft_auth/stage1.h"

int microsoft_auth(struct microsoft_auth_result** resultPtr, struct microsoft_auth_arg* arg) {
  int res = 0;
  struct microsoft_auth_result* result = malloc(sizeof(*result));
  if (!result)
    return -ENOMEM;
  *result = (struct microsoft_auth_result) {};
  
  struct microsoft_auth_stage1* stage1Data = microsoft_auth_stage1_new(result, arg);

  if ((res = microsoft_auth_stage1_run(stage1Data)) < 0)
    goto stage1_failure;

stage1_failure:
  microsoft_auth_stage1_free(stage1Data);
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
