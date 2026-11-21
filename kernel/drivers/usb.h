/* ZERO KERNEL - USB Subsystem Architecture (Milestone 3.0.0 Hardware Foundation).
 *
 * Defines the USB host controller abstraction, standard USB descriptor models,
 * transfer request blocks, endpoint descriptors, and PCI USB controller discovery.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "hardware/pci.h"
#include "drivers/driver.h"

#define MAX_USB_CONTROLLERS 8u
#define MAX_USB_DEVICES     16u

typedef enum {
    USB_CONTROLLER_UNKNOWN = 0,
    USB_CONTROLLER_UHCI,    /* USB 1.1 Universal Host Controller */
    USB_CONTROLLER_OHCI,    /* USB 1.1 Open Host Controller */
    USB_CONTROLLER_EHCI,    /* USB 2.0 Enhanced Host Controller */
    USB_CONTROLLER_XHCI     /* USB 3.0+ Extensible Host Controller */
} usb_controller_type_t;

typedef enum {
    USB_SPEED_UNKNOWN = 0,
    USB_SPEED_LOW,
    USB_SPEED_FULL,
    USB_SPEED_HIGH,
    USB_SPEED_SUPER
} usb_speed_t;

typedef enum {
    USB_TRANSFER_CONTROL = 0,
    USB_TRANSFER_ISOCHRONOUS,
    USB_TRANSFER_BULK,
    USB_TRANSFER_INTERRUPT
} usb_transfer_type_t;

/* Standard USB Setup Packet (8 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_packet_t;

/* Standard USB Device Descriptor (18 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} usb_device_descriptor_t;

/* Standard USB Configuration Descriptor (9 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
} usb_config_descriptor_t;

/* Standard USB Interface Descriptor (9 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} usb_interface_descriptor_t;

/* Standard USB Endpoint Descriptor (7 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} usb_endpoint_descriptor_t;

/* USB Host Controller Representation */
typedef struct {
    usb_controller_type_t type;
    pci_device_t *pci_dev;
    uint64_t base_addr;
    bool is_io_space;
    uint8_t irq;
    device_t dev_entry;
} usb_controller_t;

/* Initialize USB subsystem and discover USB host controllers via PCI */
void usb_init(void);

/* Number of discovered USB host controllers */
size_t usb_controller_count(void);

/* Retrieve USB host controller by index */
const usb_controller_t *usb_get_controller(size_t index);

/* Convert controller type to readable string */
const char *usb_controller_type_name(usb_controller_type_t type);

/* Output discovered USB controllers and architectural status */
void usb_dump_info(void);
