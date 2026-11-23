/* ZERO KERNEL - Security Audit Logging Implementation (Milestone 7.0.0 ZERO Security).
 */
#include "security/audit.h"
#include "fs/vfs.h"
#include "zero/timer.h"
#include "zero/string.h"

static audit_event_t s_ring[AUDIT_RING_BUFFER_SIZE];
static uint32_t      s_head = 0;
static uint32_t      s_count = 0;
static uint64_t      s_total_count = 0;

static const char *const s_type_names[AUDIT_EVENT_TYPE_COUNT] = {
    [AUDIT_EVENT_AUTH_FAILURE]     = "AUTH_FAILURE",
    [AUDIT_EVENT_CAP_GRANT]        = "CAP_GRANT",
    [AUDIT_EVENT_CAP_REVOKE]       = "CAP_REVOKE",
    [AUDIT_EVENT_PERM_DENY]        = "PERM_DENY",
    [AUDIT_EVENT_SANDBOX_VIOLATION]= "SANDBOX_VIOLATION",
    [AUDIT_EVENT_SYSCALL_VIOLATION]= "SYSCALL_VIOLATION",
    [AUDIT_EVENT_PACKAGE_VERIFY]   = "PACKAGE_VERIFY",
    [AUDIT_EVENT_PACKAGE_INSTALL]  = "PACKAGE_INSTALL",
    [AUDIT_EVENT_FIREWALL_DROP]    = "FIREWALL_DROP",
    [AUDIT_EVENT_CONFIG_CHANGE]    = "CONFIG_CHANGE"
};

const char *audit_event_type_name(audit_event_type_t type)
{
    if ((uint32_t)type < AUDIT_EVENT_TYPE_COUNT && s_type_names[type]) {
        return s_type_names[type];
    }
    return "UNKNOWN";
}

void audit_init(void)
{
    memset(s_ring, 0, sizeof(s_ring));
    s_head = 0;
    s_count = 0;
    s_total_count = 0;
}

static size_t audit_append(char *out, size_t cap, size_t pos, const char *src)
{
    if (!src) src = "-";
    while (*src && pos + 1 < cap) out[pos++] = *src++;
    out[pos] = '\0';
    return pos;
}

static size_t audit_append_u64(char *out, size_t cap, size_t pos, uint64_t value)
{
    char digits[24];
    size_t n = 0;
    do { digits[n++] = (char)('0' + (value % 10u)); value /= 10u; } while (value && n < sizeof(digits));
    while (n && pos + 1 < cap) out[pos++] = digits[--n];
    out[pos] = '\0';
    return pos;
}

static void audit_persist_line(const audit_event_t *ev)
{
    if (!ev) return;
    (void)vfs_mkdir("/data");
    (void)vfs_mkdir("/data/log");
    file_t *f = vfs_open(AUDIT_LOG_FILE_PATH, O_WRONLY | O_CREAT | O_APPEND);
    if (!f) return;
    char line[256] = {0};
    size_t pos = 0;
    pos = audit_append(line, sizeof(line), pos, "[");
    pos = audit_append_u64(line, sizeof(line), pos, ev->timestamp_ms);
    pos = audit_append(line, sizeof(line), pos, "] ");
    pos = audit_append(line, sizeof(line), pos, audit_event_type_name(ev->type));
    pos = audit_append(line, sizeof(line), pos, " pid=");
    pos = audit_append_u64(line, sizeof(line), pos, ev->pid);
    pos = audit_append(line, sizeof(line), pos, " res=");
    pos = audit_append(line, sizeof(line), pos, ev->resource[0] ? ev->resource : "-");
    pos = audit_append(line, sizeof(line), pos, ev->success ? " result=ALLOW details=" : " result=DENY details=");
    pos = audit_append(line, sizeof(line), pos, ev->details[0] ? ev->details : "-");
    if (pos < sizeof(line)) line[pos++] = '\n';
    (void)vfs_write(f, line, pos);
    vfs_close(f);
}
void audit_log(audit_event_type_t type, uint32_t pid, const char *resource, bool success, const char *details)
{
    s_total_count++;

    audit_event_t *ev = &s_ring[s_head];
    memset(ev, 0, sizeof(*ev));

    ev->id = s_total_count;
    ev->timestamp_ms = timer_is_initialized() ? timer_get_uptime_ms() : 0;
    ev->type = type;
    ev->pid = pid;
    ev->success = success;

    if (resource) {
        strncpy(ev->resource, resource, sizeof(ev->resource) - 1);
        ev->resource[sizeof(ev->resource) - 1] = '\0';
    }
    if (details) {
        strncpy(ev->details, details, sizeof(ev->details) - 1);
        ev->details[sizeof(ev->details) - 1] = '\0';
    }

    s_head = (s_head + 1) % AUDIT_RING_BUFFER_SIZE;
    if (s_count < AUDIT_RING_BUFFER_SIZE) {
        s_count++;
    }

    /* Attempt to persist to filesystem */
    audit_persist_line(ev);
}

uint32_t audit_get_events(audit_event_t *out_events, uint32_t max_events)
{
    if (!out_events || max_events == 0) return 0;

    uint32_t available = (s_count < max_events) ? s_count : max_events;
    /* Return events from newest to oldest or oldest to newest.
     * We return from newest to oldest for easy dashboard display. */
    for (uint32_t i = 0; i < available; i++) {
        uint32_t idx = (s_head + AUDIT_RING_BUFFER_SIZE - 1 - i) % AUDIT_RING_BUFFER_SIZE;
        out_events[i] = s_ring[idx];
    }

    return available;
}

uint64_t audit_get_event_count(void)
{
    return s_total_count;
}

void audit_clear(void)
{
    memset(s_ring, 0, sizeof(s_ring));
    s_head = 0;
    s_count = 0;
}
