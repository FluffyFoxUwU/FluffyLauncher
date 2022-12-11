#ifndef _headers_1668919947_FluffyLauncher_easy
#define _headers_1668919947_FluffyLauncher_easy

#include <stdint.h>
#include <stdbool.h>

#include "http_request.h"
#include "networking/transport/transport.h"
#include "parser/json/json.h"

// Only if both key and value is NULL is where its treated end of header list
struct easy_http_headers {
  const char* key;
  const char* value;
};

// Contain convenience wrappers for commonly used stuffs
int networking_easy_new_connection(bool isSecure, 
                                   const char* hostname, 
                                   uint16_t port, 
                                   struct transport** result);

// Conveniencly create and prepare HTTP request
// return 0 on success
// Negative errno on error
int networking_easy_new_http_va(struct http_request** result,
                             enum http_method method, 
                             const char* hostname, 
                             const char* location, 
                             struct easy_http_headers* headers,
                             const char* requestBodyFormat,
                             va_list args);

// Dont forget to free (*result)->requestData
int networking_easy_new_http(struct http_request** result,
                             enum http_method method, 
                             const char* hostname, 
                             const char* location, 
                             struct easy_http_headers* headers,
                             const char* requestBodyFormat,
                             ...);

int networking_easy_do_http_va(void** response, 
                            size_t* responseLength, 
                            bool isSecure,
                            enum http_method method, 
                            const char* hostname, 
                            const char* location, 
                            struct easy_http_headers* headers,
                            const char* requestBodyFormat,
                            va_list args);

int networking_easy_do_http(void** response, 
                            size_t* responseLength, 
                            bool isSecure,
                            enum http_method method, 
                            const char* hostname, 
                            const char* location, 
                            struct easy_http_headers* headers,
                            const char* requestBodyFormat,
                            ...);

// Request is arbitary and response giving out in JSON
int networking_easy_do_json_http_rpc_va(struct json_node** root, 
                                     bool isSecure,
                                     enum http_method method, 
                                     const char* hostname, 
                                     const char* location, 
                                     struct easy_http_headers* headers,
                                     const char* requestBodyFormat,
                                     va_list args);
int networking_easy_do_json_http_rpc(struct json_node** root, 
                                     bool isSecure,
                                     enum http_method method, 
                                     const char* hostname, 
                                     const char* location, 
                                     struct easy_http_headers* headers,
                                     const char* requestBodyFormat,
                                     ...);

#endif

