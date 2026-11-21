/* ZERO KERNEL - Security Audit Logging Subsystem (Milestone 7.0.0 ZERO Security).
 *
 * Implements persistent and ring-buffered security event auditing, tracking
 * authorization failures, capability grants/revocations, permission checks,
 * sandbox violations, binary verifications, and firewall drops.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define AUDIT_RING_BUFFER_SIZE  64u
#define AUDIT_LOG_FILE_PATH     "/data/log/audit.log"

typedef enum {
    AUDIT_EVENT_AUTH_FAILURE = 0,
    AUDIT_EVENT_CAP_GRANT,
    AUDIT_EVENT_CAP_REVOKE,
    AUDIT_EVENT_PERM_DENY,
    AUDIT_EVENT_SANDBOX_VIOLATION,
    AUDIT_EVENT_SYSCALL_VIOLATION,
    AUDIT_EVENT_PACKAGE_VERIFY,
    AUDIT_EVENT_PACKAGE_INSTALL,
    AUDIT_EVENT_FIREWALL_DROP,
    AUDIT_EVENT_CONFIG_CHANGE,
    AUDIT_EVENT_TYPE_COUNT
} audit_event_type_t;

typedef struct audit_event {
    uint64_t           id;
    uint64_t           timestamp_ms;
    audit_event_type_t type;
    uint32_t           pid;
    char               resource[48];
    bool               success;
    char               details[64];
} audit_event_t;

/* Initialize audit subsystem and allocate memory/buffers */
void audit_init(void);

/* Log a structured security audit event */
void audit_log(audit_event_type_t type, uint32_t pid, const char *resource, bool success, const char *details);

/* Read audit events from ring buffer */
uint32_t audit_get_events(audit_event_t *out_events, uint32_t max_events);

/* Total cumulative events logged */
uint64_t audit_get_event_count(void);

/* Clear ring buffer */
void audit_clear(void);

/* String representation of audit event type */
const char *audit_event_type_name(audit_event_type_t type);
