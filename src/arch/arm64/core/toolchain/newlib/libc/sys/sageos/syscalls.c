/* newlib/libc/sys/sageos/syscalls.c */

#include <sys/stat.h>
#include <sys/times.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <reent.h>

/* SageOS Syscall Numbers (must match kernel) */
#define SYS_read        0
#define SYS_write       1
#define SYS_open        2
#define SYS_close       3
#define SYS_fstat       5
#define SYS_lseek       8
#define SYS_brk        12
#define SYS_exit       60
#define SYS_getpid     39
#define SYS_isatty    100
#define SYS_kill        62
/* Add missing syscall definitions to newlib */
#define SYS_vfork      58
#define SYS_execve     59
#define SYS_waitpid    61
#define SYS_getcwd     79
#define SYS_chdir      80
#define SYS_mkdir      83
#define SYS_dup2       33
#define SYS_gettimeofday 96
#define SYS_nanosleep  35
#define SYS_unlink     87
#define SYS_getdents64 217

/* Implementation of new syscalls */
pid_t _vfork(void) {
    long ret = __syscall(SYS_vfork, 0, 0, 0, 0, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return (pid_t)ret;
}

int _execve(const char *path, char *const argv[], char *const envp[]) {
    long ret = __syscall(SYS_execve, (long)path, (long)argv, (long)envp, 0, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

pid_t _waitpid(pid_t pid, int *status, int options) {
    long ret = __syscall(SYS_waitpid, (long)pid, (long)status, (long)options, 0, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return (pid_t)ret;
}

char *_getcwd(char *buf, size_t size) {
    long ret = __syscall(SYS_getcwd, (long)buf, (long)size, 0, 0, 0);
    if (ret < 0) { errno = -ret; return NULL; }
    return buf;
}

int _chdir(const char *path) {
    long ret = __syscall(SYS_chdir, (long)path, 0, 0, 0, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

int _mkdir(const char *path, mode_t mode) {
    long ret = __syscall(SYS_mkdir, (long)path, (long)mode, 0, 0, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

int _dup2(int oldfd, int newfd) {
    long ret = __syscall(SYS_dup2, (long)oldfd, (long)newfd, 0, 0, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

int _gettimeofday(struct timeval *tv, void *tz) {
    long ret = __syscall(SYS_gettimeofday, (long)tv, (long)tz, 0, 0, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

int _nanosleep(const struct timespec *req, struct timespec *rem) {
    long ret = __syscall(SYS_nanosleep, (long)req, (long)rem, 0, 0, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

int _unlink(const char *path) {
    long ret = __syscall(SYS_unlink, (long)path, 0, 0, 0, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

int _getdents64(int fd, void *dirp, size_t count) {
    long ret = __syscall(SYS_getdents64, (long)fd, (long)dirp, (long)count, 0, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

/* Helper macro for syscalls */
#if defined(__x86_64__)
static inline long __syscall(long num, long a1, long a2, long a3, long a4, long a5) {
    long ret;
    __asm__ volatile (
        "mov %5, %%r8\n"
        "mov %4, %%r10\n"
        "syscall"
        : "=a"(ret)
        : "0"(num), "D"(a1), "S"(a2), "d"(a3), "r"(a4), "r"(a5)
        : "rcx", "r11", "memory"
    );
    return ret;
}
#elif defined(__aarch64__)
static inline long __syscall(long num, long a1, long a2, long a3, long a4, long a5) {
    register long x8 __asm__("x8") = num;
    register long x0 __asm__("x0") = a1;
    register long x1 __asm__("x1") = a2;
    register long x2 __asm__("x2") = a3;
    register long x3 __asm__("x3") = a4;
    register long x4 __asm__("x4") = a5;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4)
        : "memory"
    );
    return x0;
}
#elif defined(__riscv)
static inline long __syscall(long num, long a1, long a2, long a3, long a4, long a5) {
    register long a7 __asm__("a7") = num;
    register long a0 __asm__("a0") = a1;
    register long a1_ __asm__("a1") = a2;
    register long a2_ __asm__("a2") = a3;
    register long a3_ __asm__("a3") = a4;
    register long a4_ __asm__("a4") = a5;
    __asm__ volatile (
        "ecall"
        : "+r"(a0)
        : "r"(a7), "r"(a1_), "r"(a2_), "r"(a3_), "r"(a4_)
        : "memory"
    );
    return a0;
}
#endif

/* ---- write ---- */
ssize_t _write(int fd, const void *buf, size_t count) {
    long ret = __syscall(SYS_write, (long)fd, (long)buf, (long)count, 0, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return (ssize_t)ret;
}

/* ---- read ---- */
ssize_t _read(int fd, void *buf, size_t count) {
    long ret = __syscall(SYS_read, (long)fd, (long)buf, (long)count, 0, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return (ssize_t)ret;
}

/* ---- open ---- */
int _open(const char *path, int flags, int mode) {
    long ret = __syscall(SYS_open, (long)path, (long)flags, (long)mode, 0, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return (int)ret;
}

/* ---- close ---- */
int _close(int fd) {
    long ret = __syscall(SYS_close, (long)fd, 0, 0, 0, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return 0;
}

/* ---- lseek ---- */
off_t _lseek(int fd, off_t offset, int whence) {
    long ret = __syscall(SYS_lseek, (long)fd, (long)offset, (long)whence, 0, 0);
    if (ret < 0) { errno = -ret; return (off_t)-1; }
    return (off_t)ret;
}

/* ---- fstat ---- */
int _fstat(int fd, struct stat *st) {
    long ret = __syscall(SYS_fstat, (long)fd, (long)st, 0, 0, 0);
    if (ret < 0) { errno = -ret; return -1; }
    return 0;
}

/* ---- isatty ---- */
int _isatty(int fd) {
    long ret = __syscall(SYS_isatty, (long)fd, 0, 0, 0, 0);
    return (ret == 1);
}

/* ---- sbrk ---- */
void *_sbrk(intptr_t incr) {
    long cur = __syscall(SYS_brk, 0, 0, 0, 0, 0);
    if (cur < 0) { errno = ENOMEM; return (void *)-1; }
    
    if (incr == 0) return (void *)cur;

    long next = cur + incr;
    long ret = __syscall(SYS_brk, next, 0, 0, 0, 0);
    if (ret < 0) { errno = ENOMEM; return (void *)-1; }
    
    return (void *)cur;
}

/* ---- exit ---- */
void _exit(int code) {
    __syscall(SYS_exit, (long)code, 0, 0, 0, 0);
    while(1);
}

/* ---- getpid ---- */
int _getpid(void) {
    return 1;
}

/* ---- kill ---- */
int _kill(int pid, int sig) {
    (void)pid; (void)sig;
    errno = EINVAL;
    return -1;
}

/* ---- times ---- */
clock_t _times(struct tms *buf) {
    if (buf) {
        buf->tms_utime = 0;
        buf->tms_stime = 0;
        buf->tms_cutime = 0;
        buf->tms_cstime = 0;
    }
    return 0;
}

/* ---- stat ---- */
int _stat(const char *path, struct stat *st) {
    /* stat is not in our syscall list yet, but we can implement it via open/fstat/close */
    int fd = _open(path, 0, 0);
    if (fd < 0) return -1;
    int ret = _fstat(fd, st);
    _close(fd);
    return ret;
}
