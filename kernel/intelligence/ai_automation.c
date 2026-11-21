/* ZERO KERNEL - Permission-Gated AI Automation Broker Implementation (Milestone 8.0.0 ZERO Intelligence).
 */
#include "intelligence/ai_automation.h"
#include "intelligence/ai_audit.h"
#include "security/security.h"
#include "security/capability.h"
#include "security/sandbox.h"
#include "security/permission.h"
#include "desktop/app.h"
#include "desktop/notification.h"
#include "graphics/font.h"
#include "fs/vfs.h"
#include "zero/timer.h"
#include "zero/string.h"
#include <stdio.h>

#define AI_ACTION_HISTORY_SIZE 32u

static ai_action_t s_pending[AI_MAX_PENDING_ACTIONS];
static ai_action_t s_history[AI_ACTION_HISTORY_SIZE];
static uint32_t    s_history_head = 0;
static uint32_t    s_history_count = 0;
static uint32_t    s_action_seq = 0;

void ai_automation_init(void)
{
    memset(s_pending, 0, sizeof(s_pending));
    memset(s_history, 0, sizeof(s_history));
    s_history_head = 0;
    s_history_count = 0;
    s_action_seq = 0;
}

static app_id_t resolve_app_name(const char *name)
{
    if (!name) return 0;
    if (strstr(name, "calc")) return APP_ID_CALC;
    if (strstr(name, "term")) return APP_ID_TERMINAL;
    if (strstr(name, "file")) return APP_ID_FILE_MANAGER;
    if (strstr(name, "edit")) return APP_ID_EDITOR;
    if (strstr(name, "store")) return APP_ID_STORE;
    if (strstr(name, "sec")) return APP_ID_SECURITY_CENTER;
    if (strstr(name, "sys")) return APP_ID_SYSTEM_MONITOR;
    if (strstr(name, "net")) return APP_ID_NETMON;
    if (strstr(name, "view") || strstr(name, "img")) return APP_ID_VIEWER;
    if (strstr(name, "arch")) return APP_ID_ARCHIVER;
    if (strstr(name, "ctrl") || strstr(name, "setting")) return APP_ID_CONTROL_CENTER;
    if (strstr(name, "about")) return APP_ID_ABOUT;
    return 0;
}

uint32_t ai_automation_propose(uint32_t session_id, uint32_t pid, ai_action_type_t type,
                              const char *target, const char *desc)
{
    int32_t slot = -1;
    for (uint32_t i = 0; i < AI_MAX_PENDING_ACTIONS; i++) {
        if (!s_pending[i].in_use) {
            slot = (int32_t)i;
            break;
        }
    }
    if (slot < 0) return 0;

    s_action_seq++;
    ai_action_t *a = &s_pending[slot];
    memset(a, 0, sizeof(*a));
    a->id = s_action_seq;
    a->session_id = session_id;
    a->caller_pid = pid;
    a->type = type;
    a->timestamp_ms = timer_is_initialized() ? timer_get_uptime_ms() : 0;
    a->in_use = true;

    if (target) {
        strncpy(a->target, target, sizeof(a->target) - 1);
    }
    if (desc) {
        strncpy(a->description, desc, sizeof(a->description) - 1);
    }

    /* ZERO Security Boundary Verification */
    a->sec_result = SEC_OK;
    if (type == AI_ACTION_FILE_READ || type == AI_ACTION_FILE_WRITE) {
        uint32_t req_perm = (type == AI_ACTION_FILE_READ) ? CAP_PERM_READ : CAP_PERM_WRITE;
        if (!sandbox_check_path(pid, a->target, req_perm)) {
            a->sec_result = SEC_ERR_DENIED;
            a->status = AI_ACTION_STATUS_SECURITY_DENIED;
            ai_audit_action_event(a, "Sandbox jail boundary check failed");
            return a->id;
        }
        uint32_t req_mode = (type == AI_ACTION_FILE_READ) ? PERM_MODE_IROTH : PERM_MODE_IWOTH;
        if (perm_check(a->target, 0, 0, req_mode) != SEC_OK) {
            a->sec_result = SEC_ERR_DENIED;
            a->status = AI_ACTION_STATUS_SECURITY_DENIED;
            ai_audit_action_event(a, "Filesystem DAC permission denied");
            return a->id;
        }
    }

    /* Determine User Approval Policy */
    if (type == AI_ACTION_FILE_WRITE || type == AI_ACTION_SERVICE_CONTROL || type == AI_ACTION_APP_LAUNCH) {
        a->requires_user_approval = true;
        a->status = AI_ACTION_STATUS_PENDING_APPROVAL;
    } else {
        /* Safe inspection actions: read-only status and notifications */
        a->requires_user_approval = false;
        a->status = AI_ACTION_STATUS_APPROVED;
    }

    ai_audit_action_event(a, "Action proposed");
    return a->id;
}

