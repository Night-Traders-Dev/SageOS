# Virtual Filesystem Manager for SageOS
# Orchestrates multiple filesystems and provides a unified interface.

import os.vfs as vfs_lib

class VFSManager:
    proc init(self):
        self.vfs = vfs_lib.create_vfs()
        self.initialized = false

    proc mount(self, path, backend):
        print "VFS: Mounting backend at " + path
        vfs_lib.mount(self.vfs, path, backend)

    proc open(self, path, mode):
        return vfs_lib.vfs_open(self.vfs, path, mode)

    proc read(self, handle, size):
        return vfs_lib.vfs_read(handle, size)

    proc write(self, handle, data):
        return vfs_lib.vfs_write(handle, data)

    proc close(self, handle):
        return vfs_lib.vfs_close(handle)

    proc list_dir(self, path):
        return vfs_lib.vfs_readdir(self.vfs, path)

    proc exists(self, path):
        return vfs_lib.vfs_exists(self.vfs, path)

    proc stat(self, path):
        return vfs_lib.vfs_stat(self.vfs, path)

let global_vfs = VFSManager()
