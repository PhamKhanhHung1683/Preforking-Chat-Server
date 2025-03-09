#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "socket.h"
#include "util.h"

#define BACKLOG 10

static int print_server_info(int listener);

int setup_listener_socket(const char *port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;       /* IPv4 */
    hints.ai_socktype = SOCK_STREAM; /* TCP socket */
    hints.ai_flags = AI_PASSIVE;     /* For wildcard IP address */
    hints.ai_protocol = 0;           /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    s = getaddrinfo(NULL, port, &hints, &result);
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
        s = bind(sfd, rp->ai_addr, rp->ai_addrlen);
        if (s == 0)
        {
            break; /* Success */
        }

        close(sfd);
    }

    freeaddrinfo(result);

    if (rp == NULL) /* No address succeeded */
    {
        fprintf(stderr, "Could not bind\n");
        return -1;
    }

    if (set_non_blocking(sfd) == -1)
    {
        return -1;
    }

    if (print_server_info(sfd) == -1)
    {
        return -1;
    }

    if (listen(sfd, BACKLOG) == -1)
    {
        perror("listen");
        return -1;
    }

    return sfd;
}

static int print_server_info(int listener)
{
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    if (getsockname(listener, (struct sockaddr *)&server_addr, &addr_len) == -1)
    {
        perror("getsockname");
        return -1;
    }

    if (server_addr.sin_addr.s_addr == INADDR_ANY)
    {
        printf("Server IP: 0.0.0.0 (listening on all interfaces)\n");
    }
    else
    {
        printf("Server IP: %s\n", inet_ntoa(server_addr.sin_addr));
    }
    printf("Server Port: %d\n", ntohs(server_addr.sin_port));

    return 0;
}