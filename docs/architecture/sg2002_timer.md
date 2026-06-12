# Timer Management for SG2002

Timer interrupts are crucial for the SageOS scheduler and for maintaining system time.

## Implementation Details
- **Mechanism:** Leverages RISC-V SBI (Supervisor Binary Interface) `sbi_set_timer` calls for scheduling timer interrupts.
- **Interrupt Source:** External timer interrupts, which are handled via the PLIC (Platform-Level Interrupt Controller).
- **Functionality:** 
  - Periodic timer interrupts to drive scheduler ticks.
  - Integration with the `handle_trap` subsystem.

## Future Plans
- Develop high-resolution timer support.
- Refine the scheduler tick rate for balance between responsiveness and power consumption.
