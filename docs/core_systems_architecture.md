# SageOS Core Systems Architecture Specification

Revision 0.8.0 (Formalized)

Project: "github.com/Night-Traders-Dev/SageOS"

---

## 1. System Philosophy

SageOS is a runtime-centric operating environment built around the SGVM execution substrate. 

The operating system is not designed as a traditional POSIX-first UNIX clone. Instead, SageOS treats the kernel, SGVM, SageLang, system services, and userspace as components of a unified execution architecture.

### Architectural Pillars
- **Kernel**: Hardware abstraction, scheduling, memory management, security enforcement, IPC primitives, and runtime orchestration.
- **SGVM (MetalVM)**: Portable execution, application ABI stability, runtime isolation, and service interoperability.
- **Sage-First**: Complex system logic, driver management, and user interaction are prioritized in SageLang.

The system is designed for portability, self-hosting, runtime introspection, and deterministic service architecture.

---

## 2. Core Architectural Specifications

Detailed specifications for core subsystems can be found in the following documents:

### Foundation
- [**Platform Specification**](architecture/platform_spec.md): Canonical contract defining boot stages, runtime ownership, and ABI guarantees.
- [**Boot Model & Lifecycle**](architecture/execution_model.md): Detailed 8-stage granular bootstrap sequence.
- [**Security Model**](architecture/security.md): Overview of the Capability-First authority gating and system permissions.

### Execution & Runtime
- [**Execution Model**](architecture/execution_model.md): Hybrid execution architecture (Native vs SGVM) and asynchronous runtime.
- [**MetalVM / SGVM Specification**](architecture/sgvm_spec.md): The portable execution substrate and MetalVM internals.
- [**Runtime Contract**](architecture/runtime_contract.md): The formal relationship between the kernel and the language runtime.

### Resource Management
- [**Memory Model**](architecture/memory_model.md): Layered physical, virtual, and managed memory architecture.
- [**IPC Subsystem**](architecture/ipc.md): Formal specification of the communication backbone and capability manager.
- [**Virtual Filesystem (VFS)**](architecture/internal_apis.md): Hybrid VFS implementation and capability-based access.

### System Services
- [**Driver Model**](architecture/driver_model.md): Modular and externalizable driver architecture.
- [**Scheduler**](architecture/scheduler.md): Preemptive, SMP-aware, and priority-based task management.
- [**Syscall ABI**](architecture/syscall_abi.md): Canonical interface for system services.

### Diagnostics & Evolution
- [**Telemetry & Observability**](architecture/telemetry.md): System-wide tracing and event logging infrastructure.
- [**Internal API Contracts**](architecture/internal_apis.md): Documentation of the stable interfaces between kernel subsystems.
- [**Gap Analysis**](architecture/gap_analysis.md): Assessment of deviations between design specifications and implementation status.

---

## 3. Long-Term Direction

SageOS is designed to evolve toward:
- **Self-hosting**: Developing the OS from within itself.
- **Runtime-Native Services**: Moving more system logic into the managed SageLang environment.
- **Distributed Execution**: Network-transparent services and runtime migration.
- **Capability-Secured Computing**: Ensuring that all resources are governed by unforgeable tokens.

The project prioritizes architectural coherence, runtime integration, and execution portability over strict UNIX compatibility.
