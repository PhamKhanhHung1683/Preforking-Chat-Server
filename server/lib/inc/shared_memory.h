#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <pthread.h>

#include "client_handler.h"

typedef struct
{
    int client_fd;
    int pid;
    char name[MAX_NAME_LENGTH + 1];
} client_info;

struct shm_buf {
    pthread_rwlock_t rwlock;
    client_info clients[NUMBER_OF_CHILD_PROCESSESES][MAX_CLIENTS_PER_PROCESS];
    int count_joined_clients;
};

extern struct shm_buf *shm_clients;
extern int shm_fd;

int setup_shm();
int cleanup_shm();

#endif /* SHARED_MEMORY_H */