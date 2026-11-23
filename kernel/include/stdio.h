#pragma once
/* Minimal freestanding stdio compatibility for kernel code. */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

static inline size_t zero_stdio_putc(char *dst, size_t cap, size_t n, char c)
{
    if (cap && n + 1 < cap) dst[n] = c;
    return n + 1;
}
static inline size_t zero_stdio_puts(char *dst, size_t cap, size_t n, const char *s, size_t limit)
{
    if (!s) s = "(null)";
    size_t i = 0;
    while (s[i] && i < limit) n = zero_stdio_putc(dst, cap, n, s[i++]);
    return n;
}
static inline size_t zero_stdio_number(char *dst, size_t cap, size_t n, uint64_t v, unsigned base, int upper)
{
    char tmp[32]; size_t k = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    do { tmp[k++] = digits[v % base]; v /= base; } while (v && k < sizeof(tmp));
    while (k) n = zero_stdio_putc(dst, cap, n, tmp[--k]);
    return n;
}
static inline int vsnprintf(char *dst, size_t cap, const char *fmt, va_list ap)
{
    size_t n = 0;
    for (size_t i = 0; fmt && fmt[i]; ++i) {
        if (fmt[i] != '%') { n = zero_stdio_putc(dst, cap, n, fmt[i]); continue; }
        ++i;
        if (fmt[i] == '%') { n = zero_stdio_putc(dst, cap, n, '%'); continue; }
        size_t precision = (size_t)-1;
        if (fmt[i] == '.') {
            precision = 0; ++i;
            while (fmt[i] >= '0' && fmt[i] <= '9') precision = precision * 10 + (size_t)(fmt[i++] - '0');
        }
        while (fmt[i] == '-' || fmt[i] == '+' || fmt[i] == '0' || fmt[i] == ' ' || fmt[i] == '#') ++i;
        while (fmt[i] >= '0' && fmt[i] <= '9') ++i;
        int ll = 0;
        if (fmt[i] == 'z' || fmt[i] == 'l') { ll = 1; if (fmt[i] == 'l' && fmt[i+1] == 'l') { ll = 2; ++i; } ++i; }
        char spec = fmt[i];
        if (spec == 's') {
            const char *s = va_arg(ap, const char *);
            n = zero_stdio_puts(dst, cap, n, s, precision);
        } else if (spec == 'c') {
            n = zero_stdio_putc(dst, cap, n, (char)va_arg(ap, int));
        } else if (spec == 'd' || spec == 'i') {
            int64_t v = ll ? va_arg(ap, int64_t) : va_arg(ap, int);
            if (v < 0) { n = zero_stdio_putc(dst, cap, n, '-'); v = -v; }
            n = zero_stdio_number(dst, cap, n, (uint64_t)v, 10, 0);
        } else if (spec == 'u' || spec == 'x' || spec == 'X') {
            uint64_t v = ll ? va_arg(ap, uint64_t) : va_arg(ap, unsigned int);
            n = zero_stdio_number(dst, cap, n, v, (spec == 'u') ? 10 : 16, spec == 'X');
        } else if (spec == 'p') {
            uintptr_t v = (uintptr_t)va_arg(ap, void *);
            n = zero_stdio_puts(dst, cap, n, "0x", 2);
            n = zero_stdio_number(dst, cap, n, v, 16, 0);
        } else {
            n = zero_stdio_putc(dst, cap, n, '%');
            if (spec) n = zero_stdio_putc(dst, cap, n, spec);
        }
    }
    if (cap) dst[(n < cap) ? n : cap - 1] = '\0';
    return (int)n;
}
static inline int snprintf(char *dst, size_t cap, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(dst, cap, fmt, ap);
    va_end(ap); return n;
}
