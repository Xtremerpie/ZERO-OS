# ZERO Intelligence — AI Provider Abstraction
**Milestone:** 8.0.0 — ZERO Intelligence  
**Component:** `kernel/intelligence/ai_provider.h`, `ai_provider.c`  

---

## 1. Design Overview

To ensure longevity, flexibility, and architectural isolation, ZERO OS does not hardcode inference logic directly into the operating system shell. Instead, an abstract provider interface decouples high-level assistant behavior from the underlying execution backends.

The subsystem supports three primary classes of AI backends:
1. **Local CPU Micro-Runtime** (Native in-kernel/Ring 3 inference)
2. **Host Development Adapter** (Development-time bridge to host environment accelerators)
3. **Remote Reference Proxy** (Standard networking bridge via BSD sockets)

---

## 2. Provider Operations Interface

Every provider registers a table of operational callbacks (`ai_provider_ops_t`):

```c
typedef struct ai_provider_ops {
    bool                (*init)(struct ai_provider *prov);
    uint32_t            (*discover_models)(struct ai_provider *prov);
    bool                (*load_model)(struct ai_provider *prov, uint32_t model_id);
    bool                (*unload_model)(struct ai_provider *prov, uint32_t model_id);
    int32_t             (*generate)(struct ai_provider *prov, const char *prompt, char *out_text, size_t max_out);
    bool                (*cancel)(struct ai_provider *prov);
    ai_provider_state_t (*get_status)(struct ai_provider *prov);
} ai_provider_ops_t;
```

---

## 3. Supported Provider Backends

### 3.1 Local CPU Micro-Runtime (`AI_PROVIDER_TYPE_LOCAL_CPU`)
- **ID:** 1
- **Name:** `"Local CPU Micro-Runtime"`
- **Execution Target:** x86_64 CPU (AVX2 / SSE4.2 / scalar instructions).
- **Description:** Runs directly inside the ZERO OS environment without requiring any network connectivity or external hosts. Tailored for lightweight quantized models (e.g. 15M to 35M parameters, 4-bit quantization) taking under 64 MB of physical memory.
- **Inference Latency:** 10 ms to 150 ms depending on context size.

### 3.2 Host Development Adapter (`AI_PROVIDER_TYPE_ADAPTER`)
- **ID:** 2
- **Name:** `"Host Development Adapter"`
- **Execution Target:** Development host hardware via hypervisor shared memory or loopback socket.
- **Description:** Enables development builds running inside QEMU to tap into high-capacity host models (e.g. 70M to 7B parameters) for testing complex automation flows without constraining local emulated RAM.
- **Inference Latency:** 50 ms to 300 ms.

### 3.3 Remote Reference Proxy (`AI_PROVIDER_TYPE_REMOTE`)
- **ID:** 3
- **Name:** `"Remote Reference Proxy"`
- **Execution Target:** External network endpoint.
- **Description:** Interacts with external inference servers over TCP/IP sockets provided by the ZERO OS network stack (Milestone 4.0.0). Can be enabled when outbound network access is granted and configured.
- **Inference Latency:** Dependent on external network round-trip time.

---

## 4. Provider Registry API

| Function | Signature | Purpose |
|---|---|---|
| `ai_provider_init` | `void ai_provider_init(void)` | Initializes registry table and registers default providers. |
| `ai_provider_register` | `bool ai_provider_register(ai_provider_t *prov)` | Adds a third-party or custom provider into the system. |
| `ai_provider_count` | `uint32_t ai_provider_count(void)` | Returns the number of registered providers. |
| `ai_provider_get_by_id` | `ai_provider_t *ai_provider_get_by_id(uint32_t id)` | Looks up provider record by numeric identifier. |
| `ai_provider_get_active` | `ai_provider_t *ai_provider_get_active(void)` | Returns pointer to currently active generation provider. |
| `ai_provider_set_active` | `bool ai_provider_set_active(uint32_t id)` | Activates specified provider and dispatches audit notification. |
| `ai_provider_generate` | `int32_t ai_provider_generate(const char *prompt, char *out_text, size_t max_out)` | Unified top-level inference call that directs generation to active provider. |
