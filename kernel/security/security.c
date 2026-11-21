/* ZERO KERNEL - Central Security Architecture Implementation (Milestone 7.0.0 ZERO Security).
 */
#include "security/security.h"
#include "security/capability.h"
#include "security/sandbox.h"
#include "security/permission.h"
#include "security/firewall.h"
#include "security/audit.h"
#include "security/crypto/crypto.h"
#include "zero/string.h"

static bool        s_enforced = true;
static sec_stats_t s_stats;

void security_init(void)
{
    memset(&s_stats, 0, sizeof(s_stats));
    s_enforced = true;

    /* Initialize security sub-modules */
    crypto_init();
    audit_init();
    capability_init();
    sandbox_init();
    permission_init();
    firewall_init();

    audit_log(AUDIT_EVENT_CONFIG_CHANGE, 0, "security", true, "Central Security Architecture initialized");
}

bool security_is_enforced(void)
{
    return s_enforced;
}

void security_set_enforced(bool enabled)
{
    s_enforced = enabled;
    audit_log(AUDIT_EVENT_CONFIG_CHANGE, 0, "security.enforced", enabled,
              enabled ? "Security enforcement enabled" : "Security enforcement relaxed");
}

void security_get_stats(sec_stats_t *out_stats)
{
    if (!out_stats) return;
    *out_stats = s_stats;
    out_stats->active_capabilities = capability_get_active_count();
    out_stats->revoked_capabilities = capability_get_revoked_count();
    out_stats->active_firewall_rules = firewall_get_rule_count();
    out_stats->firewall_packets_blocked = firewall_get_blocked_count();
    out_stats->audit_events_logged = audit_get_event_count();
}

void security_record_decision(sec_action_t action, bool granted, const char *reason)
{
    (void)action;
    (void)reason;
    s_stats.checks_total++;
    if (granted) {
        s_stats.checks_granted++;
    } else {
        s_stats.checks_denied++;
    }
}
