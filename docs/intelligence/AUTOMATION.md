# ZERO Intelligence — Permission-Gated Automation Broker
**Milestone:** 8.0.0 — ZERO Intelligence  
**Component:** `kernel/intelligence/ai_automation.h`, `ai_automation.c`  

---

## 1. Safety Paradigm & Boundary Enforcement

A foundational hazard of OS-level AI assistants is unconstrained execution: an LLM generating shell commands or writing arbitrary files without user awareness.

ZERO OS solves this by making the AI assistant **incapable of directly executing system calls or file mutations**. All automated operational proposals must be routed through the **Automation Broker** (`ai_automation`), which evaluates:
1. **Action Risk Taxonomy** (Safe vs. Sensitive).
2. **ZERO Security 7.0.0 Capability Tokens** (`capability_verify()`).
3. **Interactive Human-in-the-Loop Approval**.
4. **Sandbox Containment** (`sandbox_enter()`).

---

## 2. Action Risk Classification

```c
typedef enum {
    AI_ACTION_NONE = 0,
    AI_ACTION_SYS_INFO,         /* Tier 1: Read-only system metrics (CPU, RAM, Uptime) */
    AI_ACTION_NOTIFICATION,     /* Tier 1: Non-disruptive desktop toast banner */
    AI_ACTION_APP_LAUNCH,       /* Tier 2: Launch authorized desktop application */
    AI_ACTION_FILE_READ,        /* Tier 2: Read permitted file path */
    AI_ACTION_FILE_WRITE,       /* Tier 3: Create/modify file in filesystem */
    AI_ACTION_SERVICE_CONTROL   /* Tier 3: Start/stop system daemons or background services */
} ai_action_type_t;
```

### Risk Tiers & Approval Policies:

| Action Type | Risk Tier | Requires User Approval? | Capability Required | Sandbox Restriction |
|---|---|---|---|---|
| `AI_ACTION_SYS_INFO` | Tier 1 (Low) | **No** (Auto-Approved) | None | Read-only |
| `AI_ACTION_NOTIFICATION`| Tier 1 (Low) | **No** (Auto-Approved) | None | UI surface only |
| `AI_ACTION_APP_LAUNCH` | Tier 2 (Medium)| **Yes** (User Prompt) | `CAP_TYPE_PROCESS` | Isolated PID |
| `AI_ACTION_FILE_READ` | Tier 2 (Medium)| **Yes** (If sensitive) | `CAP_TYPE_FILE` (Read)| POSIX DAC check |
| `AI_ACTION_FILE_WRITE`| Tier 3 (High) | **YES (Always Required)** | `CAP_TYPE_FILE` (Write)| Jail path checking |
| `AI_ACTION_SERVICE_CONTROL`| Tier 3 (High)| **YES (Always Required)** | `CAP_TYPE_SYSTEM` (Admin)| Root/PID 0 audit |

---

## 3. The User Approval Lifecycle

When the AI engine recognizes an intent to perform a sensitive operation (e.g. "Save system diagnostic report to `/data/report.txt`"):
1. The assistant engine calls `ai_automation_propose()`.
2. The broker verifies that the proposed path does not violate kernel jail rules.
3. Because `AI_ACTION_FILE_WRITE` is a Tier 3 action, the broker marks status as `AI_ACTION_STATUS_PENDING_APPROVAL` and returns an action handle.
4. The Assistant Desktop GUI displays an alert card with two interactive buttons: `[Approve]` and `[Reject]`.
5. **If Approved (`ai_automation_approve`):** The broker updates the status to `AI_ACTION_STATUS_APPROVED`. Subsequent execution via `ai_automation_execute()` performs the operation and transitions state to `AI_ACTION_STATUS_EXECUTED`.
6. **If Rejected (`ai_automation_reject`):** The broker updates the status to `AI_ACTION_STATUS_REJECTED`. Any attempt to execute this action is rejected.

---

## 4. Automation Broker API

```c
/* Propose an automation action from the AI engine */
uint32_t ai_automation_propose(uint32_t session_id, uint32_t pid, ai_action_type_t type,
                              const char *target, const char *desc);

/* User approval / rejection interface */
bool ai_automation_approve(uint32_t action_id);
bool ai_automation_reject(uint32_t action_id);

/* Execute approved action within security sandbox */
bool ai_automation_execute(uint32_t action_id, char *out_result, size_t max_out);

/* Query pending and historical actions */
uint32_t ai_automation_get_pending(ai_action_t *out_actions, uint32_t max_actions);
uint32_t ai_automation_get_history(ai_action_t *out_actions, uint32_t max_actions);
const ai_action_t *ai_automation_get_by_id(uint32_t action_id);
```
