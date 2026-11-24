/* ZERO KERNEL - Filesystem Discretionary Access Control Implementation (Milestone 7.0.0 ZERO Security).
 */
#include "security/permission.h"
#include "security/audit.h"
#include "zero/string.h"

static file_perm_t s_perm_table[PERM_MAX_ENTRIES];

static void safe_strcpy(char *dst, const char *src, size_t max)
{
    size_t i = 0;
    while (src && src[i] && i + 1 < max) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void permission_init(void)
{
    memset(s_perm_table, 0, sizeof(s_perm_table));

    /* Populate standard system paths with strict permissions */
    perm_set("/", 0, 0, 0, PERM_DEFAULT_DIR);
    perm_set("/bin", 0, 0, 0, PERM_DEFAULT_DIR);
    perm_set("/etc", 0, 0, 0, PERM_MODE_IRUSR | PERM_MODE_IWUSR | PERM_MODE_IXUSR | PERM_MODE_IRGRP | PERM_MODE_IXGRP); /* 0750 */
    perm_set("/data", 0, 0, 0, PERM_DEFAULT_DIR);
    perm_set("/data/apps", 0, 0, 0, PERM_DEFAULT_DIR);
    perm_set("/data/config", 0, 0, 0, PERM_DEFAULT_DIR);
    perm_set("/data/log", 0, 0, 0, PERM_MODE_IRUSR | PERM_MODE_IWUSR | PERM_MODE_IXUSR); /* 0700 */
}

sec_result_t perm_set(const char *path, uint32_t caller_uid, uint32_t new_uid, uint32_t new_gid, uint32_t new_mode)
{
    if (!path) return SEC_ERR_BAD_ARG;

    /* Check if already present */
    file_perm_t *p = NULL;
    for (size_t i = 0; i < PERM_MAX_ENTRIES; i++) {
        if (s_perm_table[i].in_use && strcmp(s_perm_table[i].path, path) == 0) {
            p = &s_perm_table[i];
            break;
        }
    }

    if (p) {
        /* Only owner or root UID 0 can modify permissions */
        if (caller_uid != 0 && caller_uid != p->uid) {
            audit_log(AUDIT_EVENT_PERM_DENY, caller_uid, path, false, "Denied: unauthorized attempt to chmod/chown file");
            return SEC_ERR_DENIED;
        }
    } else {
        /* Allocate new slot */
        for (size_t i = 0; i < PERM_MAX_ENTRIES; i++) {
            if (!s_perm_table[i].in_use) {
                p = &s_perm_table[i];
                break;
            }
        }
        if (!p) return SEC_ERR_NO_MEM;
    }

    p->in_use = true;
    safe_strcpy(p->path, path, sizeof(p->path));
    p->uid = new_uid;
    p->gid = new_gid;
    p->mode = new_mode;

    return SEC_OK;
}

sec_result_t perm_get(const char *path, uint32_t *out_uid, uint32_t *out_gid, uint32_t *out_mode)
{
    if (!path) return SEC_ERR_BAD_ARG;

    for (size_t i = 0; i < PERM_MAX_ENTRIES; i++) {
        if (s_perm_table[i].in_use && strcmp(s_perm_table[i].path, path) == 0) {
            if (out_uid) *out_uid = s_perm_table[i].uid;
            if (out_gid) *out_gid = s_perm_table[i].gid;
            if (out_mode) *out_mode = s_perm_table[i].mode;
            return SEC_OK;
        }
    }

    /* Fallback to default permissions */
    if (out_uid) *out_uid = 0;
    if (out_gid) *out_gid = 0;
    if (out_mode) *out_mode = (path[strlen(path) - 1] == '/') ? PERM_DEFAULT_DIR : PERM_DEFAULT_FILE;
    return SEC_OK;
}

sec_result_t perm_check(const char *path, uint32_t uid, uint32_t gid, uint32_t req_mode)
{
    if (!path) return SEC_ERR_BAD_ARG;

    /* Root UID 0 always passes */
    if (uid == 0) return SEC_OK;

    uint32_t f_uid = 0;
    uint32_t f_gid = 0;
    uint32_t f_mode = 0;
    perm_get(path, &f_uid, &f_gid, &f_mode);

    bool allowed = false;

    /* Callers may request either POSIX mode-bit constants (0400/0200/0100,
     * etc.) or the compact R/W/X mask (4/2/1). Normalize the request to
     * the selected identity class before comparing it with stored bits. */
    uint32_t requested = req_mode & 0x07u;
    if (req_mode & 0700u) {
        requested = (req_mode >> 6) & 0x07u;
    } else if (req_mode & 0070u) {
        requested = (req_mode >> 3) & 0x07u;
    }

    if (uid == f_uid) {
        uint32_t user_bits = (f_mode >> 6) & 0x07u;
        allowed = (user_bits & requested) == requested;
    } else if (gid == f_gid) {
        uint32_t group_bits = (f_mode >> 3) & 0x07u;
        allowed = (group_bits & requested) == requested;
    } else {
        uint32_t other_bits = f_mode & 0x07u;
        allowed = (other_bits & requested) == requested;
    }

    if (!allowed) {
        audit_log(AUDIT_EVENT_PERM_DENY, uid, path, false, "Denied: DAC filesystem permission check failed");
        return SEC_ERR_DENIED;
    }

    return SEC_OK;
}
