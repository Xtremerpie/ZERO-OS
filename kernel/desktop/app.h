/* ZERO KERNEL - Native Application Framework & Registry (Milestone 5.0.0 ZERO Desktop).
 *
 * Implements application registration, metadata management, and lifecycle dispatch.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "desktop/window.h"
#include "desktop/shell.h"

typedef struct app_desc {
    app_id_t     id;
    const char  *name;
    uint8_t      icon_glyph;
    window_t   *(*launch)(void);
    bool        (*is_open)(void);
} app_desc_t;

/* Initialize native application registry */
void app_init(void);

/* Launch application by ID */
window_t *app_launch(app_id_t id);

/* Query application descriptors */
const app_desc_t *app_get(app_id_t id);
const app_desc_t *app_get_all(uint32_t *count);
