/* ZERO KERNEL - ZEROFS Native Filesystem Implementation (Milestone 4.0.0 Storage & Networking).
 *
 * Implements persistent on-disk filesystem formatting, mounting, directory lookup,
 * file creation, reading, writing, truncation, unlinking, and block cache integration.
 */
#include "fs/zerofs.h"
#include "storage/bcache.h"
#include "memory/heap.h"
#include "zero/log.h"
#include "zero/spinlock.h"
#include "lib/string.h"

static const struct vnode_ops zerofs_vnode_ops;
static spinlock_t g_zerofs_lock;

/* Forward declarations */
static int zerofs_read_inode(zerofs_mount_info_t *mnt, uint32_t inode_num, zerofs_inode_t *out_inode);
static int zerofs_write_inode(zerofs_mount_info_t *mnt, uint32_t inode_num, const zerofs_inode_t *in_inode);

static void set_bitmap_bit(uint8_t *bitmap, uint32_t bit)
{
    bitmap[bit / 8] |= (uint8_t)(1u << (bit % 8));
}

static void clear_bitmap_bit(uint8_t *bitmap, uint32_t bit)
{
    bitmap[bit / 8] &= (uint8_t)~(1u << (bit % 8));
}

static bool test_bitmap_bit(const uint8_t *bitmap, uint32_t bit)
{
    return (bitmap[bit / 8] & (1u << (bit % 8))) != 0;
}

