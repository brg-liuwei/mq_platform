#include "mq_channel.h"
#include "mq_io.h"
#include <unistd.h>
#include <zmq.h>

extern void *mq_manage_ctx;
extern void *mq_manage_sock;
extern int mq_main(void);

void *mq_create_channel(int type, const char *channel_name)
{
    void *ctx, *sock;

    if (channel_name == NULL) {
        mq_log_error("channel name or controller NULL\n");
        return NULL;
    }
    ctx = zmq_ctx_new();
    sock = zmq_socket(ctx, type);
    if (sock == NULL) {
        goto free1;
    }
    if (type == ZMQ_SUB) {
        zmq_setsockopt(sock, ZMQ_SUBSCRIBE, "", 0);
    }

    if (zmq_bind(sock, channel_name) == -1) {
        mq_log_error("mq_create_channel zmq_bind failed, errno: %d,"
                "channel addr: %s, sock_type: %d\n", 
                errno, channel_name, type);
        goto free2;
    }
    return sock;

free2:
    zmq_close(sock);
free1:
    zmq_ctx_term(ctx);
    return NULL;
}

int mq_channel_poll(void *input, void *output, void *controller)
{
    int            nrcv, rcv_more;
    char           buf[1024 * 1024];
    mq_msg_hdr_t  *msg_hdr;

    zmq_pollitem_t items[] = {
        {input, 0, ZMQ_POLLIN, 0},
        {output, 0, ZMQ_POLLIN | ZMQ_POLLOUT, 0},  /* ZMQ_POLLIN for res-rep mode */
        {controller, 0, ZMQ_POLLIN, 0},
    };

    int val = 3;
    zmq_setsockopt(output, ZMQ_SNDHWM, &val, sizeof val);
    mq_io_init();
    while (1) {
        zmq_poll(items, 3, 1000);
        mq_time_update();

        if (items[0].revents & ZMQ_POLLIN) {
            nrcv = zmq_recv(input, buf, sizeof buf, 0);
            mq_write_msg(buf, nrcv);
        }

        if (items[1].revents & ZMQ_POLLIN) {
            nrcv = zmq_recv(output, buf, sizeof buf, 0);
            zmq_send(input, buf, nrcv, 0);
        }

        if (items[1].revents & ZMQ_POLLOUT) {
            msg_hdr = mq_read_msg();
            if (msg_hdr != NULL) {
                zmq_send(output, msg_hdr->msg, msg_hdr->msg_size, 0);
            } else {
                if (!(items[0].revents & ZMQ_POLLIN)
                        && !(items[0].revents & ZMQ_POLLIN)
                        && !(items[2].revents & ZMQ_POLLIN))
                {
                    sleep(1);
                }

            }
        }

        if (items[2].revents & ZMQ_POLLIN) {
            nrcv = zmq_recv(controller, buf, sizeof buf, 0);
#ifdef MQ_DEBUG
            mq_log_debug("[%d] controller recv cmd: %s\n", getpid(), buf);
#endif
            zmq_send(controller, "cmd has recved\n", sizeof("cmd has recved\n"), 0);
        }
    }

    mq_log_debug("mq_channel_poll return\n");
    return -1;
}

int mq_create_manage_channel(const char *channel)
{
    static int instance = 0;

    if (instance == 0) {
        instance = 1;
    } else {
        mq_log_error("manage channel exist\n");
        return -1;
    }

    mq_manage_ctx = zmq_ctx_new();
    mq_manage_sock = zmq_socket(mq_manage_ctx, ZMQ_REP);
    if (zmq_bind(mq_manage_sock, channel) == -1) {
        mq_log_error("create manage channel error, channel: %s, errno: %d\n",
                channel, errno);
        return -1;
    }
    return mq_main();
}

