#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <openssl/ssl.h>
#include <stdatomic.h>
#include <errno.h>
#include <string.h>
#include <threads.h>

#include "auth/microsoft_auth.h"
#include "auth/minecraft_auth.h"
#include "auth/xbox_live_auth.h"
#include "auth/xsts_auth.h"
#include "buffer.h"
#include "bug.h"
#include "logging/logging.h"
#include "minecraft_api/api.h"
#include "networking/http_headers.h"
#include "networking/http_request.h"
#include "io/io_threads.h"
#include "networking/networking.h"
#include "networking/transport/transport.h"
#include "networking/transport/transport_socket.h"
#include "networking/transport/transport_ssl.h"
#include "config.h"
#include "parser/json/decoder.h"
#include "stacktrace/stacktrace.h"
#include "util/json_schema_loader.h"
#include "parser/json/json.h"
#include "util/util.h"
#include "util/circular_buffer.h"

static atomic_bool shuttingDown = false;

static void* logReader(void* v) {
  struct log_entry entry;
  while (!shuttingDown || logging_has_more_entry()) {
    logging_read_log(&entry);
    fprintf(stderr, "[%f] %s\n", entry.realtime, entry.message);
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
  
  stacktrace_init();
  
  struct microsoft_auth_result* microsoftResult = NULL;
  
  struct microsoft_auth_arg arg = {
    .clientID = CONFIG_AUTH_AZURE_CLIENT_ID,
    .tenant = "consumers",
    .hostname = CONFIG_MICROSOFT_LOGIN_HOSTNAME,
    .scope = "XboxLive.signin%20offline_access",
    .refreshToken = CONFIG_TMP_REFRESH_TOKEN,
    
    .protocol = MICROSOFT_AUTH_HTTPS,
    .port = 443
  };
  
  pr_info("Authenticating with Microsoft...");
  int res = microsoft_auth(&microsoftResult, &arg);
  if (res < 0) {
    pr_error("Fail to authenticate with Microsoft: %d", res);
    goto microsoft_auth_failure;
  }
  pr_info("Sucessfully authenticated with Microsoft!"); 
  
  pr_info("Authenticating with XBoxLive..."); 
  struct xbox_live_auth_result* xboxLiveResult = NULL;
  res = xbox_live_auth(microsoftResult->accessToken, &xboxLiveResult);
  if (res < 0) {
    pr_error("Fail to authenticate with XBoxLive: %d", res);
    goto xbl_auth_failure;
  }
  pr_info("Sucessfully authenticated with XBoxLive!"); 
  
  pr_info("Authenticating with XSTS..."); 
  struct xsts_auth_result* xstsResult = NULL;
  res = xsts_auth(xboxLiveResult->token, &xstsResult);
  if (res < 0) {
    pr_error("Fail to authenticate with XSTS: %d", res);
    goto xsts_auth_failure;
  }
  pr_info("Sucessfully authenticated with XSTS!"); 

  pr_info("Authenticating with Minecraft...");
  struct minecraft_auth_result* minecraftAuthResult = NULL;
  res = minecraft_auth(xstsResult->userhash, xstsResult->token, &minecraftAuthResult);
  if (res < 0) {
    pr_error("Fail to authenticate with Minecraft: %d", res);
    goto minecraft_auth_failed;
  }
  pr_info("Sucessfully authenticated with Minecraft...");

  struct minecraft_api* minecraftAPI = minecraft_api_new(minecraftAuthResult->token);
  if (!minecraftAPI) {
    res = -ENOMEM;
    goto minecraft_api_create_failure;
  }
  
  if ((res = minecraft_api_get_profile(minecraftAPI)) < 0)
    goto fail_to_fetch_profile;
  pr_info("Sucessfully getting Minecraft profile...");
  
  pr_info("Logged in as %s (UUID: %s)", minecraftAPI->callResult.profile.username, minecraftAPI->callResult.profile.uuid);
fail_to_fetch_profile:
  minecraft_api_free(minecraftAPI);
minecraft_api_create_failure:
  minecraft_auth_free(minecraftAuthResult);
minecraft_auth_failed:
  xsts_free(xstsResult);  
xsts_auth_failure:
  xbox_live_free(xboxLiveResult); 
xbl_auth_failure:
  microsoft_auth_free(microsoftResult); 
microsoft_auth_failure:
  
  pr_info("Shutting down...");
  
  stacktrace_cleanup();
  atomic_store(&shuttingDown, true);
  pr_info("Shutting down logger thread. Bye!");
  pthread_join(loggerThread, NULL);
  
  util_cleanup();
  return EXIT_SUCCESS;
}

