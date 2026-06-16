// Early bump-pointer heap allocator
#include <stdint.h>
#include <stddef.h>

extern char _heap_start[];
static uintptr_t heap_ptr = (uintptr_t)_heap_start;

void* early_malloc(size_t size) {
    // Align size to 8-byte boundary
    size = (size + 7) & ~7;
    
    // Bump heap_ptr
    uintptr_t ptr = heap_ptr;
    heap_ptr += size;
    
    return (void*)ptr;
}
