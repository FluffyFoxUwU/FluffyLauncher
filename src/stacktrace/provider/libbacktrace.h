#ifndef _headers_1671355520_FluffyLauncher_libbacktrace
#define _headers_1671355520_FluffyLauncher_libbacktrace

#include "stacktrace/stacktrace.h"

int stacktrace_libbacktrace_init();
void stacktrace_libbacktrace_cleanup();
void stacktrace_libbacktrace_walk_through_stack(stacktrace_walker_block walker);

#endif

