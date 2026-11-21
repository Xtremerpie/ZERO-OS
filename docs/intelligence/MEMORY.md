# ZERO Intelligence — Persistent Memory & History Subsystem
**Milestone:** 8.0.0 — ZERO Intelligence  
**Component:** `kernel/intelligence/ai_memory.h`, `ai_memory.c`  

---

## 1. Overview & Directory Layout

ZERO Intelligence maintains user preferences, session history, and action outcome traces persistently across reboots using the native Virtual File System (`vfs`) and RAMFS/ZEROFS filesystem drivers.

All intelligence data resides strictly under `/data/ai`:
```
/data/ai/
├── preferences.cfg          # User preferences and security thresholds
└── history/                 # Bounded session transcripts
    ├── session_1.log
    ├── session_2.log
    └── session_3.log
```

---

## 2. User Preferences Schema (`/data/ai/preferences.cfg`)

User preferences are encoded in simple, deterministic key-value text lines:

```ini
default_model=ZeroMicro-15M
max_context_tokens=512
auto_approve_safe_info=1
confirm_sensitive_writes=1
notifications_enabled=1
active_provider_id=1
```

### Preference Fields (`ai_user_prefs_t`):
- `default_model`: Model ID or name loaded on startup.
- `max_context_tokens`: Maximum context length in tokens (default 512, maximum 2048).
- `auto_approve_safe_info`: If `1`, Tier 1 queries execute without prompting.
- `confirm_sensitive_writes`: If `1`, Tier 3 file writes mandate interactive user approval.
- `notifications_enabled`: If `1`, AI completion and alert toasts appear on desktop.
- `active_provider_id`: Default provider (1 = Local CPU, 2 = Adapter, 3 = Remote).

---

## 3. Session History Format & Context Window Bounds

Transient conversational context in memory is capped at `AI_MAX_SESSION_MSGS = 16` messages per session. This provides sufficient multi-turn reasoning context while guaranteeing that memory consumption never exceeds a few kilobytes per session.

When a session is serialized to disk via `ai_memory_persist_session()`:
```
[2026-11-21 09:15:00] [SYSTEM] You are ZERO AI Assistant running on ZERO OS 8.0.0.
[2026-11-21 09:15:02] [USER] Show current CPU and memory status.
[2026-11-21 09:15:03] [ASSISTANT] System status: CPU Load: 4%, Memory: 142 MB / 4096 MB.
```

---

## 4. Privacy & Atomic Erasure

To protect user confidentiality, ZERO Intelligence implements `ai_memory_clear_all_history()`:
- Scans `/data/ai/history/` directory.
- Unlinks every session history transcript.
- Resets in-memory session message ring buffers.
- Emits an audit entry `AI_MEMORY_PURGE` into the security log.
