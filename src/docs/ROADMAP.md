# SageOS Pure SageLang Development Roadmap

This document outlines the phased plan to implement a fully functional, booting operating system where all core logic, drivers, and utilities are written in **pure SageLang**.

## Architecture Principles
- **Minimal Native Surface**: The C-based `MetalVM` provides only the bare minimum primitives (MMIO access, trap entry).
- **Sage-Driven Hardware**: All hardware logic (PCI, VirtIO, MMU) is implemented as SageLang classes.
- **Strict Boot Stages**: Initialization follows a non-linear, strictly ordered 8-stage sequence.

---

## Phase 1: Memory Foundation (STAGE_1)
*Goal: Move beyond static pools to dynamic resource management.*

1.  **Physical Memory Manager (PMM)**
    *   **Logic**: Bitmap-based allocator tracking 4KB pages.
    *   **Input**: Parse memory regions from device tree/firmware map.
    *   **File**: `src/drivers/memory/pmm.sage`
2.  **Virtual Memory Manager (VMM)**
    *   **Logic**: Software page table walker.
    *   **Targets**: Sv39 (RISC-V) and 4-level (x86_64).
    *   **File**: `src/drivers/memory/vmm.sage`

## Phase 2: Trap & Interrupt Infrastructure (STAGE_2)
*Goal: Allow the kernel to react to asynchronous events.*

1.  **Generic Trap Handler**
    *   Unified entry point for exceptions and IRQs.
2.  **Interrupt Controller Drivers**
    *   **PLIC** (RISC-V Platform Level Interrupt Controller).
    *   **APIC/IOAPIC** (x86_64).
    *   **GIC** (ARM64).

## Phase 3: Virtual Hardware (STAGE_3)
*Goal: Interface with QEMU virtualized devices.*

1.  **VirtIO Transport Layer**
    *   Implementation of the VirtIO MMIO protocol.
    *   Virtqueue management (Descriptor rings).
2.  **VirtIO-Block Driver**
    *   Provides the primary storage interface.
3.  **VirtIO-Input/GPU** (Future)
    *   Keyboard support and basic framebuffer output.

## Phase 4: Storage & VFS (STAGE_4)
*Goal: Persistence and file-based system initialization.*

1.  **FAT32 Persistence**
    *   Link the FAT32 driver to the real VirtIO-Block device.
2.  **VFS Path Resolution**
    *   Support for mounting multiple devices and symlink resolution.
3.  **Rootfs Loading**
    *   Loading configuration from `/etc` inside the OS image.

## Phase 5: Multitasking & Runtime (STAGE_5 & STAGE_6)
*Goal: Enabling concurrent execution and IPC.*

1.  **Process/Thread Manager**
    *   Control blocks for tracking execution state.
2.  **Preemptive Scheduler**
    *   Round-robin or Priority-based context switching.
3.  **IPC Namespace**
    *   Message passing between SageVM instances.
4.  **Runtime Manager (PID 1)**
    *   Initial SageLang process that activates system services.

## Phase 6: Userspace & Shell (STAGE_7)
*Goal: User interaction and diagnostic utilities.*

1.  **ELF/Bytecode Loader**
    *   Loading `.sgvm` binaries from storage into new process contexts.
2.  **Ssh (Sage Shell)**
    *   Interactive command processor.
3.  **CoreUtils**
    *   `ls`, `cat`, `mkdir`, `neofetch`, `top`.

---

## Execution Workflow
For every sub-task:
1.  **Implement**: Write the logic in `src/`.
2.  **Compile**: Ensure `sgvmc` handles the new code.
3.  **Test**: Write a standalone test in `src/tests/` and run via `./sagemake test-suite`.
4.  **Verify**: Run in QEMU via `./sagemake build-run`.
5.  **Sync**: Commit and push to GitHub `main`.
