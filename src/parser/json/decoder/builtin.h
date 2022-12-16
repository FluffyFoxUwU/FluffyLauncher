#ifndef _headers_1670739325_FluffyLauncher_builtin
#define _headers_1670739325_FluffyLauncher_builtin

#include <stddef.h>
#include "parser/json/json.h"

int json_decode_builtin(struct json_node** root, const char* data, size_t len);

#endif

