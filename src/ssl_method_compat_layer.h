#ifndef _headers_1668687227_FluffyLauncher_ssl_context_manager
#define _headers_1668687227_FluffyLauncher_ssl_context_manager

#include <openssl/ssl.h>

#define SSL_METHOD_COMPAT_LAYER_NULL_FUNCTION(prefix) \
  [[maybe_unused]] \
  static inline const SSL_METHOD* prefix ## _method() { \
    return NULL; \
  } \
  [[maybe_unused]] \
  static inline const SSL_METHOD* prefix ## _client_method() { \
    return NULL; \
  } \
  [[maybe_unused]] \
  static inline const SSL_METHOD* prefix ## _server_method() { \
    return NULL; \
  }

#ifdef OPENSSL_NO_SSL3_METHOD
SSL_METHOD_COMPAT_LAYER_NULL_FUNCTION(SSLv3);
#endif

#ifdef OPENSSL_NO_TLS1_METHOD
SSL_METHOD_COMPAT_LAYER_NULL_FUNCTION(TLSv1);
#endif

#ifdef OPENSSL_NO_TLS1_1_METHOD
SSL_METHOD_COMPAT_LAYER_NULL_FUNCTION(TLSv1_1);
#endif

#ifdef OPENSSL_NO_TLS1_2_METHOD
SSL_METHOD_COMPAT_LAYER_NULL_FUNCTION(TLSv1_2);
#endif

#ifdef OPENSSL_NO_DTLS1_METHOD
SSL_METHOD_COMPAT_LAYER_NULL_FUNCTION(DTLSv1);
#endif

#ifdef OPENSSL_NO_DTLS1_2_METHOD
SSL_METHOD_COMPAT_LAYER_NULL_FUNCTION(DTLSv1_2);
#endif

#endif

