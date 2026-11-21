# ZERO OS Graphics Architecture (Milestone 5.0.0 ZERO Desktop)

**Status:** COMPLETE (Milestone 5.0.0 ZERO Desktop)  
**Implementation:** `kernel/graphics/` (Framebuffer, Draw, Font, Cursor, Color)

---

## 1. Overview

ZERO OS 5.0.0 provides a lightweight, tear-free, software-rendered graphics architecture tailored for x86_64 higher-half execution without third-party userspace or windowing servers. It is optimized for responsiveness and minimal power consumption on target hardware (Intel Celeron J3355, 4 GB RAM, Intel HD Graphics 500).

---

## 2. Architectural Layers

```
+--------------------------------------------------------------+
|              ZERO Desktop Shell & Native Apps                |
|  (Terminal, File Manager, Control Center, Sysmon, About)     |
+--------------------------------------------------------------+
                               |
+--------------------------------------------------------------+
|                Window Manager & Compositor                   |
|     (Z-Order Stacking, Damage Tracking, Scene Pipeline)      |
+--------------------------------------------------------------+
                               |
+--------------------------------------------------------------+
|             2D Vector Engine & Typography                    |
|   (Bresenham Lines, Rects, Circles, Alpha Blending, Fonts)   |
+--------------------------------------------------------------+
                               |
+--------------------------------------------------------------+
|             Framebuffer Abstraction & Double Buffer          |
|    (Damage Region Accumulation, Safe Clipping, Page Flip)    |
+--------------------------------------------------------------+
                               |
+--------------------------------------------------------------+
|           Hardware Linear Framebuffer (UEFI GOP / Limine)    |
+--------------------------------------------------------------+
```

---

## 3. Core Subsystems

### 3.1 Linear Framebuffer & Tear-Free Double Buffering (`framebuffer.c`, `framebuffer.h`)
- Discovers physical linear framebuffer details (address, width, height, pitch, bpp) from the Limine bootloader response.
- Allocates an off-screen backbuffer in system RAM matching the screen dimensions.
- All drawing operations write to the backbuffer in RAM, completely eliminating visible screen flicker.
- `fb_flip()` copies only the accumulated dirty bounding box (`damage_x0`, `damage_y0`, `damage_x1`, `damage_y1`) from backbuffer to hardware VRAM.
- If system memory is constrained, falls back to direct frontbuffer writing safely without crashing.

### 3.2 Strict Boundary Clipping (`fb_set_clip`, `fb_reset_clip`)
- Every draw primitive strictly enforces `fb->clip`.
- Any write outside the clip region is immediately suppressed.
- Window client areas set the clip boundary to `[win->x + 1, win->y + 26, win->client_w, win->client_h]`, physically preventing client applications from overdrawing titlebars, borders, or adjacent windows.

### 3.3 2D Vector Primitives (`draw.c`, `draw.h`)
- **Pixels:** `draw_pixel` (direct write with clip validation), `draw_blend_pixel` (8-bit alpha blending).
- **Lines:** `draw_line` implements Bresenham's line algorithm with integer arithmetic across all 8 octants.
- **Rectangles:** `draw_rect`, `draw_fill_rect`, `draw_rounded_rect`, `draw_fill_rounded_rect`.
- **Circles:** `draw_circle`, `draw_fill_circle` using the Midpoint circle algorithm.
- **Gradients:** `draw_gradient_v` vertical linear interpolation.
- **Images:** `draw_image` block pixel transfer with transparency support.

### 3.4 Font & Typography Subsystem (`font.c`, `font.h`)
- Scalable 8x8 bitmap glyph matrix with integer scaling factors (1x, 2x, 3x).
- String width and height metrics computation.
- Horizontal and vertical alignment helpers (`font_draw_string_centered`).
- Custom UI icon glyphs (`0x80` through `0x8A`):
  * `0x80`: Minimize icon (`_`)
  * `0x81`: Maximize icon (`[]`)
  * `0x82`: Close icon (`X`)
  * `0x83`: Folder icon
  * `0x84`: File icon
  * `0x85`: Terminal prompt icon (`>_`)
  * `0x86`: Settings gear icon
  * `0x87`: CPU icon
  * `0x88`: Storage disk icon
  * `0x89`: Network activity icon
  * `0x8A`: Checkmark icon

### 3.5 Flicker-Free Vector Mouse Cursor Overlay (`cursor.c`, `cursor.h`)
- 16x16 vector pointer arrow styled with metallic gold body, black outline, and white highlight.
- Background preservation buffer (`saved_bg`): preserves the 16x16 pixel area under the cursor before drawing, and restores it prior to redrawing at new coordinates.
- Screen boundary clamping prevents the cursor from wandering outside display bounds.
- Hardware button state tracking for left, right, and middle clicks.
