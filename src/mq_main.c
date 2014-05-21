#include "mq_channel.h"
#include "mq_process.h"
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <zmq.h>

#define MQ_CMDSIZE 256

#define MQ_CMDMAXARG 8
struct mq_command_t {
    int    argc;
    void  *argv[MQ_CMDMAXARG];
};

static int mq_sigchld = 0;

void *mq_manage_ctx;
void *mq_manage_sock;

static void mq_sigchld_handler(int signum)
{
    if (signum == SIGCHLD) {
        mq_sigchld = 1;
    }
}

static int mq_get_input_type(const char *in)
{
    if (strcasecmp(in, "PUB") == 0) {
        return ZMQ_SUB;
    } else if (strcasecmp(in, "PUSH") == 0) {
        return ZMQ_PULL;
    } else if (strcasecmp(in, "REQ") == 0) {
        return ZMQ_REP;
    } else {
        return -1;
    }
}

static int mq_get_output_type(const char *out)
{
    if (strcasecmp(out, "SUB") == 0) {
        return ZMQ_PUB;
    } else if (strcasecmp(out, "PULL") == 0) {
        return ZMQ_PUSH;
    } else if (strcasecmp(out, "REP") == 0) {
        return ZMQ_REQ;
    } else {
        return -1;
    }
}

static void mq_get_cmd(struct mq_command_t *cmd, char *command)
{
    char  *p;
    int    state;

    for (cmd->argc = 0, p = command, state = 0; 
            cmd->argc < MQ_CMDMAXARG && *p != '\0'; 
            ++p)
    {
        if (*p == ' ' || *p == '\t' || *p == '\n') {
            *p = '\0';
        }
        switch (state) {
            case 0:
                if (*p != '\0') {
                    state = 1;
                    cmd->argv[cmd->argc++] = p;
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
                    cmd->argv[cmd->argc++] = p;
                }
                break;

            case 3:
                if (*p == '\0') {
                    state = 2;
                }
                break;
        }
    }
}

static void mq_execute_cmd(struct mq_command_t *cmd)
{
    int input_type, output_type;

    switch (cmd->argc) {
        case 1:
            /* 1 op cmd */
            if (strcasecmp("info", cmd->argv[0]) == 0) {
                mq_get_process_info(mq_manage_sock);
            }
            break;

        case 5:
            /* destroy input xxx://xxx:xxxx output xxx://xxx:xxxx */
            if (strcasecmp("destroy", cmd->argv[0]) == 0
                    && strcasecmp("input", cmd->argv[1]) == 0
                    && strcasecmp("output", cmd->argv[3]) == 0)
            {
                mq_kill_process(cmd->argv[2], cmd->argv[4]);

            } else {
                mq_log_error("illegal cmd: %s %s %s %s %s\n", 
                        cmd->argv[0], cmd->argv[1], cmd->argv[2],
                        cmd->argv[3], cmd->argv[4]);
            }
            break;

        case 7:
            /* create input input_type xxx://xxx:xxxx output output_type xxx://xxx:xxxx */
            if (strcasecmp("create", cmd->argv[0]) == 0
                    && strcasecmp("input", cmd->argv[1]) == 0
                    && strcasecmp("output", cmd->argv[4]) == 0)
            {
                input_type = mq_get_input_type(cmd->argv[2]);
                output_type = mq_get_output_type(cmd->argv[5]);
                if (input_type == -1 || output_type == -1) {
                    mq_log_error("error cmd IO type: input: %s, output: %s\n",
                            cmd->argv[2], cmd->argv[5]);
                    break;
                }
                mq_spawn_process(input_type, cmd->argv[3], output_type, cmd->argv[6]);

            }
            break;

        default:
            mq_log_error("command error\n");
            break;
    }
}

int mq_main(void)
{
    int   nrecv, i;
    char  command[MQ_CMDSIZE];

    struct mq_command_t cmd;

    signal(SIGCHLD, mq_sigchld_handler);

    while (1) {
        memset(command, '\0', sizeof command);
        nrecv = zmq_recv(mq_manage_sock, command, MQ_CMDSIZE, 0);
        mq_time_update();
        if (nrecv == -1 && errno == EINTR) {
            if (mq_sigchld == 1) {
                mq_sigchld = 0;
                mq_update_children_process_info();
            }
            continue;
        }
#ifdef MQ_DEBUG
        mq_log_debug("nrecv: %d, errno: %d\n", nrecv, errno);
#endif

        if (nrecv >= MQ_CMDSIZE) {
            command[MQ_CMDSIZE - 1] = '\0';
            /* REQ-REP model need to response */
            zmq_send(mq_manage_sock, "", 0, 0);
            mq_log_error("Command too long: %s\n", command);
            continue;
        }

        /* get cmd */
#ifdef MQ_DEBUG
        mq_log_debug("get cmd: %s\n", command);
#endif
        mq_get_cmd(&cmd, command);

        mq_execute_cmd(&cmd);


        /* REQ-REP model need to response */
        zmq_send(mq_manage_sock, "", 0, 0);
        errno = 0;
    }
}

void daemonize(void)
{
    int               i;
    pid_t             pid;
    struct rlimit     rl;
    struct sigaction  sa;

    umask(0);

    switch (pid = fork()) {
        case -1:
            exit(1);
        case 0:  /* child */
            break;
        default:  /* father */
            exit(0);
    }

    setsid();

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        exit(2);
    }

    switch (pid = fork()) {
        case -1:
            exit(3);
        case 0:  /* child */
            break;
        default:  /* father */
            exit(0);
    }

    //if (chdir("/") < 0) {
    //    exit(4);
    //}

    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        exit(5);
    }
    if (rl.rlim_max == RLIM_INFINITY) {
        rl.rlim_max = 8192;
    }
    for (i = 0; i < rl.rlim_max; ++i) {
        close(i);
    }
    
    if (open("/dev/null", O_RDWR) != 0) {
        exit(6);
    }
    if (dup(0) != 1) {
        exit(7);
    }
    if (dup(0) != 2) {
        exit(8);
    }
}

int main(int argc, char *argv[])
{

    const char *manage_channel;

    daemonize();

    /* TODO: read conf file and init log and manage channel */
    if (mq_log_init(MQ_LOG_DEBUG, "mq_debug.log") < 0) {
        exit(9);
    }
    mq_log_init(MQ_LOG_INFO, "mq_info.log");
    mq_log_init(MQ_LOG_ERROR, "mq_error.log");

    mq_time_init();
    
    if (argc > 1) {
        manage_channel = argv[1];
    } else {
        //manage_channel = "tcp://localhost:5555";
        manage_channel = "tcp://*:5555";
    }

    mq_create_manage_channel(manage_channel);
    return 0;
}

