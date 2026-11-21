# Security Audit Logging Subsystem

## 1. Overview
The ZERO OS Security Audit Subsystem (`kernel/security/audit.c`) provides non-volatile and in-memory event tracking for security-critical decisions across the operating system.

---

## 2. Event Types & Triggers

| Event Constant | Code | Trigger Condition |
| :--- | :---: | :--- |
| `AUDIT_EVENT_AUTH_FAILURE` | 0 | Failed login attempt or invalid credentials on graphical lock screen. |
| `AUDIT_EVENT_CAP_GRANT` | 1 | Creation and delegation of a new capability handle to a process. |
| `AUDIT_EVENT_CAP_REVOKE` | 2 | Revocation of an active capability token. |
| `AUDIT_EVENT_PERM_DENY` | 3 | Filesystem discretionary access control (DAC) mode check denial. |
| `AUDIT_EVENT_SANDBOX_VIOLATION` | 4 | Sandboxed process attempting to access outside its jail root or open unauthorized sockets. |
| `AUDIT_EVENT_SYSCALL_VIOLATION` | 5 | Process requesting a syscall prohibited by its active sandbox profile. |
| `AUDIT_EVENT_PACKAGE_VERIFY` | 6 | Package SHA-256 digest verification result during package manager installation. |
| `AUDIT_EVENT_PACKAGE_INSTALL` | 7 | Successful application package extraction and registration in `/data/apps`. |
| `AUDIT_EVENT_FIREWALL_DROP` | 8 | Ingress or egress packet dropped by network filtering rule or default-deny policy. |
| `AUDIT_EVENT_CONFIG_CHANGE` | 9 | Security policy modification (e.g. toggling global enforcement or firewall default). |

---

## 3. Storage Architecture
The audit subsystem implements a two-tier storage model:
1. **Primary RAM Ring Buffer**:
   - Fixed capacity of 64 structured records (`AUDIT_RING_BUFFER_SIZE = 64`).
   - Lock-free, zero-allocation ring storage guaranteeing that logging never exhausts kernel dynamic heap memory.
   - Provides instantaneous display updates to the ZERO Security Center GUI.
2. **Persistent VFS Stream**:
   - In append mode (`O_WRONLY | O_CREAT | O_APPEND`), events are written to `/data/log/audit.log`.
   - Format: `[<timestamp_ms>] <EVENT_TYPE> pid=<PID> res=<RESOURCE> result=<ALLOW|DENY> details=<DETAILS>`
