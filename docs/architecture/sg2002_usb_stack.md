# USB Stack Implementation Plan for SG2002

This document outlines the roadmap to implement a native USB stack on the SG2002 platform, primarily to support CDC-ACM serial for console access.

## Implementation Roadmap
1.  **USB OTG Controller Driver:**
    - Develop driver for the SG2002 OTG controller (register-level access).
    - Implement support for device-mode operation.
2.  **USB Core Stack:**
    - Develop USB protocol layer (endpoints, pipes, setup requests).
    - Implement device enumeration handling.
3.  **CDC-ACM Class Driver:**
    - Implement USB class-specific requests for serial communication.
    - Bridge this driver with the `console.c` abstraction.
4.  **Integration:**
    - Configure console backend to use `CONSOLE_USB`.

## Requirements
- Access to the SG2002 Technical Reference Manual (TRM) for OTG controller register definitions.
- Implementation of high-priority interrupt handling for USB events.
