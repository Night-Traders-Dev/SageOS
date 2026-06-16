#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "sage_libc_shim.h"
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

static uint64_t btrfs_logical_to_physical(uint64_t logical) {
    uint8_t *ptr = g_super.sys_chunk_array;
    uint8_t *end = ptr + g_super.sys_chunk_array_size;

    while (ptr < end) {
        btrfs_key *key = (btrfs_key *)ptr;
        if (key->type == BTRFS_CHUNK_ITEM_KEY) {
            btrfs_chunk *chunk = (btrfs_chunk *)(ptr + sizeof(btrfs_key));
            if (logical >= key->offset && logical < key->offset + chunk->length) {
                /* Found the chunk. BTRFS sys chunks usually have 1 stripe in simple setups. */
                btrfs_stripe *stripe = (btrfs_stripe *)(ptr + sizeof(btrfs_key) + sizeof(btrfs_chunk));
                return stripe->offset + (logical - key->offset);
            }
            ptr += sizeof(btrfs_key) + sizeof(btrfs_chunk) + (chunk->num_stripes * sizeof(btrfs_stripe));
        } else {
            ptr++;
        }
    }
    /* Fallback to identity if not found in sys array (might be in chunk tree, which we don't support yet) */
    return logical;
}

static int btrfs_read_node(uint64_t logical_addr, void *buffer) {
    uint64_t physical = btrfs_logical_to_physical(logical_addr);
    uint32_t lba = BTRFS_PARTITION_START_LBA + (uint32_t)(physical / 512);
    uint32_t nodesize = g_super.nodesize ? g_super.nodesize : BTRFS_NODE_SIZE;
    
    for (int i = 0; i < (int)(nodesize / 512); i++) {
        if (!btrfs_read_sector(lba + i, (uint8_t*)buffer + (i * 512))) return 0;
    }
    return 1;
}

static int btrfs_compare_keys(btrfs_key *a, btrfs_key *b) {
    if (a->objectid < b->objectid) return -1;
    if (a->objectid > b->objectid) return 1;
    if (a->type < b->type) return -1;
    if (a->type > b->type) return 1;
    if (a->offset < b->offset) return -1;
    if (a->offset > b->offset) return 1;
    return 0;
}

static int btrfs_tree_search(uint64_t root_logical, btrfs_key *key, btrfs_node *node_out, int *item_idx_out) {
    uint64_t cur_logical = root_logical;
    
    while (1) {
        if (!btrfs_read_node(cur_logical, node_out)) return 0;
        
        int n = node_out->header.nritems;
        int low = 0;
        int high = n - 1;
        int mid;
        int slot = 0;

        if (node_out->header.level > 0) {
            /* Internal node: find child */
            slot = 0;
            while (slot < n - 1) {
                if (btrfs_compare_keys(key, &node_out->ptrs[slot + 1].key) < 0) break;
                slot++;
            }
            cur_logical = node_out->ptrs[slot].blockptr;
        } else {
            /* Leaf node: find item */
            for (int i = 0; i < n; i++) {
                int cmp = btrfs_compare_keys(key, &node_out->items[i].key);
                if (cmp == 0) {
                    if (item_idx_out) *item_idx_out = i;
                    return 1;
                }
                if (cmp < 0) break;
            }
            return 0;
        }
    }
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

static uint64_t btrfs_get_fs_root(void) {
    btrfs_key search_key;
    search_key.objectid = BTRFS_FS_TREE_OBJECTID;
    search_key.type = BTRFS_ROOT_ITEM_KEY;
    search_key.offset = 0;

    static btrfs_node node;
    int idx;
    if (btrfs_tree_search(g_super.root, &search_key, &node, &idx)) {
        /* ROOT_ITEM data starts with generation (8 bytes) then root logical addr (8 bytes) */
        uint8_t *data = (uint8_t *)&node + 101 + node.items[idx].offset;
        return *(uint64_t *)(data + 16);
    }
    /* Fallback to FS_TREE_OBJECTID if not found (unlikely for valid BTRFS) */
    return 0;
}

static uint64_t btrfs_resolve_path(const char *path) {
    if (!path || path[0] != '/') return 0;
    if (path[1] == 0) return BTRFS_FIRST_FREE_OBJECTID;

    uint64_t fs_root = btrfs_get_fs_root();
    if (!fs_root) return 0;

    uint64_t current_objectid = BTRFS_FIRST_FREE_OBJECTID;
    const char *p = path;
    
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        
        const char *next_p = p;
        while (*next_p && *next_p != '/') next_p++;
        int comp_len = (int)(next_p - p);
        
        static btrfs_node node;
        btrfs_key search_key;
        search_key.objectid = current_objectid;
        search_key.type = BTRFS_DIR_INDEX_KEY;
        search_key.offset = 0;

        uint64_t cur_logical = fs_root;
        while (1) {
            if (!btrfs_read_node(cur_logical, &node)) return 0;
            int n = node.header.nritems;
            if (node.header.level > 0) {
                int slot = 0;
                while (slot < n - 1) {
                    if (btrfs_compare_keys(&search_key, &node.ptrs[slot + 1].key) < 0) break;
                    slot++;
                }
                cur_logical = node.ptrs[slot].blockptr;
            } else break;
        }

        /* Scan leaf for the name */
        int found = 0;
        for (uint32_t i = 0; i < node.header.nritems; i++) {
            btrfs_item *item = &node.items[i];
            if (item->key.objectid == current_objectid && item->key.type == BTRFS_DIR_INDEX_KEY) {
                btrfs_dir_item *di = (btrfs_dir_item *)((uint8_t *)&node + 101 + item->offset);
                if (di->name_len == comp_len && memcmp((uint8_t *)di + sizeof(btrfs_dir_item), p, comp_len) == 0) {
                    current_objectid = di->location.objectid;
                    found = 1;
                    break;
                }
            } else if (item->key.objectid > current_objectid) break;
        }
        
        if (!found) return 0;
        p = next_p;
    }
    
    return current_objectid;
}

