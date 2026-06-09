#!/bin/bash
# scripts/compile_sage_shell.sh
#
# Compiles the SageLang shell sources into a single bytecode blob and
# emits it as a C header: kernel/shell/sage_shell_bytecode.h

set -euo pipefail

SAGE="${1:-sage}"
OUT_DIR="${2:-sageos_build/kernel/shell}"

SAGE_SHELL_DIR="${OUT_DIR}"
INPUT_HELPER="${SAGE_SHELL_DIR}/sage_shell/input.sage"
COMMANDS="${SAGE_SHELL_DIR}/sage_shell/commands.sage"
DMESG="${SAGE_SHELL_DIR}/sage_shell/dmesg.sage"
NEOFETCH="${SAGE_SHELL_DIR}/sage_shell/neofetch.sage"
STATUS="${SAGE_SHELL_DIR}/sage_shell/status.sage"
POWER="${SAGE_SHELL_DIR}/sage_shell/power.sage"
KERNEL_INFO="${SAGE_SHELL_DIR}/sage_shell/kernel_info.sage"
INPUT="${SAGE_SHELL_DIR}/sage_shell/shell.sage"

BYTECODE="${SAGE_SHELL_DIR}/sage_shell.bc"
OUT_H="${SAGE_SHELL_DIR}/sage_shell_bytecode.h"
COMBINED="${SAGE_SHELL_DIR}/sage_shell_combined.sage"

echo "[sage-shell] Combining .sage sources..."
cat "${INPUT_HELPER}" "${COMMANDS}" "${DMESG}" "${NEOFETCH}" "${STATUS}" "${POWER}" "${KERNEL_INFO}" "${INPUT}" > "${COMBINED}"
mkdir -p "$(dirname "${OUT_DIR}")/bin"
cp "${COMBINED}" "$(dirname "${OUT_DIR}")/bin/sage_shell_combined.sage"

echo "[sage-shell] Compiling to bytecode..."
"${SAGE}" --emit-vm "${COMBINED}" -o "${BYTECODE}"

echo "[sage-shell] Generating binary SGVM and C header..."
"${SAGE}" scripts/compile_to_sgvm.sage "${BYTECODE}" "${BYTECODE}" --header "${OUT_H}" "sage_shell_bytecode"

echo "[sage-shell] Done."
