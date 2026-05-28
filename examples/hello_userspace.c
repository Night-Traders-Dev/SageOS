/* Simple Hello World using raw syscalls for SageOS */
#include <stdint.h>

/* syscall numbers must match our syscall_numbers.h */
#define SYS_write 1
#define SYS_exit  60

long syscall3(long num, long a1, long a2, long a3) {
    long ret;
    __asm__ volatile (
        "mov x8, %1\n"
        "mov x0, %2\n"
        "mov x1, %3\n"
        "mov x2, %4\n"
        "svc #0\n"
        "mov %0, x0"
        : "=r"(ret)
        : "r"(num), "r"(a1), "r"(a2), "r"(a3)
        : "x0", "x1", "x2", "x8", "memory"
    );
    return ret;
}

void _start() {
    const char *msg = "Hello from SageOS Userspace (AArch64)!\n";
    syscall3(SYS_write, 1, (long)msg, 39);
    syscall3(SYS_exit, 0, 0, 0);
}
