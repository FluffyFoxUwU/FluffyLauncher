#include "config.h"

#if IS_ENABLED(CONFIG_STACKTRACE_PROVIDER_LIBBACKTRACE)
#if IS_ENABLED(CONFIG_STACKTRACE_USE_DLADDR)
# define _GNU_SOURCE
# include <dlfcn.h>
#else

// Fake dladdr that always fails
typedef struct {
  const char* dli_fname; 
  void* dli_fbase; 
  const char* dli_sname; 
  void* dli_saddr; 
} Dl_info;

static int dladdr(const void* addr, Dl_info* info) {
  if (info)
    *info = (Dl_info) {};
  return 0;
}
#endif

#include <backtrace-supported.h>
#include <backtrace.h>
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <errno.h>

#include "libbacktrace.h"
#include "stacktrace/stacktrace.h"
#include "logging/logging.h"

static_assert(BACKTRACE_SUPPORTED, "libbacktrace not working!");
static_assert(BACKTRACE_SUPPORTS_THREADS, "libbacktrace not supporting threads!");

static struct backtrace_state* backtraceState = NULL;

struct libbacktrace_error {
  const char *msg;
  int errnum;
  bool occured;
};

struct libbacktrace_args {
  stacktrace_walker_block walker;
  struct libbacktrace_error error;
};

static void errorHandler(void* data, const char* msg, int errnum) {
  struct libbacktrace_args* err = data;
  err->error.errnum = errnum;
  err->error.msg = msg;
  err->error.occured = true;
}

int stacktrace_libbacktrace_init() {
  struct libbacktrace_args args = {};
  backtraceState = backtrace_create_state(NULL, 1, errorHandler, &args);
  
  if (args.error.occured) {
    pr_warn("Cant initialize libbacktrace: %s (Error: %d)", args.error.msg, args.error.errnum);
    pr_warn("Continuing without stacktrace support");
    return -ENOSYS;
  }
  
  return 0;
}

void stacktrace_libbacktrace_cleanup() {
}

static int walkFunction(void* data, uintptr_t pc, const char* filename, int lineno, const char* function) {
  struct libbacktrace_args* args = data;
  struct stacktrace_element element = {
    .ip = pc,
    .sourceFile = filename,
    .sourceColumn = -1,
    .sourceLine = lineno,
    .symbolName = function,
    .sourceInfoPresent = filename != NULL && lineno > 0 && function != NULL
  };
  
  if (!element.symbolName) {
    // Get it via dladdr
    Dl_info addrInfo = {};
    if (dladdr((void*) pc, &addrInfo) == 0)
      goto dladdr_not_working;
    
    if (addrInfo.dli_sname) {
      element.symbolName = addrInfo.dli_sname;
      goto got_function_name;
    }
dladdr_not_working:
    ;
  }
  
[[maybe_unused]]
got_function_name:
  element.printableName = element.symbolName;
  return args->walker(&element);
}

void stacktrace_libbacktrace_walk_through_stack(stacktrace_walker_block walker) {
  struct libbacktrace_args args = {
    .walker = walker
  };
  
  backtrace_full(backtraceState, 0, walkFunction, errorHandler, &args);
}

#endif
