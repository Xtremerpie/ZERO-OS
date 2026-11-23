/* ZERO KERNEL - Network Security & Stateful Firewall Implementation (Milestone 7.0.0 ZERO Security).
 */
#include "security/firewall.h"
#include "security/audit.h"
#include "net/net.h"
#include "net/ethernet.h"
#include "net/ipv4.h"
#include "zero/string.h"

static firewall_rule_t   s_rules[FIREWALL_MAX_RULES];
static firewall_action_t s_default_policy = FIREWALL_ACTION_ALLOW;
static uint32_t          s_rule_seq = 1u;
static uint32_t          s_rule_count = 0u;
static uint64_t          s_blocked_count = 0u;

void firewall_init(void)
{
    memset(s_rules, 0, sizeof(s_rules));
    s_default_policy = FIREWALL_ACTION_ALLOW;
    s_rule_seq = 1u;
    s_rule_count = 0u;
    s_blocked_count = 0u;

    /* Add baseline security rules: Allow local loopback and gateway DNS/DHCP */
    firewall_rule_t r1 = {
        .priority = 10,
        .proto = FIREWALL_PROTO_UDP,
        .direction = FIREWALL_DIR_BOTH,
        .dst_port_min = 67, .dst_port_max = 68, /* DHCP */
        .action = FIREWALL_ACTION_ALLOW,
        .description = "Allow DHCP Configuration"
    };
    firewall_add_rule(&r1, NULL);

    firewall_rule_t r2 = {
        .priority = 20,
        .proto = FIREWALL_PROTO_UDP,
        .direction = FIREWALL_DIR_OUT,
        .dst_port_min = 53, .dst_port_max = 53, /* DNS */
        .action = FIREWALL_ACTION_ALLOW,
        .description = "Allow Outbound DNS Queries"
    };
    firewall_add_rule(&r2, NULL);
}

firewall_action_t firewall_get_default_policy(void)
{
    return s_default_policy;
}

void firewall_set_default_policy(firewall_action_t policy)
{
    s_default_policy = policy;
    audit_log(AUDIT_EVENT_CONFIG_CHANGE, 0, "firewall.default_policy",
              policy == FIREWALL_ACTION_ALLOW,
              policy == FIREWALL_ACTION_ALLOW ? "Firewall default policy: ALLOW" : "Firewall default policy: DENY");
}

sec_result_t firewall_add_rule(const firewall_rule_t *rule, uint32_t *out_id)
{
    if (!rule) return SEC_ERR_BAD_ARG;

    int slot = -1;
    for (size_t i = 0; i < FIREWALL_MAX_RULES; i++) {
        if (!s_rules[i].in_use) {
            slot = (int)i;
            break;
        }
    }
    if (slot < 0) return SEC_ERR_NO_MEM;

    firewall_rule_t *dest = &s_rules[slot];
    *dest = *rule;
    dest->id = s_rule_seq++;
    dest->in_use = true;
    s_rule_count++;

    if (out_id) *out_id = dest->id;

    audit_log(AUDIT_EVENT_CONFIG_CHANGE, 0, dest->description, true, "Firewall rule created");
    return SEC_OK;
}

sec_result_t firewall_delete_rule(uint32_t rule_id)
{
    for (size_t i = 0; i < FIREWALL_MAX_RULES; i++) {
        if (s_rules[i].in_use && s_rules[i].id == rule_id) {
            s_rules[i].in_use = false;
            if (s_rule_count > 0) s_rule_count--;
            audit_log(AUDIT_EVENT_CONFIG_CHANGE, 0, "firewall.rule", true, "Firewall rule deleted");
            return SEC_OK;
        }
    }
    return SEC_ERR_BAD_ARG;
}

void firewall_clear_rules(void)
{
    memset(s_rules, 0, sizeof(s_rules));
    s_rule_count = 0;
    audit_log(AUDIT_EVENT_CONFIG_CHANGE, 0, "firewall.rules", true, "All firewall rules flushed");
}

