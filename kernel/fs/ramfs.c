/* ZERO KERNEL - In-Memory RAMFS Implementation (Version 2.0.0 System Architecture). */

#include "fs/ramfs.h"
#include "fs/vfs.h"
#include "memory/heap.h"
#include "zero/string.h"
#include "zero/spinlock.h"
#include "zero/log.h"

static struct vnode_ops g_ramfs_ops;
static ramfs_node_t *g_ramfs_root = NULL;
static spinlock_t g_ramfs_lock;

static int ramfs_op_open(vnode_t *vn, int flags)
{
    (void)vn;
    (void)flags;
    return 0;
}

static int ramfs_op_close(vnode_t *vn)
{
    (void)vn;
    return 0;
}

static int64_t ramfs_op_read(vnode_t *vn, uint64_t offset, void *buf, size_t count)
{
    if (!vn || !buf)
        return -1;

    ramfs_node_t *node = (ramfs_node_t *)vn->fs_data;
    if (!node || node->type != VNODE_TYPE_REG)
        return -1;

    spinlock_acquire(&g_ramfs_lock);

    if (offset >= node->size) {
        spinlock_release(&g_ramfs_lock);
        return 0; /* EOF */
    }

    size_t available = node->size - (size_t)offset;
    size_t to_read = (count < available) ? count : available;

    if (to_read > 0 && node->data) {
        memcpy(buf, node->data + offset, to_read);
    }

    spinlock_release(&g_ramfs_lock);
    return (int64_t)to_read;
}

static int64_t ramfs_op_write(vnode_t *vn, uint64_t offset, const void *buf, size_t count)
{
    if (!vn || !buf)
        return -1;

    ramfs_node_t *node = (ramfs_node_t *)vn->fs_data;
    if (!node || node->type != VNODE_TYPE_REG)
        return -1;

    spinlock_acquire(&g_ramfs_lock);

    size_t req_end = (size_t)offset + count;
    if (req_end > node->capacity) {
        size_t new_cap = node->capacity ? (node->capacity * 2) : RAMFS_INITIAL_FILE_CAP;
        while (new_cap < req_end) {
            new_cap *= 2;
        }

        uint8_t *new_data = (uint8_t *)krealloc(node->data, new_cap);
        if (!new_data) {
            spinlock_release(&g_ramfs_lock);
            return -1;
        }

        /* Zero out any newly allocated unwritten gaps */
        if (offset > node->size) {
            memset(new_data + node->size, 0, (size_t)offset - node->size);
        }

        node->data = new_data;
        node->capacity = new_cap;
    }

    memcpy(node->data + offset, buf, count);

    if (req_end > node->size) {
        node->size = req_end;
        vn->size = (uint64_t)req_end;
    }

    spinlock_release(&g_ramfs_lock);
    return (int64_t)count;
}

static vnode_t *ramfs_op_lookup(vnode_t *vn, const char *name)
{
    if (!vn || !name)
        return NULL;

    ramfs_node_t *node = (ramfs_node_t *)vn->fs_data;
    if (!node || node->type != VNODE_TYPE_DIR)
        return NULL;

    if (strcmp(name, ".") == 0)
        return vn;

    if (strcmp(name, "..") == 0) {
        return node->parent ? &node->parent->vnode : vn;
    }

    spinlock_acquire(&g_ramfs_lock);

    for (size_t i = 0; i < node->child_count; i++) {
        if (strcmp(node->children[i]->name, name) == 0) {
            vnode_t *result = &node->children[i]->vnode;
            spinlock_release(&g_ramfs_lock);
            return result;
        }
    }

    spinlock_release(&g_ramfs_lock);
    return NULL;
}

static int ramfs_add_child(ramfs_node_t *parent, ramfs_node_t *child)
{
    if (parent->child_count >= parent->child_capacity) {
        size_t new_cap = parent->child_capacity ? (parent->child_capacity * 2) : RAMFS_INITIAL_DIR_CAP;
        ramfs_node_t **new_children = (ramfs_node_t **)krealloc(parent->children, sizeof(ramfs_node_t *) * new_cap);
        if (!new_children)
            return -1;
        parent->children = new_children;
        parent->child_capacity = new_cap;
    }

    parent->children[parent->child_count++] = child;
    child->parent = parent;
    return 0;
}

static int ramfs_op_create(vnode_t *parent_vn, const char *name, vnode_t **out_vn)
{
    if (!parent_vn || !name)
        return -1;

    ramfs_node_t *parent = (ramfs_node_t *)parent_vn->fs_data;
    if (!parent || parent->type != VNODE_TYPE_DIR)
        return -1;

    spinlock_acquire(&g_ramfs_lock);

    for (size_t i = 0; i < parent->child_count; i++) {
        if (strcmp(parent->children[i]->name, name) == 0) {
            spinlock_release(&g_ramfs_lock);
            return -1; /* Already exists */
        }
    }

    ramfs_node_t *node = (ramfs_node_t *)kmalloc(sizeof(ramfs_node_t));
    if (!node) {
        spinlock_release(&g_ramfs_lock);
        return -1;
    }

    memset(node, 0, sizeof(ramfs_node_t));
    strncpy(node->name, name, VFS_MAX_NAME - 1);
    node->type = VNODE_TYPE_REG;
    node->vnode.type = VNODE_TYPE_REG;
    node->vnode.ref_count = 1;
    node->vnode.size = 0;
    node->vnode.ops = &g_ramfs_ops;
    node->vnode.fs_data = node;

    if (ramfs_add_child(parent, node) != 0) {
        kfree(node);
        spinlock_release(&g_ramfs_lock);
        return -1;
    }

    if (out_vn) {
        *out_vn = &node->vnode;
    }

    spinlock_release(&g_ramfs_lock);
    return 0;
}

