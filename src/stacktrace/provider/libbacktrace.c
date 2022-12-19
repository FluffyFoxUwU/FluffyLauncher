#include "config.h"

#if IS_ENABLED(CONFIG_STACKTRACE_PROVIDER_LIBBACKTRACE)
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

static bool isSupported = false;
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

void stacktrace_libbacktrace_init() {
  struct libbacktrace_args args = {};
  backtraceState = backtrace_create_state(NULL, 1, errorHandler, &args);
  
  if (args.error.occured) {
    pr_warn("Cant initialize libbacktrace: %s (Error: %d)", args.error.msg, args.error.errnum);
    pr_warn("Continuing without stacktrace support");
    return;
  }
  
  isSupported = true;
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
    .prettySymbolName = function,
    .sourceInfoPresent = filename != NULL && lineno > 0 && function != NULL
  };
  
  return args->walker(&element);
}

int stacktrace_libbacktrace_walk_through_stack(stacktrace_walker_block walker) {
  if (!isSupported) 
    return -ENOSYS;
  
  struct libbacktrace_args args = {
    .walker = walker
  };
  
  backtrace_full(backtraceState, 0, walkFunction, errorHandler, &args);
  return 0;
}

#endif
