/* ZERO KERNEL - Process Sandboxing & Isolation Engine (Milestone 7.0.0 ZERO Security).
 *
 * Enforces hierarchical sandbox containment profiles (RESTRICTED, BASIC, STANDARD, SYSTEM),
 * filesystem jail roots, syscall access filters, and memory boundary sanitization.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "security/security.h"

#define SANDBOX_MAX_PROCS    64u
#define SANDBOX_MAX_PATH     128u

typedef enum {
    SANDBOX_PROFILE_NONE = 0,
    SANDBOX_PROFILE_RESTRICTED,
    SANDBOX_PROFILE_BASIC,
    SANDBOX_PROFILE_STANDARD,
    SANDBOX_PROFILE_SYSTEM
} sandbox_profile_t;

typedef struct sandbox_policy {
    uint32_t          pid;
    sandbox_profile_t profile;
    char              root_dir[SANDBOX_MAX_PATH];
    bool              net_allowed;
    bool              dev_allowed;
    bool              ipc_allowed;
    uint32_t          max_open_files;
    bool              in_use;
} sandbox_policy_t;

/* Initialize process sandboxing subsystem */
void sandbox_init(void);

/* Assign sandbox profile and restricted root directory to a process */
sec_result_t sandbox_assign_profile(uint32_t pid, sandbox_profile_t profile, const char *root_dir);

/* Query active sandbox profile for a process */
sandbox_profile_t sandbox_get_profile(uint32_t pid);
const sandbox_policy_t *sandbox_get_policy(uint32_t pid);

/* Validate if process is allowed to access specific VFS path */
bool sandbox_check_path(uint32_t pid, const char *path, uint32_t req_perms);

/* Check if process has permission to open network sockets */
bool sandbox_check_network(uint32_t pid);

/* Check if process has permission to interact with character/block devices */
bool sandbox_check_device(uint32_t pid, const char *dev_name);

/* Check if specific system call number is permitted under process profile */
bool sandbox_check_syscall(uint32_t pid, uint32_t syscall_num);

/* Strengthened user-space pointer and range validation */
bool sandbox_validate_user_ptr(const void *ptr, size_t len, bool is_write);
