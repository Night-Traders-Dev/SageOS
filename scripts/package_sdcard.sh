#!/usr/bin/env bash
# scripts/package_sdcard.sh
# Packages SageOS kernel, rootfs, and bootloader into an SD card image.

set -e

# Configuration
IMG_NAME="sageos_sdcard.img"
ROOTFS_DIR="SageRoot/rv64"
FIP_BIN="boot_binaries/fip.bin"

echo "--- Packaging SageOS for LicheeRV Nano ---"

# 1. Validation
if [ ! -f "$FIP_BIN" ]; then echo "Error: Bootloader $FIP_BIN not found."; exit 1; fi
if [ ! -d "$ROOTFS_DIR" ]; then echo "Error: Rootfs directory $ROOTFS_DIR not found."; exit 1; fi

# 2. Generate Partitioned Disk Image
echo "  Generating base disk image..."
./scripts/gen_virt_disk.sh "$IMG_NAME"

# 3. Merge Rootfs
echo "  Merging rootfs into disk image..."
ROOTFS="$ROOTFS_DIR" DISK_IMG="$IMG_NAME" ./scripts/merge_rootfs.sh

# 4. Write Bootloader (FIP)
echo "  Writing bootloader (FIP) to image..."
# SG2002/LicheeRV Nano typically requires the FIP at a specific offset.
# Assuming standard offset 0 or 0x4400 (if MBR/GPT used).
# Using dd to write directly to the start of the image.
dd if="$FIP_BIN" of="$IMG_NAME" bs=1M seek=0 conv=notrunc status=none

echo "--- Packaging Complete ---"
echo "Final image: $IMG_NAME"
echo "Flash using: dd if=$IMG_NAME of=/dev/sdX bs=1M"
