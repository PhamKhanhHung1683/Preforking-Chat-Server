#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "util.h"
#include "message_queue.h"
#include "shared_memory.h"
#include "client_handler.h"

#define MAX_EVENTS 11

#define MAX_SIZE_OF_BUF 255

static void *mq_listener(void *arg);

/* command handle function */
static int handle_join_command(int pid, int client_fd, char *buf);
static int handle_quit_command(int pid, int client_fd);
static int handle_msg_command(int pid, int client_fd, char *buf);
static int handle_pmsg_command(int pid, int client_fd, char *buf);
static int handle_unknown_command(int client_fd);

int handle_clients(int pid, int listener)
{
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        perror("epoll_create1");
        return -1;
    }

    if (epoll_ctl_add(epoll_fd, listener, EPOLLIN) == -1)
    {
        return -1;
    }

    pthread_t mq_listener_thread;
    if (pthread_create(&mq_listener_thread, NULL, mq_listener,
                       (void *)&pid) != 0)
    {
        perror("pthread_create");
        return -1;
    }
    pthread_detach(mq_listener_thread);

    int count_clients = 0;
    struct epoll_event events[MAX_EVENTS];

    for (;;)
    {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1)
        {
            perror("epoll_wait");
            return -1;
        }

        for (int i = 0; i < nfds; i++)
        {
            if (events[i].data.fd == listener)
            {
                struct sockaddr_in client_addr;
                printf("Process %d: Accepting new client\n", getpid());
                int client_fd = accept(listener, (struct sockaddr *)&client_addr,
                                       &(socklen_t){sizeof(client_addr)});
                if (client_fd == -1)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        printf("Process %d: Can't accept new client\n", getpid());
                        continue;
                    }
                    else
                    {
                        perror("accept");
                        return -1;
                    }
                }
                printf("Process %d: New client connected: %d - IP: %s - Port: %d\n",
                       getpid(), client_fd, inet_ntoa(client_addr.sin_addr),
                       ntohs(client_addr.sin_port));

                if (epoll_ctl_add(epoll_fd, client_fd, EPOLLIN) == -1)
                {
                    return -1;
                }

                count_clients++;
                if (count_clients >= MAX_CLIENTS_PER_PROCESS)
                {
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, listener, NULL) == -1)
                    {
                        perror("epoll_ctl");
                        return -1;
                    }
                    printf("Process %d: Max clients reached, stop listenning\n", getpid());
                    break;
                }
            }
            else
            {
                int client_fd = events[i].data.fd;
                char buf[MAX_SIZE_OF_BUF + 1];
                memset(buf, 0, sizeof(buf));
                int ret = recv(client_fd, buf, sizeof(buf) - 1, 0);

                if (ret == -1)
                {
                    perror("recv");
                    return -1;
                }

                buf[ret] = 0;

                if (ret == 0)
                {
                    printf("Client disconnected\n");
                    if (handle_quit_command(pid, client_fd) == -1)
                    {
                        return -1;
                    }
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL) == -1)
                    {
                        perror("epoll_ctl");
                        return -1;
                    }
                    close(client_fd);
                    printf("Closed client socket: %d\n", client_fd);
                    count_clients--;
                    if (count_clients == MAX_CLIENTS_PER_PROCESS - 1)
                    {
                        if (epoll_ctl_add(epoll_fd, listener, EPOLLIN) == -1)
                        {
                            return -1;
                        }
                        printf("Process %d: Continue listenning socket\n", getpid());
                    }
                }
                else
                {
                    printf("Received: %s\n", buf);
                    if (strncmp(buf, "JOIN ", 5) == 0)
                    {
                        if (handle_join_command(pid, client_fd, buf) == -1)
                        {
                            return -1;
                        }
                    }
                    else if (strcmp(buf, "QUIT") == 0)
                    {
                        if (handle_quit_command(pid, client_fd) == -1)
                        {
                            return -1;
                        }
                    }
                    else if (strncmp(buf, "MSG ", 4) == 0)
                    {
                        if (handle_msg_command(pid, client_fd, buf) == -1)
                        {
                            return -1;
                        }
                    }
                    else if (strncmp(buf, "PMSG ", 5) == 0)
                    {
                        if (handle_pmsg_command(pid, client_fd, buf) == -1)
                        {
                            return -1;
                        }
                    }
                    else
                    {
                        if (handle_unknown_command(client_fd) == -1)
                        {
                            return -1;
                        }
                    }
                }
            }
        }
    }
}

