# Test script for Native Bridge and Security Sandboxing

import mem
import struct
import ffi
import sys

# Test mem module
print "Testing mem module..."
let ptr = mem.alloc(1024)
if ptr != nil:
    print "mem.alloc(1024) success"
    mem.write(ptr, 0, "int", 42)
    let val = mem.read(ptr, 0, "int")
    print "mem.read(ptr, 0, 'int') = " + str(val)
    if val == 42:
        print "mem read/write test passed"
    else:
        print "mem read/write test failed"
    mem.free(ptr)
    print "mem.free(ptr) success"
else:
    print "mem.alloc(1024) failed"

# Test struct module
print "Testing struct module..."
let point_def = struct.def([["x", "int"], ["y", "int"]])
let p = struct.new(point_def)
struct.set(p, point_def, "x", 10)
struct.set(p, point_def, "y", 20)
let px = struct.get(p, point_def, "x")
let py = struct.get(p, point_def, "y")
print "point.x = " + str(px) + ", point.y = " + str(py)
if px == 10 and py == 20:
    print "struct test passed"
else:
    print "struct test failed"

# Test ffi module (if possible, try to load libc)
print "Testing ffi module..."
try:
    let libc_path = "/usr/lib/aarch64-linux-gnu/libc.so.6"
    let libc = ffi.open(libc_path)
    if libc == nil:
        # Try generic name
        libc = ffi.open("libc.so.6")
    
    if libc != nil:
        print "ffi.open(libc) success"
        # Try a simple call: abs(-42)
        # Note: ffi.call(handle, name, return_type, args)
        let abs_val = ffi.call(libc, "abs", "int", [-42])
        print "ffi.call(libc, 'abs', 'int', [-42]) = " + str(abs_val)
        if abs_val == 42:
            print "ffi test passed"
        else:
            print "ffi test failed"
        ffi.close(libc)
    else:
        print "ffi.open(libc) failed (might be expected on some platforms)"
catch e:
    print "ffi test error: " + str(e)

# Test sys.exec (should be disabled in safe mode)
print "Testing sys.exec..."
try:
    sys.exec("ls") # Simple command without special characters
catch e:
    print "sys.exec error: " + str(e)

print "All tests finished"
