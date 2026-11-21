# Network Security & Packet-Filtering Firewall

## 1. Overview
The ZERO OS network stack incorporates a centralized stateful packet-filtering firewall (`kernel/security/firewall.c`) operating directly within the packet ingress (`net_rx`) and egress (`net_tx`) paths of `kernel/net/net.c`.

---

## 2. Rule Architecture
The firewall manages up to 32 active filtering rules (`FIREWALL_MAX_RULES = 32`). Each rule defines:
- `id`: Monotonically assigned 32-bit rule identifier.
- `priority`: Numeric priority (lower numerical value = evaluated first).
- `proto`: Transport protocol (`ALL`, `TCP` [6], `UDP` [17], `ICMP` [1]).
- `direction`: Traffic flow (`FIREWALL_DIR_IN`, `FIREWALL_DIR_OUT`, `FIREWALL_DIR_BOTH`).
- `src_ip` & `src_mask`: Source IPv4 address and subnet mask (0 = any).
- `dst_ip` & `dst_mask`: Destination IPv4 address and subnet mask (0 = any).
- `src_port_min` & `src_port_max`: Source port range (host byte order, 0 = any).
- `dst_port_min` & `dst_port_max`: Destination port range (host byte order, 0 = any).
- `action`: Evaluation verdict (`ALLOW`, `DROP`, `REJECT`).
- `description`: 32-character human-readable rule label.

---

## 3. Evaluation & Default Policy
When a packet traverses the network stack:
1. `firewall_filter(buf, direction)` is invoked before protocol parsing.
2. The rules table is evaluated in ascending order of priority.
3. The first matching rule immediately dictates the verdict:
   - `FIREWALL_ACTION_ALLOW`: The packet continues through the networking stack.
   - `FIREWALL_ACTION_DROP` / `REJECT`: The buffer is dropped (`net_buf_free()`), `s_blocked_count` is incremented, an `AUDIT_EVENT_FIREWALL_DROP` is logged, and the function returns `false`.
4. If no rules match the packet, the global default policy is applied:
   - Default Policy can be toggled between `ALLOW` (open mode) and `DROP` (strict zero-trust default-deny) from the ZERO Security Center or kernel configuration.
