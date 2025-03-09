#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_SIZE_OF_BUF 255

static int setup_client_socket(char *host, char *port);
static void *recv_func(void *arg);

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    int client = setup_client_socket(argv[1], argv[2]);
    if (client == -1)
    {
        exit(EXIT_FAILURE);
    }
    printf("Connected to server\n");

    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, recv_func, (void *)&client) != 0)
    {
        printf("pthread_create failed\n");
        return 1;
    }
    pthread_detach(recv_thread);

    char buf[MAX_SIZE_OF_BUF + 2]; // include '\n' of fgets
    while (1)
    {
        memset(buf, 0, sizeof(buf));
        fgets(buf, sizeof(buf), stdin);
        if(buf[strlen(buf) - 1] != '\n') {
            printf("Input too long, try again\n");
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            continue;;
        }
        buf[strcspn(buf, "\n")] = 0;
        if (send(client, buf, strlen(buf), 0) < 0)
        {
            perror("send");
            return 1;
        }
        if (strcmp(buf, "exit") == 0)
        {
            break;
        }
    }

    close(client);
    return 0;
}

static void *recv_func(void *arg)
{
    int client = *(int *)arg;
    char buf[MAX_SIZE_OF_BUF + 1];
    while (1)
    {
        memset(buf, 0, sizeof(buf));
        int ret = recv(client, buf, MAX_SIZE_OF_BUF, 0);
        if (ret < 0)
        {
            perror("recv");
            break;
        }
        else if (ret == 0)
        {
            printf("Server disconnected\n");
            break;
        }
        buf[ret] = 0;
        puts(buf);
    }
    close(client);
    return 0;
}

static int setup_client_socket(char *host, char *port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;       /* IPv4 */
    hints.ai_socktype = SOCK_STREAM; /* TCP socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0; /* Any protocol */

    s = getaddrinfo(host, port, &hints, &result);
    if (s != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
        {
            continue;
        }
        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
        {
            break; /* Success */
        }
        close(sfd);
    }

    freeaddrinfo(result);

    if (rp == NULL) /* No address succeeded */
    {
        fprintf(stderr, "Could not connect\n");
        return -1;
    }
    return sfd;
}