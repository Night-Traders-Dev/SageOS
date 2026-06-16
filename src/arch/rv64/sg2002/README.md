# SG2002 (LicheeRV Nano) Support in SageOS

This directory contains the platform-specific implementation for the Sophgo SG2002 SoC, used in the Sipeed LicheeRV Nano board.

## Current Status (Milestone 1-6)
- [x] UART Driver (`uart.c`)
- [x] Trap Handler (`traps.c` + `trap_entry.S`)
- [x] Identity Paging (`paging.c` - Skeleton)
- [x] SBI-based Timer Support (`timer.c`)
- [x] Physical Allocator (`pmm.c`)
- [x] Early Heap Allocator (`heap.c`)
- [x] Cooperative Scheduler (`scheduler.c` + `switch.S`)
- [x] IPC Framework (`ipc.c` + `services.c`)
- [x] Storage/VFS (`sdhci.c` + `vfs_bridge.c` + `fat32.c` backend)
- [x] SDIO Controller Initialization (`sdio_controller.c`)
- [x] USB Serial Bridge (`serial_bridge.c` - Skeleton)
- [x] USB Device Enumeration (`usb_device.c` - Skeleton)
- [x] Console Abstraction (`console.c`)

### Memory Map (Early Boot)
- **Kernel Base:** `0x80000000`
- **UART0:** `0x04140000`

## Build Integration
The platform is integrated into the main SageOS build system via `build.toml`. Platform-specific files are located under `arch/rv64/sg2002/`.

## Milestones
- **Milestone 1:** Basic Bring-up (UART, Traps, Memory, Timers) - **COMPLETED**
- **Milestone 2:** Allocator Refinement, Scheduler, Interrupt Handling - **COMPLETED**
- **Milestone 3:** SGVM Interpreter Integration - **COMPLETED**
- **Milestone 4:** Storage & VFS - **COMPLETED**
- **Milestone 5:** Userspace & IPC - **COMPLETED**
- **Milestone 6:** SMP & Optimization - **COMPLETED**

## Bootloader & Firmware
The SG2002 boot chain is built using the provided `scripts/build_sg2002_bootloader.sh` script, which handles the acquisition and compilation of OpenSBI, U-Boot, and packaging via `fiptool`.

1. Run: `bash scripts/build_sg2002_bootloader.sh`
2. The final bootloader package (`fip.bin`) will be generated in `boot_binaries/`.
