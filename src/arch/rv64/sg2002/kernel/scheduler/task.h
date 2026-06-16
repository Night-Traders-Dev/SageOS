#ifndef TASK_H
#define TASK_H

#include <stdint.h>

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED
} task_state_t;

typedef struct task_struct {
    uintptr_t sp; // Saved stack pointer
    task_state_t state;
    // ... additional task info
} task_t;

#endif
