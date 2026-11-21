/* ZERO KERNEL - Graphics Acceleration & Hardware Abstraction (Milestone 9.0.0 ZERO Ultimate).
 *
 * Implements graphics hardware device discovery, capabilities reporting,
 * accelerated 2D blitting, page-flipping synchronization, and robust software fallback.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "graphics/framebuffer.h"

/* GPU Hardware Capability Flags */
#define GPU_CAP_2D_BLIT         (1u << 0)
#define GPU_CAP_PAGE_FLIP       (1u << 1)
#define GPU_CAP_HARDWARE_CURSOR (1u << 2)
#define GPU_CAP_VSYNC           (1u << 3)
#define GPU_CAP_COMMAND_RING    (1u << 4)
#define GPU_CAP_3D_ACCEL        (1u << 5)

typedef enum {
    GPU_TYPE_NONE = 0,
    GPU_TYPE_INTEL_HD500,     /* Intel HD Graphics 500 (Apollo Lake Gen9) */
    GPU_TYPE_VIRTIO_GPU,      /* VirtIO-GPU (QEMU/KVM) */
    GPU_TYPE_BOCHS_VGA,       /* Bochs / QEMU Standard VGA */
    GPU_TYPE_VMWARE_SVGA,     /* VMware SVGA II */
    GPU_TYPE_GENERIC_FRAMEBUFFER /* Standard UEFI GOP / VESA linear framebuffer */
} gpu_type_t;

typedef struct gpu_device {
    char        name[48];
    gpu_type_t  type;
    uint16_t    vendor_id;
    uint16_t    device_id;
    uint32_t    capabilities;
    uint64_t    vram_size_bytes;
    bool        is_accelerated;
    bool        initialized;
} gpu_device_t;

typedef struct gpu_telemetry {
    char     active_renderer[48];
    uint32_t capabilities;
    uint64_t frames_presented;
    uint64_t accelerated_blits;
    uint64_t software_blits;
    uint32_t avg_blit_time_us;
    bool     vsync_enabled;
    uint32_t display_width;
    uint32_t display_height;
    uint32_t refresh_rate_hz;
} gpu_telemetry_t;

/* Initialize graphics device abstraction and probe display hardware */
void gpu_init(void);

/* Query primary graphics device descriptor */
const gpu_device_t *gpu_get_device(void);

/* Query graphics subsystem telemetry */
void gpu_get_telemetry(gpu_telemetry_t *out_telem);

/* Accelerated or optimized 2D rectangle blit */
bool gpu_blit_rect(uint32_t dst_x, uint32_t dst_y,
                   uint32_t src_w, uint32_t src_h,
                   const uint32_t *src_pixels, uint32_t src_stride);

/* Hardware or double-buffered page flip presentation */
bool gpu_present(framebuffer_t *fb);

/* Wait for vertical retrace synchronization */
void gpu_wait_vsync(void);
