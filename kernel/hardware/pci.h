/* ZERO KERNEL - PCI / PCIe Discovery Interface (Milestone 3.0.0 Hardware Foundation).
 *
 * Implements PCI Type 1 Configuration Space access, bus/device/function enumeration,
 * BAR probing, interrupt configuration extraction, and driver registry integration.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "drivers/driver.h"

#define PCI_MAX_DEVICES 64u

/* PCI Configuration Registers */
#define PCI_REG_VENDOR_ID       0x00
#define PCI_REG_DEVICE_ID       0x02
#define PCI_REG_COMMAND         0x04
#define PCI_REG_STATUS          0x06
#define PCI_REG_REVISION        0x08
#define PCI_REG_PROG_IF         0x09
#define PCI_REG_SUBCLASS        0x0A
#define PCI_REG_CLASS           0x0B
#define PCI_REG_CACHE_LINE_SIZE 0x0C
#define PCI_REG_LATENCY_TIMER   0x0D
#define PCI_REG_HEADER_TYPE     0x0E
#define PCI_REG_BIST            0x0F
#define PCI_REG_BAR0            0x10
#define PCI_REG_BAR1            0x14
#define PCI_REG_BAR2            0x18
#define PCI_REG_BAR3            0x1C
#define PCI_REG_BAR4            0x20
#define PCI_REG_BAR5            0x24
#define PCI_REG_INTERRUPT_LINE  0x3C
#define PCI_REG_INTERRUPT_PIN   0x3D

/* Command register bits */
#define PCI_COMMAND_IO          (1u << 0)
#define PCI_COMMAND_MEMORY      (1u << 1)
#define PCI_COMMAND_BUS_MASTER  (1u << 2)

/* Header Types */
#define PCI_HEADER_NORMAL       0x00
#define PCI_HEADER_BRIDGE       0x01
#define PCI_HEADER_CARDBUS      0x02
#define PCI_HEADER_MULTIFUNCTION 0x80

typedef struct pci_device {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;

    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision;
    uint8_t header_type;

    uint32_t bar[6];
    uint32_t bar_size[6];
    bool bar_is_io[6];

    uint8_t irq_line;
    uint8_t irq_pin;

    device_t dev_entry;
} pci_device_t;

/* Initialize PCI subsystem and enumerate bus */
void pci_init(void);

/* Configuration space read primitives */
uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint8_t pci_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

/* Configuration space write primitives */
void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);
void pci_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val);
void pci_write8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t val);

/* Search discovered devices */
pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id);
pci_device_t *pci_find_class(uint8_t class_code, uint8_t subclass);

/* Device list access */
size_t pci_device_count(void);
#define pci_get_device_count() pci_device_count()
pci_device_t *pci_get_device(size_t index);

/* Look up human-readable class description */
const char *pci_class_name(uint8_t class_code, uint8_t subclass);

/* Print all discovered PCI devices to diagnostic log */
void pci_dump_devices(void);
