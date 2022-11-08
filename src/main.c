#include <stdlib.h>
#include <stdio.h>

#include "bug.h"
#include "http_headers.h"
#include "http_request.h"
#include "networking.h"
#include "transport/transport.h"
#include "transport/transport_socket.h"

int main2(int argc, char** argv) {
  // https://launchermeta.mojang.com/mc/game/version_manifest.json
  FILE* manifestFile = fopen("./manifest.json", "w");
  BUG_ON(!manifestFile);
  
  struct ip_address ip;
  int resolveRes = networking_resolve("launchermeta.mojang.com", &ip, NETWORKING_RESOLVE_PREFER_IPV4);
  BUG_ON(resolveRes < 0);
  ip.port = 80;
  
  struct transport_socket* socketTransport = transport_socket_new(1000);
  transport_socket_connect(socketTransport, &ip);
  
  struct http_request* req = http_new_request();
  struct http_response* res = NULL;
  http_set_location(req, "/mc/game/version_manifest.json");
  http_set_method(req, HTTP_GET);
  
  http_headers_set(req->headers, "Host", "launchermeta.mojang.com");
  http_headers_set(req->headers, "Accept", "application/json");
  // http_headers_set(req->headers, "Transfer-Encoding", "identity");
  // http_headers_set(req->headers, "Connection", "Keep-Alive");
  
  int code = http_exec(req, &socketTransport->super, &res, manifestFile);
  printf("Res: %d\n", code); 
  printf("(Response body size: %zu)\n", res->writtenSize);
  
  http_free_response(res);
  http_free_request(req);
  
  transport_socket_free(socketTransport);
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
  return EXIT_SUCCESS;
}

