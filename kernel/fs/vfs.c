/* ZERO KERNEL - Virtual File System (VFS) Implementation (Version 2.0.0 System Architecture). */

#include "fs/vfs.h"
#include "memory/heap.h"
#include "zero/string.h"
#include "zero/spinlock.h"
#include "zero/log.h"

static struct vfs_mount g_mounts[VFS_MAX_MOUNTS];
static spinlock_t g_vfs_lock;

void vfs_init(void)
{
    spinlock_init(&g_vfs_lock);
    for (size_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        g_mounts[i].active = false;
        g_mounts[i].mount_point[0] = '\0';
        g_mounts[i].root_vnode = NULL;
    }
    KLOG_INFO("vfs", "VFS abstraction initialized (max mounts: %u)", (unsigned)VFS_MAX_MOUNTS);
}

int vfs_mount(const char *path, vnode_t *root)
{
    if (!path || !root)
        return -1;

    spinlock_acquire(&g_vfs_lock);

    /* Check if already mounted */
    for (size_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (g_mounts[i].active && strcmp(g_mounts[i].mount_point, path) == 0) {
            spinlock_release(&g_vfs_lock);
            return -1;
        }
    }

    /* Find free mount slot */
    for (size_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!g_mounts[i].active) {
            strncpy(g_mounts[i].mount_point, path, VFS_MAX_PATH - 1);
            g_mounts[i].mount_point[VFS_MAX_PATH - 1] = '\0';
            g_mounts[i].root_vnode = root;
            g_mounts[i].active = true;
            root->ref_count++;
            spinlock_release(&g_vfs_lock);
            KLOG_INFO("vfs", "Mounted filesystem at '%s'", path);
            return 0;
        }
    }

    spinlock_release(&g_vfs_lock);
    return -1;
}

/* Helper: find mount point for a given path and return pointer to subpath within mount */
static vnode_t *vfs_resolve_mount(const char *path, const char **out_subpath)
{
    vnode_t *best_root = NULL;
    size_t best_len = 0;
    const char *subpath = path;

    for (size_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (g_mounts[i].active) {
            size_t mlen = strlen(g_mounts[i].mount_point);
            if (strncmp(path, g_mounts[i].mount_point, mlen) == 0) {
                if (mlen > best_len) {
                    best_len = mlen;
                    best_root = g_mounts[i].root_vnode;
                    subpath = path + mlen;
                    /* Skip leading slash on remainder */
                    while (*subpath == '/') {
                        subpath++;
                    }
                }
            }
        }
    }

    if (out_subpath) {
        *out_subpath = subpath;
    }
    return best_root;
}

/* Helper to resolve a path to its vnode */
static vnode_t *vfs_lookup_path(const char *path)
{
    if (!path || path[0] == '\0')
        return NULL;

    const char *subpath = NULL;
    vnode_t *curr = vfs_resolve_mount(path, &subpath);
    if (!curr)
        return NULL;

    if (*subpath == '\0') {
        return curr;
    }

    char token[VFS_MAX_NAME];
    while (*subpath != '\0') {
        size_t idx = 0;
        while (*subpath != '\0' && *subpath != '/' && idx < (VFS_MAX_NAME - 1)) {
            token[idx++] = *subpath++;
        }
        token[idx] = '\0';

        while (*subpath == '/') {
            subpath++;
        }

        if (idx == 0) {
            continue;
        }

        if (!curr->ops || !curr->ops->lookup) {
            return NULL;
        }

        vnode_t *next = curr->ops->lookup(curr, token);
        if (!next) {
            return NULL;
        }
        curr = next;
    }

    return curr;
}

/* Helper to split path into parent directory path and leaf name */
static bool vfs_split_path(const char *path, char *out_parent, char *out_name)
{
    if (!path)
        return false;

    size_t len = strlen(path);
    if (len == 0 || len >= VFS_MAX_PATH)
        return false;

    /* Trim trailing slashes */
    while (len > 1 && path[len - 1] == '/') {
        len--;
    }

    int last_slash = -1;
    for (int i = (int)len - 1; i >= 0; i--) {
        if (path[i] == '/') {
            last_slash = i;
            break;
        }
    }

    if (last_slash == -1) {
        /* No slash: parent is root "/", name is path */
        strcpy(out_parent, "/");
        strncpy(out_name, path, VFS_MAX_NAME - 1);
        out_name[VFS_MAX_NAME - 1] = '\0';
    } else if (last_slash == 0) {
        /* Root slash: parent is "/", name is path+1 */
        strcpy(out_parent, "/");
        size_t nlen = len - 1;
        if (nlen >= VFS_MAX_NAME) nlen = VFS_MAX_NAME - 1;
        memcpy(out_name, path + 1, nlen);
        out_name[nlen] = '\0';
    } else {
        size_t plen = (size_t)last_slash;
        if (plen >= VFS_MAX_PATH) plen = VFS_MAX_PATH - 1;
        memcpy(out_parent, path, plen);
        out_parent[plen] = '\0';

        size_t nlen = len - (last_slash + 1);
        if (nlen >= VFS_MAX_NAME) nlen = VFS_MAX_NAME - 1;
        memcpy(out_name, path + last_slash + 1, nlen);
        out_name[nlen] = '\0';
    }

    return true;
}

