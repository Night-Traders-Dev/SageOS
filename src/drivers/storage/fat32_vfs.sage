# FAT32 VFS Backend for SageOS
# Plugs the FAT32 driver into the VFS system.

import drivers.storage.fat32 as fat32
import os.vfs as vfs_lib

class FAT32VFSBackend:
    proc init(self, block_driver):
        self.driver = fat32.FAT32Driver(block_driver.read_sector)
        self.block_driver = block_driver
        self.mounted = self.driver.mount()

    proc open(self, path, mode):
        if not self.mounted: return nil end
        let entry = self.driver.resolve_path(path)
        if entry == nil:
            if (mode & vfs_lib.VFS_CREATE) != 0:
                print "FAT32: Write/Create support pending."
                return nil
            end
            return nil
        end
        if entry["is_dir"]: return nil end
        
        # Return internal handle
        return entry

    proc read(self, internal_handle, pos, size):
        # We need a partial read from cluster chain
        # Our current FAT32Driver.read_file reads the whole thing
        # Let's add a refined read to the driver or handle it here
        let full_data = self.driver.read_file(internal_handle["cluster"], internal_handle["size"])
        if full_data == nil: return [] end
        
        return slice(full_data, pos, pos + size)

    proc stat(self, path):
        let entry = self.driver.resolve_path(path)
        if entry == nil: return nil end
        
        let type = vfs_lib.VFS_FILE
        if entry["is_dir"]: type = vfs_lib.VFS_DIR end
        
        return vfs_lib.make_stat(type, entry["size"], entry["name"])

    proc readdir(self, path):
        let entry = self.driver.resolve_path(path)
        if entry == nil or not entry["is_dir"]: return nil end
        
        let raw_entries = self.driver.read_directory(entry["cluster"])
        let vfs_entries = []
        
        for e in raw_entries:
            let type = vfs_lib.VFS_FILE
            if e["is_dir"]: type = vfs_lib.VFS_DIR end
            push(vfs_entries, vfs_lib.make_dirent(e["name"], type, e["size"]))
        end
        
        return vfs_entries

    proc close(self, internal_handle):
        return 0
