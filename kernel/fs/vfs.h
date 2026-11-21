/* ZERO KERNEL - Virtual File System (VFS) Abstraction (Version 2.0.0 System Architecture).
 *
 * Defines the vnode, file, and mount abstractions providing a uniform
 * interface across different filesystems (RAMFS, ZERO-FS, etc.).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define VFS_MAX_PATH     256u
#define VFS_MAX_NAME     64u
#define VFS_MAX_MOUNTS   8u

/* File open flags */
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_TRUNC     0x0200
#define O_APPEND    0x0400

/* Seek whence */
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

enum vnode_type {
    VNODE_TYPE_NONE = 0,
    VNODE_TYPE_REG,
    VNODE_TYPE_DIR,
    VNODE_TYPE_DEVICE
};

struct vnode;
struct file;

struct vnode_ops {
    int (*open)(struct vnode *vn, int flags);
    int (*close)(struct vnode *vn);
    int64_t (*read)(struct vnode *vn, uint64_t offset, void *buf, size_t count);
    int64_t (*write)(struct vnode *vn, uint64_t offset, const void *buf, size_t count);
    struct vnode *(*lookup)(struct vnode *vn, const char *name);
    int (*create)(struct vnode *parent, const char *name, struct vnode **out_vn);
    int (*mkdir)(struct vnode *parent, const char *name, struct vnode **out_vn);
    int (*unlink)(struct vnode *parent, const char *name);
};

typedef struct vnode {
    enum vnode_type type;
    uint32_t ref_count;
    uint64_t size;
    const struct vnode_ops *ops;
    void *fs_data; /* filesystem-specific node pointer */
} vnode_t;

typedef struct file {
    vnode_t *vnode;
    uint64_t offset;
    int flags;
    uint32_t ref_count;
} file_t;

struct vfs_mount {
    char mount_point[VFS_MAX_PATH];
    vnode_t *root_vnode;
    bool active;
};

/* Initialize the Virtual File System framework. */
void vfs_init(void);

/* Mount a filesystem root vnode at the specified path (e.g. "/"). */
int vfs_mount(const char *path, vnode_t *root);

/* Open a file by path. Returns a new file_t handle or NULL on error. */
file_t *vfs_open(const char *path, int flags);

/* Close an open file handle. */
int vfs_close(file_t *file);

/* Read count bytes from file into buffer. */
int64_t vfs_read(file_t *file, void *buf, size_t count);

/* Write count bytes from buffer into file. */
int64_t vfs_write(file_t *file, const void *buf, size_t count);

/* Adjust file offset. */
int64_t vfs_seek(file_t *file, int64_t offset, int whence);

/* Create a new empty regular file. */
int vfs_create(const char *path);

/* Create a new directory. */
int vfs_mkdir(const char *path);

/* Delete a file. */
int vfs_unlink(const char *path);