static int ramfs_op_mkdir(vnode_t *parent_vn, const char *name, vnode_t **out_vn)
{
    if (!parent_vn || !name)
        return -1;

    ramfs_node_t *parent = (ramfs_node_t *)parent_vn->fs_data;
    if (!parent || parent->type != VNODE_TYPE_DIR)
        return -1;

    spinlock_acquire(&g_ramfs_lock);

    for (size_t i = 0; i < parent->child_count; i++) {
        if (strcmp(parent->children[i]->name, name) == 0) {
            spinlock_release(&g_ramfs_lock);
            return -1; /* Already exists */
        }
    }

    ramfs_node_t *node = (ramfs_node_t *)kmalloc(sizeof(ramfs_node_t));
    if (!node) {
        spinlock_release(&g_ramfs_lock);
        return -1;
    }

    memset(node, 0, sizeof(ramfs_node_t));
    strncpy(node->name, name, VFS_MAX_NAME - 1);
    node->type = VNODE_TYPE_DIR;
    node->vnode.type = VNODE_TYPE_DIR;
    node->vnode.ref_count = 1;
    node->vnode.size = 0;
    node->vnode.ops = &g_ramfs_ops;
    node->vnode.fs_data = node;

    if (ramfs_add_child(parent, node) != 0) {
        kfree(node);
        spinlock_release(&g_ramfs_lock);
        return -1;
    }

    if (out_vn) {
        *out_vn = &node->vnode;
    }

    spinlock_release(&g_ramfs_lock);
    return 0;
}

static int ramfs_op_unlink(vnode_t *parent_vn, const char *name)
{
    if (!parent_vn || !name)
        return -1;

    ramfs_node_t *parent = (ramfs_node_t *)parent_vn->fs_data;
    if (!parent || parent->type != VNODE_TYPE_DIR)
        return -1;

    spinlock_acquire(&g_ramfs_lock);

    for (size_t i = 0; i < parent->child_count; i++) {
        ramfs_node_t *child = parent->children[i];
        if (strcmp(child->name, name) == 0) {
            if (child->type == VNODE_TYPE_DIR && child->child_count > 0) {
                spinlock_release(&g_ramfs_lock);
                return -1; /* Directory not empty */
            }

            /* Shift remaining children */
            for (size_t j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;

            if (child->data) {
                kfree(child->data);
            }
            if (child->children) {
                kfree(child->children);
            }
            kfree(child);

            spinlock_release(&g_ramfs_lock);
            return 0;
        }
    }

    spinlock_release(&g_ramfs_lock);
    return -1;
}

int ramfs_init(void)
{
    spinlock_init(&g_ramfs_lock);

    g_ramfs_ops.open = ramfs_op_open;
    g_ramfs_ops.close = ramfs_op_close;
    g_ramfs_ops.read = ramfs_op_read;
    g_ramfs_ops.write = ramfs_op_write;
    g_ramfs_ops.lookup = ramfs_op_lookup;
    g_ramfs_ops.create = ramfs_op_create;
    g_ramfs_ops.mkdir = ramfs_op_mkdir;
    g_ramfs_ops.unlink = ramfs_op_unlink;

    g_ramfs_root = (ramfs_node_t *)kmalloc(sizeof(ramfs_node_t));
    if (!g_ramfs_root) {
        KLOG_ERR("ramfs", "Failed to allocate root RAMFS node");
        return -1;
    }

    memset(g_ramfs_root, 0, sizeof(ramfs_node_t));
    strcpy(g_ramfs_root->name, "/");
    g_ramfs_root->type = VNODE_TYPE_DIR;
    g_ramfs_root->vnode.type = VNODE_TYPE_DIR;
    g_ramfs_root->vnode.ref_count = 1;
    g_ramfs_root->vnode.size = 0;
    g_ramfs_root->vnode.ops = &g_ramfs_ops;
    g_ramfs_root->vnode.fs_data = g_ramfs_root;

    if (vfs_mount("/", &g_ramfs_root->vnode) != 0) {
        KLOG_ERR("ramfs", "Failed to mount RAMFS root at '/'");
        return -1;
    }

    KLOG_INFO("ramfs", "In-memory RAMFS initialized and mounted at '/'");
    return 0;
}

vnode_t *ramfs_get_root(void)
{
    return g_ramfs_root ? &g_ramfs_root->vnode : NULL;
}
