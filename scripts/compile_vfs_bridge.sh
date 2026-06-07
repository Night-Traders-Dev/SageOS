#!/bin/bash
# scripts/compile_vfs_bridge.sh
#
# Compiles the SageLang VFS bridge into bytecode and emits it as a C header.

set -euo pipefail

SAGE="${1:-sage}"
OUT_DIR="${2:-sageos_build/kernel/fs}"
INPUT="${OUT_DIR}/vfs_bridge.sage"
BYTECODE="${OUT_DIR}/vfs_bridge.bc"
OUT_H="${OUT_DIR}/vfs_bridge_bytecode.h"

echo "[vfs-bridge] Compiling to bytecode..."
"${SAGE}" --emit-vm "${INPUT}" -o "${BYTECODE}"

echo "[vfs-bridge] Generating binary SGVM and C header..."
"${SAGE}" scripts/compile_to_sgvm.sage "${BYTECODE}" "${BYTECODE}" --header "${OUT_H}" "vfs_bridge_bytecode"

echo "[vfs-bridge] Done."
