#include <Block.h>
#include <stdlib.h>
#include <string.h>

#include "api.h"
#include "parser/sjson.h"

static void cleanLastCall(struct minecraft_api* self) {
  if (self->lastResponseJSON)
    sjson_destroy_context(self->lastResponseJSON);
}

struct minecraft_api* minecraft_api_new(const char* token) {
  struct minecraft_api* self = malloc(sizeof(*self));
  if (!self)
    return NULL;
  *self = (struct minecraft_api) {};
  
  self->token = strdup(token);
  if (!self->token)
    goto failure;
  
  return self;

failure:
  minecraft_api_free(self);
  return NULL;
}

int minecraft_api_get_profile(struct minecraft_api* self) {
  cleanLastCall(self);
  int res = 0;
  
  return res;
}

void minecraft_api_free(struct minecraft_api* self) {
  if (!self)
    return;
  
  cleanLastCall(self);
  
  free((char*) self->token);
  free(self);
}
