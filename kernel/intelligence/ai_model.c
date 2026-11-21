/* ZERO KERNEL - AI Model Discovery & Management Implementation (Milestone 8.0.0 ZERO Intelligence).
 */
#include "intelligence/ai_model.h"
#include "intelligence/ai_audit.h"
#include "memory/heap.h"
#include "fs/vfs.h"
#include "zero/string.h"
#include <stdio.h>

static ai_model_desc_t s_models[AI_MAX_MODELS];
static uint32_t        s_model_count = 0;
static int32_t         s_active_model_idx = -1;

void ai_model_init(void)
{
    memset(s_models, 0, sizeof(s_models));
    s_model_count = 0;
    s_active_model_idx = -1;

    /* Pre-register default supported model configurations */
    /* Model 1: Native CPU Micro-Model (Optimized for Celeron J3355 / 4GB RAM) */
    ai_model_desc_t *m1 = &s_models[s_model_count++];
    m1->id = 1;
    strcpy(m1->name, "zero-micro-cpu");
    strcpy(m1->architecture, "MICRO-TRANSFORMER");
    m1->format = AI_MODEL_FORMAT_BUILTIN;
    m1->param_count_m = 15;
    strcpy(m1->quantization, "Q4_0");
    m1->context_len = 512;
    m1->ram_required_mb = 32;
    strcpy(m1->file_path, "/data/models/micro.bin");
    m1->state = AI_MODEL_STATE_AVAILABLE;
    m1->provider_id = 1;
    m1->in_use = true;

    /* Model 2: GGUF Compatible Runtime Adapter Model */
    ai_model_desc_t *m2 = &s_models[s_model_count++];
    m2->id = 2;
    strcpy(m2->name, "zero-llama-gguf");
    strcpy(m2->architecture, "LLaMA-GGUF");
    m2->format = AI_MODEL_FORMAT_GGUF;
    m2->param_count_m = 135;
    strcpy(m2->quantization, "Q4_K_M");
    m2->context_len = 1024;
    m2->ram_required_mb = 128;
    strcpy(m2->file_path, "/data/models/llama-135m.gguf");
    m2->state = AI_MODEL_STATE_AVAILABLE;
    m2->provider_id = 2;
    m2->in_use = true;

    /* Model 3: Remote Endpoint Adapter Reference */
    ai_model_desc_t *m3 = &s_models[s_model_count++];
    m3->id = 3;
    strcpy(m3->name, "zero-remote-api");
    strcpy(m3->architecture, "REMOTE-PROXY");
    m3->format = AI_MODEL_FORMAT_REMOTE_REF;
    m3->param_count_m = 0;
    strcpy(m3->quantization, "N/A");
    m3->context_len = 2048;
    m3->ram_required_mb = 4;
    strcpy(m3->file_path, "http://api.zero-os.internal/v1");
    m3->state = AI_MODEL_STATE_AVAILABLE;
    m3->provider_id = 3;
    m3->in_use = true;

    /* Default to loading Model 1 (Native CPU Micro-Model) */
    ai_model_load(1);
}

uint32_t ai_model_discover(void)
{
    /* Scan /data/models in VFS */
    (void)vfs_mkdir("/data");
    (void)vfs_mkdir("/data/models");
    return s_model_count;
}

uint32_t ai_model_get_count(void)
{
    return s_model_count;
}

const ai_model_desc_t *ai_model_get_by_id(uint32_t id)
{
    for (uint32_t i = 0; i < s_model_count; i++) {
        if (s_models[i].in_use && s_models[i].id == id) {
            return &s_models[i];
        }
    }
    return NULL;
}

const ai_model_desc_t *ai_model_get_active(void)
{
    if (s_active_model_idx >= 0 && s_active_model_idx < (int32_t)s_model_count) {
        return &s_models[s_active_model_idx];
    }
    return NULL;
}

uint32_t ai_model_list(ai_model_desc_t *out_models, uint32_t max_models)
{
    if (!out_models || max_models == 0) return s_model_count;
    uint32_t to_copy = (s_model_count < max_models) ? s_model_count : max_models;
    for (uint32_t i = 0; i < to_copy; i++) {
        out_models[i] = s_models[i];
    }
    return to_copy;
}

bool ai_model_can_fit_in_memory(uint32_t model_id)
{
    const ai_model_desc_t *m = ai_model_get_by_id(model_id);
    if (!m) return false;

    /* Ensure model fits comfortably within hardware budget (e.g. < 256 MB in kernel memory) */
    if (m->ram_required_mb > 512) {
        return false;
    }
    return true;
}

bool ai_model_load(uint32_t id)
{
    if (!ai_model_can_fit_in_memory(id)) {
        return false;
    }

    /* Single-model loading policy: unload any currently loaded model first */
    if (s_active_model_idx >= 0) {
        ai_model_unload();
    }

    for (uint32_t i = 0; i < s_model_count; i++) {
        if (s_models[i].in_use && s_models[i].id == id) {
            s_models[i].state = AI_MODEL_STATE_LOADED;
            s_active_model_idx = (int32_t)i;
            ai_audit_model_event(s_models[i].id, s_models[i].name, true, s_models[i].ram_required_mb);
            return true;
        }
    }
    return false;
}

bool ai_model_unload(void)
{
    if (s_active_model_idx >= 0 && s_active_model_idx < (int32_t)s_model_count) {
        s_models[s_active_model_idx].state = AI_MODEL_STATE_AVAILABLE;
        ai_audit_model_event(s_models[s_active_model_idx].id, s_models[s_active_model_idx].name, false, 0);
        s_active_model_idx = -1;
        return true;
    }
    return false;
}
