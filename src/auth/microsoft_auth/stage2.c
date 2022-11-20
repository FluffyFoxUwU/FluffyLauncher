#include <stdlib.h>

#include "stage2.h"

struct microsoft_auth_stage2* microsoft_auth_stage2_new(struct microsoft_auth_result* result, struct microsoft_auth_arg* arg, struct microsoft_auth_stage1* stag1) {
  struct microsoft_auth_stage2* self = malloc(sizeof(*self));
  if (!self)
    goto failure;
  
  return self;

failure:
  microsoft_auth_stage2_free(self);
  return NULL;
}

void microsoft_auth_stage2_free(struct microsoft_auth_stage2 *self) {
  if (!self)
    return;
  
  free(self);
}

int microsoft_auth_stage2_run(struct microsoft_auth_stage2* self) {
  int res = 0;
  return res;
}
