#ifndef SAGEOS_SAGE_LIBC_SHIM_H
#define SAGEOS_SAGE_LIBC_SHIM_H

/*
 * sage_libc_shim.h — Macro overrides redirecting libc to kernel functions
 *
 * Include this BEFORE any SageLang source that uses libc.
 * All malloc/free/string/stdio calls are redirected to the kernel
 * bump allocator and console_write.
 */

#include <stdint.h>
#include <stddef.h>

#include "sage_alloc.h"

void *sage_malloc(size_t s);
void *sage_realloc(void *p, size_t s);
void *sage_calloc(size_t n, size_t s);
void  sage_free(void *p);
char *sage_strdup(const char *s);

/* Memory allocation */
#define malloc        sage_malloc
#define free          sage_free
#define realloc       sage_realloc
#define calloc        sage_calloc
#define strdup        sage_strdup

/* String functions */
size_t sage_strlen(const char *s);
int    sage_strcmp(const char *a, const char *b);
int    sage_strncmp(const char *a, const char *b, size_t n);
char  *sage_strcpy(char *dest, const char *src);
char  *sage_strcat(char *dest, const char *src);
char  *sage_strncpy(char *dest, const char *src, size_t n);
char  *sage_strchr(const char *s, int c);
char  *sage_strstr(const char *h, const char *n);

#define strlen        sage_strlen
#define strcmp        sage_strcmp
#define strncmp       sage_strncmp
#define strcpy        sage_strcpy
#define strcat        sage_strcat
#define strncpy       sage_strncpy
#define strchr        sage_strchr
#define strstr        sage_strstr

/* Memory ops */
void *sage_memset(void *s, int c, size_t n);
void *sage_memcpy(void *d, const void *s, size_t n);
void *sage_memmove(void *d, const void *s, size_t n);
int   sage_memcmp(const void *a, const void *b, size_t n);

#define memset        sage_memset
#define memcpy        sage_memcpy
#define memmove       sage_memmove
#define memcmp        sage_memcmp

/* I/O — printf family */
int  sage_printf(const char *fmt, ...);
int  sage_snprintf(char *buf, size_t n, const char *fmt, ...);

#define printf(...)       sage_printf(__VA_ARGS__)
#define fprintf(f,...)    sage_printf(__VA_ARGS__)
#define sprintf(b,...)    sage_printf(__VA_ARGS__)
#define snprintf(b,n,...) sage_snprintf(b,n,__VA_ARGS__)

/* Control flow */
extern volatile int sage_exit_flag;
extern int sage_exit_code;
void sage_exit(int code);
void exit(int code);
void abort(void);

/* Math - removed to avoid conflicts with full SageLang */
uint64_t sage_strtod(const char *s, char **end);
long     sage_strtol(const char *s, char **end, int base);
int    sage_atoi(const char *s);
int    sage_isdigit(int c);
int    sage_isalpha(int c);
int    sage_isalnum(int c);
int    sage_isspace(int c);

#define strtod(s,e)      sage_strtod(s,e)
#define strtol(s,e,b)    sage_strtol(s,e,b)
#define atoi(s)          sage_atoi(s)
#define isdigit(c)       sage_isdigit(c)
#define isalpha(c)       sage_isalpha(c)
#define isalnum(c)       sage_isalnum(c)
#define isspace(c)       sage_isspace(c)

int sage_vsnprintf(char *buf, size_t n, const char *fmt, __builtin_va_list ap);
#define vsnprintf        sage_vsnprintf

/* Suppress headers that would conflict */
/* Standard library redirection */
#include <stdint.h>
#include <stddef.h>

/* Headers are now provided by actual_sagelang_build/libc if nostdlibinc/isystem is used, 
   otherwise by system. Redirections below. */

typedef uint64_t time_t;
typedef uint64_t clock_t;

/* Stub jmp_buf for repl.h — no real longjmp in kernel */
typedef int jmp_buf[1];
#define setjmp(b) 0
#define longjmp(b,v) sage_exit(v)

/* Stub FILE for any fprintf references */
typedef void FILE;
#define stderr ((FILE*)0)
#define stdout ((FILE*)0)
#define stdin  ((FILE*)0)
#define fflush(f) ((void)0)
int vfprintf(FILE* stream, const char* fmt, __builtin_va_list args);
int fputc(int c, FILE* stream);

static inline int sage_tolower(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}
static inline int sage_toupper(int c) {
    if (c >= 'a' && c <= 'z') return c - ('a' - 'A');
    return c;
}
#define tolower(c) sage_tolower(c)
#define toupper(c) sage_toupper(c)
#define isspace(c) sage_isspace(c)
int putchar(int c);

/* NULL */
#ifndef NULL
#define NULL ((void*)0)
#endif

#endif /* SAGEOS_SAGE_LIBC_SHIM_H */
