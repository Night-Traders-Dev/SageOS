# Porting SageOS to the LicheeRV Nano W

## SG2002 RISC-V Bring-Up and Runtime Integration Guide

### Target Platform

* Board: LicheeRV Nano W
* SoC: Sophgo SG2002
* Architecture: RISC-V 64
* RAM: 256MB DDR3
* Boot Medium: microSD
* Firmware Stack:

  * Vendor FSBL
  * OpenSBI
  * U-Boot
  * SageOS Kernel

Primary references:

* [Mainline U-Boot LicheeRV Nano Docs](https://docs.u-boot.org/en/stable/board/sophgo/licheerv_nano.html?utm_source=chatgpt.com)
* [Sipeed LicheeRV Nano Wiki](https://wiki.sipeed.com/licheerv-nano?utm_source=chatgpt.com)
* [Mainline Linux SG2002 Bring-Up Patches](https://lkml.iu.edu/2406.1/04241.html?utm_source=chatgpt.com)

---

# 1. Objectives

The first SageOS milestone on SG2002 is:

```text
Boot ROM
    ↓
Vendor FSBL
    ↓
OpenSBI
    ↓
U-Boot
    ↓
SageOS Kernel
    ↓
UART Console
    ↓
Memory Manager
    ↓
Scheduler
    ↓
SGVM Runtime
    ↓
Interactive Shell
```

The initial target is a stable runtime platform.

The following are intentionally deferred:

* graphics
* camera
* multimedia acceleration
* NPU support
* WiFi firmware integration
* advanced power management

---

# 2. Understanding the Boot Chain

The SG2002 does not boot like a traditional PC.

The actual boot sequence is:

```text
BROM
    ↓
FSBL
    ↓
OpenSBI
    ↓
U-Boot
    ↓
Kernel
```

The ROM loads `fip.bin` from the FAT partition of the SD card.

The `fip.bin` package contains:

* FSBL
* OpenSBI
* U-Boot

The FSBL initializes:

* DRAM
* clocks
* early SoC state

This means SageOS cannot currently replace the earliest boot stages.

That is acceptable.

---

# 3. Development Environment Setup

## Required Toolchains

Install:

* riscv64-unknown-elf
* riscv64-linux-gnu
* OpenSBI toolchain dependencies
* U-Boot build dependencies

Recommended host:

* Ubuntu 24.04+
* Arch Linux
* Fedora

---

## Required Repositories

Clone:

* OpenSBI
* U-Boot
* Sophgo fiptool
* SageOS

References:

* [OpenSBI](https://github.com/riscv-software-src/opensbi?utm_source=chatgpt.com)
* [U-Boot](https://github.com/u-boot/u-boot?utm_source=chatgpt.com)
* [Sophgo fiptool](https://github.com/sophgo/fiptool?utm_source=chatgpt.com)
* [SageOS](https://github.com/Night-Traders-Dev/SageOS?utm_source=chatgpt.com)

---

# 4. Establishing a Known-Good Boot Baseline

Before booting SageOS:

* boot mainline U-Boot successfully,
* verify serial console access,
* verify SD card boot reliability.

This eliminates hardware ambiguity early.

Expected output:

```text
U-Boot 2024.xx
DRAM: 256 MiB
MMC: mmc@4310000
In: serial@4140000
```

---

# 5. Creating the SageOS Platform Layer

Create:

```text
arch/riscv64/sg2002/
```

Subsystems:

* boot
* uart
* interrupt
* timer
* memory
* dtb
* platform init

---

# 6. Stage 1 Bring-Up

## Minimal Kernel Requirements

The first boot target should implement ONLY:

### Required

* UART
* identity paging
* trap handler
* timer interrupt
* physical allocator
* early heap
* SBI console support

### Not Required

* SMP
* VFS
* SGVM
* ELF loader
* userspace

---

## UART Bring-Up

The SG2002 uses ns16550-compatible UART.

This is the single most important subsystem first.

Expected first output:

```text
[SAGEOS] Early boot start
[SAGEOS] MMU initialized
[SAGEOS] Timer initialized
```

If UART fails:

* nothing else matters yet.

---

# 7. OpenSBI Integration

SageOS should initially rely heavily on SBI calls.

Use SBI for:

* timer
* hart management
* shutdown
* console fallback

Avoid direct machine-mode complexity initially.

Kernel should run in:

* supervisor mode

NOT:

* machine mode

---

# 8. Device Tree Support

The SG2002 ecosystem is DTB-centric.

SageOS should:

* parse flattened device trees early,
* extract memory regions,
* locate UART,
* locate interrupt controller,
* locate timers.

Critical nodes:

* `/memory`
* `/cpus`
* `/soc/serial`
* `/soc/interrupt-controller`

Reference Linux DTS patches extensively.

---

# 9. Memory Management Bring-Up

## Initial Memory Layout

Suggested layout:

```text
0x80000000  Kernel Base
0x80200000  Kernel Heap
0x81000000  SGVM Runtime
0x88000000  Userspace
```

Actual addresses should derive from DTB memory regions.

---

## Required Features

Initial memory subsystem:

* SV39 paging
* kernel higher-half mapping
* direct physical mapping
* page allocator
* kernel heap allocator

Avoid:

* swap
* huge pages
* NUMA
* memory compression

during early bring-up.

---

# 10. Interrupt and Timer Initialization

The SG2002 exposes:

* CLINT
* PLIC

through standard RISC-V infrastructure.

Required:

* timer interrupt
* external interrupt dispatch
* trap frame handling

At this stage:

* periodic scheduler ticks become possible.

---

# 11. Scheduler Bring-Up

Start simple.

Recommended:

* single-core scheduler first
* cooperative threading initially
* preemption later

Boot target:

```text
kernel idle task
shell task
SGVM runtime task
```

SMP can wait.

---

# 12. SGVM Runtime Bring-Up

This is where SageOS becomes distinct.

Do NOT delay SGVM until “later.”

Bring it up immediately after:

* memory
* scheduler
* IPC primitives

---

## Initial SGVM Runtime

Minimal runtime:

* bytecode loader
* object allocator
* async task scheduler
* runtime heap
* syscall bridge

No JIT initially.

Interpreter only.

---

## First SGVM Program

Target:

```text
hello.sgvm
```

Output:

```text
Hello from SGVM on SG2002
```

Once this works:

* SageOS is genuinely portable.

---

# 13. Storage and Filesystem

After runtime stability:

Implement:

* SDHCI driver
* FAT32 reader
* VFS layer
* init package loader

The SG2002 SD controller already has Linux support upstream.

Use Linux DT patches as reference.

---

# 14. Userspace Bring-Up

First userspace target:

```text
init
    ↓
runtime manager
    ↓
shell
```

NOT:

* POSIX environment
* full libc
* GNU tooling

Focus on:

* runtime-native services
* async execution
* SGVM-first tooling

---

# 15. IPC Integration

Initial IPC should be:

* message queues
* capability handles
* async event dispatch

Avoid:

* UNIX signal semantics
* global shared state
* complex socket stacks

initially.

---

# 16. Debugging Strategy

Required hardware:

* USB serial adapter
* logic analyzer (recommended)
* secondary Linux machine

Critical tools:

* OpenOCD
* GDB
* QEMU RISC-V (for partial testing)

---

## Logging Strategy

Implement:

* early serial logging
* panic dumps
* trap dumps
* page fault diagnostics

early.

This will save enormous amounts of time.

---

# 17. Recommended Milestones

## Milestone 1

* UART output
* SBI timer
* paging enabled

## Milestone 2

* allocator
* heap
* interrupts
* scheduler

## Milestone 3

* SGVM interpreter
* runtime task
* shell

## Milestone 4

* SD card driver
* VFS
* package loader

## Milestone 5

* userspace services
* IPC
* runtime-native applications

## Milestone 6

* SMP
* async runtime optimization
* JIT experimentation

---

# 18. Deferred Features

These should NOT block bring-up:

| Feature        | Reason                 |
| -------------- | ---------------------- |
| GPU/display    | Vendor complexity      |
| NPU            | Proprietary interfaces |
| Camera         | MMF dependency         |
| WiFi           | Firmware-heavy         |
| Multimedia     | Linux-centric stack    |
| Audio pipeline | Lower priority         |

---

# 19. Long-Term Vision

The LicheeRV Nano is not merely a “supported board.”

It should become:

* the primary SGVM embedded development target,
* a runtime experimentation platform,
* and a distributed systems research node.

The SG2002 architecture aligns unusually well with:

* SageOS’s runtime-centric philosophy,
* asynchronous execution model,
* and service-oriented architecture.

---

# 20. Final Recommendation

Do NOT attempt to:

* fully replace Linux immediately,
* replicate desktop UNIX semantics,
* or support every peripheral early.

Instead:

Build a stable runtime platform first.

The correct success metric is not:

```text
Can it run Linux software?
```

The correct success metric is:

```text
Can SGVM become the native execution environment?
```

Once that exists:

* the rest of the operating system becomes dramatically easier to evolve.f