int zerofs_format(block_device_t *bdev, const char *volume_name)
{
    if (!bdev) return -1;

    uint64_t total_blocks = (bdev->sector_count * bdev->sector_size) / ZEROFS_BLOCK_SIZE;
    if (total_blocks < 64) {
        KLOG_WARN("zerofs", "device %s too small for ZEROFS (%llu blocks)",
                  bdev->name, (unsigned long long)total_blocks);
        return -2;
    }

    uint32_t total_inodes = 512;
    uint32_t inode_table_blocks = (total_inodes * ZEROFS_INODE_SIZE) / ZEROFS_BLOCK_SIZE; /* 16 blocks */

    zerofs_super_t sb;
    memset(&sb, 0, sizeof(sb));
    sb.magic = ZEROFS_MAGIC;
    sb.version = ZEROFS_VERSION;
    sb.block_size = ZEROFS_BLOCK_SIZE;
    sb.total_blocks = (uint32_t)total_blocks;
    sb.inode_bitmap_lba = 2;
    sb.inode_bitmap_blocks = 1;
    sb.block_bitmap_lba = 3;
    sb.block_bitmap_blocks = 1;
    sb.inode_table_lba = 4;
    sb.inode_table_blocks = inode_table_blocks;
    sb.first_data_block = 4 + inode_table_blocks; /* Block 20 */
    sb.root_inode = ZEROFS_ROOT_INODE;
    sb.total_inodes = total_inodes;
    sb.free_inodes = total_inodes - 2; /* 0 reserved, 1 root */
    sb.free_blocks = (uint32_t)total_blocks - (sb.first_data_block + 1); /* +1 for root dir data block */
    sb.mount_count = 0;
    sb.state = 1; /* Clean */
    strncpy(sb.volume_name, volume_name ? volume_name : "ZERO_ROOT", sizeof(sb.volume_name) - 1);

    uint8_t buf[ZEROFS_BLOCK_SIZE];

    /* 1. Write Superblock to Block 1 */
    memset(buf, 0, sizeof(buf));
    memcpy(buf, &sb, sizeof(sb));
    if (bcache_write(bdev, 1, buf, true) != 0) return -3;

    /* 2. Write Inode Bitmap (Block 2) */
    memset(buf, 0, sizeof(buf));
    set_bitmap_bit(buf, 0); /* Reserved */
    set_bitmap_bit(buf, ZEROFS_ROOT_INODE); /* Root */
    if (bcache_write(bdev, sb.inode_bitmap_lba, buf, true) != 0) return -4;

    /* 3. Write Block Bitmap (Block 3) */
    memset(buf, 0, sizeof(buf));
    /* Mark metadata blocks (0..first_data_block) + root data block as allocated */
    for (uint32_t b = 0; b <= sb.first_data_block; b++) {
        set_bitmap_bit(buf, b);
    }
    if (bcache_write(bdev, sb.block_bitmap_lba, buf, true) != 0) return -5;

    /* 4. Clear Inode Table Blocks (Blocks 4..19) */
    memset(buf, 0, sizeof(buf));
    for (uint32_t i = 0; i < inode_table_blocks; i++) {
        if (bcache_write(bdev, sb.inode_table_lba + i, buf, true) != 0) return -6;
    }

    /* 5. Initialize Root Inode (Inode 1) */
    zerofs_inode_t root_ino;
    memset(&root_ino, 0, sizeof(root_ino));
    root_ino.mode = ZEROFS_TYPE_DIR | 0755;
    root_ino.nlink = 2;
    root_ino.size = 2 * sizeof(zerofs_dirent_t);
    root_ino.blocks = 1;
    root_ino.direct[0] = sb.first_data_block;

    /* Write root inode into first inode table block */
    memset(buf, 0, sizeof(buf));
    memcpy(buf + (ZEROFS_ROOT_INODE * ZEROFS_INODE_SIZE), &root_ino, sizeof(root_ino));
    if (bcache_write(bdev, sb.inode_table_lba, buf, true) != 0) return -7;

    /* 6. Initialize Root Directory Data Block (Block 20) with '.' and '..' */
    memset(buf, 0, sizeof(buf));
    zerofs_dirent_t *dot = (zerofs_dirent_t *)buf;
    dot->inode = ZEROFS_ROOT_INODE;
    dot->rec_len = sizeof(zerofs_dirent_t);
    dot->name_len = 1;
    dot->file_type = 2;
    dot->name[0] = '.';
    dot->name[1] = '\0';

    zerofs_dirent_t *dotdot = (zerofs_dirent_t *)(buf + sizeof(zerofs_dirent_t));
    dotdot->inode = ZEROFS_ROOT_INODE;
    dotdot->rec_len = sizeof(zerofs_dirent_t);
    dotdot->name_len = 2;
    dotdot->file_type = 2;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';
    dotdot->name[2] = '\0';

    if (bcache_write(bdev, sb.first_data_block, buf, true) != 0) return -8;

    bcache_sync(bdev);
    KLOG_INFO("zerofs", "formatted %s as ZEROFS volume '%s' (%u blocks, %u inodes)",
              bdev->name, sb.volume_name, sb.total_blocks, sb.total_inodes);
    return 0;
}

static int zerofs_read_inode(zerofs_mount_info_t *mnt, uint32_t inode_num, zerofs_inode_t *out_inode)
{
    if (!mnt || !out_inode || inode_num >= mnt->sb.total_inodes) return -1;

    uint32_t inodes_per_block = ZEROFS_BLOCK_SIZE / ZEROFS_INODE_SIZE;
    uint32_t block_idx = mnt->sb.inode_table_lba + (inode_num / inodes_per_block);
    uint32_t offset = (inode_num % inodes_per_block) * ZEROFS_INODE_SIZE;

    uint8_t buf[ZEROFS_BLOCK_SIZE];
    if (bcache_read(mnt->bdev, block_idx, buf) != 0) return -2;

    memcpy(out_inode, buf + offset, sizeof(zerofs_inode_t));
    return 0;
}

static int zerofs_write_inode(zerofs_mount_info_t *mnt, uint32_t inode_num, const zerofs_inode_t *in_inode)
{
    if (!mnt || !in_inode || inode_num >= mnt->sb.total_inodes) return -1;

    uint32_t inodes_per_block = ZEROFS_BLOCK_SIZE / ZEROFS_INODE_SIZE;
    uint32_t block_idx = mnt->sb.inode_table_lba + (inode_num / inodes_per_block);
    uint32_t offset = (inode_num % inodes_per_block) * ZEROFS_INODE_SIZE;

    uint8_t buf[ZEROFS_BLOCK_SIZE];
    if (bcache_read(mnt->bdev, block_idx, buf) != 0) return -2;

    memcpy(buf + offset, in_inode, sizeof(zerofs_inode_t));
    return bcache_write(mnt->bdev, block_idx, buf, true);
}

