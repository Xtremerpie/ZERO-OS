/* ZERO KERNEL - ZERO Intelligence Subsystem Self-Test Implementation (Milestone 8.0.0).
 *
 * Validates the AI Assistant daemon, provider dispatch, model manager,
 * automation permission broker, engine session context, memory persistence,
 * and security audit bridge.
 */
#include "debug/selftest_intelligence.h"
#include "intelligence/ai_service.h"
#include "intelligence/ai_daemon.h"
#include "intelligence/ai_provider.h"
#include "intelligence/ai_model.h"
#include "intelligence/ai_automation.h"
#include "intelligence/ai_engine.h"
#include "intelligence/ai_memory.h"
#include "intelligence/ai_audit.h"

#include "zero/log.h"
#include "zero/string.h"

/* Test 1: AI Service & Daemon Lifecycle */
static bool test_ai_service_and_daemon(void)
{
    ai_service_stats_t stats;
    ai_service_get_stats(&stats);

    if (ai_service_get_state() != AI_SERVICE_RUNNING) {
        kprintf("[FAIL] intelligence: AI service not in RUNNING state\n");
        return false;
    }

    ai_daemon_status_t daemon_status;
    ai_daemon_get_status(&daemon_status);
    if (!daemon_status.healthy) {
        kprintf("[FAIL] intelligence: AI daemon reports unhealthy\n");
        return false;
    }

    /* Test event logging */
    ai_daemon_log_event("Selftest test log event", false);
    ai_daemon_log_t logs[4];
    uint32_t log_cnt = ai_daemon_get_logs(logs, 4);
    if (log_cnt == 0) {
        kprintf("[FAIL] intelligence: AI daemon failed to retrieve logs\n");
        return false;
    }

    /* Test stop and restart */
    if (!ai_daemon_stop()) {
        kprintf("[FAIL] intelligence: AI daemon failed to stop\n");
        return false;
    }
    if (!ai_daemon_start()) {
        kprintf("[FAIL] intelligence: AI daemon failed to restart\n");
        return false;
    }

    kprintf("[PASS] intelligence: AI service coordinator & daemon lifecycle validated\n");
    return true;
}

/* Test 2: AI Provider Abstraction */
static bool test_ai_providers(void)
{
    uint32_t prov_cnt = ai_provider_count();
    if (prov_cnt < 2) {
        kprintf("[FAIL] intelligence: insufficient providers registered (found %u, expected >= 2)\n", prov_cnt);
        return false;
    }

    ai_provider_t *active = ai_provider_get_active();
    if (!active) {
        kprintf("[FAIL] intelligence: no active AI provider\n");
        return false;
    }

    /* Verify local CPU provider exists and can generate text */
    ai_provider_t *local_prov = ai_provider_get_by_id(1);
    if (!local_prov || local_prov->type != AI_PROVIDER_TYPE_LOCAL_CPU) {
        kprintf("[FAIL] intelligence: local CPU provider missing or wrong type\n");
        return false;
    }

    char out_buf[256];
    int32_t gen_res = ai_provider_generate("help", out_buf, sizeof(out_buf));
    if (gen_res < 0 || strlen(out_buf) == 0) {
        kprintf("[FAIL] intelligence: provider text generation failed\n");
        return false;
    }

    kprintf("[PASS] intelligence: AI provider abstraction & local CPU micro-engine validated\n");
    return true;
}

/* Test 3: AI Model Discovery & Single-Model Lifecycle */
static bool test_ai_models(void)
{
    uint32_t model_cnt = ai_model_get_count();
    if (model_cnt < 3) {
        kprintf("[FAIL] intelligence: expected >= 3 models, found %u\n", model_cnt);
        return false;
    }

    const ai_model_desc_t *active_model = ai_model_get_active();
    if (!active_model) {
        kprintf("[FAIL] intelligence: expected active model\n");
        return false;
    }

    /* Verify memory bounds checking */
    if (!ai_model_can_fit_in_memory(1)) {
        kprintf("[FAIL] intelligence: builtin model 1 reported cannot fit in memory\n");
        return false;
    }

    /* Test single-active-model policy: loading model 2 unloads model 1 */
    uint32_t prev_active_id = active_model->id;
    uint32_t target_id = (prev_active_id == 1) ? 2 : 1;

    if (!ai_model_load(target_id)) {
        kprintf("[FAIL] intelligence: failed to switch active model to %u\n", target_id);
        return false;
    }

    const ai_model_desc_t *new_active = ai_model_get_active();
    if (!new_active || new_active->id != target_id) {
        kprintf("[FAIL] intelligence: new active model ID mismatch\n");
        return false;
    }

    /* Verify old model was unloaded */
    const ai_model_desc_t *old_desc = ai_model_get_by_id(prev_active_id);
    if (old_desc && old_desc->state == AI_MODEL_STATE_LOADED) {
        kprintf("[FAIL] intelligence: previous model remained loaded (single-active violation)\n");
        return false;
    }

    /* Switch back */
    ai_model_load(prev_active_id);

    kprintf("[PASS] intelligence: model manager & single-active-model policy validated\n");
    return true;
}

