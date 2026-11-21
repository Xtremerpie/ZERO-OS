/* ZERO KERNEL - ZERO Software Center (ZERO Store) (Milestone 6.0.0 ZERO Ecosystem).
 *
 * Implements native graphical application store:
 * category browsing, search, package cards, installation, removal,
 * launch integration, and transaction feedback.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "desktop/window.h"

typedef enum {
    STORE_CAT_ALL = 0,
    STORE_CAT_PRODUCTIVITY,
    STORE_CAT_DEVELOPMENT,
    STORE_CAT_UTILITIES,
    STORE_CAT_INTERNET,
    STORE_CAT_MEDIA,
    STORE_CAT_INSTALLED,
    STORE_CAT_COUNT
} store_category_t;

/* Initialize ZERO Store subsystem */
void store_init(void);

/* Launch or focus the ZERO Store application window */
window_t *store_launch(void);

/* Close or query state */
void store_close(void);
bool store_is_open(void);

/* Filter packages by category or search term */
void store_set_category(store_category_t cat);
void store_search(const char *query);
