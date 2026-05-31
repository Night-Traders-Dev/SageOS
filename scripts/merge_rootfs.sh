#!/usr/bin/env bash

# merge_rootfs.sh - Merge the rootfs directory into the virtual disk image

set -e

DISK_IMG="virt.img"
ROOTFS="rootfs"

if [ ! -f "$DISK_IMG" ]; then
    echo "Error: $DISK_IMG not found."
    exit 1
fi

if [ ! -d "$ROOTFS" ]; then
    echo "Error: $ROOTFS directory not found."
    exit 1
fi

echo "Merging $ROOTFS into $DISK_IMG..."

for dir in "$ROOTFS"/*; do
    if [ -d "$dir" ]; then
        dname=$(basename "$dir")
        # Ensure directory exists in the image
        mmd -D s -i "$DISK_IMG@@1M" "::$dname" 2>/dev/null || true
        # Copy contents recursively if not empty
        if [ -n "$(ls -A "$dir")" ]; then
            mcopy -o -s -i "$DISK_IMG@@1M" "$dir"/* "::$dname/"
        fi
    elif [ -f "$dir" ]; then
        mcopy -o -i "$DISK_IMG@@1M" "$dir" ::/
    fi
done

echo "Merge complete!"
