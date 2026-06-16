import kernel.interrupts as trap_mgr
import drivers.memory.pmm as pmm_mgr
import drivers.memory.vmm as vmm_mgr
import drivers.storage.fat32_vfs as fat32_vfs
import drivers.virtio.virtio_blk as virtio_blk
import drivers.vfs as vfs_mgr
import drivers.swap as swap_mgr

proc kmain():
    print "SageOS Booting..."
    
    # STAGE_1 EARLY_MM
    # Initialize PMM starting at 4MB offset to avoid OpenSBI and Kernel
    let global_pmm = pmm_mgr.PMM(0x80400000, 124 * 1024 * 1024)
    
    # Initialize VMM
    let global_vmm = vmm_mgr.VMM(global_pmm, "rv64")
    
    # Identity map first 2MB
    let i = 0
    while i < 512:
        let addr = 0x80000000 + i * 4096
        global_vmm.map_page(addr, addr, 0x0E) # R, W, X
        i = i + 1
    end
    print "  PMM/VMM initialized."

    # STAGE_4 STORAGE_VFS
    # VirtIO-Block base address (QEMU virt machine)
    let block_dev = virtio_blk.VirtIOBlockDriver(0x10001000, global_pmm)
    let fat_backend = fat32_vfs.FAT32VFSBackend(block_dev)
    
    vfs_mgr.global_vfs.mount("/", fat_backend)
    print "  VFS Layer active. Root mounted via VirtIO-Block."

    # STAGE_5 RUNTIME_BRINGUP
    let global_swap = swap_mgr.SwapManager(block_dev, 100000, 8192)
    print "  SWAP Subsystem active."

    print "SageOS System Ready."
    
    while true:
        trap_mgr.poll_traps()
    end
end

kmain()