static uint32_t zerofs_alloc_inode(zerofs_mount_info_t *mnt)
{
    uint8_t buf[ZEROFS_BLOCK_SIZE];
    if (bcache_read(mnt->bdev, mnt->sb.inode_bitmap_lba, buf) != 0) return 0;

    for (uint32_t i = 1; i < mnt->sb.total_inodes; i++) {
        if (!test_bitmap_bit(buf, i)) {
            set_bitmap_bit(buf, i);
            bcache_write(mnt->bdev, mnt->sb.inode_bitmap_lba, buf, true);
            mnt->sb.free_inodes--;
            return i;
        }
    }
    return 0;
}

static void zerofs_free_inode(zerofs_mount_info_t *mnt, uint32_t inode_num)
{
    if (inode_num == 0 || inode_num >= mnt->sb.total_inodes) return;
    uint8_t buf[ZEROFS_BLOCK_SIZE];
    if (bcache_read(mnt->bdev, mnt->sb.inode_bitmap_lba, buf) != 0) return;

    clear_bitmap_bit(buf, inode_num);
    bcache_write(mnt->bdev, mnt->sb.inode_bitmap_lba, buf, true);
    mnt->sb.free_inodes++;
}

static uint32_t zerofs_alloc_block(zerofs_mount_info_t *mnt)
{
    uint8_t buf[ZEROFS_BLOCK_SIZE];
    if (bcache_read(mnt->bdev, mnt->sb.block_bitmap_lba, buf) != 0) return 0;

    for (uint32_t b = mnt->sb.first_data_block; b < mnt->sb.total_blocks; b++) {
        if (!test_bitmap_bit(buf, b)) {
            set_bitmap_bit(buf, b);
            bcache_write(mnt->bdev, mnt->sb.block_bitmap_lba, buf, true);
            mnt->sb.free_blocks--;
            return b;
        }
    }
    return 0;
}

static void zerofs_free_block(zerofs_mount_info_t *mnt, uint32_t block_num)
{
    if (block_num < mnt->sb.first_data_block || block_num >= mnt->sb.total_blocks) return;
    uint8_t buf[ZEROFS_BLOCK_SIZE];
    if (bcache_read(mnt->bdev, mnt->sb.block_bitmap_lba, buf) != 0) return;

    clear_bitmap_bit(buf, block_num);
    bcache_write(mnt->bdev, mnt->sb.block_bitmap_lba, buf, true);
    mnt->sb.free_blocks++;
}

/* Resolve logical block index of a file to physical block on disk */
static uint32_t zerofs_bmap(zerofs_mount_info_t *mnt, zerofs_inode_t *inode, uint32_t file_block, bool alloc)
{
    if (file_block < ZEROFS_DIRECT_BLOCKS) {
        if (inode->direct[file_block] == 0 && alloc) {
            uint32_t nb = zerofs_alloc_block(mnt);
            if (nb == 0) return 0;
            inode->direct[file_block] = nb;
            inode->blocks++;
        }
        return inode->direct[file_block];
    }

    /* Single indirect block */
    uint32_t ind_idx = file_block - ZEROFS_DIRECT_BLOCKS;
    if (ind_idx < (ZEROFS_BLOCK_SIZE / sizeof(uint32_t))) {
        if (inode->indirect_1 == 0) {
            if (!alloc) return 0;
            uint32_t ib = zerofs_alloc_block(mnt);
            if (ib == 0) return 0;
            inode->indirect_1 = ib;
            inode->blocks++;
            uint8_t zero_buf[ZEROFS_BLOCK_SIZE];
            memset(zero_buf, 0, sizeof(zero_buf));
            bcache_write(mnt->bdev, ib, zero_buf, true);
        }

        uint32_t ind_buf[ZEROFS_BLOCK_SIZE / sizeof(uint32_t)];
        if (bcache_read(mnt->bdev, inode->indirect_1, ind_buf) != 0) return 0;

        if (ind_buf[ind_idx] == 0 && alloc) {
            uint32_t nb = zerofs_alloc_block(mnt);
            if (nb == 0) return 0;
            ind_buf[ind_idx] = nb;
            inode->blocks++;
            bcache_write(mnt->bdev, inode->indirect_1, ind_buf, true);
        }
        return ind_buf[ind_idx];
    }

    return 0;
}

