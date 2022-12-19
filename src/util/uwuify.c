#include <ctype.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <threads.h>

#include "uwuify.h"

static struct {
  const char* lower;
  const char* high;
} replacementTable[256] = {
  ['s'] = {"z", "Z"},
  ['l'] = {"w", "W"},
  ['r'] = {"w", "W"},
  ['u'] = {"u", "U"}
};

static void doReplace(char chr, char** dest, size_t size) {
  bool isLow = chr == tolower(chr);
  const char* replacement = NULL;
  
  if (isLow && replacementTable[tolower(chr)].lower)
    replacement = replacementTable[tolower(chr)].lower;
  else if (!isLow && replacementTable[tolower(chr)].high)
    replacement = replacementTable[tolower(chr)].high;
  
  if (!replacement) {
    **dest = chr;
    (*dest)++;
    return;
  }
  
  // Write replacement string
  size_t len = strlen(replacement);
  size_t copyLen = len <= size ? len : size;
  memcpy(*dest, replacement, copyLen);
  *dest += copyLen;
}

void uwuify_do(const char* string, char* buffer, size_t bufferSize) {
  const char* source = string;
  char* dest = buffer;
  char* endOfBuffer = buffer + bufferSize - 1;
  
  for (; *source && dest < endOfBuffer; source++)
    doReplace(*source, &dest, endOfBuffer - dest);
  
  // Insert NULL byte at the end
  *dest = '\0';
  *endOfBuffer = '\0';
}

/*
Specifier 	Used For
%c 	        a single character
%s 	        a string
%n 	        prints nothing
%d 	        a decimal integer (assumes base 10)
%i 	        a decimal integer (detects the base automatically)
%o       	  an octal (base 8) integer
%x 	        a hexadecimal (base 16) integer
%p 	        an address (or pointer)
%f 	        a floating point number for floats
%u 	        int unsigned decimal
%e 	        a floating point number in scientific notation
%E 	        a floating point number in scientific notation
%% 	        the % symbol

%hi        	short (signed)
%hu 	      short (unsigned)
%Lf 	      long double
*/
void uwuify_do_printf_compatible(const char* string, char* buffer, size_t bufferSize) {
  const char* source = string;
  char* dest = buffer;
  char* endOfBuffer = buffer + bufferSize - 1;
  
  for (; *source && dest < endOfBuffer; source++) {
    if (*source != '%') {
      doReplace(*source, &dest, endOfBuffer - dest);
      continue;
    }
    
    static bool canStop[256] = {
      ['c'] = true, ['s'] = true, ['n'] = true, ['d'] = true,
      ['i'] = true, ['o'] = true, ['x'] = true, ['p'] = true,
      ['f'] = true, ['u'] = true, ['e'] = true, ['E'] = true,
      ['%'] = true
    };
    
    // Eat everything about printf format
    do {
      *dest = *source;
      source++;
      dest++;
    } while (dest < endOfBuffer && *source && canStop[(int) *source]);
    
    // Cancel the last increment in previous loop
    source--;
  }
  
  // Insert NULL byte at the end
  *dest = '\0';
  *endOfBuffer = '\0';
}

#define UWUIFY_BUFFER_SIZE (8 * 1024)

char* uwuify_do_printf_compatible_easy(const char* string) {
  static thread_local char formatUwuifyBuffer[UWUIFY_BUFFER_SIZE];
  uwuify_do_printf_compatible(string, formatUwuifyBuffer, sizeof(formatUwuifyBuffer));
  return formatUwuifyBuffer;
}

char* uwuify_do_easy(const char* string) {
  static thread_local char normalUwuifyBuffer[UWUIFY_BUFFER_SIZE];
  uwuify_do(string, normalUwuifyBuffer, sizeof(normalUwuifyBuffer));
  return normalUwuifyBuffer;
}

char* uwuify_do_printf_compatible_easy2(const char* string) {
  static thread_local char formatUwuifyBuffer[UWUIFY_BUFFER_SIZE];
  uwuify_do_printf_compatible(string, formatUwuifyBuffer, sizeof(formatUwuifyBuffer));
  return formatUwuifyBuffer;
}

char* uwuify_do_easy2(const char* string) {
  static thread_local char normalUwuifyBuffer[UWUIFY_BUFFER_SIZE];
  uwuify_do(string, normalUwuifyBuffer, sizeof(normalUwuifyBuffer));
  return normalUwuifyBuffer;
}
