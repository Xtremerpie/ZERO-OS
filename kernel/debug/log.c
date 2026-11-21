/* ZERO KERNEL - kernel logging and a small printf implementation. */

#include <stdbool.h>
#include <stdint.h>
#include "zero/log.h"
#include "zero/serial.h"
#include "zero/fbcon.h"

static void out_char(char c)
{
    serial_putc(c);
    fbcon_putc(c);
}

static void out_pad(int n, char pad)
{
    while (n-- > 0)
        out_char(pad);
}

static void out_string(const char *s, int width, bool left)
{
    int len = 0;
    while (s[len])
        len++;
    if (!left)
        out_pad(width - len, ' ');
    for (int i = 0; i < len; i++)
        out_char(s[i]);
    if (left)
        out_pad(width - len, ' ');
}

static void out_number(uint64_t v, unsigned base, bool upper, bool neg,
                       int width, bool left, bool zero)
{
    char buf[24];
    int n = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    if (v == 0)
        buf[n++] = '0';
    while (v) {
        buf[n++] = digits[v % base];
        v /= base;
    }

    int total = n + (neg ? 1 : 0);
    if (!left && !zero)
        out_pad(width - total, ' ');
    if (neg)
        out_char('-');
    if (!left && zero)
        out_pad(width - total, '0');
    while (n)
        out_char(buf[--n]);
    if (left)
        out_pad(width - total, ' ');
}

void kvprintf(const char *fmt, va_list ap)
{
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            out_char(*fmt);
            continue;
        }

        const char *spec_start = fmt;
        fmt++;

        bool left = false, zero = false;
        for (;; fmt++) {
            if (*fmt == '-')
                left = true;
            else if (*fmt == '0')
                zero = true;
            else
                break;
        }

        int width = 0;
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');

        int lng = 0; /* 0 = int, 1 = long / size_t, 2 = long long */
        if (*fmt == 'z') {
            lng = 1;
            fmt++;
        } else {
            while (*fmt == 'l') {
                lng++;
                fmt++;
            }
        }

        switch (*fmt) {
        case 'c':
            out_char((char)va_arg(ap, int));
            break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            out_string(s ? s : "(null)", width, left);
            break;
        }
        case 'd':
        case 'i': {
            int64_t v = lng ? va_arg(ap, int64_t) : (int64_t)va_arg(ap, int);
            bool neg = v < 0;
            out_number(neg ? (uint64_t)0 - (uint64_t)v : (uint64_t)v, 10, false, neg,
                       width, left, zero);
            break;
        }
        case 'u': {
            uint64_t v = lng ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, unsigned);
            out_number(v, 10, false, false, width, left, zero);
            break;
        }
        case 'x':
        case 'X': {
            uint64_t v = lng ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, unsigned);
            out_number(v, 16, *fmt == 'X', false, width, left, zero);
            break;
        }
        case 'p': {
            uint64_t v = (uint64_t)(uintptr_t)va_arg(ap, void *);
            out_string("0x", 0, false);
            out_number(v, 16, false, false, 16, false, true);
            break;
        }
        case '%':
            out_char('%');
            break;
        default:
            /* Unsupported specifier (or truncated format): print it literally. */
            for (const char *p = spec_start; p <= fmt && *p; p++)
                out_char(*p);
            if (*fmt == '\0')
                return;
            break;
        }
    }
}

void kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}

void klog(enum log_level level, const char *subsystem, const char *fmt, ...)
{
    static const char *const names[] = { "DEBUG", "INFO ", "WARN ", "ERROR" };
    /* Level colours on the framebuffer: grey, white, gold, crimson. */
    static const uint32_t colours[] = { 0x8A8A8A, 0xE8E8E8, 0xC9A227, 0xD0283A };

    fbcon_set_fg(colours[level]);
    kprintf("[%s] %s: ", names[level], subsystem);

    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);

    kprintf("\n");
    fbcon_set_fg(0xE8E8E8);
}
