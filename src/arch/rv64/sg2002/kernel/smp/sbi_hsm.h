#ifndef SBI_HSM_H
#define SBI_HSM_H

// SBI HSM extension IDs
#define SBI_HSM_EXT_ID 0x48534D
#define SBI_HSM_HART_START 0x0

// Helper to invoke SBI HSM
static inline void sbi_hsm_hart_start(unsigned long hartid, unsigned long start_addr, unsigned long opaque) {
    register unsigned long a0 asm("a0") = hartid;
    register unsigned long a1 asm("a1") = start_addr;
    register unsigned long a2 asm("a2") = opaque;
    register unsigned long a7 asm("a7") = SBI_HSM_EXT_ID;
    register unsigned long a6 asm("a6") = SBI_HSM_HART_START;
    asm volatile ("ecall" : "+r" (a0) : "r" (a1), "r" (a2), "r" (a7), "r" (a6) : "memory");
}

#endif
