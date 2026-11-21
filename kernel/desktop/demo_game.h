/* ZERO KERNEL - Native Demo Game: ZERO Strike (Milestone 9.0.0 ZERO Ultimate).
 *
 * Implements a native, 60 FPS vector arcade game demonstrating input handling,
 * double-buffered graphics rendering, real-time performance HUD overlay,
 * and automatic Gaming Mode profile integration.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "desktop/window.h"

/* Initialize demo game structures */
void demo_game_init(void);

/* Launch game window and activate Gaming Mode */
window_t *demo_game_launch(void);

/* Check if game window is currently open */
bool demo_game_is_open(void);

/* Update game physics, stars, enemies, and projectiles */
void demo_game_tick(void);
