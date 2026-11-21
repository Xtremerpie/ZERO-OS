/* ZERO KERNEL - AI Persistent Memory & History Implementation (Milestone 8.0.0 ZERO Intelligence).
 */
#include "intelligence/ai_memory.h"
#include "fs/vfs.h"
#include "zero/string.h"
#include <stdio.h>

static ai_user_prefs_t s_cached_prefs;

void ai_memory_init(void)
{
    /* Ensure storage hierarchy exists */
    (void)vfs_mkdir("/data");
    (void)vfs_mkdir("/data/ai");
    (void)vfs_mkdir("/data/ai/history");

    /* Default preferences */
    memset(&s_cached_prefs, 0, sizeof(s_cached_prefs));
    strcpy(s_cached_prefs.default_model, "zero-micro-cpu");
    s_cached_prefs.max_context_tokens = 1024;
    s_cached_prefs.auto_approve_safe_info = true;
    s_cached_prefs.confirm_sensitive_writes = true;
    s_cached_prefs.notifications_enabled = true;
    s_cached_prefs.active_provider_id = 1;

    (void)ai_memory_load_prefs(&s_cached_prefs);
}

bool ai_memory_load_prefs(ai_user_prefs_t *out_prefs)
{
    if (!out_prefs) return false;

    file_t *f = vfs_open(AI_MEMORY_PREFS_FILE, O_RDONLY);
    if (!f) {
        *out_prefs = s_cached_prefs;
        return true;
    }

    char buf[256];
    int64_t bytes = vfs_read(f, buf, sizeof(buf) - 1);
    vfs_close(f);

    if (bytes > 0) {
        buf[bytes] = '\0';
        /* Simple key-value parse */
        char *line = buf;
        while (line && *line) {
            char *next = strchr(line, '\n');
            if (next) *next = '\0';

            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *key = line;
                char *val = eq + 1;
                if (strcmp(key, "default_model") == 0) {
                    strncpy(s_cached_prefs.default_model, val, sizeof(s_cached_prefs.default_model) - 1);
                } else if (strcmp(key, "notifications") == 0) {
                    s_cached_prefs.notifications_enabled = (strcmp(val, "1") == 0);
                }
            }
            line = next ? next + 1 : NULL;
        }
    }

    *out_prefs = s_cached_prefs;
    return true;
}

bool ai_memory_save_prefs(const ai_user_prefs_t *prefs)
{
    if (!prefs) return false;
    s_cached_prefs = *prefs;

    file_t *f = vfs_open(AI_MEMORY_PREFS_FILE, O_WRONLY | O_CREAT);
    if (!f) return false;

    char buf[256];
    int len = snprintf(buf, sizeof(buf), "default_model=%s\nnotifications=%d\nprovider=%u\n",
                       prefs->default_model, prefs->notifications_enabled ? 1 : 0, prefs->active_provider_id);
    if (len > 0) {
        vfs_write(f, buf, (size_t)len);
    }
    vfs_close(f);
    return true;
}

bool ai_memory_persist_session(const ai_session_t *session)
{
    if (!session) return false;

    char path[64];
    snprintf(path, sizeof(path), "/data/ai/history/session_%u.log", session->id);

    file_t *f = vfs_open(path, O_WRONLY | O_CREAT);
    if (!f) return false;

    for (uint32_t i = 0; i < session->msg_count; i++) {
        char line[320];
        const char *role = (session->messages[i].role == AI_MSG_ROLE_USER) ? "USER" : "ASSISTANT";
        int len = snprintf(line, sizeof(line), "[%s]: %s\n", role, session->messages[i].text);
        if (len > 0) {
            vfs_write(f, line, (size_t)len);
        }
    }
    vfs_close(f);
    return true;
}

bool ai_memory_clear_all_history(void)
{
    for (uint32_t i = 1; i <= AI_MAX_SESSIONS; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/data/ai/history/session_%u.log", i);
        vfs_unlink(path);
    }
    return true;
}
