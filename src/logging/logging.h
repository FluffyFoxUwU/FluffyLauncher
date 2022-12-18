#ifndef _headers_1668859218_FluffyLauncher_logging
#define _headers_1668859218_FluffyLauncher_logging

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <stdbool.h>

#include "compiler_config.h"
#include "util/util.h"

#define LOG_EMERG "0"
#define LOG_ALERT "1"
#define LOG_CRIT "2"
#define LOG_ERR "3"
#define LOG_WARN "4"
#define LOG_NOTICE "5"
#define LOG_INFO "6"
#define LOG_DEBUG "7"

enum log_level {
  LOG_LEVEL_EMERGENCY,
  LOG_LEVEL_ALERT,
  LOG_LEVEL_CRITICAL,
  LOG_LEVEL_ERROR,
  LOG_LEVEL_WARNING,
  LOG_LEVEL_NOTICE,
  LOG_LEVEL_INFO,
  LOG_LEVEL_DEBUG
};

struct log_entry {
  double cpuTimestampInMs;
  double realtime;
  size_t messageLen;
  
  enum log_level logLevel;
  const char* threadSource;
  const char* message;
};

ATTRIBUTE_PRINTF(1, 2) 
void printk(const char* fmt, ...);
void printk_va(const char* fmt, va_list args);

// Read one log entry there cant be more than one reader
void logging_read_log(struct log_entry* entry);

bool logging_has_more_entry();

// Sleeps the caller until the circular buffer is free
bool logging_flush();

#define pr_fmt(level, fmt) "[" __FILE__ ":" stringify(__LINE__) "/" level "] " fmt

#define pr_emerg(fmt, ...) printk(LOG_EMERG pr_fmt("EMERGENCY", fmt) __VA_OPT__(,) __VA_ARGS__) 
#define pr_alert(fmt, ...) printk(LOG_ALERT pr_fmt("ALERT", fmt) __VA_OPT__(,) __VA_ARGS__) 
#define pr_critical(fmt, ...) printk(LOG_CRIT pr_fmt("CRITICAL", fmt) __VA_OPT__(,) __VA_ARGS__) 
#define pr_error(fmt, ...) printk(LOG_ERR pr_fmt("ERROR", fmt) __VA_OPT__(,) __VA_ARGS__) 
#define pr_warn(fmt, ...) printk(LOG_WARN pr_fmt("WARN", fmt) __VA_OPT__(,) __VA_ARGS__) 
#define pr_notice(fmt, ...) printk(LOG_NOTICE pr_fmt("NOTICE", fmt) __VA_OPT__(,) __VA_ARGS__) 
#define pr_info(fmt, ...) printk(LOG_INFO pr_fmt("INFO", fmt) __VA_OPT__(,) __VA_ARGS__) 
#define pr_debug(fmt, ...) printk(LOG_DEBUG pr_fmt("DEBUG", fmt) __VA_OPT__(,) __VA_ARGS__)

#endif

