#include "mq_channel.h"
#include <zmq.h>

extern int mq_main(void *zsock);

static int mq_create_manage_channel(const char *channel_name);
static int mq_create_req_rep_channel(const char *channel_name);
static int mq_create_pub_sub_channel(const char *channel_name);

int mq_create_channel(mq_channel_type_t type, const char *channel_name)
{
    if (channel_name == NULL) {
        mq_log_error("channel name NULL\n");
        return -1;
    }

    switch (type) {
        case MQ_CHANNEL_MANAGE:
            mq_log_info("create manage channel: %s\n", channel_name);
            return mq_create_manage_channel(channel_name);
        case MQ_CHANNEL_REQREP:
            mq_log_info("create req_rep channel: %s\n", channel_name);
            return mq_create_req_rep_channel(channel_name);
        case MQ_CHANNEL_PUBSUB:
            mq_log_info("create pub_sub channel: %s\n", channel_name);
            return mq_create_pub_sub_channel(channel_name);
        default:
            mq_log_error("cannot parse channel type of %d\n", type);
            return -1;
    }
}

static int mq_create_manage_channel(const char *channel)
{
    void *ctx, *manage;
    static int instance = 0;

    if (instance == 0) {
        instance = 1;
    } else {
        mq_log_error("manage channel exist\n");
        return -1;
    }

    ctx = zmq_ctx_new();
    manage = zmq_socket(ctx, ZMQ_REP);
    if (zmq_bind(manage, channel) == -1) {
        mq_log_error("create manage channel error, channel: %s, errno: %d\n",
                channel, errno);
        return -1;
    }
    return mq_main(manage);
}

static int mq_create_req_rep_channel(const char *channel)
{
    void *ctx, *socket;

    /* TODO: return pid */
    return 0;
}

static int mq_create_pub_sub_channel(const char *channel_name)
{
    return 0;
}

