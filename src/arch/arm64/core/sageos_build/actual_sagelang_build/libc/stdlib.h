#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

void *malloc(size_t size);
void *realloc(void *ptr, size_t size);
void *calloc(size_t nmemb, size_t size);
void free(void *ptr);

void exit(int status);
void abort(void);

char *getenv(const char *name);
int system(const char *command);

// strtod and strtol are handled by shim
int mkstemp(char *template);

int rand(void);
#define RAND_MAX 32767

#endif
