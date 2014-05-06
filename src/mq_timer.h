
#ifndef __MQ_TIME_H__
#define __MQ_TIME_H__

#include <pthread.h>
#include <sys/time.h>


void mq_time_init();
void mq_time_update();
time_t mq_cur_sec();
time_t mq_cur_msec();
time_t mq_current();
const char *mq_cur_time_str();

#endif
