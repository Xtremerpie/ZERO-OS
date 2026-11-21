# ZERO OS — ZERO Gaming Mode Specification
**Milestone:** 9.0.0 — ZERO Ultimate  
**Component:** `kernel/gaming/gaming.h`, `gaming.c`  

---

## 1. Overview & Operational Concept

ZERO Gaming Mode is an operating-system-level performance profile engine designed to transform ZERO OS into a low-latency, deterministic gaming workstation without compromising kernel stability or security invariants.

Gaming Mode is not a visual cosmetic theme: it dynamically reconfigures the kernel scheduler, coordinates memory pressure, throttles background AI assistant jobs, and prioritizes foreground rendering processes.

---

## 2. Supported System Profiles

### 2.1 NORMAL (Balanced)
- **Time Slice:** 50 ms (5 APIC ticks at 100 Hz).
- **Scheduler:** Standard fair round-robin ready queue.
- **AI Assistant:** Fully available for conversational and automation tasks.
- **Background Tasks:** Normal priority.

### 2.2 GAMING (Low-Latency Performance)
- **Time Slice:** **80 ms** for foreground game process; **20 ms** for background tasks.
- **Priority Dequeue:** Game threads are dequeued preferentially from the ready queue with a strict 4x anti-starvation ceiling.
- **AI Assistant:** Background inference tasks are throttled/paused; model memory is bounded.
- **Compositor:** Desktop animations are minimized; full render budget allocated to game window.

### 2.3 POWER_SAVER (Energy Efficiency)
- **Time Slice:** 100 ms (10 ticks) to coalesce CPU wakeups.
- **Scheduler:** Longer execution batches to maximize CPU C-state residency.
- **Background Tasks:** Throttled.

---

## 3. Dynamic Game Registration & Restoration

```
                +----------------------------+
                |     Profile: NORMAL        |
                +--------------+-------------+
                               |
                               | Game Launches: gaming_register_game(pid, "ZERO Strike")
                               v
                +----------------------------+
                |     Profile: GAMING        |
                |  - Scheduler: LOW_LATENCY  |
                |  - AI Daemon: THROTTLED    |
                |  - Foreground PID: <pid>   |
                +--------------+-------------+
                               |
                               | Game Exits: gaming_unregister_game(pid)
                               v
                +----------------------------+
                |     Profile: NORMAL        |
                |  (Automatically Restored)  |
                +----------------------------+
```

Every profile change emits a structured security audit event (`AUDIT_EVENT_CONFIG_CHANGE`) to `/data/log/audit.log`.

---

## 4. Configuration Schema (`/data/config/gaming.cfg`)

```ini
auto_switch=1
pause_ai=1
target_fps=60
```
- `auto_switch`: If `1`, launching a registered game automatically engages the `GAMING` profile.
- `pause_ai`: If `1`, AI background tasks pause while the game is active.
- `target_fps`: Framerate budget (default 60 FPS = 16.6 ms per frame).