static void *mq_listener(void *arg)
{
    int pid = *(int *)arg;
    struct mq_message mq_msg;

    while (1)
    {
        ssize_t bytes_read =
            mq_receive(mqd[pid], (char *)&mq_msg, sizeof(mq_msg), NULL);
        if (bytes_read == -1)
        {
            perror("mq_receive");
            _exit(EXIT_FAILURE);
        }
        char buf[256];
        memset(buf, 0, sizeof(buf));

        if (mq_msg.type == MSG)
        {
            // Broadcast message
            printf("Process %d: Received broadcast message: %s\n", getpid(),
                   mq_msg.message);
            snprintf(buf, sizeof(buf), "%s sent to room: %s\n",
                     mq_msg.sender_name, mq_msg.message);
            pthread_rwlock_rdlock(&shm_clients->rwlock);
            for (int i = 0; i < MAX_CLIENTS_PER_PROCESS; i++)
            {
                if (shm_clients->clients[pid][i].client_fd != 0)
                {
                    if (pid == mq_msg.sender_pid && shm_clients->clients[pid][i].client_fd == mq_msg.sender_fd)
                    {
                        continue;
                    }
                    if (send(shm_clients->clients[pid][i].client_fd, buf,
                             strlen(buf), 0) == -1)
                    {
                        perror("send");
                        _exit(EXIT_FAILURE);
                    }
                }
            }
            pthread_rwlock_unlock(&shm_clients->rwlock);
        }
        else if (mq_msg.type == PRIVATE_MSG)
        {
            // Private message
            printf("Process %d: Received private message for client %d: %s\n",
                   getpid(), mq_msg.receiver_fd, mq_msg.message);
            snprintf(buf, sizeof(buf), "%s sent to you: %s\n", mq_msg.sender_name,
                     mq_msg.message);
            if (send(mq_msg.receiver_fd, buf, strlen(buf), 0) == -1)
            {
                perror("send");
                _exit(EXIT_FAILURE);
            }
        }
        else if (mq_msg.type == JOIN_NOTIFICATION)
        {
            // Join notification
            printf("Process %d: Received join notification, client %s\n", getpid(),
                   mq_msg.sender_name);
            snprintf(buf, sizeof(buf), "Notification: %s joined\n", mq_msg.sender_name);
            pthread_rwlock_rdlock(&shm_clients->rwlock);
            for (int i = 0; i < MAX_CLIENTS_PER_PROCESS; i++)
            {
                if (shm_clients->clients[pid][i].client_fd != 0)
                {
                    if (pid == mq_msg.sender_pid && shm_clients->clients[pid][i].client_fd == mq_msg.sender_fd)
                    {
                        continue;
                    }
                    if (send(shm_clients->clients[pid][i].client_fd, buf, strlen(buf), 0) == -1)
                    {
                        perror("send");
                        _exit(EXIT_FAILURE);
                    }
                }
            }
            pthread_rwlock_unlock(&shm_clients->rwlock);
        }
        else if (mq_msg.type == QUIT_NOTIFICATION)
        {
            // Quit notification
            printf("Process %d: Received quit notification, client %s\n", getpid(),
                   mq_msg.sender_name);
            snprintf(buf, sizeof(buf), "Notification: %s quit\n", mq_msg.sender_name);
            pthread_rwlock_rdlock(&shm_clients->rwlock);
            for (int i = 0; i < MAX_CLIENTS_PER_PROCESS; i++)
            {
                if (shm_clients->clients[pid][i].client_fd != 0)
                {
                    if (send(shm_clients->clients[pid][i].client_fd, buf, strlen(buf), 0) == -1)
                    {
                        perror("send");
                        _exit(EXIT_FAILURE);
                    }
                }
            }
            pthread_rwlock_unlock(&shm_clients->rwlock);
        }
    }
}