/* VFS vnode operations */
static int zerofs_op_open(struct vnode *vn, int flags)
{
    (void)flags;
    if (!vn) return -1;
    return 0;
}

static int zerofs_op_close(struct vnode *vn)
{
    if (!vn) return -1;
    return 0;
}

static int64_t zerofs_op_read(struct vnode *vn, uint64_t offset, void *buf, size_t count)
{
    if (!vn || !vn->fs_data || !buf) return -1;
    zerofs_node_t *node = (zerofs_node_t *)vn->fs_data;
    zerofs_mount_info_t *mnt = node->mnt;

    if (offset >= node->inode.size) return 0;
    if (offset + count > node->inode.size) {
        count = (size_t)(node->inode.size - offset);
    }

    size_t bytes_read = 0;
    uint8_t block_buf[ZEROFS_BLOCK_SIZE];

    while (bytes_read < count) {
        uint64_t cur_off = offset + bytes_read;
        uint32_t block_idx = (uint32_t)(cur_off / ZEROFS_BLOCK_SIZE);
        uint32_t in_block_off = (uint32_t)(cur_off % ZEROFS_BLOCK_SIZE);
        uint32_t to_copy = ZEROFS_BLOCK_SIZE - in_block_off;
        if (to_copy > (count - bytes_read)) {
            to_copy = (uint32_t)(count - bytes_read);
        }

        uint32_t phys_block = zerofs_bmap(mnt, &node->inode, block_idx, false);
        if (phys_block == 0) {
            memset((uint8_t *)buf + bytes_read, 0, to_copy);
        } else {
            if (bcache_read(mnt->bdev, phys_block, block_buf) != 0) {
                break;
            }
            memcpy((uint8_t *)buf + bytes_read, block_buf + in_block_off, to_copy);
        }
        bytes_read += to_copy;
    }

    return (int64_t)bytes_read;
}

static int64_t zerofs_op_write(struct vnode *vn, uint64_t offset, const void *buf, size_t count)
{
    if (!vn || !vn->fs_data || !buf) return -1;
    zerofs_node_t *node = (zerofs_node_t *)vn->fs_data;
    zerofs_mount_info_t *mnt = node->mnt;

    size_t bytes_written = 0;
    uint8_t block_buf[ZEROFS_BLOCK_SIZE];

    while (bytes_written < count) {
        uint64_t cur_off = offset + bytes_written;
        uint32_t block_idx = (uint32_t)(cur_off / ZEROFS_BLOCK_SIZE);
        uint32_t in_block_off = (uint32_t)(cur_off % ZEROFS_BLOCK_SIZE);
        uint32_t to_copy = ZEROFS_BLOCK_SIZE - in_block_off;
        if (to_copy > (count - bytes_written)) {
            to_copy = (uint32_t)(count - bytes_written);
        }

        uint32_t phys_block = zerofs_bmap(mnt, &node->inode, block_idx, true);
        if (phys_block == 0) {
            break; /* Storage full */
        }

        if (to_copy < ZEROFS_BLOCK_SIZE) {
            bcache_read(mnt->bdev, phys_block, block_buf);
        }
        memcpy(block_buf + in_block_off, (const uint8_t *)buf + bytes_written, to_copy);
        if (bcache_write(mnt->bdev, phys_block, block_buf, false) != 0) {
            break;
        }

        bytes_written += to_copy;
    }

    if (offset + bytes_written > node->inode.size) {
        node->inode.size = offset + bytes_written;
        vn->size = node->inode.size;
    }
    zerofs_write_inode(mnt, node->inode_num, &node->inode);

    return (int64_t)bytes_written;
}

