/* ZERO KERNEL - VirtIO Block Storage Driver Implementation (Milestone 3.0.0 Hardware Foundation). */

#include "drivers/virtio_blk.h"
#include "hardware/pci.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "zero/cpu.h"
#include "zero/log.h"
#include "zero/string.h"

/* VirtIO Status Bits */
#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_DRIVER_OK   4
#define VIRTIO_STATUS_FAILED      128

/* VirtIO Legacy Registers */
#define VIRTIO_REG_DEVICE_FEATURES 0x00
#define VIRTIO_REG_GUEST_FEATURES  0x04
#define VIRTIO_REG_QUEUE_ADDRESS   0x08
#define VIRTIO_REG_QUEUE_SIZE      0x0C
#define VIRTIO_REG_QUEUE_SELECT    0x0E
#define VIRTIO_REG_QUEUE_NOTIFY    0x10
#define VIRTIO_REG_DEVICE_STATUS   0x12
#define VIRTIO_REG_ISR_STATUS      0x13
#define VIRTIO_REG_CONFIG_SECTORS  0x14

/* VirtIO Request Types */
#define VIRTIO_BLK_T_IN  0 /* Read */
#define VIRTIO_BLK_T_OUT 1 /* Write */

#define VRING_DESC_F_NEXT     1
#define VRING_DESC_F_WRITE    2

typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} vring_desc_t;

typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} vring_avail_t;

typedef struct __attribute__((packed)) {
    uint32_t id;
    uint32_t len;
} vring_used_elem_t;

typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t idx;
    vring_used_elem_t ring[];
} vring_used_t;

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} virtio_blk_req_hdr_t;

static bool g_virtio_available = false;
static uint16_t g_io_base = 0;
static uint16_t g_queue_size = 0;
static uint64_t g_vring_phys = 0;
static vring_desc_t *g_desc = NULL;
static vring_avail_t *g_avail = NULL;
static volatile vring_used_t *g_used = NULL;
static uint16_t g_last_used_idx = 0;

static block_device_t g_virtio_bdev;
static block_device_ops_t g_virtio_ops;

static int virtio_blk_op_read(block_device_t *dev, uint64_t lba, uint32_t count, void *buffer)
{
    (void)dev;
    if (!g_virtio_available || !buffer)
        return -1;

    static virtio_blk_req_hdr_t hdr;
    static volatile uint8_t status_byte;

    hdr.type = VIRTIO_BLK_T_IN;
    hdr.reserved = 0;
    hdr.sector = lba;
    status_byte = 0xFF;

    uint32_t data_bytes = count * BLOCK_SECTOR_SIZE;

    /* Descriptor 0: Request Header (Device Reads) */
    g_desc[0].addr = (uint64_t)(uintptr_t)&hdr;
    g_desc[0].len = sizeof(virtio_blk_req_hdr_t);
    g_desc[0].flags = VRING_DESC_F_NEXT;
    g_desc[0].next = 1;

    /* Descriptor 1: Data Buffer (Device Writes) */
    g_desc[1].addr = (uint64_t)(uintptr_t)buffer;
    g_desc[1].len = data_bytes;
    g_desc[1].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
    g_desc[1].next = 2;

    /* Descriptor 2: Status Byte (Device Writes) */
    g_desc[2].addr = (uint64_t)(uintptr_t)&status_byte;
    g_desc[2].len = 1;
    g_desc[2].flags = VRING_DESC_F_WRITE;
    g_desc[2].next = 0;

    /* Add to available ring */
    uint16_t avail_idx = g_avail->idx % g_queue_size;
    g_avail->ring[avail_idx] = 0;
    __sync_synchronize();
    g_avail->idx++;
    __sync_synchronize();

    /* Notify controller (queue 0) */
    outw(g_io_base + VIRTIO_REG_QUEUE_NOTIFY, 0);

    /* Poll for completion */
    for (int timeout = 0; timeout < 100000; timeout++) {
        if (g_used->idx != g_last_used_idx) {
            g_last_used_idx = g_used->idx;
            return (status_byte == 0) ? 0 : -1;
        }
        cpu_pause();
    }

    return -1; /* Timeout */
}

