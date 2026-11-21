/* ZERO KERNEL - Driver and Device Abstraction Layer (Milestone 3.0.0 Hardware Foundation).
 *
 * Defines device classes, resource tracking, unified driver lifecycle interfaces,
 * and the central device registry.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define MAX_DEVICE_RESOURCES 8u
#define MAX_DEVICES          128u

typedef enum {
    DEVICE_TYPE_UNKNOWN = 0,
    DEVICE_TYPE_SYSTEM,
    DEVICE_TYPE_PCI,
    DEVICE_TYPE_INPUT,
    DEVICE_TYPE_BLOCK,
    DEVICE_TYPE_DISPLAY,
    DEVICE_TYPE_TIMER,
    DEVICE_TYPE_NETWORK,
    DEVICE_TYPE_USB
} device_type_t;

typedef enum {
    DEVICE_STATE_UNINITIALIZED = 0,
    DEVICE_STATE_ACTIVE,
    DEVICE_STATE_FAILED,
    DEVICE_STATE_DISABLED
} device_state_t;

typedef enum {
    RES_NONE = 0,
    RES_IO_PORT,
    RES_MMIO,
    RES_IRQ
} resource_type_t;

typedef struct {
    resource_type_t type;
    uint64_t base;
    uint64_t size;
    uint32_t irq;
} device_resource_t;

struct device;
struct driver;

typedef struct driver {
    const char *name;
    device_type_t supported_type;
    int (*init)(struct device *dev);
    int (*start)(struct device *dev);
    int (*stop)(struct device *dev);
    int (*handle_irq)(struct device *dev, uint32_t irq);
} driver_t;

typedef struct device {
    char name[64];
    device_type_t type;
    device_state_t state;
    uint32_t id;
    uint16_t vendor_id;
    uint16_t product_id;

    device_resource_t resources[MAX_DEVICE_RESOURCES];
    size_t resource_count;

    driver_t *driver;
    void *driver_data;

    struct device *next;
} device_t;

/* Initialize central driver manager and device registry */
void driver_manager_init(void);

/* Register a discovered hardware device with the system */
int device_register(device_t *dev);

/* Retrieve device by name */
device_t *device_get_by_name(const char *name);

/* Retrieve device of specific type by index */
device_t *device_get_by_type(device_type_t type, size_t index);

/* Total registered devices */
size_t device_count(void);

/* Count devices of specific type */
size_t device_count_by_type(device_type_t type);

/* Output formatted device registry inventory to diagnostic log */
void device_dump_inventory(void);

/* Convert device type to readable string */
const char *device_type_name(device_type_t type);

/* Convert device state to readable string */
const char *device_state_name(device_state_t state);
