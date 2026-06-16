#include "drivers/sdhci.h"
#include "../../core/sageos_build/kernel/include/vfs.h"
#include <string.h>

// VFS callback for reading from the block device
static int sdhci_vfs_read(VfsBackend *self, const char *rel_path, uint64_t offset, void *buffer, size_t size) {
    // 1. Convert offset to block address
    // 2. Call sdhci_read_block
    return 0; // Simplified
}

// VFS backend structure for SDHCI
static VfsBackend sdhci_backend = {
    .name = "sdhci",
    .read = sdhci_vfs_read,
    // .stat, .readdir, etc. can be added as needed
};

void sg2002_storage_init() {
    sdhci_init();
    vfs_mount("/sd", &sdhci_backend);
}
