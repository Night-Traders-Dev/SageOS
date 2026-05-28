/* Simple Hello World using raw syscalls for SageOS */
#include <stdint.h>

/* syscall numbers must match our syscall_numbers.h */
#define SYS_write      1
#define SYS_exit       60
#define SYS_getpid     39
#define SYS_nanosleep  35

long syscall1(long num, long a1) {
    long ret;
    __asm__ volatile (
        "mov x8, %1\n"
        "mov x0, %2\n"
        "svc #0\n"
        "mov %0, x0"
        : "=r"(ret)
        : "r"(num), "r"(a1)
        : "x0", "x8", "memory"
    );
    return ret;
}

long syscall2(long num, long a1, long a2) {
    long ret;
    __asm__ volatile (
        "mov x8, %1\n"
        "mov x0, %2\n"
        "mov x1, %3\n"
        "svc #0\n"
        "mov %0, x0"
        : "=r"(ret)
        : "r"(num), "r"(a1), "r"(a2)
        : "x0", "x1", "x8", "memory"
    );
    return ret;
}

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

    long pid = syscall1(SYS_getpid, 0);
    if (pid == 1) {
        syscall3(SYS_write, 1, (long)"My PID is: 1\n", 13);
    }

    struct { long tv_sec; long tv_nsec; } req = { 1, 0 };
    syscall2(SYS_nanosleep, (long)&req, 0);

    syscall3(SYS_write, 1, (long)"Sleep done. Exiting.\n", 21);

    syscall3(SYS_exit, 0, 0, 0);
}
