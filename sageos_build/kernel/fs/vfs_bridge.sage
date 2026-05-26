# vfs_bridge.sage
# Virtual Filesystem Bridge & Sage-native RamFS Driver for SageOS

# -----------------------------------------------------------------------------
# Sage-native RAM Filesystem (RamFS) Class
# -----------------------------------------------------------------------------
class RamFS:
    proc init():
        self.root = {}
        self.root["type"] = "dir"
        self.root["name"] = "/"
        self.root["children"] = {}
        self.root["size"] = 0

    proc _resolve(path):
        let cur = self.root
        let len = os_strlen(path)
        if len == 0 or path == "/":
            return cur

        let pos = 0
        if os_char_at(path, 0) == 47: # '/'
            pos = 1

        while pos < len:
            let start = pos
            while pos < len:
                let c = os_char_at(path, pos)
                if c == 47: # '/'
                    break
                pos = pos + 1

            if pos > start:
                let comp = os_substr(path, start, pos)
                if cur["type"] != "dir":
                    return nil
                let child = cur["children"][comp]
                if child == nil:
                    return nil
                cur = child

            if pos < len and os_char_at(path, pos) == 47: # '/'
                pos = pos + 1
        return cur

    proc _resolve_parent(path):
        let len = os_strlen(path)
        let last_slash = -1
        let i = len - 1
        while i >= 0:
            if os_char_at(path, i) == 47: # '/'
                last_slash = i
                break
            i = i - 1

        if last_slash < 0:
            return [self.root, path]

        let parent_path = "/"
        if last_slash > 0:
            parent_path = os_substr(path, 0, last_slash)

        let name = os_substr(path, last_slash + 1, len)
        let parent_node = self._resolve(parent_path)
        return [parent_node, name]

    proc stat(path):
        let node = self._resolve(path)
        if node == nil:
            return nil
        let st = {}
        st["name"] = node["name"]
        if node["type"] == "dir":
            st["type"] = 1 # VFS_DIRECTORY
        else:
            st["type"] = 0 # VFS_FILE
        st["size"] = node["size"]
        return st

    proc readdir(path):
        let node = self._resolve(path)
        if node == nil or node["type"] != "dir":
            return nil
        let arr = []
        let children = node["children"]
        let keys = dict_keys(children)
        let i = 0
        while i < len(keys):
            let name = keys[i]
            let child = children[name]
            if child != nil:
                let entry = {}
                entry["name"] = name
                if child["type"] == "dir":
                    entry["type"] = 1
                else:
                    entry["type"] = 0
                entry["size"] = child["size"]
                os_array_push(arr, entry)
            i = i + 1
        return arr

    proc read(path, offset, size):
        let node = self._resolve(path)
        if node == nil or node["type"] != "file":
            return nil
        let file_data = node["data"]
        let file_len = os_strlen(file_data)
        if offset >= file_len:
            return ""
        let end_idx = offset + size
        if end_idx > file_len:
            end_idx = file_len
        return os_substr(file_data, offset, end_idx)

    proc write(path, offset, data, size):
        let node = self._resolve(path)
        if node == nil or node["type"] != "file":
            return -2 # VFS_ENOENT

        let data_len = os_strlen(data)
        let actual_size = size
        if actual_size > data_len:
            actual_size = data_len

        let old_data = node["data"]
        let old_len = os_strlen(old_data)

        let prefix = ""
        if offset > 0:
            if offset > old_len:
                prefix = old_data
                let pad_count = offset - old_len
                while pad_count > 0:
                    prefix = prefix + " "
                    pad_count = pad_count - 1
            else:
                prefix = os_substr(old_data, 0, offset)

        let suffix = ""
        let end_idx = offset + actual_size
        if end_idx < old_len:
            suffix = os_substr(old_data, end_idx, old_len)

        let write_data = data
        if actual_size < data_len:
            write_data = os_substr(data, 0, actual_size)

        node["data"] = prefix + write_data + suffix
        node["size"] = os_strlen(node["data"])
        return actual_size

    proc mkdir(path):
        let res = self._resolve_parent(path)
        let parent = res[0]
        let name = res[1]
        if parent == nil or parent["type"] != "dir":
            return -2 # VFS_ENOENT
        if parent["children"][name] != nil:
            return -17 # VFS_EEXIST
        let node = {}
        node["type"] = "dir"
        node["name"] = name
        node["children"] = {}
        node["size"] = 0
        parent["children"][name] = node
        return 0

    proc create(path):
        let res = self._resolve_parent(path)
        let parent = res[0]
        let name = res[1]
        if parent == nil or parent["type"] != "dir":
            return -2 # VFS_ENOENT
        if parent["children"][name] != nil:
            let node = parent["children"][name]
            if node["type"] == "file":
                node["size"] = 0
                node["data"] = ""
                return 0
            return -17 # VFS_EEXIST
        let node = {}
        node["type"] = "file"
        node["name"] = name
        node["size"] = 0
        node["data"] = ""
        parent["children"][name] = node
        return 0

    proc unlink(path):
        let res = self._resolve_parent(path)
        let parent = res[0]
        let name = res[1]
        if parent == nil or parent["type"] != "dir":
            return -2 # VFS_ENOENT
        if parent["children"][name] == nil:
            return -2 # VFS_ENOENT
        parent["children"][name] = nil
        return 0

