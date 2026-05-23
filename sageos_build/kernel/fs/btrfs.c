#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "console.h"
#include "ata.h"
#include "btrfs.h"
#include "vfs.h"
#include "dmesg.h"

/* 
 * BTRFS support for SageOS
 * 
 * Partition Layout (assumed):
 * 1: ESP (FAT32) - LBA 2048
 * 2: Root (BTRFS) - LBA 133120 (approx, depending on ESP size)
 * 3: SWAP
 */

#define BTRFS_PARTITION_START_LBA (2048 + (64 * 1024 * 1024 / 512)) /* ESP + 64MiB */

static int btrfs_available = 0;
static btrfs_super_block g_super;

static int btrfs_read_sector(uint32_t lba, uint8_t *buffer) {
    return ata_read_sector(lba, (uint16_t *)buffer);
}

static int btrfs_read_node(uint64_t logical_addr, void *buffer) {
    /* For now, assume logical address is directly mapped to sector offset 
       ignoring complex chunk mapping. */
    uint32_t lba = BTRFS_PARTITION_START_LBA + (uint32_t)(logical_addr / 512);
    for (int i = 0; i < (int)(g_super.nodesize / 512); i++) {
        if (!btrfs_read_sector(lba + i, (uint8_t*)buffer + (i * 512))) return 0;
    }
    return 1;
}

int btrfs_init(void) {
    uint8_t buffer[sizeof(btrfs_super_block)];
    
    if (!ata_is_available()) {
        btrfs_available = 0;
        return 0;
    }

    uint32_t super_lba = BTRFS_PARTITION_START_LBA + (BTRFS_SUPER_INFO_OFFSET / 512);
    
    /* BTRFS superblock spans 4 sectors (2048 bytes) */
    for (int i = 0; i < (int)(sizeof(btrfs_super_block) / 512); i++) {
        if (!btrfs_read_sector(super_lba + i, buffer + (i * 512))) {
            btrfs_available = 0;
            return 0;
        }
    }

    btrfs_super_block *sb = (btrfs_super_block *)buffer;
    
    if (sb->magic == BTRFS_MAGIC) {
        btrfs_available = 1;
        memcpy(&g_super, sb, sizeof(btrfs_super_block));
        console_write("\nBTRFS: Superblock detected on partition 2");
        dmesg_log("BTRFS: Superblock detected on partition 2");
        return 1;
    }

    btrfs_available = 0;
    return 0;
}

int btrfs_is_available(void) {
    return btrfs_available;
}

void btrfs_ls(void) {
    if (!btrfs_available) return;
    uint8_t buffer[16384]; /* Assume max node size */
    if (!btrfs_read_node(g_super.root, buffer)) return;
    
    btrfs_leaf *leaf = (btrfs_leaf *)buffer;
    console_write("\n/BTRFS (Root Tree):");
    for (uint32_t i = 0; i < leaf->header.nritems; i++) {
        console_write("\n  ObjectID: ");
        console_hex64(leaf->items[i].key.objectid);
        console_write(" Type: ");
        console_u32(leaf->items[i].key.type);
    }
}

static int btrfs_be_stat(VfsBackend *self, const char *rel_path, VfsStat *out) {
    (void)self;
    if (!btrfs_available) return VFS_EIO;
    if (rel_path[0] == '/' && rel_path[1] == 0) {
        strncpy(out->name, "/", VFS_NAME_MAX);
        out->type = VFS_DIRECTORY;
        out->size = 0;
        out->mode = 0755;
        return VFS_OK;
    }
    return VFS_ENOENT;
}

static int btrfs_be_readdir(VfsBackend *self, const char *rel_path,
                            VfsDirEntry *entries, int max_entries) {
    (void)self;
    if (!btrfs_available) return VFS_EIO;
    if (rel_path[0] != '/' || rel_path[1] != 0) return VFS_ENOTDIR;

    uint8_t buffer[16384]; /* Assume max node size */
    if (!btrfs_read_node(g_super.root, buffer)) return VFS_EIO;
    
    btrfs_leaf *leaf = (btrfs_leaf *)buffer;
    int count = 0;
    for (uint32_t i = 0; i < leaf->header.nritems && count < max_entries; i++) {
        /* Filter for ROOT_ITEM to show something meaningful from the metadata tree */
        if (leaf->items[i].key.type == 132 /* ROOT_ITEM */) {
            strncpy(entries[count].name, "tree_root", VFS_NAME_MAX);
            /* Just a dummy name based on objectid */
            entries[count].type = VFS_FILE;
            entries[count].size = 0;
            count++;
        }
    }
    return count;
}

static int btrfs_be_read(VfsBackend *self, const char *rel_path,
                         uint64_t offset, void *buffer, size_t size) {
    (void)self;
    if (!btrfs_available) return VFS_EIO;
    
    /* Dummy content for testing the VFS path */
    const char *msg = "BTRFS: Filesystem mounted. Full read support pending extent-tree implementation.\n";
    size_t msg_len = strlen(msg);
    
    if (offset >= msg_len) return 0;
    size_t to_copy = msg_len - (size_t)offset;
    if (to_copy > size) to_copy = size;
    
    memcpy(buffer, msg + offset, to_copy);
    return (int)to_copy;
}

static int btrfs_be_write(VfsBackend *self, const char *rel_path,
                          uint64_t offset, const void *data, size_t size) {
    (void)self;
    (void)rel_path;
    (void)offset;
    (void)data;
    (void)size;
    dmesg_log("btrfs: write (copy-on-write) requested but not fully implemented");
    return VFS_EROFS; /* Read-only for now */
}

static int btrfs_be_mkdir(VfsBackend *self, const char *rel_path) {
    (void)self;
    (void)rel_path;
    dmesg_log("btrfs: mkdir requested but not fully implemented");
    return VFS_EROFS;
}

static int btrfs_be_create(VfsBackend *self, const char *rel_path) {
    (void)self;
    (void)rel_path;
    dmesg_log("btrfs: create requested but not fully implemented");
    return VFS_EROFS;
}

static int btrfs_be_unlink(VfsBackend *self, const char *rel_path) {
    (void)self;
    (void)rel_path;
    dmesg_log("btrfs: unlink requested but not fully implemented");
    return VFS_EROFS;
}

static VfsBackend g_btrfs_backend = {
    .name    = "btrfs",
    .stat    = btrfs_be_stat,
    .readdir = btrfs_be_readdir,
    .read    = btrfs_be_read,
    .write   = btrfs_be_write,
    .mkdir   = btrfs_be_mkdir,
    .create  = btrfs_be_create,
    .unlink  = btrfs_be_unlink,
    .priv    = NULL
};

VfsBackend *btrfs_get_backend(void) {
    return &g_btrfs_backend;
}