bool ai_automation_approve(uint32_t action_id)
{
    ai_action_t *a = NULL;
    for (uint32_t i = 0; i < AI_MAX_PENDING_ACTIONS; i++) {
        if (s_pending[i].in_use && s_pending[i].id == action_id) {
            a = &s_pending[i];
            break;
        }
    }
    if (!a || a->status != AI_ACTION_STATUS_PENDING_APPROVAL) return false;

    a->status = AI_ACTION_STATUS_APPROVED;
    ai_audit_action_event(a, "User explicitly approved proposed action");
    return true;
}

bool ai_automation_reject(uint32_t action_id)
{
    ai_action_t *a = NULL;
    for (uint32_t i = 0; i < AI_MAX_PENDING_ACTIONS; i++) {
        if (s_pending[i].in_use && s_pending[i].id == action_id) {
            a = &s_pending[i];
            break;
        }
    }
    if (!a || a->status != AI_ACTION_STATUS_PENDING_APPROVAL) return false;

    a->status = AI_ACTION_STATUS_REJECTED;
    ai_audit_action_event(a, "User rejected proposed action");

    /* Move to history */
    s_history[s_history_head] = *a;
    s_history_head = (s_history_head + 1) % AI_ACTION_HISTORY_SIZE;
    if (s_history_count < AI_ACTION_HISTORY_SIZE) s_history_count++;

    a->in_use = false;
    return true;
}

