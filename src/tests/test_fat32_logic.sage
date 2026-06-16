# Unit Test for FAT32 Driver Logic
import drivers.storage.fat32 as fat32
import os.fat as fat_lib

print "=== Testing FAT32 Driver ==="

# 1. Create a mock BPB for FAT32 (Sector 0 of partition)
let bpb = []
for i in range(0, 512): push(bpb, 0) end

# BPB Common Fields
bpb[11] = 0
bpb[12] = 2   # 512 bytes per sector
bpb[13] = 8                # 8 sectors per cluster
bpb[14] = 32
bpb[15] = 0  # 32 reserved sectors
bpb[16] = 2                # 2 FATs
bpb[17] = 0
bpb[18] = 0   # 0 root entries (FAT32)
bpb[19] = 0
bpb[20] = 0   # 0 total sectors 16
bpb[21] = 0xF8             # Fixed disk
bpb[22] = 0
bpb[23] = 0   # 0 FAT size 16

# FAT32 Specific Fields (offset 36)
# total sectors 32 (offset 32)
# Need > 65525 * 8 sectors for FAT32
bpb[32] = 0
bpb[33] = 0
bpb[34] = 0x10
bpb[35] = 0 # 1,048,576 sectors
# FAT size 32 (offset 36)
bpb[36] = 0x40
bpb[37] = 0
bpb[38] = 0
bpb[39] = 0 # 64 sectors
# Root Cluster (offset 44)
bpb[44] = 2
bpb[45] = 0
bpb[46] = 0
bpb[47] = 0 # Cluster 2

# 2. Mock Block Driver
class MockDisk:
    proc init(self, bpb_data):
        self.bpb = bpb_data
        self.storage = {}
        # MBR signature
        let mbr = []
        for i in range(0, 512): push(mbr, 0) end
        mbr[510] = 0x55
        mbr[511] = 0xAA
        # Partition 1 starts at LBA 32
        mbr[446 + 8] = 32
        mbr[446 + 9] = 0
        mbr[446 + 10] = 0
        mbr[446 + 11] = 0
        self.storage["0"] = mbr
        self.storage["32"] = bpb_data

    proc read_sector(self, lba):
        let key = str(lba)
        if dict_has(self.storage, key):
            return self.storage[key]
        end
        let empty = []
        for i in range(0, 512): push(empty, 0) end
        return empty

let disk = MockDisk(bpb)

# 3. Initialize FAT32 Driver
proc wrap_read(lba):
    return disk.read_sector(lba)
end
let driver = fat32.FAT32Driver(wrap_read)

# 4. Mount
print "Attempting to mount..."
if driver.mount():
    print "SUCCESS_FAT32_MOUNT"
else:
    print "FAILURE_FAT32_MOUNT"
    # exit(1) # exit not defined
end

# 5. Verify parsed info
if driver.fs_info["fat_type"] == "FAT32":
    print "SUCCESS_FAT32_TYPE"
else:
    print "FAILURE_FAT32_TYPE"
end

if driver.fs_info["root_cluster"] == 2:
    print "SUCCESS_FAT32_ROOT_CLUSTER"
else:
    print "FAILURE_FAT32_ROOT_CLUSTER"
end
