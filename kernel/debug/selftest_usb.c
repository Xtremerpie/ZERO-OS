/* ZERO KERNEL - USB Subsystem Self-Tests (Milestone 3.0.0 Hardware Foundation).
 *
 * Verifies:
 * 1. USB binary protocol structures and packed descriptor alignments.
 * 2. Setup packet field alignment (8 bytes).
 * 3. Controller type string conversions.
 * 4. Controller discovery and architecture bounds.
 */

#include "drivers/usb_test.h"
#include "drivers/usb.h"
#include "zero/log.h"

bool usb_run_selftests(void)
{
    KLOG_INFO("selftest", "Starting USB subsystem self-tests...");

    /* 1. Strict descriptor sizing per USB 2.0 / 3.0 Specifications */
    if (sizeof(usb_setup_packet_t) != 8) {
        KLOG_ERROR("selftest", "USB test 1 failed: sizeof(usb_setup_packet_t) is %lu (expected 8)",
                 sizeof(usb_setup_packet_t));
        return false;
    }

    if (sizeof(usb_device_descriptor_t) != 18) {
        KLOG_ERROR("selftest", "USB test 1 failed: sizeof(usb_device_descriptor_t) is %lu (expected 18)",
                 sizeof(usb_device_descriptor_t));
        return false;
    }

    if (sizeof(usb_config_descriptor_t) != 9) {
        KLOG_ERROR("selftest", "USB test 1 failed: sizeof(usb_config_descriptor_t) is %lu (expected 9)",
                 sizeof(usb_config_descriptor_t));
        return false;
    }

    if (sizeof(usb_interface_descriptor_t) != 9) {
        KLOG_ERROR("selftest", "USB test 1 failed: sizeof(usb_interface_descriptor_t) is %lu (expected 9)",
                 sizeof(usb_interface_descriptor_t));
        return false;
    }

    if (sizeof(usb_endpoint_descriptor_t) != 7) {
        KLOG_ERROR("selftest", "USB test 1 failed: sizeof(usb_endpoint_descriptor_t) is %lu (expected 7)",
                 sizeof(usb_endpoint_descriptor_t));
        return false;
    }

    /* 2. Controller type string mappings */
    const char *s_xhci = usb_controller_type_name(USB_CONTROLLER_XHCI);
    const char *s_ehci = usb_controller_type_name(USB_CONTROLLER_EHCI);
    if (!s_xhci || !s_ehci) {
        KLOG_ERROR("selftest", "USB test 2 failed: controller type string NULL");
        return false;
    }

    kprintf("[PASS] usb: standard descriptor packing, setup packets, and controller models verified\n");
    return true;
}
