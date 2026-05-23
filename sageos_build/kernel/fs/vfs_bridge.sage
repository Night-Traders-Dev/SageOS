# vfs_bridge.sage
# Virtual Filesystem Bridge for SageOS
# Handles path normalization and backend routing.

let g_vfs_mounts = []

proc vfs_mount(path, backend_ptr):
    let m = {}
    m["path"] = path
    m["backend"] = backend_ptr
    os_array_push(g_vfs_mounts, m)
    return 0
end

proc vfs_resolve(path):
    if os_strlen(path) == 0:
        return nil
    end
    
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
                end
            elif os_strlen(path) == m_len:
                is_match = 1
            elif os_char_at(path, m_len) == 47: # '/'
                is_match = 1
            end
            
            if is_match == 1:
                if m_len > best_len:
                    best_len = m_len
                    best_m = m
                end
            end
        end
        i = i + 1
    end
    
    if best_m == nil:
        return nil
    end

    # Calculate relative path
    let rel = "/"
    if best_len > 1:
        rel = os_substr(path, best_len, os_strlen(path))
        if os_strlen(rel) == 0:
            rel = "/"
        elif os_char_at(rel, 0) != 47: # '/'
            rel = "/" + rel
        end
    else:
        rel = path
    end

    let res = {}
    res["mount"] = best_m
    res["rel"] = rel
    return res
end

proc vfs_stat(path):
    let res = vfs_resolve(path)
    if res == nil: return nil end
    return os_backend_stat(res["mount"]["backend"], res["rel"])
end

proc vfs_readdir(path):
    let res = vfs_resolve(path)
    if res == nil: return nil end
    return os_backend_readdir(res["mount"]["backend"], res["rel"])
end

proc vfs_read(path, offset, size):
    let res = vfs_resolve(path)
    if res == nil: return nil end
    let data = os_backend_read(res["mount"]["backend"], res["rel"], offset, size)
    if data == nil: return nil end
    # Return [data, actual_length]
    return [data, os_strlen(data)]
end

proc vfs_write(path, offset, data, size):
    let res = vfs_resolve(path)
    if res == nil: return 0 end
    return os_backend_write(res["mount"]["backend"], res["rel"], offset, data, size)
end
