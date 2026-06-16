import drivers.memory.pmm as pmm_mgr
import drivers.memory.vmm as vmm_mgr

proc kmain():
    print "SageOS Booting..."
    
    # STAGE_1 EARLY_MM
    # Initialize PMM starting at 4MB offset
    let global_pmm = pmm_mgr.PMM(0x80400000, 124 * 1024 * 1024)
    
    # Initialize VMM
    let global_vmm = vmm_mgr.VMM(global_pmm, "rv64")
    
    # Identity map first 2MB
    let i = 0
    while i < 512:
        let addr = 0x80000000 + i * 4096
        # Only map kernel and early pool region (0x80000000 to 0x80200000)
        # 0x80000000 + 512 * 4096 = 0x80200000
        if addr < 0x80200000:
            global_vmm.map_page(addr, addr, 0x0E) # R, W, X
        end
        i = i + 1
    end
    print "  PMM/VMM initialized."
    
    print "SageOS System Ready."
    
    while true:
    end
end

kmain()
