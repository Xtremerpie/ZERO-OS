# ZERO Intelligence — Subsystem Architecture
**Milestone:** 8.0.0 — ZERO Intelligence  
**Component:** System Architecture & Data Flow Specification  

---

## 1. Architectural Philosophy

Operating systems increasingly treat artificial intelligence as an external cloud endpoint or an opaque, privileged daemon running with root privileges. Both paradigms violate the core design goals of ZERO OS: independence, lightweight efficiency, and uncompromising user control.

ZERO Intelligence architecturally treats artificial intelligence as an **advisory hardware accelerator and intent classifier**. The AI subsystem cannot directly invoke kernel system calls or mutate files; it can only propose structured actions to the **Automation Broker**, which evaluates them against the active process's capability set and prompts the human user when safety boundaries are crossed.

---

## 2. Core Subsystem Decomposition

### 2.1 Central AI Service Coordinator (`ai_service.c`)
The service coordinator acts as the single administrative point of contact for the operating system kernel:
- Initializes the daemon, provider registry, model manager, automation broker, memory subsystem, and audit hooks during boot.
- Collects global telemetry: total inferences processed, cumulative tokens generated, actions proposed, approvals, rejections, active sessions, and daemon health.
- Enforces orderly startup, suspension, and shutdown sequences.

### 2.2 Native Assistant Daemon (`ai_daemon.c`)
The daemon executes as an event-driven background service managing asynchronous requests:
- **State Machine:**
  - `AI_DAEMON_IDLE`: Ready for incoming session requests.
  - `AI_DAEMON_BUSY_INFERENCE`: Running provider inference.
  - `AI_DAEMON_AWAITING_APPROVAL`: A proposed action requires user intervention.
  - `AI_DAEMON_EXECUTING_ACTION`: Running an approved automation command.
  - `AI_DAEMON_STOPPED`: Daemon halted by administrator.
- **Queueing & Ring Buffer:**
  - FIFO request queue for handling multiple concurrent client sessions.
  - 32-entry circular log buffer capturing real-time operational traces and error messages for diagnostic inspection.

### 2.3 Pluggable AI Provider Architecture (`ai_provider.c`)
Separates model execution algorithms from assistant business logic via standard operational function pointers (`ai_provider_ops_t`):
- `init`: Setup hardware runtime or communication pipe.
- `discover_models`: Scan provider-specific storage or endpoints.
- `load_model` / `unload_model`: Memory mapping and weight allocation.
- `generate`: Synchronous or streaming token synthesis.
- `cancel`: Premature termination of inference passes.
- `get_status`: Runtime health assessment.

**Implemented Providers:**
1. **Local CPU Micro-Runtime (`AI_PROVIDER_TYPE_LOCAL_CPU`):** Freestanding C token synthesis engine optimized for x86_64 AVX2 / SSE4.2 instructions, running small quantized weights (< 64 MB).
2. **Host Development Adapter (`AI_PROVIDER_TYPE_ADAPTER`):** Bridge communicating across the development harness to leverage host LLM acceleration during kernel simulation.
3. **Remote Reference Proxy (`AI_PROVIDER_TYPE_REMOTE`):** Network socket driver interfacing with standard external REST/gRPC endpoints using the BSD socket API (syscalls 11-17) introduced in Milestone 4.0.0.

### 2.4 Local Model Manager (`ai_model.c`)
Enforces hardware budgets and model cataloging:
- Discovers models located in `/data/models/` and built-in ROM descriptors.
- Validates metadata: parameter counts, quantization schemes (`Q4_0`, `Q8_0`, `FP16`), context length, and estimated RAM footprint.
- **Single-Active-Model Enforcement:** Enforces that only one model resides in RAM simultaneously. Loading model $B$ immediately invalidates and frees the memory allocated to model $A$.

### 2.5 Permission-Gated Automation Broker (`ai_automation.c`)
Acts as the security firewall between AI text output and system execution:
- Classifies candidate actions into safe (e.g. read telemetry, display toast notifications) versus sensitive (e.g. write filesystem, terminate process, reconfigure network).
- Integrates with ZERO Security 7.0.0 `capability_verify()` to check whether the requesting application possesses appropriate capability tokens (`CAP_PERM_WRITE`, `CAP_PERM_ADMIN`).
- Implements an interactive User Approval Gate: when sensitive actions are proposed, execution blocks until the user confirms or rejects the proposal in the Desktop UI.

### 2.6 Request Engine & Context Assembly (`ai_engine.c`)
Orchestrates prompt pipelines:
- Manages conversational sessions (`ai_session_t`) with up to 16 rolling message turns.
- Preserves system instructions at position 0 while evicting the oldest user/assistant pairs when the context window saturates.
- Parses natural language user input for action triggers (e.g., "launch calculator", "inspect memory", "create report").

### 2.7 Persistent Memory Subsystem (`ai_memory.c`)
Backs transient conversational state to persistent storage:
- Stores session logs in `/data/ai/history/session_<id>.log`.
- Stores user assistant configurations in `/data/ai/preferences.cfg`.
- Supports atomic history purge for privacy compliance.

### 2.8 Security Audit Integration (`ai_audit.c`)
Ensures full traceability:
- Every prompt submission, model switch, and action decision emits a structured event directly into the kernel's central security audit ring buffer (`security/audit.c`), creating an immutable record for forensic examination.
