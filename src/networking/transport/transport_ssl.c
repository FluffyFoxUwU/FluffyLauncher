#include <errno.h> 
#include <openssl/ssl.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "networking/ssl_method_compat_layer.h"
#include "bug.h"
#include "networking/transport/transport.h"
#include "transport_ssl.h"
#include "util/util.h"
#include "transport.h"

#define SELF(ptr) container_of(ptr, struct transport_ssl, super)

struct transport_ssl_private {
  SSL_CTX* sslContext;
  SSL* ssl;
  
  bool hasHandshakePerformed;
  enum ssl_version sslVersion;
};

static int impl_write(struct transport* _self, const void* data, size_t len);
static int impl_read(struct transport* _self, void* result, size_t len, size_t* szRead); 
static void impl_close(struct transport* _self);

struct transport_ssl* transport_ssl_new(struct transport* socket, bool verify) {
  struct transport_ssl* self = malloc(sizeof(*self));
  if (!self)
    return NULL;
  
  self->priv = malloc(sizeof(*self->priv));
  if (!self->priv)
    return NULL;
  
  self->transportLayer = socket;
  self->super.close = impl_close;
  self->super.read = impl_read;
  self->super.write = impl_write;
  
  self->priv->ssl = NULL;
  self->priv->sslContext = NULL;
  self->priv->hasHandshakePerformed = false;
  self->priv->sslVersion = TRANSPORT_SSL_UNKNOWN;
  
  SSL_CTX* sslCtx = SSL_CTX_new(TLS_method());
  if (!sslCtx) 
    goto error_creating_ssl_context;
  
  SSL* ssl = SSL_new(sslCtx);
  if (!ssl) 
    goto error_creating_ssl; 
  
  if (SSL_CTX_set_default_verify_paths(sslCtx) != 1) 
    goto error_load_verify_path;
  
  int sockFd = self->transportLayer->get_sockfd(self->transportLayer);
  if (sockFd < 0 || SSL_set_fd(ssl, sockFd) == 0)
    goto error_setting_sockfd;
  
  self->priv->sslContext = sslCtx;
  self->priv->ssl = ssl;
  return self;

error_load_verify_path:
error_setting_sockfd:
error_creating_ssl:
error_creating_ssl_context:
  transport_ssl_free(self);
  return NULL;
}

bool transport_ssl_verify_host(struct transport_ssl* self) {
  X509* cert = SSL_get_peer_certificate(self->priv->ssl);
  if (!cert)
    return false;
  
  X509_free(cert);
  return SSL_get_verify_result(self->priv->ssl) == X509_V_OK;
}

static void computeSSLVersion(struct transport_ssl* self) {
  self->priv->sslVersion = TRANSPORT_SSL_UNKNOWN;
  const char* sslVersion = SSL_get_version(self->priv->ssl);
  if (!sslVersion)
    return;

# define X(enum, fingerprint, ...) \
  else if (strcmp(sslVersion, fingerprint) == 0) \
    self->priv->sslVersion = enum;
  
  if (0)
    ;
  TRANSPORT_SSL_VERSIONS
#undef X
}

int transport_ssl_connect(struct transport_ssl* self, enum ssl_version minVersion) {
  int ret = SSL_connect(self->priv->ssl);
  if (ret != 1) {
    ret = -EFAULT;
    goto connect_failure;
  }
  self->priv->hasHandshakePerformed = true;
  
  computeSSLVersion(self);
  if (self->priv->sslVersion < minVersion)
    return -ENOTSUP;
    
connect_failure:
  return ret;
}

int transport_ssl_write(struct transport_ssl* self, const void* data, size_t len) {
  if (len > INT_MAX)
    return -E2BIG;
  if (len == 0)
    return 0;
  
  int ret = SSL_write(self->priv->ssl, data, len);
  switch (SSL_get_error(self->priv->ssl, ret)) {
    case SSL_ERROR_NONE:
      break;
    default:
      return -EFAULT;
  }
  return ret;
}

int transport_ssl_read(struct transport_ssl* self, void* result, size_t len, size_t* szRead) {
  int ret;
  if (szRead)
    ret = SSL_read_ex(self->priv->ssl, result, len, szRead);
  else
    ret = SSL_read(self->priv->ssl, result, len); 
  
  switch (SSL_get_error(self->priv->ssl, ret)) {
    case SSL_ERROR_NONE:
      break;
    default:
      return -EFAULT;
  }
  return ret;
}

bool transport_ssl_get_handshake_state(struct transport_ssl* self) {
  return self->priv->hasHandshakePerformed;
}

enum ssl_version transport_ssl_get_version(struct transport_ssl* self) {
  BUG_ON(transport_ssl_get_handshake_state(self) == false);
  return self->priv->sslVersion;
}

void transport_ssl_free(struct transport_ssl* self) {
  transport_base_close(&self->super);
  
  SSL_free(self->priv->ssl);
  SSL_CTX_free(self->priv->sslContext);
  
  self->transportLayer->close(self->transportLayer);
  free(self->priv);
  free(self);
}

// Implementations 
static int impl_write(struct transport* _self, const void* data, size_t len) {
  return transport_ssl_write(SELF(_self), data, len);
}

static int impl_read(struct transport* _self, void* result, size_t len, size_t* szRead) {
  return transport_ssl_read(SELF(_self), result, len, szRead);
}

static void impl_close(struct transport* _self) {
  transport_ssl_free(SELF(_self));
}
