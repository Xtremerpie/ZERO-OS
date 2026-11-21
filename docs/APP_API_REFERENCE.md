# ZERO OS Application API Reference

**Document Version:** 1.0.0  
**Milestone:** 6.0.0 ZERO Ecosystem

---

## 1. Window API (`sdk/include/zero/window.h`)

### `zero_window_t *zero_create_window(const char *title, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t flags)`
Allocates and maps a new window into the desktop compositor scene graph.
- `title`: UTF-8 title string displayed in titlebar.
- `x, y`: Screen origin coordinates.
- `w, h`: Client surface dimensions.
- `flags`: Window styling flags (`ZERO_WIN_RESIZABLE`, `ZERO_WIN_BORDERLESS`).

### `void zero_close_window(zero_window_t *win)`
Closes the window and frees associated surface buffers.

### `void zero_mark_dirty(zero_window_t *win)`
Triggers compositor re-rendering and surface blit for the specified window.

---

## 2. Drawing Primitives API (`sdk/include/zero/draw.h`)

All drawing functions operate on a `zero_surface_t` structure. Pixel values use `0x00RRGGBB` format.

### Color Constants
- `COLOR_ZERO_BLACK`: `0x08080A` (Deep void black)
- `COLOR_ZERO_PANEL`: `0x1A1A20` (Panel graphite)
- `COLOR_ZERO_GOLD`:  `0xD4AF37` (Metallic gold accent)
- `COLOR_ZERO_CRIMSON`: `0xC41230` (Crimson red accent)
- `COLOR_ZERO_WHITE`: `0xF2F2F4` (High-contrast text)

### Functions
- `void zero_draw_pixel(zero_surface_t *surf, int32_t x, int32_t y, uint32_t color)`
- `void zero_draw_fill_rect(zero_surface_t *surf, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color)`
- `void zero_draw_rect(zero_surface_t *surf, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color)`
- `void zero_draw_line(zero_surface_t *surf, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)`
- `void zero_draw_string(zero_surface_t *surf, int32_t x, int32_t y, const char *str, uint32_t color)`

---

## 3. Configuration API (`kernel/apps/config.h`)

### File Location: `/data/config/<app_id>.cfg`

- `config_t *config_load(const char *app_id)`: Loads or creates configuration instance.
- `int config_save(config_t *cfg)`: Flushes dirty key-value pairs to disk.
- `const char *config_get_string(config_t *cfg, const char *key, const char *def)`
- `int64_t config_get_int(config_t *cfg, const char *key, int64_t def)`
- `bool config_get_bool(config_t *cfg, const char *key, bool def)`
- `int config_set_string(config_t *cfg, const char *key, const char *val)`
- `int config_set_int(config_t *cfg, const char *key, int64_t val)`
- `int config_set_bool(config_t *cfg, const char *key, bool val)`
- `void config_free(config_t *cfg)`: Releases memory and autosaves if dirty.

---

## 4. Notifications API (`kernel/desktop/notification.h`)

- `void notification_show(const char *title, const char *msg, uint8_t icon_glyph, uint32_t duration_ms)`:
  Displays an animated toast notification in the lower-right corner above the taskbar.
- `void notification_dismiss(void)`:
  Immediately closes active notification toast.
