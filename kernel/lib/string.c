/* ZERO KERNEL - freestanding string/memory routines.
 * The compiler may emit calls to memcpy/memmove/memset/memcmp, so they must exist.
 * Copy/fill use `rep` string instructions so the compiler cannot turn these
 * loops back into calls to themselves. */

#include "zero/string.h"

void *memcpy(void *dst, const void *src, size_t n)
{
    void *ret = dst;
    __asm__ volatile("rep movsb" : "+D"(dst), "+S"(src), "+c"(n) : : "memory");
    return ret;
}

void *memmove(void *dst, const void *src, size_t n)
{
    void *ret = dst;
    if ((const char *)dst <= (const char *)src || (const char *)dst >= (const char *)src + n) {
        __asm__ volatile("rep movsb" : "+D"(dst), "+S"(src), "+c"(n) : : "memory");
    } else {
        /* Overlapping with dst after src: copy backwards. */
        dst = (char *)dst + n - 1;
        src = (const char *)src + n - 1;
        __asm__ volatile("std; rep movsb; cld" : "+D"(dst), "+S"(src), "+c"(n) : : "memory");
    }
    return ret;
}

void *memset(void *dst, int c, size_t n)
{
    void *ret = dst;
    __asm__ volatile("rep stosb" : "+D"(dst), "+c"(n) : "a"(c) : "memory");
    return ret;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *pa = a, *pb = b;
    for (size_t i = 0; i < n; i++) {
        if (pa[i] != pb[i])
            return pa[i] < pb[i] ? -1 : 1;
    }
    return 0;
}

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i])
            return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == '\0')
            return 0;
    }
    return 0;
}

char *strcpy(char *dst, const char *src)
{
    char *ret = dst;

    while ((*dst++ = *src++) != '\0') {
    }

    return ret;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    char *ret = dst;
    size_t i = 0;

    while (i < n && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }

    while (i < n) {
        dst[i++] = '\0';
    }

    return ret;
}

char *strcat(char *dst, const char *src)
{
    char *ret = dst;

    while (*dst != '\0') {
        dst++;
    }

    while ((*dst++ = *src++) != '\0') {
    }

    return ret;
}

char *strncat(char *dst, const char *src, size_t n)
{
    char *ret = dst;

    while (*dst != '\0') {
        dst++;
    }

    size_t i = 0;

    while (i < n && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';

    return ret;
}

char *strstr(const char *haystack, const char *needle)
{
    if (*needle == '\0') {
        return (char *)haystack;
    }

    while (*haystack != '\0') {
        const char *h = haystack;
        const char *n = needle;

        while (*h != '\0' && *n != '\0' && *h == *n) {
            h++;
            n++;
        }

        if (*n == '\0') {
            return (char *)haystack;
        }

        haystack++;
    }

    return NULL;
}

char *strchr(const char *s, int c)
{
    while (*s) { if (*s == (char)c) return (char *)s; ++s; }
    return (c == 0) ? (char *)s : NULL;
}
