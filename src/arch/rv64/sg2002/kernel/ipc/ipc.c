// Basic message queue IPC implementation
#include "ipc.h"
#include <string.h>

static ipc_msg_t queue[MAX_QUEUE_SIZE];
static int head = 0;
static int tail = 0;

void ipc_init() {
    head = 0;
    tail = 0;
}

int ipc_send(uint32_t dest_pid, ipc_msg_t* msg) {
    // Simple ring buffer send
    int next = (tail + 1) % MAX_QUEUE_SIZE;
    if (next == head) return -1; // Queue full

    queue[tail] = *msg;
    tail = next;
    return 0;
}

int ipc_receive(ipc_msg_t* msg) {
    if (head == tail) return -1; // Queue empty

    *msg = queue[head];
    head = (head + 1) % MAX_QUEUE_SIZE;
    return 0;
}
