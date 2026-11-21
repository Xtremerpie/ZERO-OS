# Threat Model

**Status: model for the whole project; as of 0.0.0 almost none of the mitigations exist.**
No security is claimed for 0.0.0. There is no user mode, no authentication and no sandbox yet.

| Threat | Asset | Planned mitigation (version) | State |
|--------|-------|------------------------------|-------|
| Malicious applications | user data, system | Ring 3 isolation, capability-style permissions, sandbox, manifest-declared permissions (1.0.0 → 6.0.0 → 8.0.0) | none |
| Privilege escalation | kernel | validated syscalls, copy-in/out, no kernel secrets in user-visible memory, least privilege services (1.0.0+) | none |
| Memory corruption | kernel integrity | W^X, NX, guard pages, stack protector evaluation, bounds-checked APIs, fuzzing of syscalls (1.0.0+) | kernel mapping verified W^X + supervisor-only in QEMU (`kernel_wx_mapping`) - this is Limine's mapping, not a ZERO feature yet; no guard pages |
| Untrusted packages | system | signed packages, publisher identity, explicit permission prompt, never run unverified (6.0.0) | none |
| Unauthorized file access | user files | per-user ownership, per-app file permissions in VFS (2.0.0, 5.0.0 UI) | none |
| Network attacks | system | default-deny firewall, minimal listening services (3.0.0+) | none |
| Credential theft | secrets | encrypted credential store, no plaintext keys in config, no embedded API keys (8.0.0) | none |
| Malicious extensions | desktop/apps | extension sandbox, same permission model (6.0.0) | none |
| Unsafe AI actions | user intent | show action → explain impact → approve sensitive → log → cancel; AI optional (7.0.0) | none |
| Update tampering | system | signed updates, rollback, recovery entry (8.0.0) | none |
| Config tampering (ZERO Config) | settings | schema validation, atomic writes, protected system config, never execute commands from config (5.0.0) | none |
| Supply chain (build) | the OS itself | pinned, hashed third-party inputs; documented dependency list | **partial: hashes recorded for Limine/header/font (0.0.0)** |
| Boot chain | everything | Secure Boot / measured boot research (8.0.0+) | none; Limine unsigned |

## 0.0.0 trust assumptions

Firmware and Limine are trusted (unverified). The kernel runs everything at ring 0. Security testing
uses only the isolated QEMU VM; no testing against external systems.

## Rules

No security feature may be a status indicator without enforcement behind it; no security claim
without evidence (a test or a review record).
