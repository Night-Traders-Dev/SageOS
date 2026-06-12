# Identity Paging for SG2002

The SG2002 (LicheeRV Nano) uses RISC-V SV39 paging. During early boot, we must establish a simple identity mapping (virtual address == physical address) to safely enable the MMU before transitioning to a more complex kernel memory layout.

## Implementation
- Implemented in `arch/rv64/sg2002/kernel/mm/paging.c`.
- Sets up Level 2 (Page Directory) and Level 1 (Page Table) entries to cover the initial boot memory region.
- Enables the MMU by setting the `satp` register.

## Assumptions
- Kernel base address is 0x80000000.
- Memory size is sufficient for initial kernel heap.
