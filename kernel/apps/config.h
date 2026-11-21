/* ZERO KERNEL - Application Configuration Framework (Milestone 6.0.0 ZERO Ecosystem).
 *
 * Persistent key-value configuration subsystem storing preferences in /data/config/<app_id>.cfg.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define CFG_MAX_ENTRIES   32u
#define CFG_MAX_KEY_LEN   64u
#define CFG_MAX_VAL_LEN   128u
#define CFG_MAX_APP_LEN   64u

typedef struct cfg_entry {
    char key[CFG_MAX_KEY_LEN];
    char val[CFG_MAX_VAL_LEN];
    bool in_use;
} cfg_entry_t;

typedef struct config {
    char        app_id[CFG_MAX_APP_LEN];
    cfg_entry_t entries[CFG_MAX_ENTRIES];
    uint32_t    count;
    bool        dirty;
} config_t;

/* Initialize configuration subsystem */
void config_init(void);

/* Load or create configuration object for application ID */
config_t *config_load(const char *app_id);

/* Save configuration to disk at /data/config/<app_id>.cfg */
int config_save(config_t *cfg);

/* Free/release configuration object */
void config_free(config_t *cfg);

/* Value accessors */
const char *config_get_string(config_t *cfg, const char *key, const char *default_val);
int64_t     config_get_int(config_t *cfg, const char *key, int64_t default_val);
bool        config_get_bool(config_t *cfg, const char *key, bool default_val);

/* Value setters */
int config_set_string(config_t *cfg, const char *key, const char *value);
int config_set_int(config_t *cfg, const char *key, int64_t value);
int config_set_bool(config_t *cfg, const char *key, bool value);

/* Clear all entries */
void config_reset_defaults(config_t *cfg);
