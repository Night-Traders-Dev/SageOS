# SageOS v0.8.0 Release Notes

## Overview
SageOS v0.8.0 marks a significant milestone in the stabilization and formalization of the hybrid execution model. This release focuses on robust service orchestration, transparent bytecode support, and deep observability of the system supervisor.

## Key Changes

### 1. Capability-First Service Supervision
The `runtime_manager.sage` (PID 1) is now fully integrated with the kernel's capability model and asynchronous scheduler.
- **Dynamic Service Spawning**: PID 1 now utilizes `os_spawn_task` to launch system services into isolated, scheduler-managed kernel threads.
- **Enhanced Health Monitoring**: The newly implemented `os_process_exists` native allows the supervisor to accurately monitor service lifecycles and trigger self-healing restarts.
- **Dependency Resolution**: Refactored the service registry to support hierarchical dependencies, ensuring critical components like `vfs.root` and `dev.manager` are active before userspace activation.

### 2. Transparent Execution Bridge
The kernel bridge now supports polyglot file execution across the virtual filesystem.
- **Format Auto-Detection**: `sage_execute_file` automatically detects SGVM bytecode via the `SGVM` magic header, transparently switching between the AST interpreter for source (`.sage`) and `MetalVM` for precompiled bytecode (`.bc`/`.sgvm`).
- **Unified Native API**: Standardized native function registration across both AST and Bytecode execution modes, ensuring consistency for system services.

### 3. Observable Bootstrap
- **Synchronous Telemetry**: Supervisor logs (`log.info/warn/error`) are now mirrored to the serial console during the Stage 6 service activation phase.
- **Quiet Mode by Default**: Disabled verbose allocator and GIL acquisition tracing in production builds to maintain a clean and actionable system log.

### 4. Corrected Disk Layout Alignment
- System services have been moved to their canonical locations in the rootfs:
    - **Bytecode services**: `/lib/*.bc`
    - **Source services**: `/etc/sagelang/*.sage`

## ABI Compatibility
- **SAGE_ABI_MAJOR**: 0
- **SAGE_ABI_MINOR**: 4

*Note: This version requires a clean build and disk image regeneration to align with the new service paths.*