void btrfs_ls(void) {
    if (!btrfs_available) return;
    static btrfs_node node;
    if (!btrfs_read_node(g_super.root, &node)) return;
    
    console_write("\n/BTRFS (Root Tree):");
    for (uint32_t i = 0; i < node.header.nritems; i++) {
        console_write("\n  ObjectID: ");
        console_hex64(node.items[i].key.objectid);
        console_write(" Type: ");
        console_u32(node.items[i].key.type);
    }
}

static int btrfs_be_stat(VfsBackend *self, const char *rel_path, VfsStat *out) {
    (void)self;
    if (!btrfs_available) return VFS_EIO;
    
    uint64_t objectid = btrfs_resolve_path(rel_path);
    if (!objectid) return VFS_ENOENT;

    uint64_t fs_root = btrfs_get_fs_root();
    if (!fs_root) return VFS_EIO;

    static btrfs_node node;
    btrfs_key search_key;
    search_key.objectid = objectid;
    search_key.type = BTRFS_INODE_ITEM_KEY;
    search_key.offset = 0;

    int idx;
    if (btrfs_tree_search(fs_root, &search_key, &node, &idx)) {
        btrfs_inode_item *ii = (btrfs_inode_item *)((uint8_t *)&node + 101 + node.items[idx].offset);
        
        /* Get name from rel_path */
        const char *name = rel_path;
        const char *last_slash = rel_path;
        while (*name) { if (*name == '/') last_slash = name + 1; name++; }
        strncpy(out->name, *last_slash ? last_slash : "/", VFS_NAME_MAX);
        
        out->type = ((ii->mode & 0170000) == 0040000) ? VFS_DIRECTORY : VFS_FILE;
        out->size = ii->size;
        out->mode = ii->mode & 0777;
        return VFS_OK;
    }

    return VFS_ENOENT;
}

static int btrfs_be_readdir(VfsBackend *self, const char *rel_path,
                            VfsDirEntry *entries, int max_entries) {
    (void)self;
    if (!btrfs_available) return VFS_EIO;
    
    uint64_t objectid = btrfs_resolve_path(rel_path);
    if (!objectid) return VFS_ENOENT;

    uint64_t fs_root = btrfs_get_fs_root();
    if (!fs_root) return VFS_EIO;

    static btrfs_node node;
    btrfs_key search_key;
    search_key.objectid = objectid;
    search_key.type = BTRFS_DIR_INDEX_KEY;
    search_key.offset = 0;

    uint64_t cur_logical = fs_root;
    while (1) {
        if (!btrfs_read_node(cur_logical, &node)) return VFS_EIO;
        int n = node.header.nritems;
        if (node.header.level > 0) {
            int slot = 0;
            while (slot < n - 1) {
                if (btrfs_compare_keys(&search_key, &node.ptrs[slot + 1].key) < 0) break;
                slot++;
            }
            cur_logical = node.ptrs[slot].blockptr;
        } else break;
    }

    int count = 0;
    for (uint32_t i = 0; i < node.header.nritems && count < max_entries; i++) {
        btrfs_item *item = &node.items[i];
        if (item->key.objectid == objectid && item->key.type == BTRFS_DIR_INDEX_KEY) {
            btrfs_dir_item *di = (btrfs_dir_item *)((uint8_t *)&node + 101 + item->offset);
            int name_len = di->name_len;
            if (name_len > VFS_NAME_MAX - 1) name_len = VFS_NAME_MAX - 1;
            
            memcpy(entries[count].name, (uint8_t *)di + sizeof(btrfs_dir_item), name_len);
            entries[count].name[name_len] = 0;
            
            entries[count].type = (di->type == 2) ? VFS_DIRECTORY : VFS_FILE;
            entries[count].size = 0; 
            count++;
        } else if (item->key.objectid > objectid) break;
    }
    return count;
}

