#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <openssl/ssl.h>
#include <stdatomic.h>

#include "auth/microsoft_auth.h"
#include "auth/xbox_live_auth.h"
#include "auth/xsts_auth.h"
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
#include "parser/sjson.h"
#include "util/util.h"
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
  
  struct microsoft_auth_result* microsoftResult = NULL;
  struct xbox_live_auth_result* xboxLiveResult = NULL;
  struct xsts_auth_result* xstsResult = NULL;
  
  struct microsoft_auth_arg arg = {
    .clientID = CONFIG_AUTH_AZURE_CLIENT_ID,
    .tenant = "consumers",
    .hostname = "login.microsoftonline.com",
    .scope = "XboxLive.signin%20offline_access",
    .refreshToken = CONFIG_TMP_REFRESH_TOKEN,
    
    .protocol = MICROSOFT_AUTH_HTTPS,
    .port = 443
  };
  
  pr_info("Authenticating with Microsoft...");
  int res = microsoft_auth(&microsoftResult, &arg);
  if (res < 0)
    goto microsoft_auth_failure;
  pr_info("Sucessfully authenticated with Microsoft!"); 
  
  pr_info("Authenticating with XBoxLive..."); 
  res = xbox_live_auth(microsoftResult->accessToken, &xboxLiveResult);
  if (res < 0)
    goto xbl_auth_failure;
  pr_info("Sucessfully authenticated with XBoxLive!"); 
  
  pr_info("Authenticating with XSTS..."); 
  res = xsts_auth(xboxLiveResult->token, &xstsResult);
  if (res < 0)
    goto xsts_auth_failure;
  pr_info("Sucessfully authenticated with XSTS!"); 

  xsts_free(xstsResult);  
xsts_auth_failure:
  xbox_live_free(xboxLiveResult); 
xbl_auth_failure:
  microsoft_auth_free(microsoftResult); 
microsoft_auth_failure:
  
  pr_info("Shutting down...");
  
  atomic_store(&shuttingDown, true);
  pr_info("Shutting down logger thread. Bye!");
  pthread_join(loggerThread, NULL);
  
  util_cleanup();
  return EXIT_SUCCESS;
}