uint32_t firewall_get_rules(firewall_rule_t *out_rules, uint32_t max_rules)
{
    if (!out_rules || max_rules == 0) return 0;

    uint32_t count = 0;
    for (size_t i = 0; i < FIREWALL_MAX_RULES && count < max_rules; i++) {
        if (s_rules[i].in_use) {
            out_rules[count++] = s_rules[i];
        }
    }
    return count;
}

uint32_t firewall_get_rule_count(void)
{
    return s_rule_count;
}

uint64_t firewall_get_blocked_count(void)
{
    return s_blocked_count;
}

bool firewall_filter(net_buf_t *buf, firewall_dir_t direction)
{
    if (!buf || buf->len < sizeof(eth_hdr_t)) return false;

    const eth_hdr_t *eth = (const eth_hdr_t *)buf->data;
    uint16_t ethertype = ntohs(eth->ethertype);

    /* Allow ARP frames unconditionally for host address resolution */
    if (ethertype == ETH_TYPE_ARP) {
        return true;
    }

    /* Only inspect IPv4 traffic; other ethertypes pass default */
    if (ethertype != ETH_TYPE_IPV4) {
        return (s_default_policy == FIREWALL_ACTION_ALLOW);
    }

    if (buf->len < sizeof(eth_hdr_t) + sizeof(ipv4_hdr_t)) {
        s_blocked_count++;
        return false;
    }

    const ipv4_hdr_t *ip = (const ipv4_hdr_t *)(buf->data + sizeof(eth_hdr_t));
    uint32_t src_ip = ip->src_ip;
    uint32_t dst_ip = ip->dst_ip;
    uint8_t  proto  = ip->protocol;

    uint16_t src_port = 0;
    uint16_t dst_port = 0;

    size_t ip_hdr_len = (size_t)((ip->version_ihl & 0x0Fu) * 4u);
    if (ip_hdr_len < IPV4_MIN_HLEN || buf->len < sizeof(eth_hdr_t) + ip_hdr_len) {
        s_blocked_count++;
        return false;
    }
    const uint8_t *transport = buf->data + sizeof(eth_hdr_t) + ip_hdr_len;
    size_t transport_len = buf->len - sizeof(eth_hdr_t) - ip_hdr_len;

    if (proto == IPV4_PROTO_TCP || proto == IPV4_PROTO_UDP) {
        if (transport_len >= 4) {
            src_port = ntohs(*(const uint16_t *)(transport));
            dst_port = ntohs(*(const uint16_t *)(transport + 2));
        }
    }

    /* Iterate through active rules in priority order */
    for (size_t i = 0; i < FIREWALL_MAX_RULES; i++) {
        const firewall_rule_t *r = &s_rules[i];
        if (!r->in_use) continue;

        /* Check direction */
        if (r->direction != FIREWALL_DIR_BOTH && r->direction != direction) {
            continue;
        }

        /* Check protocol */
        if (r->proto != FIREWALL_PROTO_ALL && r->proto != (firewall_proto_t)proto) {
            continue;
        }

        /* Check IP Subnet matching */
        if (r->src_mask != 0 && ((src_ip & r->src_mask) != (r->src_ip & r->src_mask))) {
            continue;
        }
        if (r->dst_mask != 0 && ((dst_ip & r->dst_mask) != (r->dst_ip & r->dst_mask))) {
            continue;
        }

        /* Check Port ranges */
        if (r->src_port_min != 0 && (src_port < r->src_port_min || src_port > r->src_port_max)) {
            continue;
        }
        if (r->dst_port_min != 0 && (dst_port < r->dst_port_min || dst_port > r->dst_port_max)) {
            continue;
        }

        /* Matched rule */
        if (r->action == FIREWALL_ACTION_ALLOW) {
            return true;
        } else {
            s_blocked_count++;
            audit_log(AUDIT_EVENT_FIREWALL_DROP, 0, r->description, false, "Packet blocked by firewall rule");
            return false;
        }
    }

    /* Fallback to default policy */
    if (s_default_policy == FIREWALL_ACTION_ALLOW) {
        return true;
    } else {
        s_blocked_count++;
        audit_log(AUDIT_EVENT_FIREWALL_DROP, 0, "default_deny", false, "Packet blocked by default-deny firewall policy");
        return false;
    }
}
