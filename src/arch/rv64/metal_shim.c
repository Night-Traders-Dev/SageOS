#include <stdint.h>
#include "metal_vm.h"

// Global VM to avoid huge stack usage
MetalVM g_vm;

// Trap flag variables for SageLang dispatcher
volatile unsigned long g_trap_cause = 0;
volatile unsigned long g_trap_epc = 0;
volatile int g_trap_pending = 0;

// SBI Console Putchar
static void sbi_putchar(char c) {
    register unsigned long a0 __asm__("a0") = (unsigned long)c;
    register unsigned long a7 __asm__("a7") = 0x01;
    __asm__ volatile ("ecall" : "+r"(a0) : "r"(a7) : "memory");
}

// RISC-V CSR and SBI helpers
void enable_interrupts() {
    unsigned long sstatus;
    __asm__ volatile ("csrrs %0, sstatus, %1" : "=r"(sstatus) : "r"(0x2)); // Set SIE
    unsigned long sie;
    __asm__ volatile ("csrrs %0, sie, %1" : "=r"(sie) : "r"(0x20)); // Set STIE
}

void set_timer(unsigned long interval) {
    register unsigned long a0 __asm__("a0") = 0; // Relative time (SBI)
    // SBI call to set timer
    __asm__ volatile ("li a7, 0x54494D45; li a6, 0; ecall" : : "r"(a0) : "memory");
}

void handle_trap(unsigned long cause, unsigned long epc, unsigned long sp) {
    // Basic bridge to SageLang dispatcher
    g_trap_cause = cause;
    g_trap_epc = epc;
    g_trap_pending = 1;
    
    // Resume VM to let the kernel handle the trap
    g_vm.halted = 0;
}

// libc stubs
void* memset(void* s, int c, unsigned long n) {
    unsigned char* p = (unsigned char*)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void* memcpy(void* dest, const void* src, unsigned long n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

unsigned long strlen(const char* s) {
    unsigned long n = 0;
    while (s[n]) n++;
    return n;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

static void vm_write_char(char c) {
    sbi_putchar(c);
}

extern unsigned char _binary_build_kernel_sgvm_start[];
extern unsigned char _binary_build_kernel_sgvm_end[];

static void print_string(const char* s) {
    while (*s) sbi_putchar(*s++);
}

void kmain(unsigned long handoff_ptr) {
    // Initial hardware heartbeat via SBI
    print_string("kmain: SageOS Kernel booting...\n");
    if (handoff_ptr) {
        uint64_t* magic_ptr = (uint64_t*)handoff_ptr;
        if (*magic_ptr == 0x534147454F534249ULL) {
            print_string("kmain: Booted via SageBoot!\n");
            uint64_t cmdline_ptr = *(uint64_t*)(handoff_ptr + 52);
            if (cmdline_ptr) {
                print_string("kmain: Boot cmdline: \"");
                print_string((const char*)cmdline_ptr);
                print_string("\"\n");
            }
        } else {
            print_string("kmain: Warning - invalid SageBoot handoff magic.\n");
        }
    } else {
        print_string("kmain: Warning - booted directly (no SageBoot metadata).\n");
    }

    metal_vm_init(&g_vm);
    g_vm.write_char = vm_write_char;

    unsigned char* start = _binary_build_kernel_sgvm_start;
    unsigned char* end = _binary_build_kernel_sgvm_end;
    int size = (int)(end - start);

    if (size > 0 && metal_vm_load_binary(&g_vm, start, size) == 0) {

        sbi_putchar('V');
        sbi_putchar('M');
        sbi_putchar('\n');

        metal_vm_verify(&g_vm);

        sbi_putchar('R');
        sbi_putchar('U');
        sbi_putchar('N');
        sbi_putchar('\n');
        if (metal_vm_verify(&g_vm) == 0) {
            sbi_putchar('O'); sbi_putchar('K'); sbi_putchar('\n');
            for (int i = 0; i < g_vm.chunk_count; i++) {
                metal_vm_load(&g_vm, g_vm.chunks[i], g_vm.chunk_lengths[i]);
                metal_vm_run(&g_vm);
            }
        } else {
            sbi_putchar('E'); sbi_putchar('R'); sbi_putchar('R'); sbi_putchar('\n');
        }
    } else {
         sbi_putchar('N'); sbi_putchar('O'); sbi_putchar('B'); sbi_putchar('\n');
    }

    while (1) {
        __asm__ volatile ("wfi");
    }
}