static int get_client_index(int pid, int client_fd)
{
    pthread_rwlock_rdlock(&shm_clients->rwlock);
    for (int i = 0; i < MAX_CLIENTS_PER_PROCESS; i++)
    {
        if (shm_clients->clients[pid][i].client_fd == client_fd)
        {
            pthread_rwlock_unlock(&shm_clients->rwlock);
            return i;
        }
    }
    pthread_rwlock_unlock(&shm_clients->rwlock);
    return -1;
}

static client_info find_client_by_name(char *name)
{
    client_info client;
    memset(&client, 0, sizeof(client));
    pthread_rwlock_rdlock(&shm_clients->rwlock);
    for (int i = 0; i < NUMBER_OF_CHILD_PROCESSESES; i++)
    {
        for (int j = 0; j < MAX_CLIENTS_PER_PROCESS; j++)
        {
            if (shm_clients->clients[i][j].client_fd != 0 &&
                strcmp(shm_clients->clients[i][j].name, name) == 0)
            {
                pthread_rwlock_unlock(&shm_clients->rwlock);
                client.pid = shm_clients->clients[i][j].pid;
                client.client_fd = shm_clients->clients[i][j].client_fd;

                return client;
            }
        }
    }
    pthread_rwlock_unlock(&shm_clients->rwlock);
    return client;
}

static int handle_join_command(int pid, int client_fd, char *buf)
{
    if (get_client_index(pid, client_fd) == -1)
    {
        char cmd[16];
        char name[MAX_NAME_LENGTH];
        char tmp[16];
        memset(cmd, 0, sizeof(cmd));
        memset(name, 0, sizeof(name));
        memset(tmp, 0, sizeof(tmp));
        if (sscanf(buf, "%15s %31s %15s", cmd, name, tmp) == 2)
        {
            if (find_client_by_name(name).client_fd == 0)
            {
                int index = get_client_index(pid, 0);

                pthread_rwlock_wrlock(&shm_clients->rwlock);
                shm_clients->clients[pid][index].client_fd = client_fd;
                shm_clients->clients[pid][index].pid = pid;
                strncpy(shm_clients->clients[pid][index].name, name,
                        MAX_NAME_LENGTH);
                shm_clients->count_joined_clients++;
                pthread_rwlock_unlock(&shm_clients->rwlock);

                printf("Process %d: Client %s joined\n", pid, name);

                pthread_rwlock_rdlock(&shm_clients->rwlock);
                printf("Number of joined clients: %d\n", shm_clients->count_joined_clients);
                pthread_rwlock_unlock(&shm_clients->rwlock);

                struct mq_message mq_msg;
                memset(&mq_msg, 0, sizeof(mq_msg));
                mq_msg.type = JOIN_NOTIFICATION;
                mq_msg.sender_pid = pid;
                mq_msg.sender_fd = client_fd;
                snprintf(mq_msg.sender_name, sizeof(mq_msg.sender_name), "%s", name);
                for (int i = 0; i < NUMBER_OF_CHILD_PROCESSESES; i++)
                {
                    if (mq_send(mqd[i], (char *)&mq_msg, sizeof(mq_msg), 0) == -1)
                    {
                        perror("mq_send");
                        char *msg = "999 UNKNOWN ERROR\n";
                        if (send(client_fd, msg, strlen(msg), 0) == -1)
                        {
                            perror("send");
                            return -1;
                        }
                        return -1;
                    }
                }

                char *res = "100 OK\n";
                if (send(client_fd, res, strlen(res), 0) == -1)
                {
                    perror("send");
                    return -1;
                }
            }
            else
            {
                char *res = "200 NICKNAME IN USE\n";
                if (send(client_fd, res, strlen(res), 0) == -1)
                {
                    perror("send");
                    return -1;
                }
            }
        }
        else
        {
            char *res = "999 UNKNOWN ERROR\n";
            if (send(client_fd, res, strlen(res), 0) == -1)
            {
                perror("send");
                return -1;
            }
        }
    }
    else
    {
        char *res = "888 WRONG STATE\n";
        if (send(client_fd, res, strlen(res), 0) == -1)
        {
            perror("send");
            return -1;
        }
    }
}

