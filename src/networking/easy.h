#ifndef _headers_1668919947_FluffyLauncher_easy
#define _headers_1668919947_FluffyLauncher_easy

#include <stdint.h>
#include <stdbool.h>

#include "transport/transport.h"

int networking_easy_new_connection(bool isSecure, const char* hostname, uint16_t port, struct transport** result);

#endif

