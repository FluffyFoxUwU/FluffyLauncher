#ifndef _headers_1671353850_FluffyLauncher_stacktrace
#define _headers_1671353850_FluffyLauncher_stacktrace

#include <stdint.h>
#include <stdbool.h>

struct stacktrace_element {
  // Notes:
  // Stacktrace shall provide same values for these (but not necessary
  // the same pointer for strings) for corresponding `ip`
  // for whole period of walking the stack
  // Stacktrace provider only need to `ip`, the rest is optional (set 
  // out of range value to indicate not present)
  bool sourceInfoPresent;
  const char* sourceFile;
  const char* symbolName; // Suitable for use in dlsym
  int sourceLine;
  int sourceColumn;
  uintptr_t ip;
  
  // Optional to stacktrace provider
  // for printing to user using this is recommended
  const char* printableName;
  
  // Number of time this exact frame occured (like
  // in recursive functions)
  // Never be 0
  int count;
};

// Return nonzero value to stop walking
typedef int (^stacktrace_walker_block)(struct stacktrace_element* element);

// 0 on success
// -ENOSYS: If unsupported
int stacktrace_walk_through_stack(stacktrace_walker_block walker);

int stacktrace_init();
void stacktrace_cleanup();

#endif

