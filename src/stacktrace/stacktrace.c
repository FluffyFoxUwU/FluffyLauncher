#include <string.h>
#include <errno.h>

#include "stacktrace/stacktrace.h"
#include "stacktrace/provider/libbacktrace.h"
#include "config.h"

int stacktrace_walk_through_stack(stacktrace_walker_block walker) {
  int res = 0;
  
  __block struct stacktrace_element current = {};
  stacktrace_walker_block walkerBlock = ^int (struct stacktrace_element* element) {
    if (current.ip == element->ip) {
      current.count++;
      return 0;
    } else if (current.count >= 1) {
      int res = walker(&current);
      if (res < 0)
        return res;
    }
    
    // Only if current frame not the same as previous
    current = *element;
    current.count = 1;
    return 0;
  };
  
# if IS_ENABLED(CONFIG_STACKTRACE_PROVIDER_LIBBACKTRACE)
  res = stacktrace_libbacktrace_walk_through_stack(walkerBlock);
# else
  res = -ENOSYS;
  (void) walkerBlock;
# endif
  return res;
}

void stacktrace_init() {
# if IS_ENABLED(CONFIG_STACKTRACE_PROVIDER_LIBBACKTRACE)
  stacktrace_libbacktrace_init();
# endif
}

void stacktrace_cleanup() {
# if IS_ENABLED(CONFIG_STACKTRACE_PROVIDER_LIBBACKTRACE)
  stacktrace_libbacktrace_cleanup();
# endif
}
