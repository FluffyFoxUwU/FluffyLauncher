#include <stdlib.h>
#include <stdio.h>
#include <openssl/ssl.h>

#include "auth/microsoft_auth.h"
#include "bug.h"
#include "networking/http_headers.h"
#include "networking/http_request.h"
#include "io/io_threads.h"
#include "networking/networking.h"
#include "networking/transport/transport.h"
#include "networking/transport/transport_socket.h"
#include "networking/transport/transport_ssl.h"
#include "config.h"

int main2(int argc, char** argv) {
  SSL_load_error_strings();
  SSL_library_init();
  OpenSSL_add_all_algorithms();
  OpenSSL_add_all_ciphers();
  OpenSSL_add_all_digests();
  
  struct microsoft_auth_result* result = NULL;
  struct microsoft_auth_arg arg = {
    .clientID = CONFIG_AUTH_AZURE_CLIENT_ID,
    .tenant = "consumers",
    .hostname = "login.microsoftonline.com",
    .scope = "XboxLive.signin",
    
    .protocol = MICROSOFT_AUTH_HTTPS,
    .port = 443
  };
  
  int res = microsoft_auth(&result, &arg);
  printf("Res: %d\n", res);
  microsoft_auth_free(result); 
  
  /*
  // https://launchermeta.mojang.com/mc/game/version_manifest.json
  FILE* manifestFile = fopen("./manifest.json", "w");
  BUG_ON(!manifestFile);
  
  struct ip_address ip;
  int resolveRes = networking_resolve("localhost", &ip, NETWORKING_RESOLVE_PREFER_IPV4);
  BUG_ON(resolveRes < 0);
  ip.port = 80;
  
  struct transport_socket* socketTransport = transport_socket_new(1000);
  transport_socket_connect(socketTransport, &ip);
  
  // struct transport_ssl* sslTransport = transport_ssl_new(&socketTransport->super, true);
  // BUG_ON(transport_ssl_connect(sslTransport, TRANSPORT_TLS_ANY) < 0);
  // BUG_ON(transport_ssl_verify_host(sslTransport) == false);
  
  struct transport* transport = &socketTransport->super;
  
  struct http_request* req = http_new_request();
  struct http_response* res = NULL;
  http_set_location(req, "/");
  http_set_method(req, HTTP_GET);
  
  http_headers_set(req->headers, "Host", "launchermeta.mojang.com");
  http_headers_set(req->headers, "Accept", "application/json");
  // http_headers_set(req->headers, "Transfer-Encoding", "identity");
  // http_headers_set(req->headers, "Connection", "Keep-Alive");
  
  int code = http_exec(req, transport, &res, manifestFile);
  printf("Res: %d\n", code); 
  printf("(Response body size: %zu)\n", res->writtenSize);
  
  http_free_response(res);
  http_free_request(req);
  
  transport->close(transport);
  fclose(manifestFile);
  
  // struct ip_address ip;
  // int resolveRes = networking_resolve("www.google.com", &ip, NETWORKING_RESOLVE_PREFER_IPV4);
  // BUG_ON(resolveRes < 0);
  // ip.port = 80;
  
  // puts(networking_tostring(&ip));
  
  // struct transport_socket* socket = transport_socket_new(5000);
  // BUG_ON(transport_socket_connect(socket, &ip) < 0);
  
  // struct transport* transport = &socket->super;
  // const char httpReq[] = 
  // "GET / HTTP/1.0\r\n"
  // "Host: www.google.com\r\n"
  // "\r\n";
  // transport->write(transport, httpReq, sizeof(httpReq));
  
  // char buffer[64] = "UwU untouched yay";
  // transport->read(transport, buffer, 64);
  
  // puts("Response: ");
  // fwrite(buffer, 1, 64, stdout);
  // puts("\n");
  // transport->close(transport);
  */
  
  return EXIT_SUCCESS;
}

