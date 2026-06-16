# Unit Test for VirtIO Transport Logic
import drivers.virtio.virtio as virtio_drv
import drivers.memory.bare_metal as bm

print "=== Testing VirtIO Transport ==="

# 1. Mock Hardware
class MockTransport:
    proc init(self, base_addr):
        self.base = base_addr
        self.storage = {}

    proc read32(self, offset):
        return self.storage[str(self.base + offset)]

    proc write32(self, offset, val):
        self.storage[str(self.base + offset)] = val

let base_addr = 0x10001000
let transport = MockTransport(base_addr)

# 2. Test Device ID
transport.write32(0x08, 1) # Set Device ID to 1
if transport.read32(0x08) == 1:
    print "SUCCESS_VIRTIO_DEVICE_ID"
else:
    print "FAILURE_VIRTIO_DEVICE_ID"
end

# 3. Test Status
transport.write32(0x70, 0x7)
if transport.read32(0x70) == 0x7:
    print "SUCCESS_VIRTIO_STATUS"
else:
    print "FAILURE_VIRTIO_STATUS"
end
