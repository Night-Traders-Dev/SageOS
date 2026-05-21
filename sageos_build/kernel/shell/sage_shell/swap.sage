# =============================================================================
# SageOS Swap Device Driver - Pure SageLang Port
# swap.sage
#
# C source kept: sageos_build/kernel/drivers/swap.c (unchanged, regression baseline)
#
# Partition layout (hardcoded, matches C):
#   1: ESP  (FAT32,  64 MiB,  LBA 2048)
#   2: Root (BTRFS, 128 MiB)
#   3: Swap         125 MiB)
# =============================================================================

let SWAP_ESP_MIB   = 64
let SWAP_BTRFS_MIB = 128
let SWAP_SIZE_MIB  = 125
let SWAP_START_LBA = 2048 + (SWAP_ESP_MIB * 1024 * 1024 / 512) + (SWAP_BTRFS_MIB * 1024 * 1024 / 512)

proc cmd_swap():
    os_write_str("\n")
    if os_swap_available() == 0:
        os_write_str("SWAP: No swap device active")
        os_write_str("\n")
        return nil
    end

    os_write_str("SWAP: Partition start LBA: ")
    os_write_str(os_num_to_str(SWAP_START_LBA))
    os_write_str("\n  Size: ")
    os_write_str(os_num_to_str(SWAP_SIZE_MIB))
    os_write_str(" MiB")
    os_write_str("\n  Status: active")
    os_write_str("\n")
end
