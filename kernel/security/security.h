/* ZERO KERNEL - Central Security Architecture (Milestone 7.0.0 ZERO Security).
 *
 * Coordinates authorization decisions, capability checks, process sandbox enforcement,
 * cryptographic verification, firewall rules, and structured security auditing.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* Standard Security Result Codes */
typedef enum {
    SEC_OK              = 0,
    SEC_ERR_DENIED      = -1,
    SEC_ERR_REVOKED     = -2,
    SEC_ERR_INVALID_CAP = -3,
    SEC_ERR_BOUNDS      = -4,
    SEC_ERR_INTEGRITY   = -5,
    SEC_ERR_NO_MEM      = -6,
    SEC_ERR_BAD_ARG     = -7,
    SEC_ERR_UNTRUSTED   = -8
} sec_result_t;

/* Security Action Enumeration */
typedef enum {
    SEC_ACTION_READ = 1,
    SEC_ACTION_WRITE,
    SEC_ACTION_EXECUTE,
    SEC_ACTION_CREATE,
    SEC_ACTION_DELETE,
    SEC_ACTION_NET_CONNECT,
    SEC_ACTION_NET_LISTEN,
    SEC_ACTION_IPC_SEND,
    SEC_ACTION_DEVICE_IO
} sec_action_t;

/* Security Subsystem Telemetry */
typedef struct sec_stats {
    uint64_t checks_total;
    uint64_t checks_granted;
    uint64_t checks_denied;
    uint32_t active_capabilities;
    uint32_t revoked_capabilities;
    uint32_t active_firewall_rules;
    uint64_t firewall_packets_blocked;
    uint64_t audit_events_logged;
} sec_stats_t;

/* Initialize central security subsystem */
void security_init(void);

/* Check if security enforcement is actively enabled */
bool security_is_enforced(void);
void security_set_enforced(bool enabled);

/* Retrieve security subsystem statistics */
void security_get_stats(sec_stats_t *out_stats);

/* Record check decisions */
void security_record_decision(sec_action_t action, bool granted, const char *reason);
