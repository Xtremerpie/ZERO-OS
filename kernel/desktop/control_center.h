/* ZERO KERNEL - ZERO Control Center (Milestone 5.0.0 ZERO Desktop).
 *
 * Implements native system settings and hardware configuration window:
 * Display, Input, System Specs, Network, Storage, and Desktop Themes.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "desktop/window.h"

typedef enum {
    CC_TAB_DISPLAY = 0,
    CC_TAB_INPUT,
    CC_TAB_SYSTEM,
    CC_TAB_NETWORK,
    CC_TAB_STORAGE,
    CC_TAB_THEMES,
    CC_TAB_PERMISSIONS,
    CC_TAB_COUNT
} cc_tab_t;

/* Launch or focus the ZERO Control Center window */
window_t *control_center_launch(void);

/* Close or query state */
void control_center_close(void);
bool control_center_is_open(void);
cc_tab_t control_center_get_active_tab(void);
void control_center_set_active_tab(cc_tab_t tab);