static struct vnode *zerofs_create_vnode(zerofs_mount_info_t *mnt, uint32_t inode_num, const zerofs_inode_t *ino)
{
    vnode_t *vn = (vnode_t *)kmalloc(sizeof(vnode_t));
    zerofs_node_t *zn = (zerofs_node_t *)kmalloc(sizeof(zerofs_node_t));
    if (!vn || !zn) {
        if (vn) kfree(vn);
        if (zn) kfree(zn);
        return NULL;
    }

    memset(vn, 0, sizeof(*vn));
    memset(zn, 0, sizeof(*zn));

    zn->mnt = mnt;
    zn->inode_num = inode_num;
    memcpy(&zn->inode, ino, sizeof(zerofs_inode_t));

    vn->type = (ino->mode & ZEROFS_TYPE_DIR) ? VNODE_TYPE_DIR : VNODE_TYPE_REG;
    vn->ref_count = 1;
    vn->size = ino->size;
    vn->ops = &zerofs_vnode_ops;
    vn->fs_data = zn;

    return vn;
}

static struct vnode *zerofs_op_lookup(struct vnode *vn, const char *name)
{
    if (!vn || !vn->fs_data || !name) return NULL;
    zerofs_node_t *pnode = (zerofs_node_t *)vn->fs_data;
    zerofs_mount_info_t *mnt = pnode->mnt;

    if (!(pnode->inode.mode & ZEROFS_TYPE_DIR)) return NULL;

    size_t entries_count = (size_t)(pnode->inode.size / sizeof(zerofs_dirent_t));
    uint8_t block_buf[ZEROFS_BLOCK_SIZE];

    for (size_t i = 0; i < entries_count; i++) {
        uint64_t off = i * sizeof(zerofs_dirent_t);
        uint32_t b_idx = (uint32_t)(off / ZEROFS_BLOCK_SIZE);
        uint32_t in_off = (uint32_t)(off % ZEROFS_BLOCK_SIZE);

        uint32_t pb = zerofs_bmap(mnt, &pnode->inode, b_idx, false);
        if (pb == 0) continue;

        if (bcache_read(mnt->bdev, pb, block_buf) != 0) continue;
        zerofs_dirent_t *de = (zerofs_dirent_t *)(block_buf + in_off);

        if (de->inode != 0 && strcmp(de->name, name) == 0) {
            zerofs_inode_t target_ino;
            if (zerofs_read_inode(mnt, de->inode, &target_ino) != 0) return NULL;
            return zerofs_create_vnode(mnt, de->inode, &target_ino);
        }
    }

    return NULL;
}

static int zerofs_op_create(struct vnode *parent, const char *name, struct vnode **out_vn)
{
    if (!parent || !parent->fs_data || !name || !out_vn) return -1;
    zerofs_node_t *pnode = (zerofs_node_t *)parent->fs_data;
    zerofs_mount_info_t *mnt = pnode->mnt;

    uint32_t new_ino_num = zerofs_alloc_inode(mnt);
    if (new_ino_num == 0) return -2; /* Out of inodes */

    zerofs_inode_t new_ino;
    memset(&new_ino, 0, sizeof(new_ino));
    new_ino.mode = ZEROFS_TYPE_FILE | 0644;
    new_ino.nlink = 1;
    new_ino.size = 0;
    new_ino.blocks = 0;

    if (zerofs_write_inode(mnt, new_ino_num, &new_ino) != 0) {
        zerofs_free_inode(mnt, new_ino_num);
        return -3;
    }

    /* Append dirent to parent directory */
    zerofs_dirent_t de;
    memset(&de, 0, sizeof(de));
    de.inode = new_ino_num;
    de.rec_len = sizeof(zerofs_dirent_t);
    de.name_len = (uint8_t)strlen(name);
    de.file_type = 1; /* regular */
    strncpy(de.name, name, sizeof(de.name) - 1);

    int64_t w = zerofs_op_write(parent, pnode->inode.size, &de, sizeof(de));
    if (w != sizeof(de)) {
        zerofs_free_inode(mnt, new_ino_num);
        return -4;
    }

    *out_vn = zerofs_create_vnode(mnt, new_ino_num, &new_ino);
    return 0;
}

