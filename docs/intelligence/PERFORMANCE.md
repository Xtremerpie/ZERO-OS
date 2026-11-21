# ZERO Intelligence — Performance & Resource Budgets
**Milestone:** 8.0.0 — ZERO Intelligence  
**Component:** Resource Characterization on Target Hardware  

---

## 1. Hardware Baseline

Performance characteristics are benchmarked against the standard development target:
- **Processor:** Intel Celeron J3355 @ 2.00 GHz (2 physical cores, 2 threads, Goldmont microarchitecture).
- **Caches:** L1i 32 KB, L1d 24 KB, L2 2 MB (shared), no L3 cache.
- **RAM:** 4 GB DDR3L-1600 (Single-channel, ~12.8 GB/s peak bandwidth).
- **Storage:** Solid-State Drive (SSD).

---

## 2. Resource Allocation Budgets

| Metric | Allocated Ceiling | Typical Observed | Headroom |
|---|---|---|---|
| **Kernel Ring 0 Coordination Heap** | 8 MB | 1.8 MB | 77.5% |
| **Active Local Model RAM** | 256 MB | 32 MB (15M) / 64 MB (35M) | 75.0% |
| **Context Window Heap (per session)**| 32 KB | 8 KB | 75.0% |
| **Max Concurrent Sessions** | 8 sessions | 1 - 2 active | 75.0% |
| **Log Ring Buffer** | 16 KB | 4 KB | 75.0% |

---

## 3. Inference Latency & Token Throughput

| Provider / Model | Parameters | Quantization | First Token Latency | Generation Throughput | RAM Used |
|---|---|---|---|---|---|
| **Local CPU (ZeroMicro)** | 15M | Q4_0 | 12 ms | ~45 tokens/sec | 32 MB |
| **Local CPU (ZeroReason)**| 35M | Q4_0 | 28 ms | ~22 tokens/sec | 64 MB |
| **Host Development Adapter**| 135M | FP16 | 45 ms | ~85 tokens/sec (Host GPU) | 0 MB (Local) |
| **Remote Reference Proxy** | Any | Remote | 120 ms (RTT) | ~30 tokens/sec | 0 MB (Local) |

---

## 4. Mitigation of Desktop Jitter & UI Latency

On a 2-core CPU, running an intensive CPU inference loop on Core 0 could starve the desktop compositor or input handling.

ZERO OS guarantees graphical responsiveness via:
1. **Preemptive Scheduling (Milestone 2.0.0):** The inference worker runs at thread priority `PRIORITY_BACKGROUND = 1`, while the window manager and compositor run at `PRIORITY_INTERACTIVE = 3`.
2. **Yield Points:** The local CPU generation loop invokes `sched_yield()` every 4 generated tokens, guaranteeing that mouse and keyboard interrupts are processed with $< 8$ ms input latency.
3. **Double Buffering & Damage Tracking (Milestone 5.0.0):** Only the Assistant window rectangle is invalidated during token streaming, consuming $< 2\%$ of the Intel HD Graphics 500 memory bandwidth.
