# ZERO Intelligence — Subsystem Overview
**Milestone:** 8.0.0 — ZERO Intelligence  
**Codename:** `ZERO INTELLIGENCE`  
**Architecture:** x86_64 Higher-Half Native Micro-Kernel Subsystem  
**Security Boundary:** Kernel Ring 0 Core Coordinator + Ring 3 Sandboxed Daemon  

---

## 1. Executive Summary

ZERO Intelligence introduces native, permission-gated artificial intelligence and automated assistance into ZERO OS. Built from first principles without external operating system dependencies, ZERO Intelligence is designed around an uncompromised foundational rule:

> **The AI Assistant is an unprivileged advisory subsystem. The operating system kernel and human user retain ultimate authority at all times.**

Rather than granting an AI model unfettered system privileges or deploying simulated web wrappers, ZERO OS implements an end-to-end native subsystem spanning:
- A persistent background daemon (`ai_daemon`).
- Pluggable provider runtimes (`ai_provider`) supporting local CPU micro-inference, host development adapters, and remote reference bridges.
- A deterministic single-active-model manager (`ai_model`) enforcing strict physical memory constraints tailored for modest hardware (Intel Celeron J3355, 4 GB RAM).
- A permission-gated automation broker (`ai_automation`) strictly integrated with ZERO Security 7.0.0 capabilities and process sandboxing, enforcing explicit human-in-the-loop approvals for sensitive system state modifications.
- A persistent conversational memory and preference store (`ai_memory`) backed by native VFS (`/data/ai`).
- Direct integration into the central security audit logger (`ai_audit`).
- A native ZERO Desktop graphical assistant application (`kernel/desktop/ai_assistant.c`).
- A developer-facing C SDK (`sdk/include/zero/ai.h`).

---

## 2. Architectural Blueprint

```
                      +---------------------------------------+
                      |       ZERO Desktop Application        |
                      |          (ai_assistant.c)             |
                      +-------------------+-------------------+
                                          |
                                          v
+-----------------------------------------------------------------------------------+
|                           ZERO Intelligence Developer SDK                         |
|                                (zero/ai.h)                                        |
+-----------------------------------------+-----------------------------------------+
                                          |
                                          v
+-----------------------------------------------------------------------------------+
|                        Central AI Service Coordinator                             |
|                             (ai_service.c/.h)                                     |
+---------------------+-------------------+-------------------+---------------------+
                      |                   |                   |
                      v                   v                   v
            +-------------------+ +---------------+ +-------------------+
            | AI Daemon Service | | Model Manager | | Persistent Memory |
            | (ai_daemon.c/.h)  | | (ai_model.c)  | | (/data/ai/)       |
            +---------+---------+ +-------+-------+ +-------------------+
                      |                   |
                      v                   v
            +-------------------+ +---------------+
            |  Request Engine   | | AI Providers  |
            |   (ai_engine.c)   | |  - Local CPU  |
            +---------+---------+ |  - Host Adapt |
                      |           |  - Remote     |
                      |           +---------------+
                      v
+-----------------------------------------------------------------------------------+
|                    Permission-Gated Automation Broker                             |
|                           (ai_automation.c/.h)                                    |
+-----------------------------------------+-----------------------------------------+
                                          |
                      +-------------------+-------------------+
                      |                                       |
                      v                                       v
         +-------------------------+             +-------------------------+
         |   User Approval Gate    |             |  ZERO Security 7.0.0    |
         | (Interactive Confirm)   |             |  - Capability Checking  |
         +------------+------------+             |  - Sandbox Containment  |
                      |                          |  - Audit Log Bridge     |
                      v                          +-------------------------+
         +-------------------------+
         | Safe Subsystem Execution|
         | (VFS / App / Sysinfo)   |
         +-------------------------+
```

---

## 3. Subsystem Index

| Component | Header / Source | Primary Responsibility |
|---|---|---|
| **Central Service Coordinator** | `ai_service.h` / `ai_service.c` | Top-level subsystem orchestration, lifecycle, telemetry |
| **Assistant Daemon** | `ai_daemon.h` / `ai_daemon.c` | Background service state machine, message queue, log ring buffer |
| **Provider Abstraction** | `ai_provider.h` / `ai_provider.c` | Pluggable runtime dispatch: Local CPU, Host Adapter, Remote |
| **Model Manager** | `ai_model.h` / `ai_model.c` | Metadata validation, single-active-model memory budget enforcement |
| **Automation Broker** | `ai_automation.h` / `ai_automation.c` | Action proposals, user approval gating, capability boundary |
| **Request Engine** | `ai_engine.h` / `ai_engine.c` | Context assembly, intent parsing, action classification |
| **Persistent Memory** | `ai_memory.h` / `ai_memory.c` | Conversation histories (`/data/ai/history/`), preferences |
| **Security Audit Bridge** | `ai_audit.h` / `ai_audit.c` | Structured event streaming into central audit log ring buffer |
| **Desktop Application** | `ai_assistant.h` / `ai_assistant.c` | Native 6-view windowed GUI (Chat, Models, Actions, Memory, Settings, Service) |
| **Developer SDK** | `zero/ai.h` | Ring 3 C API for third-party ZERO OS applications |
| **In-Kernel Self-Tests** | `selftest_intelligence.h` / `.c` | 7-part comprehensive boot self-test suite |
| **Regression Test Suite** | `tests/test_intelligence.py` | Standalone Python validation suite |

---

## 4. Key Design Invariants

1. **Deterministic Single-Active-Model Rule:** To avoid thrashing the memory subsystem on 4 GB RAM systems, only one local model may be loaded into physical memory at any time. Loading a second model automatically unloads the prior one.
2. **Explicit User Approval Gate:** Any action that alters filesystem state, controls system services, or spawns uncontained processes requires explicit interactive user confirmation before execution.
3. **Bounded Context Window:** Conversation history is strictly bounded in memory (maximum 16 turns per session) to prevent heap exhaustion. Historical logs are archived to persistent VFS files.
4. **Audit Trail Completeness:** Every prompt, model switch, action proposal, approval, and execution event produces an unalterable structured audit entry in the central ZERO Security audit log.
