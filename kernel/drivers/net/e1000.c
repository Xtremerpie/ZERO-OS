/* ZERO KERNEL - Intel 82540EM (E1000) Gigabit Ethernet Driver Implementation (Milestone 4.0.0 Storage & Networking).
 *
 * Implements hardware discovery via PCI, MMIO register access, DMA descriptor ring setup,
 * transmit/receive packet processing, and loopback fallback for testing.
 */
#include "drivers/net/e1000.h"
#include "hardware/pci.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "memory/heap.h"
#include "zero/log.h"
#include "zero/spinlock.h"
#include "lib/string.h"

/* E1000 Registers */
#define REG_CTRL       0x0000u
#define REG_STATUS     0x0008u
#define REG_EECD       0x0010u
#define REG_EERD       0x0014u
#define REG_ICR        0x00C0u
#define REG_IMS        0x00D0u
#define REG_IMC        0x00D8u
#define REG_RCTL       0x0100u
#define REG_TCTL       0x0400u
#define REG_TIPG       0x0410u
#define REG_RDBAL      0x2800u
#define REG_RDBAH      0x2804u
#define REG_RDLEN      0x2808u
#define REG_RDH        0x2810u
#define REG_RDT        0x2818u
#define REG_TDBAL      0x3800u
#define REG_TDBAH      0x3804u
#define REG_TDLEN      0x3808u
#define REG_TDH        0x3810u
#define REG_TDT        0x3818u
#define REG_RAL        0x5400u
#define REG_RAH        0x5404u

/* Control Register Bits */
#define CTRL_SLU       (1u << 6)  /* Set Link Up */
#define CTRL_RST       (1u << 26) /* Full Device Reset */

/* RCTL (Receive Control) Bits */
#define RCTL_EN        (1u << 1)
#define RCTL_SBP       (1u << 2)
#define RCTL_UPE       (1u << 3)
#define RCTL_MPE       (1u << 4)
#define RCTL_BAM       (1u << 15) /* Broadcast Accept */
#define RCTL_BSIZE_2048 (0u << 16)
#define RCTL_SECRC     (1u << 26) /* Strip Ethernet CRC */

/* TCTL (Transmit Control) Bits */
#define TCTL_EN        (1u << 1)
#define TCTL_PSP       (1u << 3)
#define TCTL_CT_SHIFT  4
#define TCTL_COLD_SHIFT 12

/* RX Descriptor Status Bits */
#define E1000_RXD_STAT_DD  (1u << 0)
#define E1000_RXD_STAT_EOP (1u << 1)

/* TX Descriptor Command Bits */
#define E1000_TXD_CMD_EOP  (1u << 0)
#define E1000_TXD_CMD_IFCS (1u << 1)
#define E1000_TXD_CMD_RS   (1u << 3)

/* TX Descriptor Status Bits */
#define E1000_TXD_STAT_DD  (1u << 0)

typedef struct __attribute__((packed)) e1000_rx_desc {
    uint64_t buffer_addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} e1000_rx_desc_t;

typedef struct __attribute__((packed)) e1000_tx_desc {
    uint64_t buffer_addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} e1000_tx_desc_t;

static bool g_e1000_present = false;
static uintptr_t g_mmio_base = 0;
static net_if_t g_e1000_netif;
static spinlock_t g_e1000_lock;

static e1000_rx_desc_t *g_rx_descs = NULL;
static uint8_t *g_rx_buffers[E1000_NUM_RX_DESC];
static uint16_t g_rx_cur = 0;

static e1000_tx_desc_t *g_tx_descs = NULL;
static uint8_t *g_tx_buffers[E1000_NUM_TX_DESC];
static uint16_t g_tx_cur = 0;

static inline void e1000_write32(uint32_t reg, uint32_t val)
{
    if (g_mmio_base) {
        *(volatile uint32_t *)(g_mmio_base + reg) = val;
    }
}

static inline uint32_t e1000_read32(uint32_t reg)
{
    if (!g_mmio_base) return 0;
    return *(volatile uint32_t *)(g_mmio_base + reg);
}

bool e1000_is_present(void)
{
    return g_e1000_present;
}

int e1000_send_packet(net_if_t *netif, net_buf_t *buf)
{
    (void)netif;
    if (!buf) return -1;

    if (!g_e1000_present) {
        /* Loopback fallback: immediately reflect packet back as ingress */
        net_buf_t *rx_buf = net_buf_alloc(NET_BUF_HEADROOM);
        if (rx_buf) {
            void *p = net_buf_put(rx_buf, buf->len);
            if (p) {
                memcpy(p, buf->data, buf->len);
                net_rx(netif, rx_buf);
            } else {
                net_buf_free(rx_buf);
            }
        }
        return 0;
    }

    spin_lock(&g_e1000_lock);
    e1000_tx_desc_t *desc = &g_tx_descs[g_tx_cur];

    /* Wait for descriptor to be free if previously sent */
    if (desc->cmd && !(desc->status & E1000_TXD_STAT_DD)) {
        spin_unlock(&g_e1000_lock);
        return -2; /* TX ring full */
    }

    size_t len = buf->len;
    if (len > 2048) len = 2048;

    memcpy(g_tx_buffers[g_tx_cur], buf->data, len);
    desc->length = (uint16_t)len;
    desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    desc->status = 0;

    g_tx_cur = (g_tx_cur + 1) % E1000_NUM_TX_DESC;
    e1000_write32(REG_TDT, g_tx_cur);

    spin_unlock(&g_e1000_lock);
    return 0;
}

