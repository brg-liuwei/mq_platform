#ifndef __MQ_CHANNEL_H__
#define __MQ_CHANNEL_H__

#include "mq_log.h"
#include "mq_timer.h"

typedef enum {
    MQ_CHANNEL_MANAGE,
    MQ_CHANNEL_REQREP,
    MQ_CHANNEL_PUBSUB,
    MQ_CHANNEL_PUSHPULL,
    MQ_CHANNEL_ILLEGAL
} mq_channel_type_t;

void *mq_create_channel(int type, const char *channel);
int mq_create_manage_channel(const char *channel);
void *mq_create_output_channel(int type, const char *channel);
int mq_channel_poll(void *input, void *output, void *controller);
int mq_destroy_channel(const char *channel);

#endif