static int zerofs_op_mkdir(struct vnode *parent, const char *name, struct vnode **out_vn)
{
    if (!parent || !parent->fs_data || !name || !out_vn) return -1;
    zerofs_node_t *pnode = (zerofs_node_t *)parent->fs_data;
    zerofs_mount_info_t *mnt = pnode->mnt;

    uint32_t new_ino_num = zerofs_alloc_inode(mnt);
    if (new_ino_num == 0) return -2;

    uint32_t dir_blk = zerofs_alloc_block(mnt);
    if (dir_blk == 0) {
        zerofs_free_inode(mnt, new_ino_num);
        return -3;
    }

    zerofs_inode_t new_ino;
    memset(&new_ino, 0, sizeof(new_ino));
    new_ino.mode = ZEROFS_TYPE_DIR | 0755;
    new_ino.nlink = 2;
    new_ino.size = 2 * sizeof(zerofs_dirent_t);
    new_ino.blocks = 1;
    new_ino.direct[0] = dir_blk;

    if (zerofs_write_inode(mnt, new_ino_num, &new_ino) != 0) {
        zerofs_free_block(mnt, dir_blk);
        zerofs_free_inode(mnt, new_ino_num);
        return -4;
    }

    /* Write '.' and '..' */
    uint8_t buf[ZEROFS_BLOCK_SIZE];
    memset(buf, 0, sizeof(buf));
    zerofs_dirent_t *dot = (zerofs_dirent_t *)buf;
    dot->inode = new_ino_num;
    dot->rec_len = sizeof(zerofs_dirent_t);
    dot->name_len = 1;
    dot->file_type = 2;
    dot->name[0] = '.';

    zerofs_dirent_t *dotdot = (zerofs_dirent_t *)(buf + sizeof(zerofs_dirent_t));
    dotdot->inode = pnode->inode_num;
    dotdot->rec_len = sizeof(zerofs_dirent_t);
    dotdot->name_len = 2;
    dotdot->file_type = 2;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';

    bcache_write(mnt->bdev, dir_blk, buf, true);

    /* Append dirent to parent */
    zerofs_dirent_t pde;
    memset(&pde, 0, sizeof(pde));
    pde.inode = new_ino_num;
    pde.rec_len = sizeof(zerofs_dirent_t);
    pde.name_len = (uint8_t)strlen(name);
    pde.file_type = 2;
    strncpy(pde.name, name, sizeof(pde.name) - 1);

    zerofs_op_write(parent, pnode->inode.size, &pde, sizeof(pde));
    *out_vn = zerofs_create_vnode(mnt, new_ino_num, &new_ino);
    return 0;
}

