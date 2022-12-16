#ifndef _headers_1671105347_FluffyLauncher_encoder
#define _headers_1671105347_FluffyLauncher_encoder

#include <stddef.h>

#include "buffer.h"
#include "json.h"

int json_decode_default(struct json_node* root, buffer_t* buffer);

#endif

