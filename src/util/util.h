#ifndef header_1664455065_ca91d6b5_5c0f_4230_9aad_b5dd1301e644_util_h
#define header_1664455065_ca91d6b5_5c0f_4230_9aad_b5dd1301e644_util_h

#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "buffer.h"
#include "compiler_config.h"

int util_init();
void util_cleanup();

size_t util_vasprintf(char** buffer, const char* fmt, va_list args);

ATTRIBUTE_PRINTF(2, 3)
size_t util_asprintf(char** buffer, const char* fmt, ...);

typedef void (^runnable_block)();

int util_parse_hex_size_t(const char* string, size_t* result);

// Errors:
// -ENOMEM: On error
int util_set_thread_name(pthread_t thread, const char* name);
const char* util_get_thread_name(pthread_t thread);

int util_thread_create(pthread_t* newthread, pthread_attr_t* attr, void* (*routine)(void *), void* arg);

int util_strcasecmp(const char* a, const char* b);

size_t util_hash_buffer(const buffer_t* buff);
int util_compare_buffer(const buffer_t* a, const buffer_t* b);
buffer_t* util_clone_buffer(const buffer_t* buff);

void util_usleep(uint32_t microsecs);
void util_msleep(uint32_t milisecs);
void util_nanosleep(uint64_t nanosecs);

double util_get_realtime();

#define util_microsec_to_timespec_ptr(t) (struct timespec*) {&(struct timespec) { \
  .tv_sec = (t) / 1000000, \
  .tv_nsec = (t * 1000) % 1000000000 \
}}

#define util_milisec_to_timespec_ptr(t) (struct timespec*) {&(struct timespec) { \
  .tv_sec = (t) / 1000, \
  .tv_nsec = ((t) * 1000000) % 1000000000 \
}}

#define is_same_type(a, b) _Generic((a), b: true, default: false)

// Tiny useful macros from Linux kernel
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define FIELD_SIZEOF(t, f) (sizeof(((t*) NULL)->f))

#define __stringify_1(x...)	#x
#define stringify(x...)	__stringify_1(x)

#ifdef __GNUC__
# define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})
#else
# define container_of(ptr, type, member) ((type *)( (char *)__mptr - offsetof(type,member) )) 
#endif

#endif



