#ifndef UTIL_H
#define UTIL_H
#include <stdint.h>

int setup_sigchld_handler();
int set_non_blocking(int fd); // Set the file descriptor to non-blocking mode
int epoll_ctl_add(int epfd, int fd, uint32_t events);
void cleanup(int sig);

#endif /* UTIL_H */