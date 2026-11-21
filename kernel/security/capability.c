/* ZERO KERNEL - Capability-Based Security Model Implementation (Milestone 7.0.0 ZERO Security).
 */
#include "security/capability.h"
#include "security/audit.h"
#include "zero/string.h"

typedef struct cap_entry {
    cap_handle_t handle;
    cap_type_t   type;
    char         resource[CAP_MAX_RESOURCE_LEN];
    uint32_t     perms;
    uint32_t     pid;
    bool         in_use;
    bool         revoked;
} cap_entry_t;

static cap_entry_t s_cap_table[CAP_MAX_ENTRIES];
static uint32_t    s_handle_seq = 100u;
static uint32_t    s_active_count = 0u;
static uint32_t    s_revoked_count = 0u;

static void safe_strcpy(char *dst, const char *src, size_t max)
{
    size_t i = 0;
    while (src && src[i] && i + 1 < max) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void capability_init(void)
{
    memset(s_cap_table, 0, sizeof(s_cap_table));
    s_handle_seq = 100u;
    s_active_count = 0u;
    s_revoked_count = 0u;

    /* Create default system baseline capabilities for PID 0 (kernel/init) */
    cap_create(0, CAP_TYPE_SYSTEM, "system.all", CAP_PERM_READ | CAP_PERM_WRITE | CAP_PERM_EXEC | CAP_PERM_ADMIN);
    cap_create(0, CAP_TYPE_DIR, "/", CAP_PERM_READ | CAP_PERM_WRITE | CAP_PERM_EXEC);
    cap_create(0, CAP_TYPE_NET_SOCKET, "net.socket.all", CAP_PERM_READ | CAP_PERM_WRITE | CAP_PERM_ADMIN);
}

cap_handle_t cap_create(uint32_t pid, cap_type_t type, const char *resource, uint32_t perms)
{
    if (type == CAP_TYPE_NONE || !resource) return CAP_INVALID_HANDLE;

    /* Find free slot */
    int slot = -1;
    for (size_t i = 0; i < CAP_MAX_ENTRIES; i++) {
        if (!s_cap_table[i].in_use) {
            slot = (int)i;
            break;
        }
    }
    if (slot < 0) return CAP_INVALID_HANDLE;

    s_handle_seq++;
    /* Construct opaque 32-bit handle combining sequence and slot index */
    cap_handle_t handle = ((s_handle_seq & 0xFFFFFu) << 12) | ((uint32_t)slot & 0xFFFu);

    cap_entry_t *entry = &s_cap_table[slot];
    entry->handle = handle;
    entry->type = type;
    safe_strcpy(entry->resource, resource, sizeof(entry->resource));
    entry->perms = perms;
    entry->pid = pid;
    entry->in_use = true;
    entry->revoked = false;

    s_active_count++;

    audit_log(AUDIT_EVENT_CAP_GRANT, pid, resource, true, "Capability created and granted");
    return handle;
}

sec_result_t cap_validate(cap_handle_t handle, uint32_t pid, cap_type_t type, uint32_t req_perms)
{
    if (handle == CAP_INVALID_HANDLE) return SEC_ERR_INVALID_CAP;

    uint32_t slot = handle & 0xFFFu;
    if (slot >= CAP_MAX_ENTRIES) return SEC_ERR_INVALID_CAP;

    cap_entry_t *entry = &s_cap_table[slot];
    if (!entry->in_use || entry->handle != handle) {
        return SEC_ERR_INVALID_CAP;
    }

    if (entry->revoked) {
        audit_log(AUDIT_EVENT_PERM_DENY, pid, entry->resource, false, "Denied: capability has been revoked");
        return SEC_ERR_REVOKED;
    }

    /* Process isolation: PID 0 (kernel) is permitted; otherwise process must own capability */
    if (entry->pid != 0 && entry->pid != pid) {
        audit_log(AUDIT_EVENT_PERM_DENY, pid, entry->resource, false, "Denied: cross-process capability access attempt");
        return SEC_ERR_DENIED;
    }

    if (type != CAP_TYPE_NONE && entry->type != type) {
        return SEC_ERR_INVALID_CAP;
    }

    if ((entry->perms & req_perms) != req_perms) {
        audit_log(AUDIT_EVENT_PERM_DENY, pid, entry->resource, false, "Denied: insufficient permissions on capability");
        return SEC_ERR_DENIED;
    }

    return SEC_OK;
}

sec_result_t cap_check_resource(uint32_t pid, cap_type_t type, const char *resource, uint32_t req_perms)
{
    if (!resource) return SEC_ERR_BAD_ARG;

    /* PID 0 always has full authority */
    if (pid == 0) return SEC_OK;

    for (size_t i = 0; i < CAP_MAX_ENTRIES; i++) {
        cap_entry_t *entry = &s_cap_table[i];
        if (entry->in_use && !entry->revoked && (entry->pid == pid || entry->pid == 0)) {
            if (entry->type == type && (entry->perms & req_perms) == req_perms) {
                /* Check if resource matches or is a prefix directory */
                if (strcmp(entry->resource, resource) == 0 ||
                    strcmp(entry->resource, "*") == 0 ||
                    strcmp(entry->resource, "/") == 0) {
                    return SEC_OK;
                }
                /* Prefix check for directory capabilities (e.g. /data/apps/app1/) */
                size_t prefix_len = strlen(entry->resource);
                if (strncmp(entry->resource, resource, prefix_len) == 0) {
                    return SEC_OK;
                }
            }
        }
    }

    return SEC_ERR_DENIED;
}

cap_handle_t cap_duplicate_reduced(cap_handle_t src_handle, uint32_t target_pid, uint32_t reduced_perms)
{
    if (src_handle == CAP_INVALID_HANDLE) return CAP_INVALID_HANDLE;

    uint32_t slot = src_handle & 0xFFFu;
    if (slot >= CAP_MAX_ENTRIES) return CAP_INVALID_HANDLE;

    cap_entry_t *src = &s_cap_table[slot];
    if (!src->in_use || src->handle != src_handle || src->revoked) {
        return CAP_INVALID_HANDLE;
    }

    /* Invariant: Duplication can NEVER increase permissions */
    if ((reduced_perms & ~src->perms) != 0) {
        audit_log(AUDIT_EVENT_PERM_DENY, target_pid, src->resource, false,
                  "Denied: attempt to elevate permissions during capability delegation");
        return CAP_INVALID_HANDLE;
    }

    return cap_create(target_pid, src->type, src->resource, reduced_perms);
}

sec_result_t cap_revoke(cap_handle_t handle)
{
    if (handle == CAP_INVALID_HANDLE) return SEC_ERR_INVALID_CAP;

    uint32_t slot = handle & 0xFFFu;
    if (slot >= CAP_MAX_ENTRIES) return SEC_ERR_INVALID_CAP;

    cap_entry_t *entry = &s_cap_table[slot];
    if (!entry->in_use || entry->handle != handle) {
        return SEC_ERR_INVALID_CAP;
    }

    if (!entry->revoked) {
        entry->revoked = true;
        s_revoked_count++;
        if (s_active_count > 0) s_active_count--;
        audit_log(AUDIT_EVENT_CAP_REVOKE, entry->pid, entry->resource, true, "Capability revoked");
    }

    return SEC_OK;
}

sec_result_t cap_destroy(cap_handle_t handle)
{
    if (handle == CAP_INVALID_HANDLE) return SEC_ERR_INVALID_CAP;

    uint32_t slot = handle & 0xFFFu;
    if (slot >= CAP_MAX_ENTRIES) return SEC_ERR_INVALID_CAP;

    cap_entry_t *entry = &s_cap_table[slot];
    if (!entry->in_use || entry->handle != handle) {
        return SEC_ERR_INVALID_CAP;
    }

    if (!entry->revoked && s_active_count > 0) {
        s_active_count--;
    }
    memset(entry, 0, sizeof(*entry));
    return SEC_OK;
}

uint32_t cap_enumerate_process(uint32_t pid, cap_desc_t *out_caps, uint32_t max_caps)
{
    if (!out_caps || max_caps == 0) return 0;

    uint32_t count = 0;
    for (size_t i = 0; i < CAP_MAX_ENTRIES && count < max_caps; i++) {
        cap_entry_t *e = &s_cap_table[i];
        if (e->in_use && e->pid == pid) {
            out_caps[count].handle = e->handle;
            out_caps[count].type = e->type;
            safe_strcpy(out_caps[count].resource, e->resource, sizeof(out_caps[count].resource));
            out_caps[count].perms = e->perms;
            out_caps[count].pid = e->pid;
            out_caps[count].valid = !e->revoked;
            out_caps[count].revoked = e->revoked;
            count++;
        }
    }
    return count;
}

uint32_t capability_get_active_count(void)
{
    return s_active_count;
}

uint32_t capability_get_revoked_count(void)
{
    return s_revoked_count;
}
