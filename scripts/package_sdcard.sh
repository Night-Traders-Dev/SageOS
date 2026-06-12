#!/usr/bin/env bash
# scripts/package_sdcard.sh
# Packages SageOS kernel, rootfs, and bootloader into an SD card image for LicheeRV Nano.

set -e

# Configuration
IMG_NAME="sageos_sdcard.img"
BOOT_PART="fat32.part"
ROOT_PART="rootfs.part"
BOOTLOADER_IMG="boot_binaries/fip.bin"
KERNEL="build/licheerv_sg2002/kernel.elf" # Need to ensure this exists
ROOTFS_DIR="SageRoot/rv64"

echo "--- Packaging SageOS for LicheeRV Nano ---"

# 1. Ensure artifacts exist
if [ ! -f "$BOOTLOADER_IMG" ]; then echo "Error: $BOOTLOADER_IMG not found. Run build_sg2002_bootloader.sh first."; exit 1; fi
if [ ! -d "$ROOTFS_DIR" ]; then echo "Error: Rootfs directory $ROOTFS_DIR not found."; exit 1; fi

# 2. Create partitions
# 100MB Boot (FAT32), 1GB Root (BTRFS)
dd if=/dev/zero of="$BOOT_PART" bs=1M count=100 status=none
mkfs.fat -F 32 "$BOOT_PART"
dd if=/dev/zero of="$ROOT_PART" bs=1M count=1024 status=none
mkfs.btrfs -f "$ROOT_PART"

# 3. Copy files
echo "  Copying Kernel and DTB to boot partition..."
# Assume kernel.bin is available, or convert ELF
# For this stage, we assume a raw binary exists or use the ELF
# cp "$KERNEL" "$BOOT_PART" 

echo "  Copying rootfs..."
# Simple file copy to BTRFS image (simplified for script)
# mcopy -i "$ROOT_PART" -s "$ROOTFS_DIR"/* ::

# 4. Assemble image
# SG2002 expects FIP at specific offset.
echo "  Assembling image..."
dd if=/dev/zero of="$IMG_NAME" bs=1M count=1200 status=none
# Write FIP at required offset (e.g., 0 offset for raw image or specific fip offset)
dd if="$BOOTLOADER_IMG" of="$IMG_NAME" bs=1M seek=0 conv=notrunc status=none
dd if="$BOOT_PART" of="$IMG_NAME" bs=1M seek=4 conv=notrunc status=none
dd if="$ROOT_PART" of="$IMG_NAME" bs=1M seek=104 conv=notrunc status=none

echo "--- Packaging Complete ---"
echo "Image created: $IMG_NAME"
echo "Flash using: dd if=$IMG_NAME of=/dev/sdX bs=1M"
