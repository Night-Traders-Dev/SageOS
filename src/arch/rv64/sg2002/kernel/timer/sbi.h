// Simple SBI interface for timer management

#ifndef SBI_H
#define SBI_H

// SBI Ecall IDs
#define SBI_SET_TIMER 0x0

// Helper to invoke SBI
static inline void sbi_call(unsigned long id, unsigned long arg0) {
    register unsigned long a0 asm("a0") = arg0;
    register unsigned long a7 asm("a7") = id;
    asm volatile ("ecall" : "+r" (a0) : "r" (a7) : "memory");
}

static inline void sbi_set_timer(unsigned long long stime_value) {
    sbi_call(SBI_SET_TIMER, (unsigned long)stime_value);
}

#endif
