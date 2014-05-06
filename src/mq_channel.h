#ifndef __MQ_CHANNEL_H__
#define __MQ_CHANNEL_H__

#include "mq_log.h"
#include "mq_timer.h"

typedef enum {
    MQ_CHANNEL_MANAGE,
    MQ_CHANNEL_REQREP,
    MQ_CHANNEL_PUBSUB
} mq_channel_type_t;

int mq_create_channel(mq_channel_type_t type, const char *channel);

#endif
