# IPC (Inter-Process Communication) for SG2002

This document details the message-queue-based IPC mechanism implemented for the SG2002 (LicheeRV Nano) to facilitate communication between kernel services and userspace applications.

## Implementation Details
- **Architecture:** Message-queue-based, using a ring buffer structure.
- **Data Structure:** `ipc_msg_t` containing `sender_pid`, `len`, and a data buffer (`MAX_MSG_SIZE` = 256 bytes).
- **Operations:**
  - `ipc_send`: Places a message into the ring buffer for a destination PID.
  - `ipc_receive`: Retrieves the next available message from the ring buffer.

## Assumptions
- Currently supports basic communication within the kernel-only context.
- Will be extended to support cross-process communication between SGVM instances as userspace is brought up.

## Future Plans
- Implement capability-based access control for IPC endpoints.
- Integrate with the scheduler to handle blocking/unblocking of tasks waiting for IPC messages.
- Scale to support larger data transfers and asynchronous notifications.
