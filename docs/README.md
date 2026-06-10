# SageOS Documentation Index

## Architectural Specifications
- [**Platform Specification**](architecture/platform_spec.md): Canonical contract defining boot stages, runtime ownership, and ABI guarantees.
- [**Core Systems Architecture**](core_systems_architecture.md): The high-level philosophy and architectural overview of SageOS.
- [**IPC Subsystem**](architecture/ipc.md): Formal specification of the communication backbone and capability manager.
- [**Security Model**](architecture/security.md): Overview of the Capability-First authority gating and system permissions.
- [**Execution Model**](architecture/execution_model.md): Hybrid execution architecture (Native vs SGVM) and asynchronous runtime.
- [**Memory Model**](architecture/memory_model.md): Layered physical, virtual, and managed memory architecture.
- [**Driver Model**](architecture/driver_model.md): Modular and externalizable driver architecture.
- [**SGVM Specification**](architecture/sgvm_spec.md): The portable execution substrate and MetalVM internals.
- [**Internal API Contracts**](architecture/internal_apis.md): Documentation of the stable interfaces between kernel subsystems.
- [**Gap Analysis**](architecture/gap_analysis.md): Assessment of deviations between design specifications and implementation status.
- [**Telemetry & Observability**](architecture/telemetry.md): Deep dive into the system-wide tracing and event logging infrastructure.

## Architecture Guides
- [**x86_64 (x64)**](arch/x64.md) (Active Multitasking)
- [**ARM64 (AArch64)**](arch/arm64.md) (Active Multitasking)
- [**RISC-V 64 (RV64)**](arch/rv64.md) (Active Multitasking)

## Developer Guides
- [**Build Pipeline**](guides/build_pipeline.md): Comprehensive guide to the SageOS build system and cross-compilation.
- [**Management Script**](guides/management_script.md): Usage details for the `sageos.sh` master control script.
- [**Native Toolchain**](guides/toolchain.md): Details on the integrated GCC/Binutils environment for on-device development.
- [**SageContainer Guide**](guides/sage_container.md): Guide to using the native Linux containerization for SageOS development.

## Hardware & Devices
- [**Raspberry Pi 4**](devices/rpi4.md)
- [**Lenovo 300e (Gemini Lake)**](devices/lenovo_300e.md)
- [**LicheeRV Nano**](devices/licheeRV_nano.md)
- [**OrangePi RV2**](devices/orangepi_rv2.md)
- [**Virtual Targets (QEMU)**](devices/virt_arm64.md)

## Narrative Documentation
- [**The SageOS Book**](SageOS_Book.md): A comprehensive, narrative guide to the modular hybrid operating system.
