# Storage & VFS Support for SG2002

This document describes the storage subsystem and VFS integration for the SG2002 (LicheeRV Nano) platform.

## Storage Architecture
The SG2002 platform uses an onboard SDHCI (Secure Digital Host Controller Interface) to communicate with the microSD card, which serves as the primary storage medium.

## Implementation Details
- **SDHCI Driver:** Implemented in `arch/rv64/sg2002/kernel/drivers/sdhci.c`.
- **Block Device Interface:** The driver provides low-level block read/write access (e.g., `sdhci_read_block`).
- **VFS Bridge:** Connects the block device interface to the SageOS VFS layer.
- **Filesystem:** Supports FAT32 for compatibility with the boot medium partition structure.

## Integration
- The block device is registered during kernel platform initialization.
- The VFS layer is configured to mount the FAT32 filesystem from the primary partition of the SD card.

## Future Plans
- Implement DMA-based data transfers for improved throughput.
- Add support for other filesystem formats (e.g., ext4) if needed.