void e1000_poll_rx(net_if_t *netif)
{
    if (!g_e1000_present || !g_rx_descs) return;

    spin_lock(&g_e1000_lock);
    while (g_rx_descs[g_rx_cur].status & E1000_RXD_STAT_DD) {
        e1000_rx_desc_t *desc = &g_rx_descs[g_rx_cur];
        uint16_t len = desc->length;

        if (len > 0 && len <= 2048) {
            net_buf_t *buf = net_buf_alloc(NET_BUF_HEADROOM);
            if (buf) {
                void *p = net_buf_put(buf, len);
                if (p) {
                    memcpy(p, g_rx_buffers[g_rx_cur], len);
                    spin_unlock(&g_e1000_lock);
                    net_rx(netif, buf);
                    spin_lock(&g_e1000_lock);
                } else {
                    net_buf_free(buf);
                }
            }
        }

        desc->status = 0;
        uint16_t old_cur = g_rx_cur;
        g_rx_cur = (g_rx_cur + 1) % E1000_NUM_RX_DESC;
        e1000_write32(REG_RDT, old_cur);
    }
    spin_unlock(&g_e1000_lock);
}

bool e1000_init(void)
{
    spinlock_init(&g_e1000_lock);
    memset(&g_e1000_netif, 0, sizeof(g_e1000_netif));
    strncpy(g_e1000_netif.name, "eth0", sizeof(g_e1000_netif.name) - 1);
    g_e1000_netif.mtu = NET_DEFAULT_MTU;
    g_e1000_netif.send = e1000_send_packet;
    g_e1000_netif.poll = e1000_poll_rx;

    pci_device_t *pdev = pci_find_device(E1000_VENDOR_INTEL, E1000_DEV_82540EM);
    if (!pdev) {
        pdev = pci_find_device(E1000_VENDOR_INTEL, E1000_DEV_82543GC);
    }
    if (!pdev) {
        pdev = pci_find_device(E1000_VENDOR_INTEL, E1000_DEV_82545EM);
    }

    if (!pdev) {
        KLOG_INFO("e1000", "physical Intel 82540EM NIC not present on PCI bus; creating synthetic eth0 interface");
        /* Synthetic eth0 interface for deterministic testing */
        g_e1000_netif.mac[0] = 0x52;
        g_e1000_netif.mac[1] = 0x54;
        g_e1000_netif.mac[2] = 0x00;
        g_e1000_netif.mac[3] = 0x12;
        g_e1000_netif.mac[4] = 0x34;
        g_e1000_netif.mac[5] = 0x56;
        g_e1000_netif.ip = IP4_ADDR(10, 0, 2, 15);
        g_e1000_netif.netmask = IP4_ADDR(255, 255, 255, 0);
        g_e1000_netif.gateway = IP4_ADDR(10, 0, 2, 2);
        g_e1000_netif.dns = IP4_ADDR(10, 0, 2, 3);
        g_e1000_netif.flags = NETIF_FLAG_UP | NETIF_FLAG_LINK_UP | NETIF_FLAG_BROADCAST;
        net_if_register(&g_e1000_netif);
        return true;
    }

    /* The E1000 register block is an MMIO BAR. Never dereference an
     * HHDM pointer until the BAR's physical range has an actual page-table
     * mapping. PCI BAR discovery alone does not map device memory. */
    if (pdev->bar_is_io[0] || pdev->bar_size[0] < (REG_RAH + sizeof(uint32_t))) {
        KLOG_WARN("e1000", "unsupported BAR0 type/size; skipping physical NIC");
        return false;
    }

    uint64_t bar0_phys = (uint64_t)(pdev->bar[0] & ~0xFu);
    if (bar0_phys == 0 ||
        vmm_ensure_hhdm_mapping(bar0_phys, pdev->bar_size[0]) != VMM_OK) {
        KLOG_WARN("e1000", "BAR0 MMIO range 0x%llX (size 0x%X) could not be mapped; skipping physical NIC",
                  (unsigned long long)bar0_phys, pdev->bar_size[0]);
        return false;
    }

    /* Enable bus mastering and memory space only after the MMIO range is
     * mapped, so no register access can fault on an absent HHDM mapping. */
    uint16_t cmd = pci_read16(pdev->bus, pdev->slot, pdev->func, PCI_REG_COMMAND);
    cmd |= (PCI_COMMAND_BUS_MASTER | PCI_COMMAND_MEMORY);
    pci_write16(pdev->bus, pdev->slot, pdev->func, PCI_REG_COMMAND, cmd);

    g_mmio_base = (uintptr_t)vmm_phys_to_virt(bar0_phys);
    g_e1000_present = true;

    /* Read MAC address */
    uint32_t ral = e1000_read32(REG_RAL);
    uint32_t rah = e1000_read32(REG_RAH);

    if (rah & (1u << 31)) {
        g_e1000_netif.mac[0] = (uint8_t)(ral & 0xFFu);
        g_e1000_netif.mac[1] = (uint8_t)((ral >> 8) & 0xFFu);
        g_e1000_netif.mac[2] = (uint8_t)((ral >> 16) & 0xFFu);
        g_e1000_netif.mac[3] = (uint8_t)((ral >> 24) & 0xFFu);
        g_e1000_netif.mac[4] = (uint8_t)(rah & 0xFFu);
        g_e1000_netif.mac[5] = (uint8_t)((rah >> 8) & 0xFFu);
    } else {
        /* Default QEMU MAC fallback */
        g_e1000_netif.mac[0] = 0x52;
        g_e1000_netif.mac[1] = 0x54;
        g_e1000_netif.mac[2] = 0x00;
        g_e1000_netif.mac[3] = 0x12;
        g_e1000_netif.mac[4] = 0x34;
        g_e1000_netif.mac[5] = 0x56;
    }

    /* Allocate and initialize RX descriptor ring */
    uint64_t rx_desc_frame = pmm_alloc_page();
    g_rx_descs = (e1000_rx_desc_t *)vmm_phys_to_virt(rx_desc_frame);
    memset(g_rx_descs, 0, VMM_PAGE_SIZE);

    for (size_t i = 0; i < E1000_NUM_RX_DESC; i++) {
        uint64_t buf_frame = pmm_alloc_page();
        g_rx_buffers[i] = (uint8_t *)vmm_phys_to_virt(buf_frame);
        g_rx_descs[i].buffer_addr = buf_frame;
        g_rx_descs[i].status = 0;
    }

    e1000_write32(REG_RDBAL, (uint32_t)(rx_desc_frame & 0xFFFFFFFFu));
    e1000_write32(REG_RDBAH, (uint32_t)(rx_desc_frame >> 32));
    e1000_write32(REG_RDLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    e1000_write32(REG_RDH, 0);
    e1000_write32(REG_RDT, E1000_NUM_RX_DESC - 1);
    g_rx_cur = 0;

    e1000_write32(REG_RCTL, RCTL_EN | RCTL_BAM | RCTL_SECRC | RCTL_BSIZE_2048);

    /* Allocate and initialize TX descriptor ring */
    uint64_t tx_desc_frame = pmm_alloc_page();
    g_tx_descs = (e1000_tx_desc_t *)vmm_phys_to_virt(tx_desc_frame);
    memset(g_tx_descs, 0, VMM_PAGE_SIZE);

    for (size_t i = 0; i < E1000_NUM_TX_DESC; i++) {
        uint64_t tbuf_frame = pmm_alloc_page();
        g_tx_buffers[i] = (uint8_t *)vmm_phys_to_virt(tbuf_frame);
        g_tx_descs[i].buffer_addr = tbuf_frame;
        g_tx_descs[i].cmd = 0;
        g_tx_descs[i].status = E1000_TXD_STAT_DD;
    }

    e1000_write32(REG_TDBAL, (uint32_t)(tx_desc_frame & 0xFFFFFFFFu));
    e1000_write32(REG_TDBAH, (uint32_t)(tx_desc_frame >> 32));
    e1000_write32(REG_TDLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    e1000_write32(REG_TDH, 0);
    e1000_write32(REG_TDT, 0);
    g_tx_cur = 0;

    e1000_write32(REG_TCTL, TCTL_EN | TCTL_PSP | (15u << TCTL_CT_SHIFT) | (0x40u << TCTL_COLD_SHIFT));
    e1000_write32(REG_TIPG, 10u | (8u << 10) | (6u << 20));

    /* Set Link Up */
    e1000_write32(REG_CTRL, e1000_read32(REG_CTRL) | CTRL_SLU);

    g_e1000_netif.ip = IP4_ADDR(10, 0, 2, 15);
    g_e1000_netif.netmask = IP4_ADDR(255, 255, 255, 0);
    g_e1000_netif.gateway = IP4_ADDR(10, 0, 2, 2);
    g_e1000_netif.dns = IP4_ADDR(10, 0, 2, 3);
    g_e1000_netif.flags = NETIF_FLAG_UP | NETIF_FLAG_LINK_UP | NETIF_FLAG_BROADCAST;

    net_if_register(&g_e1000_netif);

    KLOG_INFO("e1000", "Intel 82540EM initialized (BAR0 0x%llX, MAC %02X:%02X:%02X:%02X:%02X:%02X)",
              (unsigned long long)bar0_phys,
              g_e1000_netif.mac[0], g_e1000_netif.mac[1], g_e1000_netif.mac[2],
              g_e1000_netif.mac[3], g_e1000_netif.mac[4], g_e1000_netif.mac[5]);
    return true;
}
