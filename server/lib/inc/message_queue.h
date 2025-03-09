#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <mqueue.h>

#include "client_handler.h"

#define MQ_PATH "/process_%d_mq"

typedef enum
{
    MSG,
    PRIVATE_MSG,
    JOIN_NOTIFICATION,
    QUIT_NOTIFICATION,
} msg_type;

struct mq_message
{
    msg_type type;
    char message[200];
    int sender_pid;
    int sender_fd;
    char sender_name[MAX_NAME_LENGTH + 1];
    int receiver_pid;
    int receiver_fd;
};

extern mqd_t mqd[NUMBER_OF_CHILD_PROCESSESES];

int setup_mq();
int cleanup_mq();

#endif /* MESSAGE_QUEUE_H */