# ZERO Intelligence — Local Model Formats & Constraints
**Milestone:** 8.0.0 — ZERO Intelligence  
**Component:** Model Architecture & Low-Resource Optimization  

---

## 1. Hardware Context: Intel Celeron J3355 & 4 GB RAM

The primary physical target for ZERO OS development is an Intel Celeron J3355 machine:
- **CPU:** 2 Cores / 2 Threads (Apollo Lake, base 2.0 GHz, boost 2.5 GHz).
- **RAM:** 4 GB DDR3L/DDR4 (shared with Intel HD Graphics 500).
- **GPU:** Integrated Intel HD Graphics 500 (no discrete VRAM, no dedicated NPU/TPU).
- **Storage:** SATA/NVMe SSD.

Running modern multi-billion-parameter neural networks directly on this hardware would exhaust physical memory and trigger kernel page exhaustion or system unresponsiveness.

ZERO Intelligence solves this by establishing **strict physical memory budgets** and adopting **micro-model architectures**.

---

## 2. Memory Budget Allocation

```
Total Physical RAM: 4096 MB
├── Framebuffer & Intel HD Graphics:       ~128 MB
├── ZERO OS Kernel (Ring 0 & Buffers):     ~64 MB
├── ZERO Desktop Environment & Apps:       ~256 MB
├── AI Assistant Maximum Model Budget:     ~256 MB (Strict Ceiling)
└── Available System & User Page Cache:    ~3392 MB
```

### The 256 MB Invariant
No local AI model loaded by the ZERO OS Model Manager may require more than **256 MB** of system RAM. Any model descriptor whose `ram_required_mb > 256` is rejected by `ai_model_can_fit_in_memory()` with an explicit `ERR_NO_MEMORY` warning.

---

## 3. Supported Model Formats

```c
typedef enum {
    AI_MODEL_FORMAT_BUILTIN = 0,      /* Compiled read-only micro-weights in kernel */
    AI_MODEL_FORMAT_GGUF,             /* Standard quantized GGUF format on disk */
    AI_MODEL_FORMAT_MICRO_WEIGHTS,    /* Zero-specific compact linear weights */
    AI_MODEL_FORMAT_REMOTE_REF        /* Symbolic proxy reference to remote endpoint */
} ai_model_format_t;
```

### 3.1 Builtin Micro-Weights (`AI_MODEL_FORMAT_BUILTIN`)
- Fast, zero-allocation intent classifier and query engine.
- Resident in the kernel executable's `.rodata` section.
- Memory overhead: ~32 MB.
- Parameter count: ~15 Million.
- Quantization: `Q4_0` (4-bit linear symmetric quantization).

### 3.2 Compact Micro-Weights (`AI_MODEL_FORMAT_MICRO_WEIGHTS`)
- Stored on disk under `/data/models/*.zmw`.
- Designed specifically for ZERO OS without bulky header bloat.
- Memory overhead: ~64 MB.
- Parameter count: ~35 Million.
- Quantization: `Q4_0`.

### 3.3 GGUF Quantized Models (`AI_MODEL_FORMAT_GGUF`)
- Standard format widely used in the open-source community.
- ZERO OS reads tensor metadata and loads layers incrementally.
- Quantization options supported: `Q4_0`, `Q8_0`, `FP16`.

---

## 4. Quantization Strategy

| Scheme | Bits / Weight | Relative Quality | Memory Footprint (35M Params) | CPU Instruction Speedup |
|---|---|---|---|---|
| **FP32** | 32 | Baseline (100%) | ~140 MB | 1.0x (Slow, memory bottlenecked) |
| **FP16** | 16 | 99.8% | ~70 MB | 1.6x |
| **Q8_0** | 8 | 99.2% | ~37 MB | 2.8x (AVX2 integer dot product) |
| **Q4_0** | 4 | 96.5% | ~19 MB | 4.2x (Optimal cache residency) |

By standardizing on **4-bit quantization (`Q4_0`)**, ZERO Intelligence fits an entire 35M-parameter reasoning model in under 20 MB of compressed weights, comfortably residing in the CPU's L3/L2 cache and memory channels without stalling desktop responsiveness.
