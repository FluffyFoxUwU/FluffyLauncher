#ifndef _headers_1671451851_FluffyLauncher_uwuify
#define _headers_1671451851_FluffyLauncher_uwuify

#include <stddef.h>

void uwuify_do(const char* string, char* buffer, size_t bufferSize);

// Same as above but aware of printf format specifier
// and preserve them
void uwuify_do_printf_compatible(const char* string, char* buffer, size_t bufferSize);

// Return to static thread local buffer (these two arent sharing same buffer)
char* uwuify_do_printf_compatible_easy(const char* string);
char* uwuify_do_easy(const char* string);

// Return to static thread local buffer (these two arent sharing same buffer as previous)
char* uwuify_do_printf_compatible_easy2(const char* string);
char* uwuify_do_easy2(const char* string);

#endif

