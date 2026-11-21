/* ZERO KERNEL - USB Subsystem Architecture Implementation (Milestone 3.0.0 Hardware Foundation). */

#include "drivers/usb.h"
#include "hardware/pci.h"
#include "drivers/driver.h"
#include "zero/log.h"
#include "zero/string.h"

static usb_controller_t g_usb_controllers[MAX_USB_CONTROLLERS];
static size_t g_usb_controller_count = 0;

void usb_init(void)
{
    g_usb_controller_count = 0;

    size_t pci_count = pci_device_count();
    for (size_t i = 0; i < pci_count; i++) {
        pci_device_t *pdev = pci_get_device(i);
        if (!pdev)
            continue;

        /* Look for PCI Class 0x0C (Serial Bus), Subclass 0x03 (USB Controller) */
        if (pdev->class_code == 0x0C && pdev->subclass == 0x03) {
            if (g_usb_controller_count >= MAX_USB_CONTROLLERS) {
                KLOG_WARN("usb", "Maximum USB controllers limit reached");
                break;
            }

            usb_controller_t *ctrl = &g_usb_controllers[g_usb_controller_count++];
            memset(ctrl, 0, sizeof(usb_controller_t));

            ctrl->pci_dev = pdev;
            ctrl->irq = pdev->irq_line;

            switch (pdev->prog_if) {
            case 0x00: ctrl->type = USB_CONTROLLER_UHCI; break;
            case 0x10: ctrl->type = USB_CONTROLLER_OHCI; break;
            case 0x20: ctrl->type = USB_CONTROLLER_EHCI; break;
            case 0x30: ctrl->type = USB_CONTROLLER_XHCI; break;
            default:   ctrl->type = USB_CONTROLLER_UNKNOWN; break;
            }

            if (pdev->bar_size[0] > 0) {
                ctrl->base_addr = pdev->bar[0];
                ctrl->is_io_space = pdev->bar_is_io[0];
            }

            /* Register with Driver Manager */
            ctrl->dev_entry.type = DEVICE_TYPE_USB;
            ctrl->dev_entry.vendor_id = pdev->vendor_id;
            ctrl->dev_entry.product_id = pdev->device_id;
            ctrl->dev_entry.state = DEVICE_STATE_ACTIVE;
            strncpy(ctrl->dev_entry.name, usb_controller_type_name(ctrl->type), sizeof(ctrl->dev_entry.name) - 1);

            device_register(&ctrl->dev_entry);

            KLOG_INFO("usb", "Discovered %s at [%02x:%02x.%x] base: 0x%08lx (%s), IRQ: %u",
                      usb_controller_type_name(ctrl->type), pdev->bus, pdev->slot, pdev->func,
                      ctrl->base_addr, ctrl->is_io_space ? "I/O" : "MMIO", ctrl->irq);
        }
    }

    KLOG_INFO("usb", "USB subsystem architecture initialized (%lu controller(s) discovered)",
              g_usb_controller_count);
}

size_t usb_controller_count(void)
{
    return g_usb_controller_count;
}

const usb_controller_t *usb_get_controller(size_t index)
{
    return (index < g_usb_controller_count) ? &g_usb_controllers[index] : NULL;
}

const char *usb_controller_type_name(usb_controller_type_t type)
{
    switch (type) {
    case USB_CONTROLLER_UHCI: return "UHCI Controller (USB 1.1)";
    case USB_CONTROLLER_OHCI: return "OHCI Controller (USB 1.1)";
    case USB_CONTROLLER_EHCI: return "EHCI Controller (USB 2.0)";
    case USB_CONTROLLER_XHCI: return "xHCI Controller (USB 3.0+)";
    default:                  return "USB Host Controller";
    }
}

void usb_dump_info(void)
{
    KLOG_INFO("usb", "===== ZERO OS USB CONTROLLER INVENTORY (%lu controllers) =====", g_usb_controller_count);
    for (size_t i = 0; i < g_usb_controller_count; i++) {
        const usb_controller_t *c = &g_usb_controllers[i];
        KLOG_INFO("usb", "  USB #%02lu: %-28s Base: 0x%08lx (%s) IRQ: %u",
                  i + 1, usb_controller_type_name(c->type), c->base_addr,
                  c->is_io_space ? "I/O" : "MMIO", c->irq);
    }
    KLOG_INFO("usb", "==============================================================");
}
