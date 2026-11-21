/* ZERO KERNEL - Driver and Device Manager Implementation (Milestone 3.0.0 Hardware Foundation). */

#include "drivers/driver.h"
#include "zero/spinlock.h"
#include "zero/log.h"
#include "zero/string.h"

static device_t *g_device_list_head = NULL;
static size_t g_device_total = 0;
static spinlock_t g_driver_lock;

void driver_manager_init(void)
{
    spinlock_init(&g_driver_lock);
    g_device_list_head = NULL;
    g_device_total = 0;
    KLOG_INFO("driver", "Hardware device abstraction manager initialized");
}

int device_register(device_t *dev)
{
    if (!dev)
        return -1;

    spinlock_acquire(&g_driver_lock);

    if (g_device_total >= MAX_DEVICES) {
        spinlock_release(&g_driver_lock);
        KLOG_ERR("driver", "Cannot register device '%s': registry full", dev->name);
        return -1;
    }

    dev->id = (uint32_t)(g_device_total + 1);
    dev->next = g_device_list_head;
    g_device_list_head = dev;
    g_device_total++;

    if (dev->driver && dev->driver->init) {
        int res = dev->driver->init(dev);
        if (res == 0) {
            dev->state = DEVICE_STATE_ACTIVE;
        } else {
            dev->state = DEVICE_STATE_FAILED;
            KLOG_WARN("driver", "Device '%s' driver init returned %d", dev->name, res);
        }
    } else {
        dev->state = DEVICE_STATE_ACTIVE;
    }

    spinlock_release(&g_driver_lock);
    KLOG_INFO("driver", "Registered device #%u: '%s' [%s] state=%s",
              dev->id, dev->name, device_type_name(dev->type), device_state_name(dev->state));
    return 0;
}

device_t *device_get_by_name(const char *name)
{
    if (!name)
        return NULL;

    spinlock_acquire(&g_driver_lock);
    device_t *curr = g_device_list_head;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            spinlock_release(&g_driver_lock);
            return curr;
        }
        curr = curr->next;
    }
    spinlock_release(&g_driver_lock);
    return NULL;
}

device_t *device_get_by_type(device_type_t type, size_t index)
{
    spinlock_acquire(&g_driver_lock);
    device_t *curr = g_device_list_head;
    size_t match_idx = 0;
    while (curr) {
        if (curr->type == type) {
            if (match_idx == index) {
                spinlock_release(&g_driver_lock);
                return curr;
            }
            match_idx++;
        }
        curr = curr->next;
    }
    spinlock_release(&g_driver_lock);
    return NULL;
}

size_t device_count(void)
{
    return g_device_total;
}

size_t device_count_by_type(device_type_t type)
{
    spinlock_acquire(&g_driver_lock);
    device_t *curr = g_device_list_head;
    size_t count = 0;
    while (curr) {
        if (curr->type == type)
            count++;
        curr = curr->next;
    }
    spinlock_release(&g_driver_lock);
    return count;
}

const char *device_type_name(device_type_t type)
{
    switch (type) {
    case DEVICE_TYPE_SYSTEM:  return "SYSTEM";
    case DEVICE_TYPE_PCI:     return "PCI";
    case DEVICE_TYPE_INPUT:   return "INPUT";
    case DEVICE_TYPE_BLOCK:   return "BLOCK";
    case DEVICE_TYPE_DISPLAY: return "DISPLAY";
    case DEVICE_TYPE_TIMER:   return "TIMER";
    case DEVICE_TYPE_NETWORK: return "NETWORK";
    case DEVICE_TYPE_USB:     return "USB";
    default:                  return "UNKNOWN";
    }
}

const char *device_state_name(device_state_t state)
{
    switch (state) {
    case DEVICE_STATE_ACTIVE:   return "ACTIVE";
    case DEVICE_STATE_FAILED:   return "FAILED";
    case DEVICE_STATE_DISABLED: return "DISABLED";
    default:                    return "UNINIT";
    }
}

void device_dump_inventory(void)
{
    KLOG_INFO("driver", "===== ZERO OS HARDWARE DEVICE INVENTORY (%lu devices) =====", g_device_total);
    spinlock_acquire(&g_driver_lock);
    device_t *curr = g_device_list_head;
    while (curr) {
        KLOG_INFO("driver", "  Device #%02u: %-20s Type: %-8s State: %-6s (Vid:0x%04x Pid:0x%04x)",
                  curr->id, curr->name, device_type_name(curr->type),
                  device_state_name(curr->state), curr->vendor_id, curr->product_id);
        curr = curr->next;
    }
    spinlock_release(&g_driver_lock);
    KLOG_INFO("driver", "============================================================");
}
