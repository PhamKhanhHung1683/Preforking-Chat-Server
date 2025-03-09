#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#define MQ_PATH "/process_%d_mq"

#define MAX_NAME_LENGTH 31
#define NUMBER_OF_CHILD_PROCESSESES 2
#define MAX_CLIENTS_PER_PROCESS 1

int handle_clients(int pid, int listener);

#endif /* CLIENT_HANDLER_H */