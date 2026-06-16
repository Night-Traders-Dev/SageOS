# Unit Test for VFS Logic
import drivers.vfs as vfs_mgr
import os.vfs as vfs_lib

print "=== Testing VFS Manager ==="

# 1. Initialize VFS
let mgr = vfs_mgr.VFSManager()
print "VFS Initialized."

# 2. Create a MemFS backend
let memfs = vfs_lib.create_memfs()
mgr.mount("/mem", memfs)
print "MemFS mounted at /mem"

# 3. Create a file
let handle = mgr.open("/mem/test.txt", vfs_lib.VFS_CREATE | vfs_lib.VFS_WRITE)
if handle == nil:
    print "FAILURE: Could not create file"
    exit(1)
end

let data = [72, 101, 108, 108, 111] # "Hello"
mgr.write(handle, data)
mgr.close(handle)
print "File /mem/test.txt created and written."

# 4. Read the file
let read_handle = mgr.open("/mem/test.txt", vfs_lib.VFS_READ)
let read_data = mgr.read(read_handle, 5)
mgr.close(read_handle)

print "Data read back:"
print read_data

if len(read_data) == 5 and read_data[0] == 72:
    print "SUCCESS_VFS_READ_WRITE"
else:
    print "FAILURE_VFS_READ_WRITE"
end

# 5. List directory
let entries = mgr.list_dir("/mem")
print "Directory listing /mem:"
for e in entries:
    print "  " + e["name"]
end

if len(entries) > 0:
    print "SUCCESS_VFS_LIST_DIR"
else:
    print "FAILURE_VFS_LIST_DIR"
end
