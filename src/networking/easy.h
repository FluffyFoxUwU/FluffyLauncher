#ifndef _headers_1668919947_FluffyLauncher_easy
#define _headers_1668919947_FluffyLauncher_easy

#include <stdint.h>
#include <stdbool.h>

#include "http_request.h"
#include "networking/transport/transport.h"

// Contain convenience wrappers for commonly used stuffs
int networking_easy_new_connection(bool isSecure, const char* hostname, uint16_t port, struct transport** result);

// Conveniencly create and prepare HTTP request
struct http_request* networking_easy_new_http(enum http_method method, const char* location, void* requestBody, size_t requestBodySize);

#endif

