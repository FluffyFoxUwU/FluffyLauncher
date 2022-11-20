#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <openssl/ssl.h>
#include <stdatomic.h>

#include "auth/microsoft_auth.h"
#include "bug.h"
#include "logging/logging.h"
#include "networking/http_headers.h"
#include "networking/http_request.h"
#include "io/io_threads.h"
#include "networking/networking.h"
#include "networking/transport/transport.h"
#include "networking/transport/transport_socket.h"
#include "networking/transport/transport_ssl.h"
#include "config.h"
#include "util.h"
#include "util/circular_buffer.h"

static atomic_bool shuttingDown = false;

static void* logReader(void* v) {
  struct log_entry entry;
  while (!shuttingDown || logging_has_more_entry()) {
    logging_read_log(&entry);
    printf("[%f] %s\n", entry.timestampInMs, entry.message);
    logging_release_critical();
  }
  return NULL;
}

int main2(int argc, char** argv) {
  util_init();
  
  SSL_load_error_strings();
  SSL_library_init();
  OpenSSL_add_all_algorithms();
  OpenSSL_add_all_ciphers();
  OpenSSL_add_all_digests();
  
  util_set_thread_name(pthread_self(), "Main-Thread");
  pr_info("Fluffy Launcher starting...");
  
  pthread_t loggerThread;
  pthread_create(&loggerThread, NULL, logReader, NULL);
  
  /*
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
  */
  
  pr_info("Shutting down...");
  pr_info("Shutting down logger thread. Bye!");
  atomic_store(&shuttingDown, true);
  pthread_join(loggerThread, NULL);
  
  util_cleanup();
  return EXIT_SUCCESS;
}

