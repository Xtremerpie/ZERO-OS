/* ZERO KERNEL - AI Model Discovery & Management (Milestone 8.0.0 ZERO Intelligence).
 *
 * Implements model discovery, metadata validation, memory footprint estimation,
 * and single-model load/unload lifecycle management tailored to lightweight hardware.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define AI_MAX_MODELS       8u
#define AI_MODEL_NAME_LEN   32u
#define AI_MODEL_PATH_LEN   64u

typedef enum {
    AI_MODEL_FORMAT_BUILTIN = 0,
    AI_MODEL_FORMAT_GGUF,
    AI_MODEL_FORMAT_MICRO_WEIGHTS,
    AI_MODEL_FORMAT_REMOTE_REF
} ai_model_format_t;

typedef enum {
    AI_MODEL_STATE_AVAILABLE = 0,
    AI_MODEL_STATE_LOADING,
    AI_MODEL_STATE_LOADED,
    AI_MODEL_STATE_UNLOADING,
    AI_MODEL_STATE_ERROR
} ai_model_state_t;

typedef struct ai_model_desc {
    uint32_t          id;
    char              name[AI_MODEL_NAME_LEN];
    char              architecture[24];
    ai_model_format_t format;
    uint32_t          param_count_m;  /* In millions e.g. 15M, 135M */
    char              quantization[12];/* e.g. "Q4_0", "Q8_0", "FP16" */
    uint32_t          context_len;    /* e.g. 512, 1024, 2048 */
    uint32_t          ram_required_mb;/* Memory estimate */
    char              file_path[AI_MODEL_PATH_LEN];
    ai_model_state_t  state;
    uint32_t          provider_id;
    bool              in_use;
} ai_model_desc_t;

/* Initialize model manager and discover available models */
void ai_model_init(void);

/* Discover models in local storage (/data/models) */
uint32_t ai_model_discover(void);

/* Query available model descriptors */
uint32_t ai_model_get_count(void);
const ai_model_desc_t *ai_model_get_by_id(uint32_t id);
const ai_model_desc_t *ai_model_get_active(void);
uint32_t ai_model_list(ai_model_desc_t *out_models, uint32_t max_models);

/* Load and unload active model */
bool ai_model_load(uint32_t id);
bool ai_model_unload(void);

/* Check if enough system memory exists to load candidate model */
bool ai_model_can_fit_in_memory(uint32_t model_id);
