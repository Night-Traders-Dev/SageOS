# Scheduler for SG2002

The scheduler is responsible for managing task execution on the SG2002, ensuring cooperative multitasking in the early stages and preemption later.

## Implementation Details
- **Architecture:** Single-core scheduler (initially).
- **Strategy:** Cooperative threading based on explicit yield points, transitioning to preemption via periodic timer interrupts (Milestone 2/3).
- **Task Management:** Maintains a queue of ready tasks and manages task state (Ready, Running, Blocked).
- **Trap Integration:** Triggered by timer interrupts for potential preemption and by syscalls/yield calls.

## Future Plans
- Implement SMP support.
- Enhance task prioritization.
- Refine preemption logic for responsiveness.
