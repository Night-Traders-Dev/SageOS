# UART Driver for SG2002

The SG2002 (LicheeRV Nano) uses an `ns16550`-compatible UART controller for early system console and debugging output.

## Implementation Details
- **Base Address:** `0x04140000` (UART0).
- **Register Interface:** Memory-mapped registers accessed via byte-sized reads/writes.
- **Initialization:** Assumed to be initialized by the vendor FSBL/SBI. The driver provides basic output functionality.
- **Register Map:** 
  - `REG_DATA` (Offset 0x0)
  - `REG_LINE_STATUS` (Offset 0x5)
- **Status Bits:** 
  - `LSR_TX_HOLDING_EMPTY` (Bit 5, value 32) indicates the UART transmit buffer is ready for data.

## Usage
- The `sg2002_uart_init()` function initializes the hardware state.
- The `sg2002_uart_putc(char c)` function performs a busy-wait until the UART is ready and then transmits the character.