static int zerofs_op_unlink(struct vnode *parent, const char *name)
{
    if (!parent || !parent->fs_data || !name) return -1;
    zerofs_node_t *pnode = (zerofs_node_t *)parent->fs_data;
    zerofs_mount_info_t *mnt = pnode->mnt;

    size_t entries_count = (size_t)(pnode->inode.size / sizeof(zerofs_dirent_t));
    uint8_t block_buf[ZEROFS_BLOCK_SIZE];

    for (size_t i = 0; i < entries_count; i++) {
        uint64_t off = i * sizeof(zerofs_dirent_t);
        uint32_t b_idx = (uint32_t)(off / ZEROFS_BLOCK_SIZE);
        uint32_t in_off = (uint32_t)(off % ZEROFS_BLOCK_SIZE);

        uint32_t pb = zerofs_bmap(mnt, &pnode->inode, b_idx, false);
        if (pb == 0) continue;

        if (bcache_read(mnt->bdev, pb, block_buf) != 0) continue;
        zerofs_dirent_t *de = (zerofs_dirent_t *)(block_buf + in_off);

        if (de->inode != 0 && strcmp(de->name, name) == 0) {
            uint32_t target_ino_num = de->inode;
            zerofs_inode_t target_ino;
            if (zerofs_read_inode(mnt, target_ino_num, &target_ino) == 0) {
                /* Free direct blocks */
                for (size_t d = 0; d < ZEROFS_DIRECT_BLOCKS; d++) {
                    if (target_ino.direct[d] != 0) {
                        zerofs_free_block(mnt, target_ino.direct[d]);
                    }
                }
                /* Free indirect blocks if allocated */
                if (target_ino.indirect_1 != 0) {
                    uint32_t ind_buf[ZEROFS_BLOCK_SIZE / sizeof(uint32_t)];
                    if (bcache_read(mnt->bdev, target_ino.indirect_1, ind_buf) == 0) {
                        for (size_t ind = 0; ind < (ZEROFS_BLOCK_SIZE / sizeof(uint32_t)); ind++) {
                            if (ind_buf[ind] != 0) {
                                zerofs_free_block(mnt, ind_buf[ind]);
                            }
                        }
                    }
                    zerofs_free_block(mnt, target_ino.indirect_1);
                }
                zerofs_free_inode(mnt, target_ino_num);
            }
            /* Mark directory entry deleted */
            de->inode = 0;
            de->name[0] = '\0';
            bcache_write(mnt->bdev, pb, block_buf, true);
            return 0;
        }
    }
    return -2; /* Not found */
}

static const struct vnode_ops zerofs_vnode_ops = {
    .open = zerofs_op_open,
    .close = zerofs_op_close,
    .read = zerofs_op_read,
    .write = zerofs_op_write,
    .lookup = zerofs_op_lookup,
    .create = zerofs_op_create,
    .mkdir = zerofs_op_mkdir,
    .unlink = zerofs_op_unlink,
};

int zerofs_mount(block_device_t *bdev, const char *mount_point)
{
    if (!bdev || !mount_point) return -1;

    uint8_t buf[ZEROFS_BLOCK_SIZE];
    if (bcache_read(bdev, 1, buf) != 0) return -2;

    const zerofs_super_t *sb = (const zerofs_super_t *)buf;
    if (sb->magic != ZEROFS_MAGIC) {
        KLOG_WARN("zerofs", "%s has invalid magic 0x%08X (not ZEROFS)", bdev->name, sb->magic);
        return -3;
    }

    zerofs_mount_info_t *mnt = (zerofs_mount_info_t *)kmalloc(sizeof(zerofs_mount_info_t));
    if (!mnt) return -4;
    memset(mnt, 0, sizeof(*mnt));
    mnt->bdev = bdev;
    memcpy(&mnt->sb, sb, sizeof(zerofs_super_t));
    mnt->sb.mount_count++;

    /* Read root inode */
    zerofs_inode_t root_ino;
    if (zerofs_read_inode(mnt, ZEROFS_ROOT_INODE, &root_ino) != 0) {
        kfree(mnt);
        return -5;
    }

    mnt->root_vnode = zerofs_create_vnode(mnt, ZEROFS_ROOT_INODE, &root_ino);
    if (!mnt->root_vnode) {
        kfree(mnt);
        return -6;
    }

    int rc = vfs_mount(mount_point, mnt->root_vnode);
    if (rc != 0) {
        kfree(mnt->root_vnode->fs_data);
        kfree(mnt->root_vnode);
        kfree(mnt);
        return rc;
    }

    KLOG_INFO("zerofs", "mounted volume '%s' on '%s' (blocks free: %u/%u)",
              mnt->sb.volume_name, mount_point, mnt->sb.free_blocks, mnt->sb.total_blocks);
    return 0;
}

int zerofs_sync(block_device_t *bdev)
{
    return bcache_sync(bdev);
}
