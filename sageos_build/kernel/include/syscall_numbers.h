#ifndef SAGEOS_SYSCALL_NUMBERS_H
#define SAGEOS_SYSCALL_NUMBERS_H

/* 
 * SageOS Syscall Numbers
 * Based on Linux x86_64/AArch64/RISC-V 64 conventions where possible.
 */

#define SYS_read        0
#define SYS_write       1
#define SYS_open        2
#define SYS_close       3
#define SYS_fstat       5
#define SYS_lseek       8
#define SYS_brk        12
#define SYS_exit       60
#define SYS_getpid     39
#define SYS_isatty    100   /* SageOS custom */
#define SYS_kill        62
#define SYS_times     101   /* Adjusted to avoid clash with isatty if needed, but following plan */

#endif