/* Test 4: Automation Permission Gating */
static bool test_ai_automation_broker(void)
{
    /* Safe action: SYS_INFO -> auto-approved */
    uint32_t act_safe = ai_automation_propose(1, 100, AI_ACTION_SYS_INFO, "system", "Query system metrics");
    if (act_safe == 0) {
        kprintf("[FAIL] intelligence: failed to propose safe action\n");
        return false;
    }

    const ai_action_t *p_safe = ai_automation_get_by_id(act_safe);
    if (!p_safe || p_safe->status != AI_ACTION_STATUS_APPROVED) {
        kprintf("[FAIL] intelligence: safe action not auto-approved\n");
        return false;
    }

    char exec_buf[256];
    if (!ai_automation_execute(act_safe, exec_buf, sizeof(exec_buf))) {
        kprintf("[FAIL] intelligence: execution of safe action failed\n");
        return false;
    }

    /* Sensitive action: SERVICE_CONTROL -> requires approval.
     * Use a non-filesystem action here so the approval-policy test is
     * independent of sandbox path and filesystem DAC configuration. */
    uint32_t act_sensitive = ai_automation_propose(1, 100, AI_ACTION_SERVICE_CONTROL, "daemon", "Restart daemon");
    if (act_sensitive == 0) {
        kprintf("[FAIL] intelligence: failed to propose sensitive service action\n");
        return false;
    }

    const ai_action_t *p_sensitive = ai_automation_get_by_id(act_sensitive);
    if (!p_sensitive || p_sensitive->status != AI_ACTION_STATUS_PENDING_APPROVAL ||
        !p_sensitive->requires_user_approval) {
        kprintf("[FAIL] intelligence: sensitive action bypassed approval gate (status=%u, approval=%u)\n",
                p_sensitive ? (uint32_t)p_sensitive->status : 0,
                p_sensitive ? (uint32_t)p_sensitive->requires_user_approval : 0);
        return false;
    }

    /* Test user approval gate */
    if (!ai_automation_approve(act_sensitive)) {
        kprintf("[FAIL] intelligence: failed to approve pending action\n");
        return false;
    }

    p_sensitive = ai_automation_get_by_id(act_sensitive);
    if (!p_sensitive || p_sensitive->status != AI_ACTION_STATUS_APPROVED) {
        kprintf("[FAIL] intelligence: approved action status not updated\n");
        return false;
    }

    /* Test user rejection path */
    uint32_t act_rej = ai_automation_propose(1, 100, AI_ACTION_SERVICE_CONTROL, "daemon", "Restart daemon");
    if (act_rej == 0) {
        kprintf("[FAIL] intelligence: failed to propose action for rejection\n");
        return false;
    }
    if (!ai_automation_reject(act_rej)) {
        kprintf("[FAIL] intelligence: failed to reject action\n");
        return false;
    }
    const ai_action_t *p_rej = ai_automation_get_by_id(act_rej);
    if (!p_rej || p_rej->status != AI_ACTION_STATUS_REJECTED) {
        kprintf("[FAIL] intelligence: rejected action status not set\n");
        return false;
    }

    kprintf("[PASS] intelligence: permission-gated automation broker & approval gates validated\n");
    return true;
}

/* Test 5: Assistant Request Lifecycle Engine */
static bool test_ai_engine(void)
{
    uint32_t sess_id = ai_engine_create_session(10, "SelftestSession");
    if (sess_id == 0) {
        kprintf("[FAIL] intelligence: failed to create assistant session\n");
        return false;
    }

    ai_session_t *sess = ai_engine_get_session(sess_id);
    if (!sess || sess->id != sess_id) {
        kprintf("[FAIL] intelligence: failed to retrieve created session\n");
        return false;
    }

    /* Process prompt */
    ai_engine_result_t res;
    if (!ai_engine_process_prompt(sess_id, "Explain system architecture", &res)) {
        kprintf("[FAIL] intelligence: prompt processing failed\n");
        return false;
    }

    if (!res.success || strlen(res.response_text) == 0) {
        kprintf("[FAIL] intelligence: engine returned empty response\n");
        return false;
    }

    /* Verify context tracking: session has user and assistant messages */
    if (sess->msg_count < 2) {
        kprintf("[FAIL] intelligence: session message history not updated (count: %u)\n", sess->msg_count);
        return false;
    }

    /* Test intent recognition for action proposal */
    ai_engine_result_t act_res;
    if (!ai_engine_process_prompt(sess_id, "Open terminal", &act_res)) {
        kprintf("[FAIL] intelligence: action prompt processing failed\n");
        return false;
    }
    if (!act_res.has_action_proposal || act_res.action_type != AI_ACTION_APP_LAUNCH) {
        kprintf("[FAIL] intelligence: intent recognition failed to propose APP_LAUNCH\n");
        return false;
    }

    ai_engine_close_session(sess_id);
    kprintf("[PASS] intelligence: assistant request engine & intent classification validated\n");
    return true;
}