int vfs_create(const char *path)
{
    if (!path)
        return -1;

    char parent_path[VFS_MAX_PATH];
    char name[VFS_MAX_NAME];
    if (!vfs_split_path(path, parent_path, name))
        return -1;

    vnode_t *parent = vfs_lookup_path(parent_path);
    if (!parent || parent->type != VNODE_TYPE_DIR)
        return -1;

    if (!parent->ops || !parent->ops->create)
        return -1;

    vnode_t *out_vn = NULL;
    return parent->ops->create(parent, name, &out_vn);
}

int vfs_mkdir(const char *path)
{
    if (!path)
        return -1;

    char parent_path[VFS_MAX_PATH];
    char name[VFS_MAX_NAME];
    if (!vfs_split_path(path, parent_path, name))
        return -1;

    vnode_t *parent = vfs_lookup_path(parent_path);
    if (!parent || parent->type != VNODE_TYPE_DIR)
        return -1;

    if (!parent->ops || !parent->ops->mkdir)
        return -1;

    vnode_t *out_vn = NULL;
    return parent->ops->mkdir(parent, name, &out_vn);
}

int vfs_unlink(const char *path)
{
    if (!path)
        return -1;

    char parent_path[VFS_MAX_PATH];
    char name[VFS_MAX_NAME];
    if (!vfs_split_path(path, parent_path, name))
        return -1;

    vnode_t *parent = vfs_lookup_path(parent_path);
    if (!parent || parent->type != VNODE_TYPE_DIR)
        return -1;

    if (!parent->ops || !parent->ops->unlink)
        return -1;

    return parent->ops->unlink(parent, name);
}

file_t *vfs_open(const char *path, int flags)
{
    if (!path)
        return NULL;

    vnode_t *vn = vfs_lookup_path(path);
    if (!vn) {
        if (flags & O_CREAT) {
            if (vfs_create(path) != 0) {
                return NULL;
            }
            vn = vfs_lookup_path(path);
            if (!vn)
                return NULL;
        } else {
            return NULL;
        }
    }

    if (vn->ops && vn->ops->open) {
        if (vn->ops->open(vn, flags) != 0)
            return NULL;
    }

    file_t *f = (file_t *)kmalloc(sizeof(file_t));
    if (!f)
        return NULL;

    f->vnode = vn;
    f->offset = (flags & O_APPEND) ? vn->size : 0;
    f->flags = flags;
    f->ref_count = 1;
    vn->ref_count++;

    if ((flags & O_TRUNC) && vn->type == VNODE_TYPE_REG) {
        vn->size = 0;
        f->offset = 0;
    }

    return f;
}

int vfs_close(file_t *file)
{
    if (!file)
        return -1;

    if (file->ref_count > 1) {
        file->ref_count--;
        return 0;
    }

    vnode_t *vn = file->vnode;
    if (vn) {
        if (vn->ops && vn->ops->close) {
            vn->ops->close(vn);
        }
        if (vn->ref_count > 0) {
            vn->ref_count--;
        }
    }

    kfree(file);
    return 0;
}

int64_t vfs_read(file_t *file, void *buf, size_t count)
{
    if (!file || !buf || !file->vnode)
        return -1;

    if ((file->flags & O_WRONLY) && !(file->flags & O_RDWR))
        return -1;

    vnode_t *vn = file->vnode;
    if (!vn->ops || !vn->ops->read)
        return -1;

    int64_t bytes = vn->ops->read(vn, file->offset, buf, count);
    if (bytes > 0) {
        file->offset += (uint64_t)bytes;
    }
    return bytes;
}

int64_t vfs_write(file_t *file, const void *buf, size_t count)
{
    if (!file || !buf || !file->vnode)
        return -1;

    if (file->flags == O_RDONLY)
        return -1;

    vnode_t *vn = file->vnode;
    if (!vn->ops || !vn->ops->write)
        return -1;

    if (file->flags & O_APPEND) {
        file->offset = vn->size;
    }

    int64_t bytes = vn->ops->write(vn, file->offset, buf, count);
    if (bytes > 0) {
        file->offset += (uint64_t)bytes;
    }
    return bytes;
}

int64_t vfs_seek(file_t *file, int64_t offset, int whence)
{
    if (!file || !file->vnode)
        return -1;

    int64_t new_offset = 0;
    switch (whence) {
    case SEEK_SET:
        new_offset = offset;
        break;
    case SEEK_CUR:
        new_offset = (int64_t)file->offset + offset;
        break;
    case SEEK_END:
        new_offset = (int64_t)file->vnode->size + offset;
        break;
    default:
        return -1;
    }

    if (new_offset < 0)
        return -1;

    file->offset = (uint64_t)new_offset;
    return new_offset;
}
