#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "bug.h"
#include "http_headers.h"
#include "microsoft_auth.h"
#include "http_request.h"
#include "transport/transport.h"
#include "transport/transport_socket.h"
#include "transport/transport_ssl.h"
#include "util.h"

struct stage1 {
  struct microsoft_auth_result* result;
  struct microsoft_auth_arg* arg;
};

static int commonConnection(bool isSecure, const char* hostname, uint16_t port, struct transport** result) {
  int res = 0;
  struct ip_address ip;
  struct transport* transportResult = NULL;
  int resolveRes = networking_resolve(hostname, &ip, NETWORKING_RESOLVE_PREFER_IPV4);
  if (resolveRes < 0)
    return -EFAULT;
  
  ip.port = port;
  
  struct transport_socket* socketTransport = transport_socket_new(1000);
  if (socketTransport == NULL) {
    res = -ENOMEM;
    goto socket_transport_creation_error;
  }
  
  if (transport_socket_connect(socketTransport, &ip) < 0) {
    res = -EFAULT;
    goto connect_error;
  }
  
  if (!isSecure) {
    transportResult = &socketTransport->super;
    goto ssl_not_needed;
  }
  
  struct transport_ssl* sslTransport = transport_ssl_new(&socketTransport->super, true);
  if (sslTransport == NULL) {
    res = -ENOMEM;
    goto ssl_transport_creation_error;
  }
  
  if (transport_ssl_connect(sslTransport, TRANSPORT_TLS_ANY) < 0) {
    res = -EFAULT;
    goto ssl_connect_error;
  }
  
  if (transport_ssl_verify_host(sslTransport) == false) {
    res = -EFAULT;
    goto verify_host_error;
  }
  
  transportResult = &sslTransport->super;
verify_host_error:
ssl_connect_error:
  if (res < 0)
    transport_ssl_free(sslTransport);
ssl_transport_creation_error:
connect_error:
  if (res < 0 && !sslTransport)
    transport_socket_free(socketTransport);
socket_transport_creation_error:
ssl_not_needed:
  *result = transportResult;
  return res;
}

static int stage1_run(struct stage1* stage1) {
  int res = 0;
  struct http_request* req = http_new_request();
  if (!req) {
    res = -ENOMEM;
    goto request_creation_error;
  }
  
  char* location = NULL;
  util_asprintf(&location, "/%s/oauth2/v2.0/devicecode", stage1->arg->tenant);
  if (!location) {
    res = -ENOMEM;
    goto location_creation_error;
  }
  
  char* requestBody = NULL;
  size_t requestBodyLen = util_asprintf(&requestBody, "client_id=%s&scope=%s", stage1->arg->clientID, stage1->arg->scope);
  if (!requestBody) {
    res = -ENOMEM;
    goto request_body_creation_error;
  }
  
  char* requestBodyLenString = NULL;
  util_asprintf(&requestBodyLenString, "%zu", requestBodyLen);
  if (!requestBodyLenString) {
    res = -ENOMEM;
    goto request_body_len_error;
  }
  
  http_set_location(req, location);
  http_set_method(req, HTTP_POST);
  
  if ((res = http_headers_set(req->headers, "Content-Type", "application/x-www-form-urlencoded")) < 0 ||
      (res = http_headers_set(req->headers, "Content-Length", requestBodyLenString)) < 0 ||
      (res = http_headers_set(req->headers, "Accept", "application/json")) < 0 ||
      (res = http_headers_set(req->headers, "Host", stage1->arg->hostname)) < 0) {
    BUG_ON(res == -EINVAL);
    res = -ENOMEM;
    goto request_preparation_error;
  }
  
  req->requestData = requestBody;
  req->requestDataLen = requestBodyLen;
  
  // Do actual request
  struct transport* connection;
  if ((res = commonConnection(true, stage1->arg->hostname, stage1->arg->port, &connection)) < 0) 
    goto create_connection_error; 
  
  http_set_transport(req, connection);
  res = http_send(req);
  if (res < 0)
    goto http_request_send_error;
  
  res = http_recv(req, NULL, stdout);
  if (res < 0 || res != 200) {
    goto http_request_recv_error;
  }

http_request_recv_error:
http_request_send_error:
  connection->close(connection);
create_connection_error:
request_preparation_error:
  free(requestBodyLenString);
request_body_len_error:
  free(requestBody);
request_body_creation_error:
  free(location);
location_creation_error:
request_creation_error:
  http_free_request(req);
  return res;
} 

int microsoft_auth(struct microsoft_auth_result** resultPtr, struct microsoft_auth_arg* arg) {
  int res = 0;
  struct microsoft_auth_result* result = malloc(sizeof(*result));
  if (!result)
    return -ENOMEM;
  *result = (struct microsoft_auth_result) {};
  
  struct stage1 stage1Data = {
    .arg = arg,
    .result = result
  };
  
  if ((res = stage1_run(&stage1Data)) < 0)
    goto stage1_failure;
  
stage1_failure:
  if (res < 0) {
    microsoft_auth_free(result);
    result = NULL;
  }
  *resultPtr = result;
  return res;
}

void microsoft_auth_free(struct microsoft_auth_result* authResult) {
  if (!authResult)
    return;
  
  free((char*) authResult->errorMessage);
  free((char*) authResult->accessToken);
  free((char*) authResult->refreshToken);
  free((char*) authResult->idToken);
  free(authResult);
}
