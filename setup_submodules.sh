#!/usr/bin/env bash

# SageOS Optimized Submodule Startup Script
#
# This script performs a parallel, non-recursive initialization of all 
# submodules. it ensures that forked libraries (lwip, mbedtls) are used
# and prevents infinite recursion by reusing the root repository as the
# 'core' for architecture submodules.
#
# Usage: ./setup_submodules.sh

set -e

# Configuration
JOBS=$(nproc)
ROOT_DIR=$(pwd)
ARCHS=("arch/arm64" "arch/rv64" "arch/x64")

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

log() { echo -e "${GREEN}[SageOS]${NC} $1"; }
step() { echo -e "${BLUE}>>${NC} $1"; }

echo "===================================================="
echo "   SageOS Optimized Submodule Initializer           "
echo "===================================================="

# 1. Root Submodules
step "Initializing root submodules (lwip, mbedtls, archs)..."
# We use --remote on the first pass to ensure we bypass any broken pinned commits
# and correctly target the branches of our forked repositories.
git submodule update --init --jobs "$JOBS" --remote

# 2. Local Reference Configuration
step "Configuring architecture submodules to eliminate redundancy..."
for arch in "${ARCHS[@]}"; do
    if [ -d "$arch" ]; then
        log "Setting local reference for $arch/core"
        # Initialize the submodule entry in .git/config
        git -C "$arch" submodule init core >/dev/null 2>&1 || true
        # Point the 'core' submodule to the local root directory to avoid re-downloading
        git -C "$arch" config submodule.core.url "$ROOT_DIR"
    fi
done

# 3. Architecture Initialization
step "Initializing architecture components in parallel..."
for arch in "${ARCHS[@]}"; do
    if [ -d "$arch" ]; then
        (
            log "Updating $arch..."
            # Check out 'core' from the local root
            git -C "$arch" submodule update --init --jobs "$JOBS" core
            
            # Disable redundant nested submodules within the arch core
            # This ensures we don't have multiple copies of lwip/mbedtls on disk
            if [ -d "$arch/core" ]; then
                git -C "$arch/core" config submodule.sageos_build/kernel/third_party/lwip.update none
                git -C "$arch/core" config submodule.sageos_build/kernel/third_party/mbedtls.update none
            fi
        ) &
    fi
done
wait

echo -e "\n===================================================="
echo "   Setup Complete!                                  "
echo "   Redundancy eliminated. Infinite recursion broken. "
echo "===================================================="
