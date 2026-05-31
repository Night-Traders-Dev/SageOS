#!/usr/bin/env bash

# populate_rootfs.sh - Create and populate the SageOS rootfs directory

set -e

ROOTFS="rootfs"
SAGE_BIN="./sage"

echo "Populating $ROOTFS directory..."

# 0. Clean old rootfs
rm -rf "$ROOTFS"
mkdir -p "$ROOTFS"

# 1. Create directory structure
mkdir -p "$ROOTFS/bin"
mkdir -p "$ROOTFS/etc/commands"
mkdir -p "$ROOTFS/etc/system/sagelang"
mkdir -p "$ROOTFS/lib"
mkdir -p "$ROOTFS/system/sagelang"
mkdir -p "$ROOTFS/usr/bin"
mkdir -p "$ROOTFS/usr/lib"
mkdir -p "$ROOTFS/dev"
mkdir -p "$ROOTFS/proc"
mkdir -p "$ROOTFS/tmp"
mkdir -p "$ROOTFS/mnt/fat32"
mkdir -p "$ROOTFS/mnt/btrfs"

# 2. Copy system scripts
echo "  Copying system scripts..."
cp sageos_build/kernel/core/sagelang/*.sage "$ROOTFS/system/sagelang/"
cp sageos_build/kernel/etc/system/sagelang/*.sage "$ROOTFS/etc/system/sagelang/"

# 3. Copy commands
echo "  Copying commands..."
cp sageos_build/kernel/etc/commands/*.sage "$ROOTFS/etc/commands/"

# 4. Copy bytecode if it exists
if [ -f "sageos_build/kernel/fs/vfs_bridge.bc" ]; then
    echo "  Copying VFS bridge bytecode..."
    cp sageos_build/kernel/fs/vfs_bridge.bc "$ROOTFS/lib/"
fi

if [ -f "sageos_build/kernel/shell/sage_shell.bc" ]; then
    echo "  Copying SageShell bytecode..."
    cp sageos_build/kernel/shell/sage_shell.bc "$ROOTFS/lib/"
fi

# 5. Populate /bin with command aliases
for f in "$ROOTFS/etc/commands"/*.sage; do
    name=$(basename "$f")
    cp "$f" "$ROOTFS/bin/$name"
done

echo "Rootfs population complete!"
