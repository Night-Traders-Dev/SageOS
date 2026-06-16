# Unit Test for Virtqueue Descriptor/Ring Manipulation
import drivers.virtio.virtio_blk as virtio_blk
import drivers.memory.pmm as pmm_mgr

print "=== Testing Virtqueue Descriptor Logic ==="

# 1. Mock PMM and Hardware
class MockPMM:
    proc alloc_page(self): return 0x1000 # Return static address

class MockTransport:
    proc init(self, base_addr):
        self.base = base_addr
        self.storage = {}

    proc set_descriptor(self, table_base, idx, addr, len, flags, next):
        let desc_base = table_base + (idx * 16)
        self.storage[str(desc_base + 0)] = addr
        self.storage[str(desc_base + 8)] = len
        self.storage[str(desc_base + 12)] = (flags << 16) | next
        print "  SET_DESC idx=" + str(idx) + " addr=" + str(addr)

    proc set_available_idx(self, avail_base, idx, desc_idx):
        let ring_start = avail_base + 4
        self.storage[str(ring_start + (idx * 2))] = desc_idx
        print "  SET_AVAIL idx=" + str(idx) + " desc=" + str(desc_idx)

    proc set_status(self, status): pass
    proc setup_queue(self, qidx, size, pfn): pass
    proc notify(self, qidx): print "  Device notified on queue " + str(qidx)
    
    # MMIO handlers for testing
    proc poke32(self, addr, val):
        self.storage[str(addr)] = val
        print "  POKE32 [" + str(addr) + "] = " + str(val)
    proc poke64(self, addr, val):
        self.storage[str(addr)] = val
        print "  POKE64 [" + str(addr) + "] = " + str(val)
    proc peek32(self, addr): return 0 # Mock

# Inject mock into VirtIOBlockDriver by overriding transport methods
let pmm = MockPMM()
let blk = virtio_blk.VirtIOBlockDriver(0x10001000, pmm)
let mock_transport = MockTransport(0x10001000)
blk.transport = mock_transport # Swap out real transport for mock

# 2. Test Sector Read (Descriptor Population)
print "Queuing sector read..."
blk.read_sector(1234)

# 3. Verify Descriptor Table (expecting POKE64/POKE32)
# Descriptor 0 (Header): 16 bytes. Starts at 0x1000
if dict_has(mock_transport.storage, str(0x1000)):
    print "SUCCESS_DESC_HEADER_WRITTEN"
else:
    print "FAILURE_DESC_HEADER_WRITTEN"
end

# Descriptor 1 (Buffer): 16 bytes. Starts at 0x1010
if dict_has(mock_transport.storage, str(0x1010)):
    print "SUCCESS_DESC_BUF_WRITTEN"
else:
    print "FAILURE_DESC_BUF_WRITTEN"
end
