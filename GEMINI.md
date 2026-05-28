# SageOS Development Guidelines

- **Sage-First Principle**: Wherever possible, new code, drivers, and system services MUST be implemented in `SageLang` (`.sage` files). C should be reserved strictly for low-level kernel primitives, memory management shims, and performance-critical hardware drivers that cannot yet be expressed in `SageLang`.
- **Architectural Integrity**: All new drivers and system components should strive to use the `virt` kernel architecture. If a feature requires platform-specific functionality, use the defined `MetalVM` native interfaces rather than implementing new, complex C-based subsystems.
- **Security & Isolation**: SageOS currently operates in a single privileged address space. While this simplifies development, it means that bugs in kernel components or native bridges can compromise the entire system. Future efforts will focus on implementing address space isolation via MMPU/Paging for SageLang scripts.
- **Documentation**: All new `SageLang` modules must be documented within the source.
- **Code Style**: Prefer idiomatic `SageLang` patterns over complex C boilerplate.
