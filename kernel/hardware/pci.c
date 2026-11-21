/* ZERO KERNEL - PCI / PCIe Discovery Implementation (Milestone 3.0.0 Hardware Foundation). */

#include "hardware/pci.h"
#include "drivers/driver.h"
#include "zero/cpu.h"
#include "zero/log.h"
#include "zero/string.h"

#define PCI_CONFIG_ADDRESS 0x0CF8
#define PCI_CONFIG_DATA    0x0CFC

static pci_device_t g_pci_devices[PCI_MAX_DEVICES];
static size_t g_pci_device_count = 0;

static inline uint32_t pci_make_address(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    return (1u << 31) |
           ((uint32_t)bus << 16) |
           ((uint32_t)slot << 11) |
           ((uint32_t)func << 8) |
           (offset & 0xFC);
}

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    outl(PCI_CONFIG_ADDRESS, pci_make_address(bus, slot, func, offset));
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    outl(PCI_CONFIG_ADDRESS, pci_make_address(bus, slot, func, offset));
    uint32_t val = inl(PCI_CONFIG_DATA);
    return (uint16_t)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    outl(PCI_CONFIG_ADDRESS, pci_make_address(bus, slot, func, offset));
    uint32_t val = inl(PCI_CONFIG_DATA);
    return (uint8_t)((val >> ((offset & 3) * 8)) & 0xFF);
}

void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val)
{
    outl(PCI_CONFIG_ADDRESS, pci_make_address(bus, slot, func, offset));
    outl(PCI_CONFIG_DATA, val);
}

void pci_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val)
{
    uint32_t current = pci_read32(bus, slot, func, offset & ~3);
    uint32_t shift = (offset & 2) * 8;
    current = (current & ~(0xFFFF << shift)) | ((uint32_t)val << shift);
    pci_write32(bus, slot, func, offset & ~3, current);
}

void pci_write8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t val)
{
    uint32_t current = pci_read32(bus, slot, func, offset & ~3);
    uint32_t shift = (offset & 3) * 8;
    current = (current & ~(0xFF << shift)) | ((uint32_t)val << shift);
    pci_write32(bus, slot, func, offset & ~3, current);
}

static void pci_probe_bars(pci_device_t *dev)
{
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t offset = PCI_REG_BAR0 + (i * 4);
        uint32_t orig = pci_read32(dev->bus, dev->slot, dev->func, offset);

        if (orig == 0 || orig == 0xFFFFFFFF) {
            dev->bar[i] = 0;
            dev->bar_size[i] = 0;
            dev->bar_is_io[i] = false;
            continue;
        }

        /* Probe size safely */
        pci_write32(dev->bus, dev->slot, dev->func, offset, 0xFFFFFFFF);
        uint32_t mask = pci_read32(dev->bus, dev->slot, dev->func, offset);
        pci_write32(dev->bus, dev->slot, dev->func, offset, orig);

        if (mask == 0 || mask == 0xFFFFFFFF) {
            dev->bar[i] = 0;
            dev->bar_size[i] = 0;
            dev->bar_is_io[i] = false;
            continue;
        }

        if (orig & 1) {
            /* I/O Space BAR */
            dev->bar_is_io[i] = true;
            dev->bar[i] = orig & ~0x3u;
            dev->bar_size[i] = ~(mask & ~0x3u) + 1;
        } else {
            /* Memory Space BAR */
            dev->bar_is_io[i] = false;
            dev->bar[i] = orig & ~0xFu;
            dev->bar_size[i] = ~(mask & ~0xFu) + 1;
        }
    }
}

static void pci_check_function(uint8_t bus, uint8_t slot, uint8_t func)
{
    uint16_t vendor_id = pci_read16(bus, slot, func, PCI_REG_VENDOR_ID);
    if (vendor_id == 0xFFFF || vendor_id == 0x0000)
        return;

    if (g_pci_device_count >= PCI_MAX_DEVICES) {
        KLOG_WARN("pci", "PCI device limit reached (%u)", PCI_MAX_DEVICES);
        return;
    }

    pci_device_t *dev = &g_pci_devices[g_pci_device_count++];
    memset(dev, 0, sizeof(pci_device_t));

    dev->bus = bus;
    dev->slot = slot;
    dev->func = func;
    dev->vendor_id = vendor_id;
    dev->device_id = pci_read16(bus, slot, func, PCI_REG_DEVICE_ID);
    dev->class_code = pci_read8(bus, slot, func, PCI_REG_CLASS);
    dev->subclass = pci_read8(bus, slot, func, PCI_REG_SUBCLASS);
    dev->prog_if = pci_read8(bus, slot, func, PCI_REG_PROG_IF);
    dev->revision = pci_read8(bus, slot, func, PCI_REG_REVISION);
    dev->header_type = pci_read8(bus, slot, func, PCI_REG_HEADER_TYPE) & 0x7F;
    dev->irq_line = pci_read8(bus, slot, func, PCI_REG_INTERRUPT_LINE);
    dev->irq_pin = pci_read8(bus, slot, func, PCI_REG_INTERRUPT_PIN);

    /* Probe BARs if standard device header */
    if (dev->header_type == PCI_HEADER_NORMAL) {
        pci_probe_bars(dev);
    }

    /* Register with central driver manager */
    dev->dev_entry.type = DEVICE_TYPE_PCI;
    dev->dev_entry.vendor_id = dev->vendor_id;
    dev->dev_entry.product_id = dev->device_id;
    dev->dev_entry.state = DEVICE_STATE_ACTIVE;

    /* Build device name */
    const char *cname = pci_class_name(dev->class_code, dev->subclass);
    strncpy(dev->dev_entry.name, cname, sizeof(dev->dev_entry.name) - 1);

    /* Populate resources */
    for (size_t b = 0; b < 6 && dev->dev_entry.resource_count < MAX_DEVICE_RESOURCES; b++) {
        if (dev->bar_size[b] > 0) {
            size_t r = dev->dev_entry.resource_count++;
            dev->dev_entry.resources[r].type = dev->bar_is_io[b] ? RES_IO_PORT : RES_MMIO;
            dev->dev_entry.resources[r].base = dev->bar[b];
            dev->dev_entry.resources[r].size = dev->bar_size[b];
        }
    }

    if (dev->irq_line != 0xFF && dev->irq_line != 0 && dev->dev_entry.resource_count < MAX_DEVICE_RESOURCES) {
        size_t r = dev->dev_entry.resource_count++;
        dev->dev_entry.resources[r].type = RES_IRQ;
        dev->dev_entry.resources[r].irq = dev->irq_line;
    }

    device_register(&dev->dev_entry);
}

