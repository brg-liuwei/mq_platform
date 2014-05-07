#ifndef __MQ_PROCESS_H__
#define __MQ_PROCESS_H__

#include "mq_channel.h"
#include <unistd.h>
#include <errno.h>

#define MQ_CHANNELSIZE 128

typedef struct mq_proc_s {
    pid_t     pid;
    int       input_type;
    char      input_addr[MQ_CHANNELSIZE];
    int       output_type;
    char      output_addr[MQ_CHANNELSIZE];
    void     *controller;
    void     *zctx;
} mq_proc_t;

void mq_update_children_process_info();
void mq_get_process_info(void *zsock);
void mq_spawn_process(int input_zmq_type, const char *input_ch,
        int output_zmq_type, const char *output_ch);
void mq_kill_process(const char *input_ch, const char *output_ch);

#endif
