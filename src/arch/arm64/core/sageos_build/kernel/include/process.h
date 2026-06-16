#ifndef SAGEOS_PROCESS_H
#define SAGEOS_PROCESS_H

#include "scheduler.h"

/* 
 * GCC Port: process.h
 * Provides task_t and FD definitions for the syscall layer.
 */

typedef struct thread task_t;

/* 
 * Convenience functions for tasks 
 */
static inline task_t* current_task(void) {
    return (task_t*)sched_current_thread();
}

#endif
