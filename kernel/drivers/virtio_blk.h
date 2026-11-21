/* ZERO KERNEL - VirtIO Block Storage Driver Interface (Milestone 3.0.0 Hardware Foundation).
 *
 * Implements QEMU VirtIO block device discovery, queue initialization,
 * and sector read/write operations.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "drivers/block.h"

/* VirtIO Vendor & Device IDs */
#define VIRTIO_PCI_VENDOR_ID        0x1AF4
#define VIRTIO_PCI_DEV_BLOCK_LEGACY 0x1001
#define VIRTIO_PCI_DEV_BLOCK_MODERN 0x1042

/* Initialize VirtIO block driver and probe PCI bus */
int virtio_blk_init(void);

/* Check if VirtIO block storage was discovered and active */
bool virtio_blk_is_available(void);

/* Retrieve pointer to VirtIO block device structure */
block_device_t *virtio_blk_get_device(void);