# -----------------------------------------------------------------------------
# Unified VFS Router
# -----------------------------------------------------------------------------
let g_vfs_mounts = []

proc vfs_mount(path, backend_ptr):
    let m = {}
    m["path"] = path
    m["backend"] = backend_ptr
    m["is_sage"] = 0
    os_array_push(g_vfs_mounts, m)
    return 0

proc vfs_mount_sage(path, sage_backend):
    let m = {}
    m["path"] = path
    m["backend"] = sage_backend
    m["is_sage"] = 1
    os_array_push(g_vfs_mounts, m)
    return 0

proc vfs_resolve(path):
    if os_strlen(path) == 0:
        return nil

    let best_m = nil
    let best_len = -1

    let i = 0
    let m_count = os_array_len(g_vfs_mounts)

    while i < m_count:
        let m = g_vfs_mounts[i]
        let m_path = m["path"]
        let m_len = os_strlen(m_path)

        # Longest prefix match
        if os_starts_with(path, m_path):
            let is_match = 0
            if m_len == 1:
                let c = os_char_at(m_path, 0)
                if c == 47: # '/'
                    is_match = 1
            elif os_strlen(path) == m_len:
                is_match = 1
            elif os_char_at(path, m_len) == 47: # '/'
                is_match = 1

            if is_match == 1:
                if m_len > best_len:
                    best_len = m_len
                    best_m = m
        i = i + 1

    if best_m == nil:
        return nil

    # Calculate relative path
    let rel = "/"
    if best_len > 1:
        rel = os_substr(path, best_len, os_strlen(path))
        if os_strlen(rel) == 0:
            rel = "/"
        elif os_char_at(rel, 0) != 47: # '/'
            rel = "/" + rel
    else:
        rel = path

    let res = {}
    res["mount"] = best_m
    res["rel"] = rel
    return res

proc vfs_stat(path):
    let res = vfs_resolve(path)
    if res == nil: return nil
    let mount = res["mount"]
    if mount["is_sage"] == 1:
        return mount["backend"].stat(res["rel"])
    else:
        return os_backend_stat(mount["backend"], res["rel"])

proc vfs_readdir(path):
    let res = vfs_resolve(path)
    if res == nil: return nil
    let mount = res["mount"]
    if mount["is_sage"] == 1:
        return mount["backend"].readdir(res["rel"])
    else:
        return os_backend_readdir(mount["backend"], res["rel"])

proc vfs_read(path, offset, size):
    let res = vfs_resolve(path)
    if res == nil: return nil
    let mount = res["mount"]
    if mount["is_sage"] == 1:
        let data = mount["backend"].read(res["rel"], offset, size)
        if data == nil: return nil
        return [data, os_strlen(data)]
    else:
        let data = os_backend_read(mount["backend"], res["rel"], offset, size)
        if data == nil: return nil
        return [data, os_strlen(data)]

proc vfs_write(path, offset, data, size):
    let res = vfs_resolve(path)
    if res == nil: return 0
    let mount = res["mount"]
    if mount["is_sage"] == 1:
        return mount["backend"].write(res["rel"], offset, data, size)
    else:
        return os_backend_write(mount["backend"], res["rel"], offset, data, size)

proc vfs_mkdir(path):
    let res = vfs_resolve(path)
    if res == nil: return -2
    let mount = res["mount"]
    if mount["is_sage"] == 1:
        return mount["backend"].mkdir(res["rel"])
    else:
        return -1

proc vfs_create(path):
    let res = vfs_resolve(path)
    if res == nil: return -2
    let mount = res["mount"]
    if mount["is_sage"] == 1:
        return mount["backend"].create(res["rel"])
    else:
        return -1

proc vfs_unlink(path):
    let res = vfs_resolve(path)
    if res == nil: return -2
    let mount = res["mount"]
    if mount["is_sage"] == 1:
        return mount["backend"].unlink(res["rel"])
    else:
        return -1

# -----------------------------------------------------------------------------
# Filesystem Initializer / Populator
# -----------------------------------------------------------------------------
proc vfs_init_fs():
    let r = RamFS()

    # Pre-create standard layout
    r.mkdir("/etc")
    r.mkdir("/etc/commands")
    r.mkdir("/bin")
    r.mkdir("/dev")
    r.mkdir("/proc")
    r.mkdir("/tmp")
    r.mkdir("/fat32")
    r.mkdir("/btrfs")

    # Fetch and populate all C-embedded files dynamically
    let count = os_get_embedded_count()
    let i = 0
    while i < count:
        let file_info = os_get_embedded_file(i)
        if file_info != nil:
            r.create(file_info["path"])
            let data = file_info["data"]
            r.write(file_info["path"], 0, data, os_strlen(data))
        i = i + 1

    # Mount our clean Sage-native RamFS on "/"
    vfs_mount_sage("/", r)

# Automatically bootstrap filesystems on startup
vfs_init_fs()
