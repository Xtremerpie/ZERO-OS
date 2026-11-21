# Process Sandboxing & Isolation Engine

## 1. Overview
Process sandboxing in ZERO OS confines untrusted or third-party applications to restricted runtime profiles. The engine guarantees that a compromised application cannot compromise the host kernel, steal sensitive data from other applications, or access hardware peripherals without explicit authorization.

## 2. Containment Profiles

| Profile | Level | Network Sockets | Raw Devices | IPC Access | Filesystem Root | Syscall Subset |
| :--- | :---: | :---: | :---: | :---: | :--- | :--- |
| **RESTRICTED** | 1 | Disallowed | Disallowed | Disallowed | Isolated Jail (`/data/apps/<app_id>`) | Minimal (read, write, exit, yield, sleep) |
| **BASIC** | 2 | Disallowed | Disallowed | Allowed | App Jail Root | Standard User IO + IPC |
| **STANDARD** | 3 | Allowed | Disallowed | Allowed | App Jail Root + Shared | Full User Suite + Sockets |
| **SYSTEM** | 4 | Allowed | Allowed | Allowed | Global VFS (`/`) | Unrestricted System Calls |

## 3. Filesystem Jail Rooting
When a process executes inside a sandbox, all VFS path operations are filtered through `sandbox_check_path()`.
- The engine checks whether the normalized target path begins with the process's assigned `root_dir`.
- Any attempt to use dot-dot sequences (`..`), escape slashes, or reference paths outside the jail immediately fails and triggers an `AUDIT_EVENT_SANDBOX_VIOLATION`.

## 4. Syscall Gating
Before dispatching any system call in `kernel/syscall/syscall.c`, the dispatcher calls `sandbox_check_syscall(pid, syscall_num)`.
- If the syscall is not permitted under the process's active profile, execution terminates with `SYS_ERR_PERM` and is logged to the audit subsystem.

## 5. Memory Boundary & User Pointer Sanitization
To prevent Confused Deputy attacks and kernel memory corruption:
- `sandbox_validate_user_ptr(ptr, len, is_write)` verifies:
  1. Non-null pointer and non-zero length.
  2. Arithmetic addition of pointer and length does not wrap around 64-bit integer space.
  3. Strict canonical address compliance.
  4. The entire address range strictly resides in user space (`< 0x0000800000000000`).
  5. Any overlap with higher-half kernel space (`>= 0xFFFF800000000000`) is immediately faulted.
