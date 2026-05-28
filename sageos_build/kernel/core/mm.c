#include "process.h"
#include "vfs.h"

/* 
 * sys_brk: Change the data segment size (heap)
 */
long sys_brk(uintptr_t addr) {
    task_t *t = current_task();
    if (t == NULL) return -1;

    /* If addr is 0, return the current end of the heap */
    if (addr == 0)
        return (long)t->heap_end;

    /* Validate bounds */
    if (addr < t->heap_base || addr > t->heap_limit)
        return VFS_EINVAL; /* Using VFS_EINVAL as a fallback for -ENOMEM for now */

    t->heap_end = addr;
    return (long)addr;
}
