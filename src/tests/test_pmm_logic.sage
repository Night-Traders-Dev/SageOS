# Unit Test for PMM Logic
import drivers.memory.pmm as pmm_mgr

print "=== Testing PMM ==="

# 1. Initialize PMM with 1MB of mock RAM (256 pages)
let ram_start = 0x80200000
let ram_size = 1024 * 1024 # 1MB
let pmm = pmm_mgr.PMM(ram_start, ram_size)

if pmm.total_pages == 256:
    print "SUCCESS_PMM_INIT"
else:
    print "FAILURE_PMM_INIT"
end

# 2. Test allocation
let p1 = pmm.alloc_page()
let p2 = pmm.alloc_page()

print "Allocated P1: " + str(p1)
print "Allocated P2: " + str(p2)

if p1 == ram_start and p2 == ram_start + 4096:
    print "SUCCESS_PMM_ALLOC"
else:
    print "FAILURE_PMM_ALLOC"
end

# 3. Test freeing
pmm.free_page(p1)
let p3 = pmm.alloc_page()

print "Allocated P3 (should be same as P1): " + str(p3)
if p3 == p1:
    print "SUCCESS_PMM_FREE"
else:
    print "FAILURE_PMM_FREE"
end

# 4. Test reservation
pmm.reserve_region(ram_start + 0x10000, 0x5000) # Reserve 5 pages starting at offset 64KB
let p4 = pmm.alloc_page()
print "Allocated P4 after reservation: " + str(p4)

# P4 should not be in the reserved range
if p4 >= ram_start + 0x10000 and p4 < ram_start + 0x15000:
    print "FAILURE_PMM_RESERVE"
else:
    print "SUCCESS_PMM_RESERVE"
end
