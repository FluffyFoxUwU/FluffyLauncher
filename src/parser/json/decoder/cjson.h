#ifndef _headers_1670933483_FluffyLauncher_cjson
#define _headers_1670933483_FluffyLauncher_cjson

#include <stddef.h>
#include "parser/json/json.h"

int json_decode_cjson(struct json_node** root, const char* data, size_t len);

#endif

