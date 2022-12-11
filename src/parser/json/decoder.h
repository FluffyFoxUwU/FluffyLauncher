#ifndef _headers_1670734550_FluffyLauncher_decoder
#define _headers_1670734550_FluffyLauncher_decoder

#include <stddef.h>

#include "json.h"

int json_decode_default(struct json_node** root, const char* data, size_t len);

#endif

