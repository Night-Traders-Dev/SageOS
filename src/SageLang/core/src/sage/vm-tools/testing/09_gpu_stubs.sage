# Test: GPU Opcodes (Stubs)
import gpu

# We just test that calling them doesn't crash the VM
# and correctly interacts with the delegation bridge stubs.

try:
    var t = gpu.get_time()
    print("gpu time: " + str(type(t)))
    
    # Poll events
    gpu.poll_events()
    
    # Mouse pos
    var pos = gpu.mouse_pos()
    print("mouse pos: " + str(type(pos)))
catch e:
    print("GPU test failed: " + str(e))
