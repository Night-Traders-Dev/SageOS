// Minimal scheduler implementation
#include "task.h"

#define MAX_TASKS 16

static task_t tasks[MAX_TASKS];
static int current_task = 0;

void scheduler_init() {
    // 1. Initialize task structures
    // 2. Set up initial idle task
}

void schedule() {
    // 1. Find next READY task
    // 2. Perform context switch
}
