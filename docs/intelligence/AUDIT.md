# ZERO Intelligence — Security Audit & Telemetry
**Milestone:** 8.0.0 — ZERO Intelligence  
**Component:** `kernel/intelligence/ai_audit.h`, `ai_audit.c`  

---

## 1. Overview & Security Objectives

Because AI models generate outputs stochastically, system operations initiated or influenced by artificial intelligence require exhaustive, immutable auditing.

The AI Audit subsystem connects all AI actions directly to the **Central Security Architecture** introduced in Milestone 7.0.0 (`security/audit.h`), guaranteeing:
1. **Traceability:** Every automated action can be mapped directly back to the session, calling PID, active model, and prompt that triggered it.
2. **Accountability:** Explicit records denote whether an action was user-approved or auto-approved.
3. **Forensic Integrity:** Audit records are written into the kernel's circular audit log and persisted to `/var/log/audit.log`.

---

## 2. Event Types & Format

AI-related events are classified under the following event schema:

| Event Type | Trigger | Example Log Entry |
|---|---|---|
| `AI_SESSION_INIT` | A conversational session is initialized | `[10500] AI_SESSION_INIT pid=10 session=1 title="DevChat"` |
| `AI_SESSION_CLOSE` | A conversational session terminates | `[10520] AI_SESSION_CLOSE pid=10 session=1 turns=6` |
| `AI_MODEL_LOAD` | A new model is loaded into memory | `[10550] AI_MODEL_LOAD model="ZeroMicro-15M" ram=32MB status=SUCCESS` |
| `AI_MODEL_UNLOAD` | A model is evicted from memory | `[10560] AI_MODEL_UNLOAD model="ZeroMicro-15M" status=SUCCESS` |
| `AI_PROVIDER_SWITCH` | Active provider changes | `[10580] AI_PROVIDER_SWITCH from=1 to=2 name="Host Development Adapter"` |
| `AI_ACTION_PROPOSE` | Engine suggests a system action | `[10600] AI_ACTION_PROPOSE act_id=4 type=FILE_WRITE target="/data/notes.txt"` |
| `AI_ACTION_APPROVE` | User approves pending action | `[10620] AI_ACTION_APPROVE act_id=4 user=admin` |
| `AI_ACTION_REJECT` | User rejects pending action | `[10625] AI_ACTION_REJECT act_id=5 user=admin reason="User declined"` |
| `AI_ACTION_EXECUTE` | Broker executes approved action | `[10630] AI_ACTION_EXECUTE act_id=4 status=SUCCESS` |
| `AI_ACTION_DENIED` | Broker blocks unapproved action | `[10640] AI_ACTION_DENIED act_id=6 reason="Capability check failed"` |

---

## 3. Audit Bridge API

```c
/* Record AI assistant session lifecycle events */
void ai_audit_session_event(uint32_t session_id, uint32_t pid, bool created, const char *title);

/* Record model load/unload events */
void ai_audit_model_event(uint32_t model_id, const char *model_name, bool loaded, uint32_t ram_mb);

/* Record provider switch events */
void ai_audit_provider_event(uint32_t provider_id, const char *prov_name, bool activated);

/* Record automation proposal, approval, execution, or denial */
void ai_audit_action_event(const ai_action_t *action, const char *details);
```
