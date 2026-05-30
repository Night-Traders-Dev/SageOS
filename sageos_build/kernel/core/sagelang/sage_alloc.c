#include <stdint.h>
#include <stddef.h>
#include "sage_alloc.h"
#include "console.h"
#include "sage_libc_shim.h"

/* 
 * sage_alloc.c — Unified memory allocator for SageOS SageLang
 *
 * Implements a robust bump allocator with basic realloc growth.
 * Consolidates fragmented memory logic from libc shims and VM.
 */

static uint8_t sage_heap[SAGE_ARENA_SIZE] __attribute__((aligned(16)));
static size_t sage_bump = 0;

void *sage_malloc(size_t size) {
    if (size == 0) return NULL;
    
    size_t raw_size = size;
    // Align to 16 bytes
    size = (size + 15) & ~(size_t)15;
    
    // Header: size (8 bytes) + padding (8 bytes for 16-byte alignment of data)
    if (sage_bump + size + 16 > SAGE_ARENA_SIZE) {
        console_write("\nsage: out of memory (requested: ");
        console_u32((uint32_t)raw_size);
        console_write(" bytes)\n");
        return NULL;
    }
    
    size_t *header = (size_t *)&sage_heap[sage_bump];
    *header = size;
    sage_bump += size + 16;
    
    void *ptr = (void *)((uint8_t*)header + 16);
    sage_memset(ptr, 0, size);
    return ptr;
}

void *sage_calloc(size_t count, size_t size) {
    return sage_malloc(count * size);
}

void *sage_realloc(void *ptr, size_t new_size) {
    if (!ptr) return sage_malloc(new_size);
    if (new_size == 0) return NULL;
    
    size_t *header = (size_t *)((uint8_t *)ptr - 16);
    size_t old_size = *header;
    
    if (new_size <= old_size) return ptr;

    // Last allocation optimization
    if ((uint8_t *)ptr + old_size == &sage_heap[sage_bump]) {
        size_t needed = (new_size + 15) & ~(size_t)15;
        size_t extra = needed - old_size;
        if (sage_bump + extra <= SAGE_ARENA_SIZE) {
            *header = needed;
            sage_bump += extra;
            return ptr;
        }
    }
    
    void *np = sage_malloc(new_size);
    if (!np) return NULL;
    
    sage_memcpy(np, ptr, old_size);
    return np;
}

void sage_free(void *ptr) {
    (void)ptr;
}

char *sage_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = sage_strlen(s);
    char *d = (char *)sage_malloc(len + 1);
    if (!d) return NULL;
    sage_memcpy(d, s, len + 1);
    return d;
}

void sage_arena_reset(void) {
    sage_bump = 0;
}

size_t sage_arena_used(void) {
    return sage_bump;
}
