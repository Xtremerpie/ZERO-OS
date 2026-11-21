/* ZERO KERNEL - Application Configuration Framework Implementation (Milestone 6.0.0 ZERO Ecosystem).
 */
#include "apps/config.h"
#include "fs/vfs.h"
#include "zero/string.h"

#define CFG_POOL_SIZE 16u
static config_t s_config_pool[CFG_POOL_SIZE];
static bool     s_pool_used[CFG_POOL_SIZE];

static void safe_strcpy(char *dst, const char *src, size_t max)
{
    size_t i = 0;
    while (src && src[i] && i + 1 < max) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void build_cfg_path(const char *app_id, char *out, size_t max)
{
    const char *prefix = "/data/config/";
    const char *suffix = ".cfg";
    size_t pos = 0;

    while (*prefix && pos + 1 < max) {
        out[pos++] = *prefix++;
    }
    while (app_id && *app_id && pos + 1 < max) {
        out[pos++] = *app_id++;
    }
    while (*suffix && pos + 1 < max) {
        out[pos++] = *suffix++;
    }
    out[pos] = '\0';
}

void config_init(void)
{
    memset(s_config_pool, 0, sizeof(s_config_pool));
    memset(s_pool_used, 0, sizeof(s_pool_used));

    /* Ensure storage hierarchy exists */
    vfs_mkdir("/data");
    vfs_mkdir("/data/config");
}

config_t *config_load(const char *app_id)
{
    if (!app_id || !*app_id) return NULL;

    /* Check if already loaded in pool */
    for (size_t i = 0; i < CFG_POOL_SIZE; i++) {
        if (s_pool_used[i] && strcmp(s_config_pool[i].app_id, app_id) == 0) {
            return &s_config_pool[i];
        }
    }

    /* Allocate free slot */
    int slot = -1;
    for (size_t i = 0; i < CFG_POOL_SIZE; i++) {
        if (!s_pool_used[i]) {
            slot = (int)i;
            break;
        }
    }
    if (slot < 0) return NULL;

    config_t *cfg = &s_config_pool[slot];
    memset(cfg, 0, sizeof(*cfg));
    s_pool_used[slot] = true;
    safe_strcpy(cfg->app_id, app_id, sizeof(cfg->app_id));

    /* Try to load from file */
    char path[VFS_MAX_PATH];
    build_cfg_path(app_id, path, sizeof(path));

    file_t *f = vfs_open(path, O_RDONLY);
    if (!f) {
        return cfg; /* Returns empty config if file doesn't exist yet */
    }

    char buf[2048];
    int64_t bytes = vfs_read(f, buf, sizeof(buf) - 1);
    vfs_close(f);

    if (bytes <= 0) {
        return cfg;
    }
    buf[bytes] = '\0';

    /* Parse key=value lines */
    char *line = buf;
    while (*line) {
        while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n') line++;
        if (!*line) break;

        char *line_end = line;
        while (*line_end && *line_end != '\n' && *line_end != '\r') line_end++;
        char saved = *line_end;
        *line_end = '\0';

        /* Ignore comments */
        if (*line != '#') {
            char *eq = line;
            while (*eq && *eq != '=') eq++;
            if (*eq == '=') {
                *eq = '\0';
                char *key = line;
                char *val = eq + 1;

                /* Trim key trailing whitespace */
                size_t klen = strlen(key);
                while (klen > 0 && (key[klen - 1] == ' ' || key[klen - 1] == '\t')) {
                    key[--klen] = '\0';
                }

                /* Trim val leading whitespace */
                while (*val == ' ' || *val == '\t') val++;
                /* Trim val trailing whitespace */
                size_t vlen = strlen(val);
                while (vlen > 0 && (val[vlen - 1] == ' ' || val[vlen - 1] == '\t')) {
                    val[--vlen] = '\0';
                }

                if (klen > 0 && cfg->count < CFG_MAX_ENTRIES) {
                    safe_strcpy(cfg->entries[cfg->count].key, key, sizeof(cfg->entries[cfg->count].key));
                    safe_strcpy(cfg->entries[cfg->count].val, val, sizeof(cfg->entries[cfg->count].val));
                    cfg->entries[cfg->count].in_use = true;
                    cfg->count++;
                }
            }
        }

        if (saved == '\0') break;
        line = line_end + 1;
    }

    cfg->dirty = false;
    return cfg;
}

int config_save(config_t *cfg)
{
    if (!cfg || !cfg->app_id[0]) return -1;

    char path[VFS_MAX_PATH];
    build_cfg_path(cfg->app_id, path, sizeof(path));

    vfs_create(path);
    file_t *f = vfs_open(path, O_WRONLY | O_TRUNC);
    if (!f) return -1;

    const char *hdr = "# ZERO OS Application Configuration\n";
    vfs_write(f, hdr, strlen(hdr));

    for (uint32_t i = 0; i < cfg->count; i++) {
        if (!cfg->entries[i].in_use) continue;

        vfs_write(f, cfg->entries[i].key, strlen(cfg->entries[i].key));
        vfs_write(f, "=", 1);
        vfs_write(f, cfg->entries[i].val, strlen(cfg->entries[i].val));
        vfs_write(f, "\n", 1);
    }

    vfs_close(f);
    cfg->dirty = false;
    return 0;
}

void config_free(config_t *cfg)
{
    if (!cfg) return;
    for (size_t i = 0; i < CFG_POOL_SIZE; i++) {
        if (&s_config_pool[i] == cfg) {
            if (cfg->dirty) {
                config_save(cfg);
            }
            s_pool_used[i] = false;
            memset(cfg, 0, sizeof(*cfg));
            break;
        }
    }
}

const char *config_get_string(config_t *cfg, const char *key, const char *default_val)
{
    if (!cfg || !key) return default_val;
    for (uint32_t i = 0; i < cfg->count; i++) {
        if (cfg->entries[i].in_use && strcmp(cfg->entries[i].key, key) == 0) {
            return cfg->entries[i].val;
        }
    }
    return default_val;
}

int64_t config_get_int(config_t *cfg, const char *key, int64_t default_val)
{
    const char *str = config_get_string(cfg, key, NULL);
    if (!str || !*str) return default_val;

    bool neg = false;
    if (*str == '-') {
        neg = true;
        str++;
    } else if (*str == '+') {
        str++;
    }

    int64_t val = 0;
    while (*str >= '0' && *str <= '9') {
        val = (val * 10) + (*str - '0');
        str++;
    }
    return neg ? -val : val;
}

bool config_get_bool(config_t *cfg, const char *key, bool default_val)
{
    const char *str = config_get_string(cfg, key, NULL);
    if (!str || !*str) return default_val;

    if (strcmp(str, "true") == 0 || strcmp(str, "1") == 0 ||
        strcmp(str, "yes") == 0  || strcmp(str, "on") == 0) {
        return true;
    }
    if (strcmp(str, "false") == 0 || strcmp(str, "0") == 0 ||
        strcmp(str, "no") == 0   || strcmp(str, "off") == 0) {
        return false;
    }
    return default_val;
}

int config_set_string(config_t *cfg, const char *key, const char *value)
{
    if (!cfg || !key || !value) return -1;

    /* Check if already present */
    for (uint32_t i = 0; i < cfg->count; i++) {
        if (cfg->entries[i].in_use && strcmp(cfg->entries[i].key, key) == 0) {
            safe_strcpy(cfg->entries[i].val, value, sizeof(cfg->entries[i].val));
            cfg->dirty = true;
            return 0;
        }
    }

    /* Add new entry */
    if (cfg->count >= CFG_MAX_ENTRIES) return -1;

    safe_strcpy(cfg->entries[cfg->count].key, key, sizeof(cfg->entries[cfg->count].key));
    safe_strcpy(cfg->entries[cfg->count].val, value, sizeof(cfg->entries[cfg->count].val));
    cfg->entries[cfg->count].in_use = true;
    cfg->count++;
    cfg->dirty = true;
    return 0;
}

int config_set_int(config_t *cfg, const char *key, int64_t value)
{
    char buf[32];
    if (value == 0) {
        buf[0] = '0'; buf[1] = '\0';
    } else {
        bool neg = (value < 0);
        uint64_t uval = neg ? (uint64_t)(-value) : (uint64_t)value;
        char tmp[32];
        int pos = 0;
        while (uval > 0 && pos < 30) {
            tmp[pos++] = '0' + (uval % 10);
            uval /= 10;
        }
        int len = 0;
        if (neg) buf[len++] = '-';
        for (int i = pos - 1; i >= 0; i--) {
            buf[len++] = tmp[i];
        }
        buf[len] = '\0';
    }
    return config_set_string(cfg, key, buf);
}

int config_set_bool(config_t *cfg, const char *key, bool value)
{
    return config_set_string(cfg, key, value ? "true" : "false");
}

void config_reset_defaults(config_t *cfg)
{
    if (!cfg) return;
    memset(cfg->entries, 0, sizeof(cfg->entries));
    cfg->count = 0;
    cfg->dirty = true;
}
