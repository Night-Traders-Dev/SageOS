import drivers.memory.pmm as pmm_mgr
import drivers.memory.vmm as vmm_mgr

## SageOS kernel entry point.
##
## The `end` terminators this file used to carry after every `if`, `while` and
## `proc` block are not valid SageLang. Blocks are delimited by indentation, and
## the compiler rejects `end` outright:
##
##   error: unknown name 'end' in compiled code
##
## which meant `make ARCH=rv64` could not even generate the kernel bytecode:
## "Failed to generate SVM from kernel/main.sage". Every architecture was
## affected, and the failure looked like a build-system problem rather than a
## source one. The terminators are simply removed here.
##
## Note the PMM is handed 0x80400000 with 124 MB, and the identity map below
## deliberately stops at 0x80200000 -- which is also the load address in
## arch/rv64/linker.ld and SageBoot's KERNEL_LOAD_ADDR, so the kernel never maps
## over its own text.

proc kmain():
    print "SageOS Booting..."

    # STAGE_1 EARLY_MM
    print "  Initializing PMM..."
    let global_pmm = pmm_mgr.PMM(0x80400000, 124 * 1024 * 1024)
    print "  PMM Init OK."

    # Initialize VMM
    print "  Initializing VMM..."
    let global_vmm = vmm_mgr.VMM(global_pmm, "rv64")
    print "  VMM Init OK."

    # Identity map first 2MB
    print "  Identity mapping memory..."
    let i = 0
    while i < 512:
        let addr = 0x80000000 + i * 4096
        if addr < 0x80200000:
            print "  Mapping addr " + str(addr)
            global_vmm.map_page(addr, addr, 0x0E) # R, W, X
        i = i + 1
    print "  PMM/VMM initialized."

    print "SageOS System Ready."

    # Park. There is nothing to schedule yet, and returning from kmain would
    # drop into whatever follows the kernel in memory.
    while true:
        print "."

kmain()
