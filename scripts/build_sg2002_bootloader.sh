#!/usr/bin/env bash
# scripts/build_sg2002_bootloader.sh
# Automates the complete build of FSBL, OpenSBI, U-Boot, and FIP generation.

set -e

# Configuration
CROSS_COMPILE="riscv64-linux-gnu-"
WORKDIR=$(pwd)
BUILD_DIR="$WORKDIR/boot_binaries"
DATA_DIR="$WORKDIR/fiptool/data"

mkdir -p "$BUILD_DIR"
mkdir -p "$DATA_DIR"

# 1. OpenSBI
echo "--- Building OpenSBI ---"
if [ ! -d "opensbi" ]; then git clone https://github.com/riscv-software-src/opensbi.git; fi
cd opensbi
make distclean
make PLATFORM=generic CROSS_COMPILE=$CROSS_COMPILE
cp build/platform/generic/firmware/fw_dynamic.bin "$DATA_DIR/"
cd "$WORKDIR"

# 2. U-Boot
echo "--- Building U-Boot ---"
if [ ! -d "u-boot" ]; then git clone https://github.com/u-boot/u-boot.git; fi
cd u-boot
make distclean
# Assuming LicheeRV Nano defconfig
make sipeed_licheerv_nano_defconfig
make CROSS_COMPILE=$CROSS_COMPILE
cp u-boot-dtb.bin "$DATA_DIR/"
cd "$WORKDIR"

# 3. FIPTool (Packaging)
echo "--- Packaging FIP ---"
if [ ! -d "fiptool" ]; then git clone https://github.com/sophgo/fiptool.git; fi
cd fiptool
# Ensure shebang uses python3
sed -i '1s|#!/usr/bin/env python|#!/usr/bin/env python3|' fiptool
# Build/Run
make
cp fip.bin "$BUILD_DIR/"
cd "$WORKDIR"

echo "--- Build Complete ---"
echo "FIP image is at: $BUILD_DIR/fip.bin"
