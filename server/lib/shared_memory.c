#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

#include "client_handler.h"
#include "shared_memory.h"

#define SHM_PATH "/client_list_shm"

struct shm_buf *shm_clients;
int shm_fd;

int setup_shm()
{
    int shm_fd = shm_open(SHM_PATH, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1)
    {
        perror("shm_open");
        return -1;
    }

    if (ftruncate(shm_fd, sizeof(struct shm_buf)) == -1)
    {
        perror("ftruncate");
        return -1;
    }

    shm_clients = mmap(NULL, sizeof(*shm_clients), PROT_READ | PROT_WRITE,
                       MAP_SHARED, shm_fd, 0);
    if (shm_clients == MAP_FAILED)
    {
        perror("mmap");
        return -1;
    }

    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
    pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);

    if(pthread_rwlock_init(&shm_clients->rwlock, &attr) != 0 ) {
        fprintf(stderr, "pthread_rwlock_init");
        return -1;
    }
}

int cleanup_shm() {
    if (munmap(shm_clients, sizeof(struct shm_buf)) == -1)
    {
        perror("munmap");
        return -1;
    }

    if (close(shm_fd) == -1)
    {
        perror("close shm_fd");
        return -1;
    }

    if (shm_unlink(SHM_PATH) == -1)
    {
        perror("shm_unlink");
        return -1;
    }
}