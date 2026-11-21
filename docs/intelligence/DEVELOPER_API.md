# ZERO Intelligence — Developer SDK Reference
**Milestone:** 8.0.0 — ZERO Intelligence  
**Component:** `sdk/include/zero/ai.h`, `sdk/templates/ai_hello_world/main.c`  

---

## 1. Overview

The ZERO Intelligence SDK exposes native C APIs for user-space (Ring 3) and ecosystem applications to query the AI daemon, submit prompts, stream response tokens, and propose system automation actions through the permission-gated broker.

Applications include the header:
```c
#include <zero/ai.h>
```

---

## 2. Error Codes (`zero_ai_err_t`)

```c
typedef enum {
    ZERO_AI_SUCCESS             = 0,
    ZERO_AI_ERR_NOT_INITIALIZED = -1,
    ZERO_AI_ERR_INVALID_PARAM   = -2,
    ZERO_AI_ERR_NO_MEMORY       = -3,
    ZERO_AI_ERR_BUSY            = -4,
    ZERO_AI_ERR_PERMISSION      = -5,
    ZERO_AI_ERR_TIMEOUT         = -6,
    ZERO_AI_ERR_MODEL_UNLOADED  = -7,
    ZERO_AI_ERR_UNKNOWN         = -8
} zero_ai_err_t;
```

---

## 3. Function Reference

### 3.1 Session Lifecycle
```c
/* Open a new conversational session with the AI Assistant */
zero_ai_err_t zero_ai_session_open(const char *session_title, zero_ai_session_t *out_session);

/* Close an active conversational session */
zero_ai_err_t zero_ai_session_close(zero_ai_session_t session);
```

### 3.2 Inferences & Text Generation
```c
/* Submit a prompt synchronously and wait for full response */
zero_ai_err_t zero_ai_prompt(zero_ai_session_t session,
                             const char *prompt,
                             char *response_buf,
                             size_t response_buf_size,
                             size_t *out_response_len);

/* Submit a prompt asynchronously with a token stream callback */
zero_ai_err_t zero_ai_prompt_async(zero_ai_session_t session,
                                   const char *prompt,
                                   zero_ai_stream_cb callback,
                                   void *userdata);
```

### 3.3 Automation Requests
```c
/* Request an automated system action through the permission broker */
zero_ai_err_t zero_ai_request_action(zero_ai_action_type_t type,
                                     const char *target,
                                     const char *details,
                                     zero_ai_action_t *out_action);

/* Query action execution status */
zero_ai_err_t zero_ai_action_poll(uint32_t action_id, zero_ai_action_t *out_action);
```

### 3.4 Telemetry & Catalog
```c
/* Query AI assistant daemon telemetry */
zero_ai_err_t zero_ai_get_telemetry(zero_ai_telemetry_t *out_telemetry);

/* Enumerate available local models */
zero_ai_err_t zero_ai_list_models(zero_ai_model_info_t *out_models,
                                  uint32_t max_models,
                                  uint32_t *out_model_count);

/* Set preferred AI provider */
zero_ai_err_t zero_ai_set_provider(zero_ai_provider_t provider);
```

---

## 4. Canonical Example: Initializing Session & Submitting Prompt

```c
#include <stdio.h>
#include <zero/ai.h>

int main(void)
{
    zero_ai_session_t session;
    if (zero_ai_session_open("MyChat", &session) != ZERO_AI_SUCCESS) {
        printf("Failed to open AI session\n");
        return 1;
    }

    char response[512];
    size_t out_len = 0;
    zero_ai_err_t err = zero_ai_prompt(session, "Explain ZERO OS design philosophy",
                                       response, sizeof(response), &out_len);
    if (err == ZERO_AI_SUCCESS) {
        printf("AI Response:\n%s\n", response);
    }

    zero_ai_session_close(session);
    return 0;
}
```