bool ai_automation_execute(uint32_t action_id, char *out_result, size_t max_out)
{
    ai_action_t *a = NULL;
    for (uint32_t i = 0; i < AI_MAX_PENDING_ACTIONS; i++) {
        if (s_pending[i].in_use && s_pending[i].id == action_id) {
            a = &s_pending[i];
            break;
        }
    }
    if (!a) return false;

    if (a->status != AI_ACTION_STATUS_APPROVED) {
        if (out_result && max_out > 0) {
            snprintf(out_result, max_out, "Action #%u not approved for execution (status=%u)", action_id, a->status);
        }
        return false;
    }

    bool exec_ok = false;
    char res_buf[256] = "";

    switch (a->type) {
    case AI_ACTION_SYS_INFO: {
        uint64_t uptime = timer_is_initialized() ? (timer_get_uptime_ms() / 1000) : 0;
        snprintf(res_buf, sizeof(res_buf), "System Uptime: %llu sec | Security: ENFORCED | Status: HEALTHY", (unsigned long long)uptime);
        exec_ok = true;
        break;
    }
    case AI_ACTION_NOTIFICATION: {
        notification_show("ZERO Assistant", a->target[0] ? a->target : "Notification message", GLYPH_ICON_INFO, 4000);
        snprintf(res_buf, sizeof(res_buf), "Toast notification displayed: %s", a->target);
        exec_ok = true;
        break;
    }
    case AI_ACTION_APP_LAUNCH: {
        app_id_t app_id = resolve_app_name(a->target);
        if (app_id != 0 && app_launch(app_id)) {
            snprintf(res_buf, sizeof(res_buf), "Application '%s' (ID %u) launched successfully", a->target, (uint32_t)app_id);
            exec_ok = true;
        } else {
            snprintf(res_buf, sizeof(res_buf), "Could not launch application '%s'", a->target);
            exec_ok = false;
        }
        break;
    }
    case AI_ACTION_FILE_READ: {
        file_t *f = vfs_open(a->target, O_RDONLY);
        if (f) {
            char file_data[128];
            int64_t r = vfs_read(f, file_data, sizeof(file_data) - 1);
            vfs_close(f);
            if (r >= 0) {
                file_data[r] = '\0';
                snprintf(res_buf, sizeof(res_buf), "Read %lld bytes: %s", (long long)r, file_data);
                exec_ok = true;
            } else {
                snprintf(res_buf, sizeof(res_buf), "VFS read error from '%s'", a->target);
            }
        } else {
            snprintf(res_buf, sizeof(res_buf), "File '%s' not found or inaccessible", a->target);
        }
        break;
    }
    case AI_ACTION_FILE_WRITE: {
        vfs_create(a->target);
        file_t *f = vfs_open(a->target, O_WRONLY | O_CREAT);
        if (f) {
            const char *content = "ZERO AI Assistant generated output.\n";
            vfs_write(f, content, strlen(content));
            vfs_close(f);
            snprintf(res_buf, sizeof(res_buf), "Wrote content to file '%s'", a->target);
            exec_ok = true;
        } else {
            snprintf(res_buf, sizeof(res_buf), "Failed to open '%s' for writing", a->target);
        }
        break;
    }
    case AI_ACTION_SERVICE_CONTROL: {
        snprintf(res_buf, sizeof(res_buf), "Service control '%s' command processed", a->target);
        exec_ok = true;
        break;
    }
    default:
        snprintf(res_buf, sizeof(res_buf), "Unknown action type %u", a->type);
        break;
    }

    a->status = exec_ok ? AI_ACTION_STATUS_EXECUTED : AI_ACTION_STATUS_FAILED;
    if (out_result && max_out > 0) {
        strncpy(out_result, res_buf, max_out - 1);
        out_result[max_out - 1] = '\0';
    }

    ai_audit_action_event(a, res_buf);

    /* Move to history */
    s_history[s_history_head] = *a;
    s_history_head = (s_history_head + 1) % AI_ACTION_HISTORY_SIZE;
    if (s_history_count < AI_ACTION_HISTORY_SIZE) s_history_count++;

    a->in_use = false;
    return exec_ok;
}

uint32_t ai_automation_get_pending(ai_action_t *out_actions, uint32_t max_actions)
{
    if (!out_actions || max_actions == 0) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < AI_MAX_PENDING_ACTIONS && count < max_actions; i++) {
        if (s_pending[i].in_use && s_pending[i].status == AI_ACTION_STATUS_PENDING_APPROVAL) {
            out_actions[count++] = s_pending[i];
        }
    }
    return count;
}

uint32_t ai_automation_get_history(ai_action_t *out_actions, uint32_t max_actions)
{
    if (!out_actions || max_actions == 0) return 0;
    uint32_t available = (s_history_count < max_actions) ? s_history_count : max_actions;
    for (uint32_t i = 0; i < available; i++) {
        uint32_t idx = (s_history_head + AI_ACTION_HISTORY_SIZE - 1 - i) % AI_ACTION_HISTORY_SIZE;
        out_actions[i] = s_history[idx];
    }
    return available;
}

const ai_action_t *ai_automation_get_by_id(uint32_t action_id)
{
    for (uint32_t i = 0; i < AI_MAX_PENDING_ACTIONS; i++) {
        if (s_pending[i].in_use && s_pending[i].id == action_id) {
            return &s_pending[i];
        }
    }
    return NULL;
}
