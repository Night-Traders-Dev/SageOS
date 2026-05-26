#!/bin/bash
# Script to run QEMU with a timeout to avoid hanging the agent
timeout 10s qemu-system-riscv64 -machine virt -m 1G -display none -serial stdio -bios none -kernel build/virt_riscv64/kernel.elf
if [ $? -eq 124 ]; then
    echo "QEMU timed out (likely hanging)."
else
    echo "QEMU finished or terminated."
fi
