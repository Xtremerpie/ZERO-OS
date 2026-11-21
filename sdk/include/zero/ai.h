/* ZERO OS SDK - ZERO Intelligence Developer API (Milestone 8.0.0 ZERO Intelligence)
 *
 * Provides developer-facing APIs for Ring 3 and native applications to interact
 * with the ZERO AI Assistant daemon, request text inferences, propose actions,
 * and query local AI subsystem telemetry.
 */
#ifndef _ZERO_SDK_AI_H
#define _ZERO_SDK_AI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Result status codes */
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

/* AI Provider types */
typedef enum {
    ZERO_AI_PROVIDER_DEFAULT    = 0,
    ZERO_AI_PROVIDER_LOCAL_CPU  = 1,
    ZERO_AI_PROVIDER_HOST_ADAPTER = 2,
    ZERO_AI_PROVIDER_REMOTE     = 3
} zero_ai_provider_t;

/* Automation action types */
typedef enum {
    ZERO_AI_ACTION_NONE         = 0,
    ZERO_AI_ACTION_SYS_INFO     = 1,
    ZERO_AI_ACTION_NOTIFICATION = 2,
    ZERO_AI_ACTION_APP_LAUNCH   = 3,
    ZERO_AI_ACTION_FILE_READ    = 4,
    ZERO_AI_ACTION_FILE_WRITE   = 5,
    ZERO_AI_ACTION_SERVICE_CTL  = 6
} zero_ai_action_type_t;

/* Action permission level */
typedef enum {
    ZERO_AI_PERM_READ_ONLY      = 1,
    ZERO_AI_PERM_NORMAL         = 2,
    ZERO_AI_PERM_SENSITIVE      = 3
} zero_ai_perm_level_t;

/* Session handle */
typedef uint32_t zero_ai_session_t;
#define ZERO_AI_INVALID_SESSION 0

/* Maximum string sizes */
#define ZERO_AI_MAX_PROMPT_LEN      512
#define ZERO_AI_MAX_RESPONSE_LEN    1024
#define ZERO_AI_MAX_NAME_LEN        32
#define ZERO_AI_MAX_MODELS          8

/* Model description */
typedef struct {
    char        name[ZERO_AI_MAX_NAME_LEN];
    char        format[16];
    uint32_t    param_count_m;
    uint32_t    context_len;
    uint32_t    mem_required_kb;
    bool        is_active;
} zero_ai_model_info_t;

/* Assistant status & telemetry */
typedef struct {
    bool        daemon_running;
    char        active_provider[ZERO_AI_MAX_NAME_LEN];
    char        active_model[ZERO_AI_MAX_NAME_LEN];
    uint32_t    total_prompts;
    uint32_t    total_tokens_out;
    uint32_t    avg_latency_ms;
    uint32_t    heap_used_kb;
} zero_ai_telemetry_t;

/* Automation action proposal */
typedef struct {
    uint32_t                action_id;
    zero_ai_action_type_t   type;
    zero_ai_perm_level_t    perm_level;
    char                    target[128];
    char                    details[256];
    bool                    approved;
    bool                    executed;
    int32_t                 result_code;
} zero_ai_action_t;

/* Response callback for streaming or async completion */
typedef void (*zero_ai_stream_cb)(zero_ai_session_t session, const char *chunk, bool is_done, void *userdata);

/* --- Core API Functions --- */

/* Open a new conversational session with the AI Assistant */
zero_ai_err_t zero_ai_session_open(const char *session_title, zero_ai_session_t *out_session);

/* Close an active conversational session */
zero_ai_err_t zero_ai_session_close(zero_ai_session_t session);

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

/* Request an automated system action through the permission broker */
zero_ai_err_t zero_ai_request_action(zero_ai_action_type_t type,
                                     const char *target,
                                     const char *details,
                                     zero_ai_action_t *out_action);

/* Query action execution status */
zero_ai_err_t zero_ai_action_poll(uint32_t action_id, zero_ai_action_t *out_action);

/* Query AI assistant daemon telemetry */
zero_ai_err_t zero_ai_get_telemetry(zero_ai_telemetry_t *out_telemetry);

/* Enumerate available local models */
zero_ai_err_t zero_ai_list_models(zero_ai_model_info_t *out_models,
                                  uint32_t max_models,
                                  uint32_t *out_model_count);

/* Set preferred AI provider */
zero_ai_err_t zero_ai_set_provider(zero_ai_provider_t provider);

#ifdef __cplusplus
}
#endif

#endif /* _ZERO_SDK_AI_H */
