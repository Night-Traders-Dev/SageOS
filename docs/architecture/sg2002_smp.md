# SMP (Symmetric Multiprocessing) for SG2002

The SG2002 (LicheeRV Nano) is a dual-core SoC. SMP support is essential for scaling performance in SageOS.

## Implementation Details
- **Architecture:** RISC-V multi-core (Harts).
- **Strategy:** SageOS will utilize the RISC-V SBI Hart State Management (HSM) extension for booting secondary harts.
- **IPI Handling:** Inter-Processor Interrupts will be managed via the PLIC (Platform-Level Interrupt Controller) using SBI `sbi_send_ipi` calls.
- **Synchronization:** Mutexes and spinlocks will be implemented for shared resource access.

## Planned Milestones
1.  **Discovery:** Parse DTB to identify available harts.
2.  **Secondary Hart Boot:** Use `sbi_hart_start` to boot secondary harts.
3.  **IPI Framework:** Implement inter-hart communication.
4.  **Scheduler SMP:** Extend the scheduler to handle multi-hart task distribution.

## Assumptions
- The bootloader/SBI implementation provides full HSM extension support.
- Cache coherency is managed by the hardware platform and/or SBI.
