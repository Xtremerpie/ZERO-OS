/* ZERO KERNEL - Central AI Intelligence Service Implementation (Milestone 8.0.0 ZERO Intelligence).
 */
#include "intelligence/ai_service.h"
#include "intelligence/ai_daemon.h"
#include "intelligence/ai_provider.h"
#include "intelligence/ai_model.h"
#include "intelligence/ai_automation.h"
#include "intelligence/ai_memory.h"
#include "intelligence/ai_engine.h"
#include "intelligence/ai_audit.h"
#include "zero/string.h"

static ai_service_state_t s_service_state = AI_SERVICE_STOPPED;
static ai_service_stats_t s_stats;

void ai_service_init(void)
{
    memset(&s_stats, 0, sizeof(s_stats));
    s_service_state = AI_SERVICE_STARTING;

    /* Initialize AI sub-modules */
    ai_provider_init();
    ai_model_init();
    ai_automation_init();
    ai_memory_init();
    ai_engine_init();
    ai_daemon_init();

    s_service_state = AI_SERVICE_RUNNING;
    s_stats.daemon_healthy = true;
    s_stats.available_models = ai_model_get_count();
}

ai_service_state_t ai_service_get_state(void)
{
    return s_service_state;
}

void ai_service_set_state(ai_service_state_t state)
{
    s_service_state = state;
}

void ai_service_get_stats(ai_service_stats_t *out_stats)
{
    if (!out_stats) return;
    *out_stats = s_stats;

    ai_daemon_status_t daemon_stat;
    ai_daemon_get_status(&daemon_stat);

    out_stats->active_sessions = daemon_stat.active_sessions;
    out_stats->available_models = ai_model_get_count();
    out_stats->daemon_healthy = daemon_stat.healthy;
}

bool ai_service_start(void)
{
    s_service_state = AI_SERVICE_RUNNING;
    return ai_daemon_start();
}

bool ai_service_stop(void)
{
    s_service_state = AI_SERVICE_STOPPED;
    return ai_daemon_stop();
}

bool ai_service_restart(void)
{
    ai_service_stop();
    return ai_service_start();
}

static bool s_gaming_mode_active = false;

void ai_service_set_gaming_mode(bool active)
{
    s_gaming_mode_active = active;
    if (active) {
        /* Under gaming mode, log event and conserve memory */
        ai_daemon_log_event("AI background inference throttled (Gaming Mode active)", false);
    } else {
        ai_daemon_log_event("AI background inference restored (Normal Profile)", false);
    }
}

bool ai_service_is_gaming_mode(void)
{
    return s_gaming_mode_active;
}