static int handle_quit_command(int pid, int client_fd)
{
    int index = get_client_index(pid, client_fd);
    if (index != -1)
    {
        char name[MAX_NAME_LENGTH];
        pthread_rwlock_rdlock(&shm_clients->rwlock);
        snprintf(name, sizeof(name), "%s", shm_clients->clients[pid][index].name);
        pthread_rwlock_unlock(&shm_clients->rwlock);

        pthread_rwlock_wrlock(&shm_clients->rwlock);
        memset(&shm_clients->clients[pid][index], 0, sizeof(client_info));
        shm_clients->count_joined_clients--;
        pthread_rwlock_unlock(&shm_clients->rwlock);

        printf("Process %d: Client %s quit\n", pid, name);
        pthread_rwlock_rdlock(&shm_clients->rwlock);
        printf("Number of joined clients: %d\n", shm_clients->count_joined_clients);
        pthread_rwlock_unlock(&shm_clients->rwlock);

        struct mq_message mq_msg;
        memset(&mq_msg, 0, sizeof(mq_msg));
        mq_msg.type = QUIT_NOTIFICATION;
        mq_msg.sender_pid = pid;
        mq_msg.sender_fd = client_fd;
        snprintf(mq_msg.sender_name, sizeof(mq_msg.sender_name), "%s", name);
        for (int i = 0; i < NUMBER_OF_CHILD_PROCESSESES; i++)
        {
            if (mq_send(mqd[i], (char *)&mq_msg, sizeof(mq_msg), 0) == -1)
            {
                perror("mq_send");
                char *res = "999 UNKNOWN ERROR\n";
                if (send(client_fd, res, strlen(res), 0) == -1)
                {
                    perror("send");
                    return -1;
                }
                return -1;
            }
        }
        char *res = "100 OK\n";
        if (send(client_fd, res, strlen(res), 0) == -1)
        {
            perror("send");
            return -1;
        }
    }
    else
    {
        char *res = "999 UNKNOWN ERROR\n";
        if (send(client_fd, res, strlen(res), 0) == -1)
        {
            perror("send");
            return -1;
        }
    }
}

