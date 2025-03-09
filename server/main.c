#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "util.h"
#include "socket.h"
#include "shared_memory.h"
#include "message_queue.h"
#include "client_handler.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int listener = setup_listener_socket(argv[1]);
    if (listener == -1)
    {
        exit(EXIT_FAILURE);
    }

    if (setup_shm() == -1)
    {
        exit(EXIT_FAILURE);
    }

    if (setup_mq() == -1)
    {
        exit(EXIT_FAILURE);
    }

    if (setup_sigchld_handler() == -1)
    {
        exit(EXIT_FAILURE);
    }

    fflush(stdout); // Flush before forking
    for (int i =0; i < NUMBER_OF_CHILD_PROCESSESES; i++)
    {
        pid_t pid = fork();
        switch (pid)
        {
        case -1:
            perror("fork");
            exit(EXIT_FAILURE);
        case 0:
            // Child process
            handle_clients(i, listener);
            close(listener);
            _exit(EXIT_FAILURE);
        default:
            // Parent process
            printf("Started new child process with PID %d\n", pid);
            break;
        }
    }
    close(listener);
    signal(SIGINT, cleanup);

    while(1);

    return 0;
}