/* ZERO KERNEL - In-Memory RAM Disk Block Device (Milestone 3.0.0 Hardware Foundation). */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "drivers/block.h"

/* Initialize the in-memory RAM disk block device */
int ramdisk_init(size_t sector_count);

/* Retrieve pointer to the RAM disk block device structure */
block_device_t *ramdisk_get_device(void);