static int handle_msg_command(int pid, int client_fd, char *buf)
{
    if (get_client_index(pid, client_fd) != -1)
    {
        char cmd[16];
        char msg[256];
        memset(cmd, 0, sizeof(cmd));
        memset(msg, 0, sizeof(msg));
        if (sscanf(buf, "%15s %200[^\n]", cmd, msg) == 2)
        {
            if (strlen(msg) >= 200)
            {
                char *res = "999 UNKNOWN ERROR\n";
                if (send(client_fd, res, strlen(res), 0) == -1)
                {
                    perror("send");
                    return -1;
                }
                return 0;
            }
            struct mq_message mq_msg;
            memset(&mq_msg, 0, sizeof(mq_msg));
            mq_msg.type = MSG;
            mq_msg.sender_pid = pid;
            mq_msg.sender_fd = client_fd;
            strncpy(mq_msg.sender_name, shm_clients->clients[pid][get_client_index(pid, client_fd)].name, MAX_NAME_LENGTH);
            strncpy(mq_msg.message, msg, strlen(msg));
            for (int i = 0; i < NUMBER_OF_CHILD_PROCESSESES; i++)
            {
                if (mq_send(mqd[i], (char *)&mq_msg, sizeof(mq_msg), 0) == -1)
                {
                    perror("mq_send");
                    char *res = "999 UNKNOWN ERROR\n";
                    if (send(client_fd, res, strlen(res), 0) == -1)
                    {
                        perror("send");
                        return -1;
                    }
                    return -1;
                }
            }
            char *res = "100 OK\n";
            if (send(client_fd, res, strlen(res), 0) == -1)
            {
                perror("send");
                return -1;
            }
        }
        else
        {
            char *res = "999 UNKNOWN ERROR\n";
            if (send(client_fd, res, strlen(res), 0) == -1)
            {
                perror("send");
                return -1;
            }
        }
    }
    else
    {
        char *res = "888 WRONG STATE\n";
        if (send(client_fd, res, strlen(res), 0) == -1)
        {
            perror("send");
            return -1;
        }
    }
}

static int handle_pmsg_command(int pid, int client_fd, char *buf)
{
    if (get_client_index(pid, client_fd) != -1)
    {
        char cmd[16];
        char receiver[33];
        char msg[256];
        memset(cmd, 0, sizeof(cmd));
        memset(receiver, 0, sizeof(receiver));
        memset(msg, 0, sizeof(msg));
        if (sscanf(buf, "%15s %32s %200[^\n]", cmd, receiver, msg) == 3)
        {
            if ((strlen(receiver) >= 32) || (strlen(msg) >= 200))
            {
                char *res = "999 UNKNOWN ERROR\n";
                if (send(client_fd, res, strlen(res), 0) == -1)
                {
                    perror("send");
                    return -1;
                }
                return 0;
            }
            client_info receiver_info = find_client_by_name(receiver);
            if (receiver_info.client_fd != 0)
            {
                struct mq_message mq_msg;
                memset(&mq_msg, 0, sizeof(mq_msg));
                mq_msg.type = PRIVATE_MSG;
                mq_msg.sender_pid = pid;
                mq_msg.sender_fd = client_fd;
                strncpy(mq_msg.sender_name, shm_clients->clients[pid][get_client_index(pid, client_fd)].name, MAX_NAME_LENGTH);
                mq_msg.receiver_pid = receiver_info.pid;
                mq_msg.receiver_fd = receiver_info.client_fd;
                strncpy(mq_msg.message, msg, strlen(msg));
                if (mq_send(mqd[mq_msg.receiver_pid], (char *)&mq_msg,
                            sizeof(mq_msg), 0) == -1)
                {
                    perror("mq_send");
                    char *msg = "999 UNKNOWN ERROR\n";
                    send(client_fd, msg, strlen(msg), 0);
                    return -1;
                }
                char *res = "100 OK\n";
                if (send(client_fd, res, strlen(res), 0) == -1)
                {
                    perror("send");
                    return -1;
                }
            }
            else
            {
                char *res = "202 UNKNOWN NICKNAME\n";
                if (send(client_fd, res, strlen(res), 0) == -1)
                {
                    perror("send");
                    return -1;
                }
            }
        }
        else
        {
            char *res = "999 UNKNOWN ERROR\n";
            if (send(client_fd, res, strlen(res), 0) == -1)
            {
                perror("send");
                return -1;
            }
        }
    }
    else
    {
        char *res = "888 WRONG STATE\n";
        if (send(client_fd, res, strlen(res), 0) == -1)
        {
            perror("send");
            return -1;
        }
    }
}

static int handle_unknown_command(int client_fd)
{
    char *res = "999 UNKNOWN COMMAND\n";
    if (send(client_fd, res, strlen(res), 0) == -1)
    {
        perror("send");
        return -1;
    }
}
