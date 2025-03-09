#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/epoll.h>

#include "message_queue.h"
#include "shared_memory.h"
#include "util.h"

static void sigchld_handler(int sig);

int setup_sigchld_handler()
{
    struct sigaction sa;

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP; // Restart interrupted system calls and don't send SIGCHLD when children stop

    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }

    return 0;
}

int set_non_blocking(int fd)
{
    int flags, s;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;
    s = fcntl(fd, F_SETFL, flags);
    if (s == -1)
    {
        perror("fcntl");
        return -1;
    }

    return 0;
}


int epoll_ctl_add(int epfd, int fd, uint32_t events)
{
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1)
    {
        perror("epoll_ctl");
        return -1;
    }
}

void cleanup(int sig) {
    printf("Parent process: Cleaning up and exiting...\n");

    if (cleanup_shm() == -1) {
        exit(EXIT_FAILURE);
    }

    if(cleanup_mq() == -1) {
        exit(EXIT_FAILURE);
    }

    if (kill(0, SIGTERM) == -1)
    {
        perror("kill");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

static void sigchld_handler(int sig)
{
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) // Non-blocking wait
    {
        if (WIFEXITED(status))
        {
            printf("Child %d exited with status %d\n", pid, WEXITSTATUS(status));
        }
        else
        {
            printf("Child %d exited abnormally\n", pid);
        }
    }
}
