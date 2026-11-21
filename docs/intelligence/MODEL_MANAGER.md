# ZERO Intelligence — Model Manager & Lifecycle
**Milestone:** 8.0.0 — ZERO Intelligence  
**Component:** `kernel/intelligence/ai_model.h`, `ai_model.c`  

---

## 1. Overview

The Model Manager governs model discovery on persistent storage, metadata extraction, hardware memory admissibility checks, and the single-active-model loading lifecycle.

---

## 2. Model Descriptor Structure

```c
typedef struct ai_model_desc {
    uint32_t          id;
    char              name[AI_MODEL_NAME_LEN];
    char              architecture[24];
    ai_model_format_t format;
    uint32_t          param_count_m;  /* Millions of parameters (e.g. 15M, 35M) */
    char              quantization[12];/* "Q4_0", "Q8_0", "FP16" */
    uint32_t          context_len;    /* e.g. 512, 1024, 2048 tokens */
    uint32_t          ram_required_mb;/* Memory budget */
    char              file_path[AI_MODEL_PATH_LEN];
    ai_model_state_t  state;
    uint32_t          provider_id;
    bool              in_use;
} ai_model_desc_t;
```

---

## 3. The Single-Active-Model Lifecycle

To prevent physical memory fragmentation and out-of-memory kernel panics, the Model Manager implements an invariant: **at most one model may be in state `AI_MODEL_STATE_LOADED` at any time**.

### Loading State Transition Sequence:
```
           [Model A: LOADED]          [Model B: AVAILABLE]
                   |                           |
                   |      ai_model_load(B)     |
                   v                           v
         ai_model_unload(A)            ai_model_can_fit_in_memory(B)
                   |                           | (PASS <= 256MB)
                   v                           v
          [Model A: UNLOADED]           [Model B: LOADING]
                                               |
                                               v
                                        [Model B: LOADED]
```

If `ai_model_load(B)` fails during memory allocation or validation, the previous model is not left in an inconsistent state: the manager logs an error and maintains a safe fallback.

---

## 4. Discovery Process (`ai_model_discover`)

During system initialization (`ai_model_init`), the manager performs the following stages:
1. Registers built-in read-only micro-models resident in kernel image:
   - `ZeroMicro-15M` (15M parameters, Q4_0, 32 MB RAM).
   - `ZeroReason-35M` (35M parameters, Q4_0, 64 MB RAM).
   - `ZeroAssistant-Host` (Symbolic adapter reference, 0 MB local RAM).
2. Traverses the filesystem directory `/data/models/`.
3. Inspects header signatures:
   - `GGUF\x01` for GGUF files.
   - `ZMW\x01` for native ZERO Micro-Weights.
4. Populates descriptor slots up to `AI_MAX_MODELS` (8 models).

---

## 5. Model Manager API

| Function | Signature | Description |
|---|---|---|
| `ai_model_init` | `void ai_model_init(void)` | Initializes internal catalog table and runs initial discovery. |
| `ai_model_discover` | `uint32_t ai_model_discover(void)` | Rescans `/data/models` and returns total discovered count. |
| `ai_model_get_count` | `uint32_t ai_model_get_count(void)` | Returns number of models registered in catalog. |
| `ai_model_get_by_id` | `const ai_model_desc_t *ai_model_get_by_id(uint32_t id)` | Finds model descriptor by unique numeric ID. |
| `ai_model_get_active` | `const ai_model_desc_t *ai_model_get_active(void)` | Returns pointer to currently loaded model descriptor. |
| `ai_model_list` | `uint32_t ai_model_list(ai_model_desc_t *out_models, uint32_t max_models)` | Copies out active model catalog to client buffer. |
| `ai_model_can_fit_in_memory` | `bool ai_model_can_fit_in_memory(uint32_t model_id)` | Checks whether model memory requirement is $\le 256$ MB. |
| `ai_model_load` | `bool ai_model_load(uint32_t id)` | Atomically unloads current model and loads target model. |
| `ai_model_unload` | `bool ai_model_unload(void)` | Unloads current model and frees mapped buffers. |
