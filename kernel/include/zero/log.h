/* ZERO KERNEL - kernel logging. Every message goes to serial and, once up, the framebuffer. */
#pragma once

#include <stdarg.h>

enum log_level {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
};

/* Supported formats: %c %s %d %i %u %x %X %p %% with optional '-' '0' flags, width,
 * and length modifiers l, ll, z. Unsupported specifiers are printed literally. */
void kvprintf(const char *fmt, va_list ap);
void kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Prints "[LEVEL] subsystem: message\n". */
void klog(enum log_level level, const char *subsystem, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

#define KLOG_DEBUG(sub, ...) klog(LOG_DEBUG, sub, __VA_ARGS__)
#define KLOG_INFO(sub, ...)  klog(LOG_INFO, sub, __VA_ARGS__)
#define KLOG_WARN(sub, ...)  klog(LOG_WARN, sub, __VA_ARGS__)
#define KLOG_ERROR(sub, ...) klog(LOG_ERROR, sub, __VA_ARGS__)
