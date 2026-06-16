# Unit Test for VMM Logic
import drivers.memory.pmm as pmm_mgr
import drivers.memory.vmm as vmm_mgr

print "=== Testing VMM ==="

# 1. Mock Hardware
class MockHardware:
    proc init(self):
        self.storage = {}
    proc peek64(self, addr):
        let key = str(addr | 0)
        if dict_has(self.storage, key): return self.storage[key] end
        return 0
    proc poke64(self, addr, val):
        self.storage[str(addr | 0)] = val

let ram_start = 0x80200000
let pmm = pmm_mgr.PMM(ram_start, 1024 * 1024)
let hw = MockHardware()

# 2. Initialize VMM
let vmm = vmm_mgr.VMM(pmm, "rv64", hw)
print "VMM Initialized."

# 3. Test Mapping
let vaddr = 0x1000
let paddr = 0x90000000
let flags = 0x0E # R, W, X bits (bits 1, 2, 3)

print "Mapping 0x1000 -> 0x90000000..."
vmm.map_page(vaddr, paddr, flags)

# 4. Verify walk
let root = vmm.root_pt
let l1_pte = hw.peek64(root) # VPN2=0, so index 0
if (l1_pte % 2) != 0:
    print "SUCCESS_VMM_L2_LINK"
else:
    print "FAILURE_VMM_L2_LINK"
end

let l1_addr = ((l1_pte / 1024) | 0) * 4096
let l0_pte = hw.peek64(l1_addr) # VPN1=0
if (l0_pte % 2) != 0:
    print "SUCCESS_VMM_L1_LINK"
else:
    print "FAILURE_VMM_L1_LINK"
end

let l0_addr = ((l0_pte / 1024) | 0) * 4096
let leaf_pte = hw.peek64(l0_addr + 8) # VPN0 = 1

if (leaf_pte % 2) != 0 and ((leaf_pte / 1024) | 0) == (paddr / 4096):
    print "SUCCESS_VMM_LEAF_MAPPING"
else:
    print "FAILURE_VMM_LEAF_MAPPING"
end
