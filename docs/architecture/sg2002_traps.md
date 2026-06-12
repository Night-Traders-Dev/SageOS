# Trap Handler for SG2002

The trap handler provides the fundamental mechanism for handling synchronous exceptions and asynchronous interrupts on the SG2002.

## Implementation Details
- **Architecture:** RISC-V Supervisor Mode (S-Mode).
- **Entry Point:** The `handle_trap` function, invoked via assembly-level glue code (to be implemented) that saves the context (registers).
- **Functionality:** 
  - Extracts the trap cause from the `scause` register.
  - Captures the trap program counter from the `sepc` register.
  - Implements basic debug output via the UART console for unexpected traps.

## Future Plans
- Extend `handle_trap` to dispatch to specialized handlers (e.g., page fault handler, syscall handler, timer interrupt handler).
- Improve context saving/restoring to ensure state integrity across traps.
