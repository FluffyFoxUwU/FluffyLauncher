#ifndef _headers_1671187809_FluffyLauncher_panic
#define _headers_1671187809_FluffyLauncher_panic

#include <stdarg.h>

#include "util/util.h"

void _panic(const char* fmt, ...);
void _panic_va(const char* fmt, va_list list);

#define panic_fmt(fmt) __FILE__ ":" stringify(__LINE__) ": " fmt
#define panic(fmt, ...) _panic(panic_fmt(fmt) __VA_OPT__(,) __VA_ARGS__) 
#define panic_va(fmt, args) _panic_va(panic_fmt(fmt), args) 

#endif

