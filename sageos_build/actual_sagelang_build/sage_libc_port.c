#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

// SageOS kernel headers for HAL
#include "console.h"
#include "vfs.h"

// Redirections from sage_libc_shim.h are active here.
// We need to provide the ACTUAL implementations for the symbols the linker sees.

#undef malloc
#undef realloc
#undef calloc
#undef free
#undef printf
#undef fprintf
#undef vsnprintf
#undef toupper
#undef tolower

// Stubs for missing libc functions
void *sage_malloc(size_t size) { extern void* kernel_malloc(size_t); return kernel_malloc(size); }
void *sage_realloc(void *ptr, size_t size) { extern void* kernel_realloc(void*, size_t); return kernel_realloc(ptr, size); }
void *sage_calloc(size_t n, size_t size) { extern void* kernel_calloc(size_t, size_t); return kernel_calloc(n, size); }
void sage_free(void *ptr) { extern void kernel_free(void*); kernel_free(ptr); }

size_t sage_strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
int sage_strcmp(const char *s1, const char *s2) { while (*s1 && *s1 == *s2) { s1++; s2++; } return (int)(unsigned char)*s1 - (int)(unsigned char)*s2; }
char *sage_strncpy(char *dest, const char *src, size_t n) { char *d = dest; while (n > 0 && *src) { *d++ = *src++; n--; } while (n > 0) { *d++ = '\0'; n--; } return dest; }
void *sage_memset(void *s, int c, size_t n) { unsigned char *p = s; while (n--) *p++ = (unsigned char)c; return s; }
void *sage_memcpy(void *dest, const void *src, size_t n) { unsigned char *d = dest; const unsigned char *s = src; while (n--) *d++ = *s++; return dest; }
void *sage_memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = dest; const unsigned char *s = src;
    if (d < s) while (n--) *d++ = *s++;
    else { d += n; s += n; while (n--) *--d = *--s; }
    return dest;
}

int sage_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[1024];
    extern int sage_vsnprintf(char*, size_t, const char*, va_list);
    int n = sage_vsnprintf(buf, sizeof(buf), fmt, ap);
    console_write(buf);
    va_end(ap);
    return n;
}

// putchar is used by MbedTLS and SageLang
int putchar(int c) {
    console_putc((char)c);
    return c;
}

void exit(int status) {
    (void)status;
    for (;;);
}

void abort(void) {
    exit(1);
}

// More stubs
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r'; }
int isdigit(int c) { return c >= '0' && c <= '9'; }
int isalpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int isalnum(int c) { return isdigit(c) || isalpha(c); }

// VFS bridge
void* fopen(const char* path, const char* mode) {
    (void)path; (void)mode;
    return NULL;
}
size_t fread(void* ptr, size_t size, size_t nmemb, void* stream) {
    (void)ptr; (void)size; (void)nmemb; (void)stream;
    return 0;
}
int fclose(void* stream) {
    (void)stream;
    return 0;
}

// Math stubs
double sin(double x) { (void)x; return 0; }
double cos(double x) { (void)x; return 0; }
double tan(double x) { (void)x; return 0; }
double asin(double x) { (void)x; return 0; }
double acos(double x) { (void)x; return 0; }
double atan(double x) { (void)x; return 0; }
double atan2(double y, double x) { (void)y; (void)x; return 0; }
double log(double x) { (void)x; return 0; }
double log10(double x) { (void)x; return 0; }
double exp(double x) { (void)x; return 0; }
double round(double x) { (void)x; return 0; }

// Compiler-rt stubs (minimal soft-float helpers)
long __fixdfsi(double a) { return (long)a; }
unsigned long __fixunsdfsi(double a) { return (unsigned long)a; }
double __floatundidf(unsigned long a) { return (double)a; }
double __floatunsidf(unsigned int a) { return (double)a; }

double __adddf3(double a, double b) { return a + b; }
double __subdf3(double a, double b) { return a - b; }
double __muldf3(double a, double b) { return a * b; }
double __divdf3(double a, double b) { return a / b; }
double __floatsidf(int a) { return (double)a; }
double __floatdidf(long long a) { return (double)a; }
int __gedf2(double a, double b) { return a >= b ? 1 : (a < b ? -1 : 0); }
int __ltdf2(double a, double b) { return a < b ? -1 : (a >= b ? 1 : 0); }
int __ledf2(double a, double b) { return a <= b ? 0 : 1; }
int __gtdf2(double a, double b) { return a > b ? 1 : 0; }
int __nedf2(double a, double b) { return a != b ? 1 : 0; }
long long __fixdfdi(double a) { return (long long)a; }
