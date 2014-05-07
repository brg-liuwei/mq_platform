#include "mq_process.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <zmq.h>

#define MQ_MAXPROCESS 128

extern void *mq_manage_ctx;
extern void *mq_manage_sock;

static int mq_process_n = 0;
static mq_proc_t mq_process[MQ_MAXPROCESS];

static const char mq_ztype[][16] = {
    "ZMQ_PAIR", "ZMQ_PUB", "ZMQ_SUB",
    "ZMQ_REQ", "ZMQ_REP", "ZMQ_DEALER",
    "ZMQ_ROUTER", "ZMQ_PULL", "ZMQ_PUSH",
    "ZMQ_XPUB", "ZMQ_XSUB", "ZMQ_STREAM"
};

void mq_spawn_process(int input_zmq_type, const char *input_ch,
        int output_zmq_type, const char *output_ch)
{
    int    rc;
    char   ipc_path[256];
    void  *ctx, *sock;
    void  *input_sock, *output_sock;
    pid_t  pid;

    if (input_ch == NULL || output_ch == NULL) {
        mq_log_error("channel addr NULL\n");
        return;
    }
    if (mq_process_n == MQ_MAXPROCESS) {
        mq_log_error("cannot create channel any more\n");
        return;
    }

    pid = fork();
    switch (pid) {
        case -1:  /* error */
            mq_log_error("fork errno: %d\n", errno);
            return;

        case 0:  /* child */

            zmq_close(mq_manage_sock);
            zmq_term(mq_manage_ctx);

            /* wait for parent update mq_process array */
            sleep(1);

            /* create controller */
            ctx = zmq_ctx_new();
            sock = zmq_socket(ctx, ZMQ_REP);
            pid = getpid();
            snprintf(ipc_path, 255, "ipc:///var/tmp/mq_platform.controller.%d", pid);
            if (zmq_connect(sock, ipc_path) == -1) {
                mq_log_error("[%d] zmq_connect ipc_path: %s failed, errno: %d\n",
                        pid, ipc_path, errno);
                exit(1);
            }

            /* create the server channel */
            input_sock = mq_create_channel(input_zmq_type, input_ch);
            output_sock = mq_create_channel(output_zmq_type, output_ch);

            if (input_sock == NULL || output_sock == NULL) {
                mq_log_error("[%d] create channel error,"
                        "input type: %d, input: %s,"
                        "output type: %d, output: %s\n",
                        pid, input_zmq_type, input_ch,
                        output_zmq_type, output_ch);
                exit(2);
            }
            rc = mq_channel_poll(input_sock, output_sock, sock);
            mq_log_error("[%d] mq_channel_poll return %d\n", rc);
            exit(rc);

        default:  /* parent */
            ctx = zmq_ctx_new();
            sock = zmq_socket(ctx, ZMQ_REQ);
            snprintf(ipc_path, 255, "ipc:///var/tmp/mq_platform.controller.%d", pid);
            if (zmq_bind(sock, ipc_path) == -1) {
                mq_log_error("zmq_bind ipc_path: %s failed, errno: %d\n",
                        ipc_path, errno);
                return;
            }
            mq_process[mq_process_n].pid = pid;
#ifdef MQ_DEBUG
            mq_log_debug("master [%d] set mq_process[%d].pid = %d, path = %s\n",
                    getpid(), mq_process_n, pid, ipc_path);
#endif
            mq_process[mq_process_n].input_type= input_zmq_type;
            mq_process[mq_process_n].output_type= output_zmq_type;
            strncpy(mq_process[mq_process_n].input_addr, input_ch, MQ_CHANNELSIZE - 1);
            strncpy(mq_process[mq_process_n].output_addr, output_ch, MQ_CHANNELSIZE - 1);
            mq_process[mq_process_n].controller = sock;
            mq_process[mq_process_n].zctx = ctx;
            ++mq_process_n;
            break;
    }
}

void mq_kill_process(const char *input_ch, const char *output_ch)
{
    int  i;

    for (i = 0; i != mq_process_n; ++i) {
        if (strcasecmp(mq_process[i].input_addr, input_ch) == 0
                && strcasecmp(mq_process[i].output_addr, output_ch) == 0)
        {
            kill(mq_process[i].pid, SIGKILL);
            mq_update_children_process_info();
        }
    }
}

void mq_update_children_process_info()
{
    int    status, i;
    pid_t  dead_child;

    for ( ; ; ) {
        if ((dead_child = waitpid(-1, &status, WNOHANG)) <= 0) {
            break;
        }

        if (WIFEXITED(status)) {
            /* child terminated normally by exit or _exit */
            mq_log_error("child terminated by exit, exit number: %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            /* child terminated by a signal */
            if (WTERMSIG(status) == SIGKILL) {
                mq_log_debug("child process [%d] was killed by master\n", dead_child);
            } else {
                mq_log_error("child process [%d] terminated by signal %d\n",
                        dead_child, WTERMSIG(status));
            }
        } else {
            /* cannot goto here! */
            mq_log_error("child process [%d] was STOPPED, status: %d\n", dead_child, status);
        }

        for (i = 0; i != mq_process_n; ++i) {
            if (mq_process[i].pid == dead_child) {
                zmq_close(mq_process[i].controller);
                zmq_term(mq_process[i].zctx);
                unlink(mq_process[i].input_addr);
                unlink(mq_process[i].output_addr);

                if (i != mq_process_n - 1) {
                    mq_process[i].pid = mq_process[mq_process_n-1].pid;
                    mq_process[i].input_type = mq_process[mq_process_n-1].input_type;
                    strncpy(mq_process[i].input_addr, 
                            mq_process[mq_process_n-1].input_addr, MQ_CHANNELSIZE);
                    mq_process[i].output_type = mq_process[mq_process_n-1].output_type;
                    strncpy(mq_process[i].output_addr, 
                            mq_process[mq_process_n-1].output_addr, MQ_CHANNELSIZE);
                    mq_process[i].controller = mq_process[mq_process_n-1].controller;
                    mq_process[i].zctx = mq_process[mq_process_n-1].zctx;
                }
                --mq_process_n;
                break;
            }
        }
    }
}

void mq_get_process_info(void *rep)
{
    int   i, nsnd;
    char  msg[4096];

    mq_log_debug("==========\nmq_get_process_info: n = %d\n", mq_process_n);
    zmq_send(rep, "mq process info:\n", sizeof("mq process info:\n"), ZMQ_SNDMORE);
    for (i = 0; i != mq_process_n; ++i) {
        memset(msg, '\0', 4096);
        nsnd = snprintf(msg, 4096, "==== [%d] ==== \npid: %d\n"
                "input_type: %s\ninput_addr: %s\n"
                "output_type: %s\noutput_addr: %s\n",
                i + 1, mq_process[i].pid,
                mq_ztype[mq_process[i].input_type],
                mq_process[i].input_addr, 
                mq_ztype[mq_process[i].output_type],
                mq_process[i].output_addr);
        mq_log_debug("msg = %s\n", msg);
        zmq_send(rep, msg, nsnd, ZMQ_SNDMORE);
    }
    //zmq_send(rep, "", 1, 0);
}

