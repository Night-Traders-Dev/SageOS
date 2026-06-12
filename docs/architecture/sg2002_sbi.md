# SBI Integration for SG2002

SageOS relies on the Supervisor Binary Interface (SBI) to abstract low-level hardware operations that require machine-mode (M-mode) access.

## Implementation Details
- **Usage:** Primarily utilized for:
  - Timer management (`sbi_set_timer`).
  - Hart management (shutdown, reboot).
  - Console fallback (if direct UART access is not available or desired).
- **Mode:** The SageOS kernel operates entirely in Supervisor Mode (S-Mode).
- **Interface:** Standard RISC-V SBI ECALL interface.

## Goals
- Maintain maximum portability across RISC-V platforms by using SBI where applicable.
- Avoid implementing complex M-mode functionality directly within the kernel where SBI provides a sufficient abstraction.
