#ifndef IPC_H
#define IPC_H

#include <stdint.h>
#include <stddef.h>

#define MAX_MSG_SIZE 256
#define MAX_QUEUE_SIZE 16

typedef struct {
    uint32_t sender_pid;
    uint32_t len;
    char data[MAX_MSG_SIZE];
} ipc_msg_t;

void ipc_init();
int ipc_send(uint32_t dest_pid, ipc_msg_t* msg);
int ipc_receive(ipc_msg_t* msg);

#endif