static int virtio_blk_op_write(block_device_t *dev, uint64_t lba, uint32_t count, const void *buffer)
{
    (void)dev;
    if (!g_virtio_available || !buffer)
        return -1;

    static virtio_blk_req_hdr_t hdr;
    static volatile uint8_t status_byte;

    hdr.type = VIRTIO_BLK_T_OUT;
    hdr.reserved = 0;
    hdr.sector = lba;
    status_byte = 0xFF;

    uint32_t data_bytes = count * BLOCK_SECTOR_SIZE;

    /* Descriptor 0: Request Header (Device Reads) */
    g_desc[0].addr = (uint64_t)(uintptr_t)&hdr;
    g_desc[0].len = sizeof(virtio_blk_req_hdr_t);
    g_desc[0].flags = VRING_DESC_F_NEXT;
    g_desc[0].next = 1;

    /* Descriptor 1: Data Buffer (Device Reads) */
    g_desc[1].addr = (uint64_t)(uintptr_t)buffer;
    g_desc[1].len = data_bytes;
    g_desc[1].flags = VRING_DESC_F_NEXT;
    g_desc[1].next = 2;

    /* Descriptor 2: Status Byte (Device Writes) */
    g_desc[2].addr = (uint64_t)(uintptr_t)&status_byte;
    g_desc[2].len = 1;
    g_desc[2].flags = VRING_DESC_F_WRITE;
    g_desc[2].next = 0;

    /* Add to available ring */
    uint16_t avail_idx = g_avail->idx % g_queue_size;
    g_avail->ring[avail_idx] = 0;
    __sync_synchronize();
    g_avail->idx++;
    __sync_synchronize();

    /* Notify controller */
    outw(g_io_base + VIRTIO_REG_QUEUE_NOTIFY, 0);

    /* Poll for completion */
    for (int timeout = 0; timeout < 100000; timeout++) {
        if (g_used->idx != g_last_used_idx) {
            g_last_used_idx = g_used->idx;
            return (status_byte == 0) ? 0 : -1;
        }
        cpu_pause();
    }

    return -1; /* Timeout */
}

int virtio_blk_init(void)
{
    pci_device_t *pdev = pci_find_device(VIRTIO_PCI_VENDOR_ID, VIRTIO_PCI_DEV_BLOCK_LEGACY);
    if (!pdev) {
        pdev = pci_find_device(VIRTIO_PCI_VENDOR_ID, VIRTIO_PCI_DEV_BLOCK_MODERN);
    }

    if (!pdev || pdev->bar_size[0] == 0) {
        KLOG_INFO("virtio", "VirtIO block controller not detected on PCI bus");
        return 0;
    }

    g_io_base = (uint16_t)pdev->bar[0];

    /* Enable bus mastering */
    uint16_t cmd = pci_read16(pdev->bus, pdev->slot, pdev->func, PCI_REG_COMMAND);
    pci_write16(pdev->bus, pdev->slot, pdev->func, PCI_REG_COMMAND, cmd | PCI_COMMAND_IO | PCI_COMMAND_BUS_MASTER);

    /* Reset device */
    outb(g_io_base + VIRTIO_REG_DEVICE_STATUS, 0);
    outb(g_io_base + VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    /* Select queue 0 and read queue size */
    outw(g_io_base + VIRTIO_REG_QUEUE_SELECT, 0);
    g_queue_size = inw(g_io_base + VIRTIO_REG_QUEUE_SIZE);
    if (g_queue_size == 0) {
        KLOG_ERR("virtio", "VirtIO queue size is 0");
        return -1;
    }

    /* Allocate physically contiguous page frame for vring */
    g_vring_phys = pmm_alloc_pages(2);
    if (!g_vring_phys) {
        KLOG_ERR("virtio", "Failed to allocate memory for VirtIO queue");
        return -1;
    }

    uint8_t *vring_virt = (uint8_t *)vmm_phys_to_virt(g_vring_phys);
    memset(vring_virt, 0, 8192);

    g_desc = (vring_desc_t *)vring_virt;
    g_avail = (vring_avail_t *)(vring_virt + (g_queue_size * sizeof(vring_desc_t)));

    /* Used ring aligned to 4096 bytes per VirtIO 1.0 spec */
    g_used = (volatile vring_used_t *)(vring_virt + 4096);
    g_last_used_idx = 0;

    /* Program queue address PFN */
    outl(g_io_base + VIRTIO_REG_QUEUE_ADDRESS, (uint32_t)(g_vring_phys >> 12));

    /* Set DRIVER_OK */
    outb(g_io_base + VIRTIO_REG_DEVICE_STATUS,
         VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_DRIVER_OK);

    /* Read capacity in sectors (64-bit value at offset 0x14) */
    uint32_t cap_low = inl(g_io_base + VIRTIO_REG_CONFIG_SECTORS);
    uint32_t cap_high = inl(g_io_base + VIRTIO_REG_CONFIG_SECTORS + 4);
    uint64_t sector_count = ((uint64_t)cap_high << 32) | cap_low;

    g_virtio_ops.read_blocks = virtio_blk_op_read;
    g_virtio_ops.write_blocks = virtio_blk_op_write;
    g_virtio_ops.flush = NULL;

    memset(&g_virtio_bdev, 0, sizeof(block_device_t));
    strcpy(g_virtio_bdev.name, "vda");
    g_virtio_bdev.sector_count = sector_count;
    g_virtio_bdev.sector_size = BLOCK_SECTOR_SIZE;
    g_virtio_bdev.read_only = false;
    g_virtio_bdev.ops = &g_virtio_ops;
    g_virtio_bdev.driver_data = NULL;

    block_device_register(&g_virtio_bdev);

    g_virtio_available = true;
    KLOG_INFO("virtio", "VirtIO block storage online: 'vda' (%lu sectors, %lu MiB)",
              sector_count, (sector_count * 512) / (1024 * 1024));
    return 0;
}

bool virtio_blk_is_available(void)
{
    return g_virtio_available;
}

block_device_t *virtio_blk_get_device(void)
{
    return g_virtio_available ? &g_virtio_bdev : NULL;
}
