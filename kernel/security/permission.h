/* ZERO KERNEL - Filesystem Discretionary Access Control (Milestone 7.0.0 ZERO Security).
 *
 * Implements POSIX-compatible UID/GID file ownership, permission mode bits (rwx),
 * inheritance rules, and VFS authorization enforcement.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "security/security.h"

#define PERM_MAX_ENTRIES    128u
#define PERM_MAX_PATH       128u

/* Standard Permission Mode Bits */
#define PERM_MODE_IXOTH     0x0001u
#define PERM_MODE_IWOTH     0x0002u
#define PERM_MODE_IROTH     0x0004u

#define PERM_MODE_IXGRP     0x0008u
#define PERM_MODE_IWGRP     0x0010u
#define PERM_MODE_IRGRP     0x0020u

#define PERM_MODE_IXUSR     0x0040u
#define PERM_MODE_IWUSR     0x0080u
#define PERM_MODE_IRUSR     0x0100u

#define PERM_DEFAULT_FILE   (PERM_MODE_IRUSR | PERM_MODE_IWUSR | PERM_MODE_IRGRP | PERM_MODE_IROTH) /* 0644 */
#define PERM_DEFAULT_DIR    (PERM_MODE_IRUSR | PERM_MODE_IWUSR | PERM_MODE_IXUSR | PERM_MODE_IRGRP | PERM_MODE_IXGRP | PERM_MODE_IROTH | PERM_MODE_IXOTH) /* 0755 */

typedef struct file_perm {
    char     path[PERM_MAX_PATH];
    uint32_t uid;
    uint32_t gid;
    uint32_t mode;
    bool     in_use;
} file_perm_t;

/* Initialize filesystem permissions subsystem */
void permission_init(void);

/* Check if caller with (uid, gid) is permitted requested access mode on path */
sec_result_t perm_check(const char *path, uint32_t uid, uint32_t gid, uint32_t req_mode);

/* Assign ownership and mode to path */
sec_result_t perm_set(const char *path, uint32_t caller_uid, uint32_t new_uid, uint32_t new_gid, uint32_t new_mode);

/* Query ownership and mode of path */
sec_result_t perm_get(const char *path, uint32_t *out_uid, uint32_t *out_gid, uint32_t *out_mode);
