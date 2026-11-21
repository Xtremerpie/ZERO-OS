/* ZERO KERNEL - System Call Interface (Version 2.0.0 System Architecture).
 *
 * Defines system call numbers, dispatch routines, user pointer validation,
 * and x86_64 MSR fast syscall configuration (STAR, LSTAR, SFMASK).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define SYS_EXIT      1
#define SYS_YIELD     2
#define SYS_SLEEP     3
#define SYS_GETPID    4
#define SYS_WRITE     5
#define SYS_READ      6
#define SYS_OPEN      7
#define SYS_CLOSE     8
#define SYS_IPC_SEND  9
#define SYS_IPC_RECV  10
#define SYS_SOCKET    11
#define SYS_BIND      12
#define SYS_LISTEN    13
#define SYS_ACCEPT    14
#define SYS_CONNECT   15
#define SYS_SEND      16
#define SYS_RECV      17

/* Standard error codes */
#define SYS_SUCCESS    0
#define SYS_ERR_INVAL -22 /* Invalid argument */
#define SYS_ERR_FAULT -14 /* Bad address / invalid user pointer */
#define SYS_ERR_BADF  -9  /* Bad file descriptor */
#define SYS_ERR_NOMEM -12 /* Out of memory */
#define SYS_ERR_NOSYS -38 /* Function not implemented */
#define SYS_ERR_PERM  -1  /* Operation not permitted */

/* Initialize MSRs for fast system calls (STAR, LSTAR, SFMASK, EFER.SCE). */
void syscall_init(void);

/* Validate that a user pointer buffer lies strictly within user virtual memory. */
bool syscall_validate_user_ptr(const void *ptr, size_t size);

/* Central system call dispatcher invoked by the assembly stub. */
int64_t syscall_dispatch(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

/* Enter Ring 3 user mode and execute code at entry_rip with user_rsp. */
__attribute__((noreturn))
void enter_user_mode(uint64_t entry_rip, uint64_t user_rsp);
