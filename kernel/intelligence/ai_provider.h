/* ZERO KERNEL - AI Provider Abstraction Interface (Milestone 8.0.0 ZERO Intelligence).
 *
 * Implements a pluggable provider abstraction separating model runtimes from
 * assistant logic. Supports local native CPU inference, host adapters, and remote APIs.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define AI_MAX_PROVIDERS     4u
#define AI_PROVIDER_NAME_LEN 32u

typedef enum {
    AI_PROVIDER_TYPE_LOCAL_CPU = 0,
    AI_PROVIDER_TYPE_ADAPTER,
    AI_PROVIDER_TYPE_REMOTE
} ai_provider_type_t;

typedef enum {
    AI_PROVIDER_STATE_UNINITIALIZED = 0,
    AI_PROVIDER_STATE_READY,
    AI_PROVIDER_STATE_BUSY,
    AI_PROVIDER_STATE_ERROR,
    AI_PROVIDER_STATE_OFFLINE
} ai_provider_state_t;

struct ai_provider;

typedef struct ai_provider_ops {
    bool (*init)(struct ai_provider *prov);
    uint32_t (*discover_models)(struct ai_provider *prov);
    bool (*load_model)(struct ai_provider *prov, uint32_t model_id);
    bool (*unload_model)(struct ai_provider *prov, uint32_t model_id);
    int32_t (*generate)(struct ai_provider *prov, const char *prompt, char *out_text, size_t max_out);
    bool (*cancel)(struct ai_provider *prov);
    ai_provider_state_t (*get_status)(struct ai_provider *prov);
} ai_provider_ops_t;

typedef struct ai_provider {
    uint32_t                id;
    char                    name[AI_PROVIDER_NAME_LEN];
    ai_provider_type_t      type;
    ai_provider_state_t     state;
    bool                    active;
    const ai_provider_ops_t *ops;
    void                   *priv_data;
} ai_provider_t;

/* Initialize provider registry */
void ai_provider_init(void);

/* Register an AI provider */
bool ai_provider_register(ai_provider_t *prov);

/* Query available providers */
uint32_t ai_provider_count(void);
ai_provider_t *ai_provider_get(uint32_t index);
ai_provider_t *ai_provider_get_by_id(uint32_t id);
ai_provider_t *ai_provider_get_active(void);
bool ai_provider_set_active(uint32_t id);

/* Unified generation through active provider */
int32_t ai_provider_generate(const char *prompt, char *out_text, size_t max_out);
