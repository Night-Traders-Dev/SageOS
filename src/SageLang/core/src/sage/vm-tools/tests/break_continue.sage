# Test break and continue
# Note: These are currently stubs in sgvm_vm.sage and will halt the VM.
# We expect this test to FAIL or show "Error: Unexpected loop break opcode"

print "Loop start"
var i = 0
while i < 5:
    i = i + 1
    if i == 3:
        continue
    if i == 4:
        break
    print i
print "Loop end"
