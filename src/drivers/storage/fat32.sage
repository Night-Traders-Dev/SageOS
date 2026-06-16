# FAT32 Storage Driver for SageOS
# Implements MBR, BPB, FAT chain, and Directory Traversal
# Leverages core SageLang FAT libraries for logic

import os.fat as fat_lib
import os.fat_dir as fat_dir

class FAT32Driver:
    proc init(self, read_sector_cb):
        self.read_sector = read_sector_cb
        self.partition_lba = 0
        self.fs_info = {}
        self.initialized = false

    proc mount(self):
        # 1. Read MBR (Sector 0)
        let mbr = self.read_sector(0)
        if len(mbr) < 512:
            print "ERROR: Failed to read MBR"
            return false
        end

        # Check signature 0x55AA
        if mbr[510] != 0x55 or mbr[511] != 0xAA:
            print "ERROR: Invalid MBR signature"
            return false
        end

        # Find first FAT32 partition (simplification)
        # Partition 1 starts at offset 446
        # Partition type at offset 446 + 4
        # LBA start at offset 446 + 8
        self.partition_lba = mbr[446 + 8] + mbr[446 + 9] * 256 + mbr[446 + 10] * 65536 + mbr[446 + 11] * 16777216
        
        # 2. Parse BPB
        let bpb_sector = self.read_sector(self.partition_lba)
        self.fs_info = fat_lib.parse_boot_sector(bpb_sector)
        
        if self.fs_info["fat_type"] != "FAT32":
            print "ERROR: Not a FAT32 partition"
            return false
        end

        print "FAT32 Mounted at LBA " + str(self.partition_lba)
        print "Root Cluster: " + str(self.fs_info["root_cluster"])
        self.initialized = true
        return true

    # Internal helper to wrap the sector reader for fat_dir
    proc _read_fat_sector(self, lba):
        return self.read_sector(self.partition_lba + lba)
    end

    proc read_fat_entry(self, cluster):
        # We need a disk object that fat_dir expects, but fat_dir functions 
        # like read_fat_entry take 'disk' as an array. 
        # For a driver, we need to adapt this to use our read_sector callback.
        
        let ft = self.fs_info["fat_type"]
        let fat_start_sector = self.fs_info["reserved_sectors"]
        
        let entry_info = fat_lib.fat_entry_offset(self.fs_info, cluster)
        let byte_offset = entry_info["byte_offset"]
        let sector_offset = (byte_offset / self.fs_info["sector_size"]) | 0
        let inner_offset = byte_offset % self.fs_info["sector_size"]
        
        let sector_data = self._read_fat_sector(fat_start_sector + sector_offset)
        
        if ft == "FAT32":
            return fat_dir.read_u32(sector_data, inner_offset) & 268435455
        end
        if ft == "FAT16":
            return fat_dir.read_u16(sector_data, inner_offset)
        end
        return 0
    end

    proc is_eof(self, entry):
        return fat_dir.is_end_of_chain(self.fs_info, entry)
    end

    proc read_directory(self, cluster):
        let entries = []
        let current_cluster = cluster
        let lfn_fragments = []
        
        while not self.is_eof(current_cluster):
            let lba = fat_lib.cluster_to_lba(self.fs_info, current_cluster)
            
            for i in range(0, self.fs_info["sectors_per_cluster"]):
                let sector = self._read_fat_sector(lba + i)
                
                for j in range_step(0, self.fs_info["sector_size"], 32):
                    let first_byte = sector[j]
                    if first_byte == 0x00: return entries end
                    if first_byte == 0xE5: 
                        lfn_fragments = []
                        continue 
                    end
                    
                    let attr = sector[j + 11]
                    if attr == 0x0F:
                        # LFN entry
                        let sequence = first_byte
                        # Fragments are in reverse order. We store them to prepend later.
                        let frag = self.parse_lfn_entry(slice(sector, j, j + 32))
                        push(lfn_fragments, frag)
                        continue
                    end
                    
                    let entry = fat_dir.parse_dir_entry(sector, j)
                    if entry != nil and not entry["is_volume"]:
                        if len(lfn_fragments) > 0:
                            entry["name"] = self.assemble_lfn(lfn_fragments)
                            lfn_fragments = []
                        end
                        push(entries, entry)
                    else:
                        lfn_fragments = []
                    end
                end
            end
            current_cluster = self.read_fat_entry(current_cluster)
        end
        return entries

    proc parse_lfn_entry(self, data):
        let name = ""
        # Characters are UCS-2 (2 bytes each)
        # Offset 1-10 (5 chars), 14-25 (6 chars), 28-31 (2 chars)
        let offsets = [1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30]
        for off in offsets:
            let low = data[off]
            let high = data[off + 1]
            if low == 0 and high == 0: break end
            if low == 0xFF and high == 0xFF: break end
            name = name + chr(low)
        end
        return name

    proc assemble_lfn(self, fragments):
        let full_name = ""
        # Fragments were pushed in order they appeared (usually reverse sequence)
        # Sequence 0x41 (Last fragment) comes first in directory listing
        for i in range(len(fragments)):
            full_name = full_name + fragments[len(fragments) - 1 - i]
        end
        return full_name

    proc list_root(self):
        return self.read_directory(self.fs_info["root_cluster"])
    end

    proc read_file(self, start_cluster, size):
        let data = []
        let current_cluster = start_cluster
        let bytes_remaining = size
        
        while bytes_remaining > 0 and not self.is_eof(current_cluster):
            let lba = fat_lib.cluster_to_lba(self.fs_info, current_cluster)
            
            for i in range(0, self.fs_info["sectors_per_cluster"]):
                if bytes_remaining <= 0: break end
                let sector = self._read_fat_sector(lba + i)
                let to_read = self.fs_info["sector_size"]
                if bytes_remaining < to_read: to_read = bytes_remaining end
                
                for k in range(0, to_read):
                    push(data, sector[k])
                end
                bytes_remaining = bytes_remaining - to_read
            end
            current_cluster = self.read_fat_entry(current_cluster)
        end
        return data

    proc resolve_path(self, path):
        if path == "/" or path == "":
            return {"is_dir": true, "cluster": self.fs_info["root_cluster"]}
        end
        
        let parts = []
        let current = ""
        for i in range(len(path)):
            if path[i] == "/":
                if len(current) > 0: push(parts, current) end
                current = ""
            else:
                current = current + path[i]
            end
        end
        if len(current) > 0: push(parts, current) end
        
        let current_entries = self.list_root()
        let target_entry = nil
        
        for i in range(len(parts)):
            target_entry = fat_dir.find_entry(current_entries, parts[i])
            if target_entry == nil: return nil end
            
            if i < len(parts) - 1:
                if not target_entry["is_dir"]: return nil end
                current_entries = self.read_directory(target_entry["cluster"])
            end
        end
        return target_entry

    proc read_file_by_path(self, path):
        let entry = self.resolve_path(path)
        if entry == nil or entry["is_dir"]: return nil end
        return self.read_file(entry["cluster"], entry["size"])
