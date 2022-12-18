#ifndef _headers_1667482340_CProjectTemplate_bug
#define _headers_1667482340_CProjectTemplate_bug

#include <stdio.h>
#include <stdlib.h>

#include "panic.h"

// Linux kernel style bug 

#define BUG() do { \
  panic("BUG: failure at %s:%d/%s()!", __FILE__, __LINE__, __func__); \
} while(0)

#define BUG_ON(cond) do { \
  if (cond)  \
    BUG(); \
} while(0)

#endif

