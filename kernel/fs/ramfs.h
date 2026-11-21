/* ZERO KERNEL - In-Memory RAMFS Filesystem (Version 2.0.0 System Architecture).
 *
 * Implements an in-memory root filesystem supporting files, directories,
 * dynamic data expansion, and directory traversal.
 */
#pragma once

#include "fs/vfs.h"

#define RAMFS_INITIAL_FILE_CAP  64u
#define RAMFS_INITIAL_DIR_CAP   8u

typedef struct ramfs_node {
    char name[VFS_MAX_NAME];
    enum vnode_type type;
    vnode_t vnode;

    /* File contents */
    uint8_t *data;
    size_t size;
    size_t capacity;

    /* Directory children */
    struct ramfs_node **children;
    size_t child_count;
    size_t child_capacity;

    struct ramfs_node *parent;
} ramfs_node_t;

/* Initialize RAMFS and mount root filesystem at "/". */
int ramfs_init(void);

/* Retrieve the root vnode of RAMFS. */
vnode_t *ramfs_get_root(void);