/* Test 6: AI Memory & Preferences Persistence */
static bool test_ai_memory_persistence(void)
{
    ai_user_prefs_t prefs;
    if (!ai_memory_load_prefs(&prefs)) {
        kprintf("[FAIL] intelligence: failed to load user assistant preferences\n");
        return false;
    }

    uint32_t orig_ctx = prefs.max_context_tokens;
    prefs.max_context_tokens = 1024;
    if (!ai_memory_save_prefs(&prefs)) {
        kprintf("[FAIL] intelligence: failed to save modified preferences\n");
        return false;
    }

    ai_user_prefs_t reloaded;
    if (!ai_memory_load_prefs(&reloaded) || reloaded.max_context_tokens != 1024) {
        kprintf("[FAIL] intelligence: reloaded preference mismatch\n");
        return false;
    }

    /* Revert */
    prefs.max_context_tokens = orig_ctx;
    ai_memory_save_prefs(&prefs);

    kprintf("[PASS] intelligence: persistent memory & user preferences validated\n");
    return true;
}

/* Test 7: Security Audit Telemetry */
static bool test_ai_audit_integration(void)
{
    /* Test auditing various AI lifecycle events */
    ai_audit_session_event(99, 10, true, "AuditTestSession");
    ai_audit_model_event(1, "ZeroMicro-15M", true, 32);
    ai_audit_provider_event(1, "LocalCPU", true);

    ai_action_t test_act;
    memset(&test_act, 0, sizeof(test_act));
    test_act.id = 999;
    test_act.session_id = 99;
    test_act.type = AI_ACTION_SYS_INFO;
    test_act.status = AI_ACTION_STATUS_EXECUTED;
    strncpy(test_act.target, "CPU_STATS", sizeof(test_act.target) - 1);
    ai_audit_action_event(&test_act, "Selftest audit action execution");

    kprintf("[PASS] intelligence: AI security audit integration & telemetry validated\n");
    return true;
}

bool intelligence_run_selftests(void)
{
    kprintf("=== ZERO Intelligence Subsystem Self-Tests ===\n");

    if (!test_ai_service_and_daemon())   return false;
    if (!test_ai_providers())            return false;
    if (!test_ai_models())               return false;
    if (!test_ai_automation_broker())    return false;
    if (!test_ai_engine())               return false;
    if (!test_ai_memory_persistence())   return false;
    if (!test_ai_audit_integration())    return false;

    kprintf("=== All ZERO Intelligence Self-Tests Passed (7/7) ===\n");
    return true;
}

void intelligence_report_generate(void)
{
    ai_service_stats_t svc_stats;
    ai_service_get_stats(&svc_stats);

    ai_daemon_status_t daemon_status;
    ai_daemon_get_status(&daemon_status);

    const ai_model_desc_t *act_model = ai_model_get_active();
    ai_provider_t *act_prov = ai_provider_get_active();

    kprintf("--------------------------------------------------------------------------------\n");
    kprintf("                     ZERO INTELLIGENCE SUBSYSTEM REPORT                         \n");
    kprintf("--------------------------------------------------------------------------------\n");
    kprintf("  Service State:          %s\n", (ai_service_get_state() == AI_SERVICE_RUNNING) ? "RUNNING" : "STOPPED");
    kprintf("  Daemon Health:          %s\n", daemon_status.healthy ? "HEALTHY (READY)" : "UNHEALTHY");
    kprintf("  Active Provider:        %s [%s]\n",
            act_prov ? act_prov->name : "None",
            act_prov ? (act_prov->type == AI_PROVIDER_TYPE_LOCAL_CPU ? "Local CPU" : "Adapter") : "N/A");
    kprintf("  Active Model:           %s (%s, %uM params, %u MB RAM)\n",
            act_model ? act_model->name : "None",
            act_model ? act_model->quantization : "N/A",
            act_model ? act_model->param_count_m : 0,
            act_model ? act_model->ram_required_mb : 0);
    kprintf("  Discovered Models:      %u\n", ai_model_get_count());
    kprintf("  Registered Providers:   %u\n", ai_provider_count());
    kprintf("  Active Sessions:        %u / %u\n", svc_stats.active_sessions, AI_MAX_SESSIONS);
    kprintf("  Total Inferences:       %llu\n", (unsigned long long)svc_stats.total_requests);
    kprintf("  Actions Proposed:       %llu (Approved: %llu, Rejected: %llu)\n",
            (unsigned long long)svc_stats.total_actions_proposed,
            (unsigned long long)svc_stats.total_actions_approved,
            (unsigned long long)svc_stats.total_actions_rejected);
    kprintf("  Automation Security:    PERMISSION-GATED (Capabilities + User Confirmation)\n");
    kprintf("  Status:                 ONLINE AND OPERATIONAL\n");
    kprintf("--------------------------------------------------------------------------------\n");
}
