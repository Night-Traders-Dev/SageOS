#include "metal_vm.h"

// SBI Console Putchar
static void sbi_putchar(char c) {
    register unsigned long a0 __asm__("a0") = (unsigned long)c;
    register unsigned long a7 __asm__("a7") = 0x01;
    __asm__ volatile ("ecall" : "+r"(a0) : "r"(a7) : "memory");
}

// libc stubs for metal_vm.c
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

// Global VM to avoid huge stack usage
MetalVM g_vm;

void kmain(void) {
    // Initial hardware heartbeat via SBI
    sbi_putchar('H');
    sbi_putchar('E');
    sbi_putchar('L');
    sbi_putchar('L');
    sbi_putchar('O');
    sbi_putchar('\n');

    metal_vm_init(&g_vm);
    g_vm.write_char = vm_write_char;

    unsigned char* start = _binary_build_kernel_sgvm_start;
    unsigned char* end = _binary_build_kernel_sgvm_end;
    int size = (int)(end - start);

    if (size > 0 && metal_vm_load_binary(&g_vm, start, size) == 0) {
        sbi_putchar('V'); sbi_putchar('M'); sbi_putchar('\n');
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
