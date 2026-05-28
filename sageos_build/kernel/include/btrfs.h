#ifndef SAGEOS_BTRFS_H
#define SAGEOS_BTRFS_H

#include "vfs.h"

/* 
 * Minimal BTRFS Reader for SageOS
 * 
 * BTRFS Superblock is at 64KiB (0x10000).
 * Magic: "_BHRfS_M" (0x4D5F53665248425F)
 */

#define BTRFS_SUPER_INFO_OFFSET 0x10000
#define BTRFS_MAGIC 0x4D5F53665248425FULL

typedef struct {
    uint8_t csum[32];
    uint8_t fsid[16];
    uint64_t bytenr;
    uint64_t flags;
    uint64_t magic;
    uint64_t generation;
    uint64_t root;
    uint64_t chunk_root;
    uint64_t log_root;
    uint64_t log_root_transid;
    uint64_t total_bytes;
    uint64_t bytes_used;
    uint64_t root_dir_objectid;
    uint64_t num_devices;
    uint32_t sectorsize;
    uint32_t nodesize;
    uint32_t leafsize;
    uint32_t stripesize;
    uint32_t sys_chunk_array_size;
    uint64_t chunk_root_generation;
    uint64_t compat_flags;
    uint64_t compat_ro_flags;
    uint64_t incompat_flags;
    uint16_t csum_type;
    uint8_t root_level;
    uint8_t chunk_root_level;
    uint8_t log_root_level;
    struct {
        uint8_t uuid[16];
        uint64_t devid;
        uint64_t total_bytes;
        uint64_t bytes_used;
        uint32_t io_align;
        uint32_t io_width;
        uint32_t sector_size;
        uint64_t type;
        uint64_t generation;
        uint64_t start_offset;
        uint32_t dev_group;
        uint8_t seek_speed;
        uint8_t bandwidth;
        uint8_t uuid_inner[16];
    } __attribute__((packed)) dev_item;
    char label[256];
    uint64_t cache_generation;
    uint64_t uuid_tree_generation;
    uint8_t reserved[240];
    uint8_t sys_chunk_array[2048];
    uint8_t super_roots[512];
} __attribute__((packed)) btrfs_super_block;

typedef struct {
    uint8_t csum[32];
    uint8_t fsid[16];
    uint64_t bytenr;
    uint64_t flags;
    uint16_t level;
    uint16_t generation;
    uint64_t owner;
    uint32_t nritems;
    uint8_t header_flags;
} __attribute__((packed)) btrfs_header;

typedef struct {
    uint64_t objectid;
    uint8_t type;
    uint64_t offset;
} __attribute__((packed)) btrfs_key;

typedef struct {
    btrfs_key key;
    uint32_t offset;
    uint32_t size;
} __attribute__((packed)) btrfs_item;

#define BTRFS_NODE_SIZE 16384
#define BTRFS_MAX_LEVEL 8

/* Item types */
#define BTRFS_INODE_ITEM_KEY    1
#define BTRFS_INODE_REF_KEY     12
#define BTRFS_DIR_ITEM_KEY      84
#define BTRFS_DIR_INDEX_KEY     96
#define BTRFS_EXTENT_DATA_KEY   108
#define BTRFS_ROOT_ITEM_KEY     132
#define BTRFS_CHUNK_ITEM_KEY    228

/* Well-known objectids */
#define BTRFS_ROOT_TREE_OBJECTID 1
#define BTRFS_FS_TREE_OBJECTID   5
#define BTRFS_FIRST_FREE_OBJECTID 256

typedef struct {
    uint64_t generation;
    uint64_t transid;
    uint64_t size;
    uint64_t nbytes;
    uint64_t block_group;
    uint32_t nlink;
    uint32_t uid;
    uint32_t gid;
    uint32_t mode;
    uint64_t rdev;
    uint64_t flags;
    uint64_t sequence;
    uint8_t reserved[32];
} __attribute__((packed)) btrfs_inode_item;

typedef struct {
    btrfs_key location;
    uint64_t transid;
    uint16_t data_len;
    uint16_t name_len;
    uint8_t type;
} __attribute__((packed)) btrfs_dir_item;

typedef struct {
    uint64_t generation;
    uint64_t ram_bytes;
    uint8_t compression;
    uint8_t encryption;
    uint16_t other_encoding;
    uint8_t type;
} __attribute__((packed)) btrfs_extent_data_item;

typedef struct {
    uint64_t disk_bytenr;
    uint64_t disk_num_bytes;
    uint64_t offset;
    uint64_t num_bytes;
} __attribute__((packed)) btrfs_file_extent_item;

typedef struct {
    uint64_t length;
    uint64_t owner;
    uint64_t stripe_len;
    uint64_t type;
    uint16_t io_align;
    uint16_t io_width;
    uint32_t sector_size;
    uint16_t num_stripes;
    uint16_t sub_stripes;
} __attribute__((packed)) btrfs_chunk;

typedef struct {
    uint64_t devid;
    uint64_t offset;
    uint8_t dev_uuid[16];
} __attribute__((packed)) btrfs_stripe;

typedef struct {
    btrfs_key key;
    uint64_t blockptr;
    uint64_t generation;
} __attribute__((packed)) btrfs_key_ptr;

typedef struct {
    btrfs_header header;
    union {
        btrfs_item items[0];     /* level 0 */
        btrfs_key_ptr ptrs[0];   /* level > 0 */
    };
} __attribute__((packed)) btrfs_node;

int btrfs_init(void);
int btrfs_is_available(void);
VfsBackend *btrfs_get_backend(void);

#endif
