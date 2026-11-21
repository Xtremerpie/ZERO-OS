# ZERO OS Window Manager (Milestone 5.0.0 ZERO Desktop)

**Status:** COMPLETE (Milestone 5.0.0 ZERO Desktop)  
**Implementation:** `kernel/desktop/window.h`, `kernel/desktop/window.c`

---

## 1. Overview

The ZERO OS Window Manager provides overlapping window management, z-ordering, focus arbitration, titlebar hit-testing, interactive dragging, minimize/maximize/restore transitions, and input event routing directly in the kernel without external windowing daemons.

---

## 2. Window Structure (`window_t`)

```c
typedef struct window {
    int32_t         id;
    char            title[64];
    int32_t         x, y, width, height;
    int32_t         saved_x, saved_y, saved_width, saved_height;
    uint32_t        flags;
    int32_t         z_order;
    int32_t         client_width, client_height;
    uint32_t       *content;
    int32_t         drag_offset_x, drag_offset_y;
    win_paint_cb_t  on_paint;
    win_mouse_cb_t  on_mouse;
    win_key_cb_t    on_key;
    win_close_cb_t  on_close;
    void           *user_data;
} window_t;
```

---

## 3. Window Flags

| Flag | Value | Description |
| :--- | :--- | :--- |
| `WINDOW_FLAG_VISIBLE` | `1 << 0` | Window is visible on screen |
| `WINDOW_FLAG_FOCUSED` | `1 << 1` | Window has active keyboard/mouse focus |
| `WINDOW_FLAG_MINIMIZED`| `1 << 2` | Window is minimized to the taskbar |
| `WINDOW_FLAG_MAXIMIZED`| `1 << 3` | Window occupies the full screen (above taskbar) |
| `WINDOW_FLAG_DRAGGING` | `1 << 4` | Window is currently being moved via titlebar |
| `WINDOW_FLAG_RESIZABLE`| `1 << 5` | Window allows interactive geometry resizing |
| `WINDOW_FLAG_CLOSING`  | `1 << 6` | Window is pending teardown and closure |

---

## 4. Geometry and Hit-Testing

Every window consists of:
- **Outer 1px Border:** Metallic Gold (`COLOR_ZERO_BORDER_FOCUS`) when focused, Dark Slate (`COLOR_ZERO_BORDER`) when unfocused.
- **Titlebar (Height 26px):**
  * Title text rendered at `x + 8, y + 8`.
  * Active window has a 2px horizontal gold stripe at the bottom of the titlebar.
  * Minimize button (`_` icon): `x + width - 66, y + 4, 18, 18`.
  * Maximize button (`[]` icon): `x + width - 44, y + 4, 18, 18`.
  * Close button (`X` icon): `x + width - 22, y + 4, 18, 18` styled with a subtle crimson alert tint.
- **Client Area:**
  * Coordinates: `x + 1, y + 26`.
  * Dimensions: `width - 2, height - 27`.

### Precision Hit-Test Regions (`wm_hit_test`):
- `WM_HIT_NONE`: Outside window bounding box or window is minimized.
- `WM_HIT_TITLEBAR`: Titlebar drag bar.
- `WM_HIT_MINIMIZE`: Minimize control button.
- `WM_HIT_MAXIMIZE`: Maximize/restore control button.
- `WM_HIT_CLOSE`: Close control button.
- `WM_HIT_CLIENT`: Active application client drawing area.
- `WM_HIT_BORDER`: Window perimeter border.

---

## 5. Z-Order Stacking & Focus Policy

1. **Stacking Order:** Windows are maintained in an ordered array from backmost (`z_order = 0`) to topmost (`z_order = count - 1`).
2. **Bring to Front:** When a window is clicked or created, it is shifted to the end of the array and assigned the highest z-order index.
3. **Focus Synchronization:** Exactly one window holds `WINDOW_FLAG_FOCUSED` at any time. When a window is focused, its titlebar and border switch to the metallic gold visual state.
4. **Minimization & Focus Fallback:** When the focused window is minimized, focus automatically transfers to the next topmost visible window in the stack.

---

## 6. Window Manager API Summary

- `wm_init(screen_w, screen_h)`: Initialize window table and constraints.
- `wm_create_window(title, x, y, w, h, flags)`: Allocate and register a new window.
- `wm_destroy_window(win)`: Deallocate client buffers and remove from window table.
- `wm_set_focus(win)` / `wm_get_focused()`: Focus management.
- `wm_move(win, x, y)`: Move window with screen boundary clamping.
- `wm_resize(win, w, h)`: Resize window and reallocate client buffer preserving overlapping pixels.
- `wm_minimize(win)` / `wm_maximize(win)` / `wm_restore(win)`: State transitions.
- `wm_handle_mouse(x, y, buttons)` / `wm_handle_key(key, scancode)`: Event routing.