static int btrfs_be_read(VfsBackend *self, const char *rel_path,
                         uint64_t offset, void *buffer, size_t size) {
    (void)self;
    if (!btrfs_available) return VFS_EIO;

    uint64_t objectid = btrfs_resolve_path(rel_path);
    if (!objectid) return VFS_ENOENT;

    uint64_t fs_root = btrfs_get_fs_root();
    if (!fs_root) return VFS_EIO;

    size_t total_read = 0;
    while (total_read < size) {
        uint64_t cur_offset = offset + total_read;
        
        static btrfs_node node;
        btrfs_key search_key;
        search_key.objectid = objectid;
        search_key.type = BTRFS_EXTENT_DATA_KEY;
        search_key.offset = cur_offset;

        /* Find the leaf containing this offset */
        uint64_t cur_logical = fs_root;
        while (1) {
            if (!btrfs_read_node(cur_logical, &node)) return VFS_EIO;
            int n = node.header.nritems;
            if (node.header.level > 0) {
                int slot = 0;
                while (slot < n - 1) {
                    if (btrfs_compare_keys(&search_key, &node.ptrs[slot + 1].key) < 0) break;
                    slot++;
                }
                cur_logical = node.ptrs[slot].blockptr;
            } else break;
        }

        /* Scan leaf for the extent covering cur_offset */
        int found_idx = -1;
        for (int i = (int)node.header.nritems - 1; i >= 0; i--) {
            btrfs_item *item = &node.items[i];
            if (item->key.objectid == objectid && item->key.type == BTRFS_EXTENT_DATA_KEY) {
                if (item->key.offset <= cur_offset) {
                    found_idx = i;
                    break;
                }
            } else if (item->key.objectid < objectid) break;
        }

        if (found_idx == -1) break;

        btrfs_item *item = &node.items[found_idx];
        btrfs_extent_data_item *ed = (btrfs_extent_data_item *)((uint8_t *)&node + 101 + item->offset);
        uint64_t extent_offset = cur_offset - item->key.offset;

        if (ed->type == 0) { /* Inline */
            uint32_t inline_size = item->size - 21;
            if (extent_offset >= inline_size) break;
            size_t to_copy = inline_size - (size_t)extent_offset;
            if (to_copy > (size - total_read)) to_copy = size - total_read;
            memcpy((uint8_t *)buffer + total_read, (uint8_t *)ed + 21 + extent_offset, to_copy);
            total_read += to_copy;
            /* Inline extents are always the whole file (or first part), but usually only one exists */
            break; 
        } else if (ed->type == 1) { /* Regular */
            btrfs_file_extent_item *fe = (btrfs_file_extent_item *)((uint8_t *)ed + 21);
            if (fe->disk_bytenr == 0) {
                /* Sparse: fill with zeros */
                size_t to_fill = (size_t)(fe->num_bytes - extent_offset);
                if (to_fill > (size - total_read)) to_fill = size - total_read;
                memset((uint8_t *)buffer + total_read, 0, to_fill);
                total_read += to_fill;
            } else {
                uint64_t phys = btrfs_logical_to_physical(fe->disk_bytenr + fe->offset + extent_offset);
                size_t to_read = (size_t)(fe->num_bytes - extent_offset);
                if (to_read > (size - total_read)) to_read = size - total_read;
                
                /* Limit to sector boundaries for simple ATA driver */
                uint32_t lba = BTRFS_PARTITION_START_LBA + (uint32_t)(phys / 512);
                size_t sectors = (to_read + 511) / 512;
                
                uint8_t temp[16384]; /* Assume max 16KB read for simplicity */
                if (sectors > 32) sectors = 32;
                
                for (size_t i = 0; i < sectors; i++) {
                    if (!btrfs_read_sector(lba + (uint32_t)i, temp + (i * 512))) return VFS_EIO;
                }
                memcpy((uint8_t *)buffer + total_read, temp, to_read);
                total_read += to_read;
            }
        } else break;
    }

    return (int)total_read;
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
