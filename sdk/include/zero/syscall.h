/* ZERO SDK - System Call Wrapper Interface (Milestone 6.0.0 ZERO Ecosystem).
 *
 * Implements user-mode x86_64 `syscall` wrappers for Ring 3 applications.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#define SYS_EXIT      1
#define SYS_FORK      2
#define SYS_READ      3
#define SYS_WRITE     4
#define SYS_OPEN      5
#define SYS_CLOSE     6
#define SYS_WAITPID   7
#define SYS_EXECVE    8
#define SYS_GETPID    9
#define SYS_YIELD     10
#define SYS_SOCKET    11
#define SYS_BIND      12
#define SYS_LISTEN    13
#define SYS_ACCEPT    14
#define SYS_CONNECT   15
#define SYS_SEND      16
#define SYS_RECV      17

static inline int64_t zero_syscall0(uint64_t num)
{
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline int64_t zero_syscall1(uint64_t num, uint64_t a1)
{
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline int64_t zero_syscall2(uint64_t num, uint64_t a1, uint64_t a2)
{
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline int64_t zero_syscall3(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3)
{
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline void zero_exit(int code)
{
    zero_syscall1(SYS_EXIT, (uint64_t)code);
    while (1) { __asm__ volatile ("pause"); }
}

static inline int64_t zero_write(int fd, const void *buf, size_t count)
{
    return zero_syscall3(SYS_WRITE, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
}

static inline int64_t zero_read(int fd, void *buf, size_t count)
{
    return zero_syscall3(SYS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
}

static inline int zero_open(const char *path, int flags)
{
    return (int)zero_syscall2(SYS_OPEN, (uint64_t)path, (uint64_t)flags);
}

static inline int zero_close(int fd)
{
    return (int)zero_syscall1(SYS_CLOSE, (uint64_t)fd);
}

static inline void zero_yield(void)
{
    zero_syscall0(SYS_YIELD);
}
