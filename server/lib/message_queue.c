#include <stdio.h>
#include <mqueue.h>
#include <fcntl.h> 

#include "client_handler.h"
#include "message_queue.h"

mqd_t mqd[NUMBER_OF_CHILD_PROCESSESES];

int setup_mq()
{
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(struct mq_message);
    attr.mq_curmsgs = 0;
    char mq_name[32];
    for (int i = 0; i < NUMBER_OF_CHILD_PROCESSESES; i++)
    {
        snprintf(mq_name, sizeof(mq_name), MQ_PATH, i);
        mqd[i] = mq_open(mq_name, O_CREAT | O_RDWR, 0666, &attr);
        if (mqd[i] == -1)
        {
            perror("mq_open");
            return -1;
        }
    }
}

int cleanup_mq() {
    char mq_name[32];
    for (int i = 0; i < NUMBER_OF_CHILD_PROCESSESES; i++)
    {
        memset(mq_name, 0, sizeof(mq_name));
        snprintf(mq_name, sizeof(mq_name), MQ_PATH, i);
        if (mq_close(mqd[i]) == -1)
        {
            perror("mq_close");
            return -1;
        }
        if (mq_unlink(mq_name) == -1)
        {
            perror("mq_unlink");
            return -1;
        }
    }
}