void pci_init(void)
{
    g_pci_device_count = 0;

    /* Check bus 0 function 0 header to see if host controller is multifunction (multiple PCI root buses) */
    uint8_t root_header = pci_read8(0, 0, 0, PCI_REG_HEADER_TYPE);
    uint8_t max_root_buses = (root_header & PCI_HEADER_MULTIFUNCTION) ? 8 : 1;

    for (uint8_t func = 0; func < max_root_buses; func++) {
        if (pci_read16(0, 0, func, PCI_REG_VENDOR_ID) == 0xFFFF)
            break;

        uint8_t bus = func;
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t v = pci_read16(bus, slot, 0, PCI_REG_VENDOR_ID);
            if (v == 0xFFFF || v == 0x0000)
                continue;

            pci_check_function(bus, slot, 0);

            uint8_t header = pci_read8(bus, slot, 0, PCI_REG_HEADER_TYPE);
            if (header & PCI_HEADER_MULTIFUNCTION) {
                for (uint8_t f = 1; f < 8; f++) {
                    pci_check_function(bus, slot, f);
                }
            }
        }
    }

    KLOG_INFO("pci", "PCI enumeration complete: %lu device(s) found", g_pci_device_count);
}

pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id)
{
    for (size_t i = 0; i < g_pci_device_count; i++) {
        if (g_pci_devices[i].vendor_id == vendor_id && g_pci_devices[i].device_id == device_id) {
            return &g_pci_devices[i];
        }
    }
    return NULL;
}

pci_device_t *pci_find_class(uint8_t class_code, uint8_t subclass)
{
    for (size_t i = 0; i < g_pci_device_count; i++) {
        if (g_pci_devices[i].class_code == class_code && g_pci_devices[i].subclass == subclass) {
            return &g_pci_devices[i];
        }
    }
    return NULL;
}

size_t pci_device_count(void)
{
    return g_pci_device_count;
}

pci_device_t *pci_get_device(size_t index)
{
    return (index < g_pci_device_count) ? &g_pci_devices[index] : NULL;
}

const char *pci_class_name(uint8_t class_code, uint8_t subclass)
{
    switch (class_code) {
    case 0x00: return "Legacy Device";
    case 0x01:
        switch (subclass) {
        case 0x01: return "IDE Storage";
        case 0x06: return "SATA AHCI Storage";
        case 0x08: return "NVMe Storage";
        default:   return "Mass Storage";
        }
    case 0x02: return "Network Controller";
    case 0x03: return "Display Controller (VGA)";
    case 0x04: return "Multimedia Controller";
    case 0x05: return "Memory Controller";
    case 0x06: return "Bridge Device";
    case 0x07: return "Simple Comm Controller";
    case 0x08: return "Generic System Peripheral";
    case 0x09: return "Input Device";
    case 0x0C:
        switch (subclass) {
        case 0x03: return "USB Controller";
        default:   return "Serial Bus Controller";
        }
    default:   return "PCI Unknown Device";
    }
}

void pci_dump_devices(void)
{
    KLOG_INFO("pci", "===== ZERO OS PCI DEVICE BUS INVENTORY (%lu devices) =====", g_pci_device_count);
    for (size_t i = 0; i < g_pci_device_count; i++) {
        const pci_device_t *d = &g_pci_devices[i];
        KLOG_INFO("pci", "  [%02x:%02x.%x] %04x:%04x Class %02x:%02x (%s) Rev: %02x IRQ: %u",
                  d->bus, d->slot, d->func, d->vendor_id, d->device_id,
                  d->class_code, d->subclass, pci_class_name(d->class_code, d->subclass),
                  d->revision, d->irq_line);
        for (int b = 0; b < 6; b++) {
            if (d->bar_size[b] > 0) {
                KLOG_INFO("pci", "       BAR%d: 0x%08x (%s, size: 0x%x)",
                          b, d->bar[b], d->bar_is_io[b] ? "I/O" : "MMIO", d->bar_size[b]);
            }
        }
    }
    KLOG_INFO("pci", "==========================================================");
}
