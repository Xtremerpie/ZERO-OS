/* ZERO KERNEL - Network Security & Stateful Firewall (Milestone 7.0.0 ZERO Security).
 *
 * Implements configurable packet filtering rules (IP, protocol, port ranges, direction),
 * default-deny policies, rule evaluation prioritization, and security drop logging.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "net/net_buf.h"
#include "security/security.h"

#define FIREWALL_MAX_RULES      32u

typedef enum {
    FIREWALL_PROTO_ALL = 0,
    FIREWALL_PROTO_TCP = 6,
    FIREWALL_PROTO_UDP = 17,
    FIREWALL_PROTO_ICMP = 1
} firewall_proto_t;

typedef enum {
    FIREWALL_DIR_IN = 1,
    FIREWALL_DIR_OUT,
    FIREWALL_DIR_BOTH
} firewall_dir_t;

typedef enum {
    FIREWALL_ACTION_ALLOW = 0,
    FIREWALL_ACTION_DROP  = 1,
    FIREWALL_ACTION_REJECT = 2
} firewall_action_t;

typedef struct firewall_rule {
    uint32_t         id;
    uint32_t         priority;      /* Lower number = higher evaluation priority */
    firewall_proto_t proto;
    firewall_dir_t   direction;
    uint32_t         src_ip;        /* Network byte order, 0 = any */
    uint32_t         src_mask;
    uint32_t         dst_ip;        /* Network byte order, 0 = any */
    uint32_t         dst_mask;
    uint16_t         src_port_min;  /* Host byte order, 0 = any */
    uint16_t         src_port_max;
    uint16_t         dst_port_min;  /* Host byte order, 0 = any */
    uint16_t         dst_port_max;
    firewall_action_t action;
    char             description[32];
    bool             in_use;
} firewall_rule_t;

/* Initialize network firewall subsystem */
void firewall_init(void);

/* Default firewall policy (ALLOW or DROP) */
firewall_action_t firewall_get_default_policy(void);
void firewall_set_default_policy(firewall_action_t policy);

/* Add or update a filtering rule */
sec_result_t firewall_add_rule(const firewall_rule_t *rule, uint32_t *out_id);

/* Remove a rule by ID */
sec_result_t firewall_delete_rule(uint32_t rule_id);

/* Clear all rules */
void firewall_clear_rules(void);

/* Enumerate all active firewall rules */
uint32_t firewall_get_rules(firewall_rule_t *out_rules, uint32_t max_rules);
uint32_t firewall_get_rule_count(void);
uint64_t firewall_get_blocked_count(void);

/* Filter incoming or outgoing network buffer. Returns true if allowed, false if dropped. */
bool firewall_filter(net_buf_t *buf, firewall_dir_t direction);
