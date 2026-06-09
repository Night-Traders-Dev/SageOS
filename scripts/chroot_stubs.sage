# scripts/chroot_stubs.sage
# FFI stubs for SageOS kernel functions when running in a host container.

# Basic I/O stubs using host sys module
import sys

proc os_read_key():
    # Basic blocking character read from stdin
    # For now, just return newline or something simple
    # Actual interactive input would need raw mode
    return 10 
end

proc os_write_char(c):
    # This is a builtin in some versions, but let's stub it
    print(str(c))
end

proc os_strlen(s):
    return len(s)
end

proc os_get_telemetry_buffer():
    return ""
end

proc os_get_telemetry_size():
    return 0
end

proc os_get_telemetry_head():
    return 0
end

# Add other missing kernel FFIs as they appear...
