/* ZERO KERNEL - Capability-Based Security Model (Milestone 7.0.0 ZERO Security).
 *
 * Implements unforgeable, kernel-managed capability tokens conferring fine-grained
 * authority over files, directories, processes, IPC, sockets, and hardware devices.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "security/security.h"

#define CAP_MAX_ENTRIES      256u
#define CAP_MAX_RESOURCE_LEN 64u
#define CAP_INVALID_HANDLE   0u

typedef uint32_t cap_handle_t;

/* Capability Resource Types */
typedef enum {
    CAP_TYPE_NONE = 0,
    CAP_TYPE_FILE,
    CAP_TYPE_DIR,
    CAP_TYPE_PROCESS,
    CAP_TYPE_IPC,
    CAP_TYPE_NET_SOCKET,
    CAP_TYPE_DEVICE,
    CAP_TYPE_SYSTEM
} cap_type_t;

/* Capability Permission Bitmask */
#define CAP_PERM_READ       (1u << 0)
#define CAP_PERM_WRITE      (1u << 1)
#define CAP_PERM_EXEC       (1u << 2)
#define CAP_PERM_DELETE     (1u << 3)
#define CAP_PERM_ADMIN      (1u << 4)

/* Capability Inspection Descriptor (safe user copy) */
typedef struct cap_desc {
    cap_handle_t handle;
    cap_type_t   type;
    char         resource[CAP_MAX_RESOURCE_LEN];
    uint32_t     perms;
    uint32_t     pid;
    bool         valid;
    bool         revoked;
} cap_desc_t;

/* Initialize capability subsystem */
void capability_init(void);

/* Create and grant a new capability to a process */
cap_handle_t cap_create(uint32_t pid, cap_type_t type, const char *resource, uint32_t perms);

/* Validate that process holds valid capability with required permissions */
sec_result_t cap_validate(cap_handle_t handle, uint32_t pid, cap_type_t type, uint32_t req_perms);

/* Quick check if process has authority over a specific resource by type and path/id */
sec_result_t cap_check_resource(uint32_t pid, cap_type_t type, const char *resource, uint32_t req_perms);

/* Duplicate capability with reduced or equal permissions (attenuation) */
cap_handle_t cap_duplicate_reduced(cap_handle_t src_handle, uint32_t target_pid, uint32_t reduced_perms);

/* Revoke capability, immediately terminating all future authorization */
sec_result_t cap_revoke(cap_handle_t handle);

/* Destroy capability and release slot */
sec_result_t cap_destroy(cap_handle_t handle);

/* Enumerate all active capabilities granted to a process */
uint32_t cap_enumerate_process(uint32_t pid, cap_desc_t *out_caps, uint32_t max_caps);

/* Telemetry counters */
uint32_t capability_get_active_count(void);
uint32_t capability_get_revoked_count(void);
