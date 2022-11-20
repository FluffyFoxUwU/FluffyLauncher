#ifndef _headers_1668919947_FluffyLauncher_easy
#define _headers_1668919947_FluffyLauncher_easy

#include <stdint.h>
#include <stdbool.h>

#include "http_request.h"
#include "networking/transport/transport.h"

// Contain convenience wrappers for commonly used stuffs
int networking_easy_new_connection(bool isSecure, const char* hostname, uint16_t port, struct transport** result);

// Conveniencly create and prepare HTTP request
// status is 0 on success
// Negative on error
struct http_request* networking_easy_new_http(int* status, enum http_method method, const char* hostname, const char* location, void* requestBody, size_t requestBodySize, const char* requestBodyType, const char* accept);

#endif

