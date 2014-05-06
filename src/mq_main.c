#include "mq_channel.h"
#include <zmq.h>
#include <string.h>

#define MQ_CMDSIZE 256

#define MQ_CMDMAXARG 4
struct mq_command_t {
    int    argc;
    void  *argv[MQ_CMDMAXARG];
};

int mq_main(void *zsock)
{
    int   nrecv, i, state;
    char  command[MQ_CMDSIZE], *p;

    struct mq_command_t cmd;

    while (1) {
        memset(command, '\0', sizeof command);
        nrecv = zmq_recv(zsock, command, MQ_CMDSIZE, 0);

        if (nrecv >= MQ_CMDSIZE) {
            command[MQ_CMDSIZE - 1] = '\0';
            mq_log_error("Command too long: %s\n", command);
            continue;
        }

        /* get cmd */
#ifdef MQ_DEBUG
        mq_log_debug("get cmd: %s\n", command);
#endif
        for (cmd.argc = 0, p = command, state = 0; 
                cmd.argc < MQ_CMDMAXARG && *p != '\0'; 
                ++p)
        {
            if (*p == ' ' || *p == '\t' || *p == '\n') {
                *p = '\0';
            }
            switch (state) {
                case 0:
                    if (*p != '\0') {
                        state = 1;
                        cmd.argv[cmd.argc++] = p;
                    } else {
                        state = 2;
                    }
                    break;

                case 1:
                    if (*p != '\0') {
                        state = 3;
                    } else {
                        state = 2;
                    }
                    break;

                case 2:
                    if (*p != '\0') {
                        state = 1;
                        cmd.argv[cmd.argc++] = p;
                    }
                    break;

                case 3:
                    if (*p == '\0') {
                        state = 2;
                    }
                    break;
            }
        }

        /* parse cmd */
        switch (cmd.argc) {
            case 1:
                /* 1 op cmd */
                break;
            case 2:
                /* 2 op cmd */
                break;
            case 3:
                /* 3 op cmd */
                if (strcasecmp("create_channel", cmd.argv[0]) == 0) {
                    /* create_channel [req-rep | pub-sub | push-pull] [tcp://xxx:xxx] */
                }
                break;
            default:
                mq_log_error("commond error\n");
                break;
        }
    }
}

int main(int argc, char *argv[])
{
    const char *manage_channel;

    // daemonize();

    /* TODO: read conf file and init log and manage channel */
    mq_log_init(MQ_LOG_DEBUG, "mq_debug.log");
    mq_log_init(MQ_LOG_INFO, "mq_info.log");
    mq_log_init(MQ_LOG_ERROR, "mq_error.log");

    mq_time_init();

    if (argc > 1) {
        manage_channel = argv[1];
    } else {
        manage_channel = "tcp://localhost:5555";
    }

    return mq_create_channel(MQ_CHANNEL_MANAGE, manage_channel);
}

