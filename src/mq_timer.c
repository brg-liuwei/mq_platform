#include "mq_timer.h"
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#define MQ_TIME_SLOTS 64
#define MQ_TIME_SIZE sizeof("2011-11-11 11:11:11 111 ")
#define MQ_TIME_SIZE_PART sizeof("2011-11-11 11:11:11")

struct mq_time_t {
    size_t   slot;
    time_t   sec;
    time_t   msec;
    time_t   cur;
};

static const char mq_time_fmt[] = "%04d-%02d-%02d %02d:%02d:%02d %03ld ";
static char mq_time_str[MQ_TIME_SLOTS][MQ_TIME_SIZE];

static volatile struct mq_time_t mq_time;
static pthread_mutex_t mq_time_mutex;
static void mq_time_update_signal(int signum)
{
    if (signum == SIGALRM) {
        mq_time_update();
        alarm(1);
    }
}

void mq_time_init()
{
    static int mq_time_initialized = 0;

    if (mq_time_initialized == 0) {
        pthread_mutex_init(&mq_time_mutex, NULL);
        mq_time_initialized = 1;
        mq_time_update();
        signal(SIGALRM, mq_time_update_signal);
        alarm(1);
    }
}

void mq_time_update()
{
    size_t          slot;
    time_t          msec;
    struct tm      *ptm;
    struct timeval  tv;

    if (pthread_mutex_trylock(&mq_time_mutex) != 0) {
        return;
    }

    gettimeofday(&tv, NULL);

    msec = (tv.tv_usec / 1000) % 1000;

    if (mq_time.sec == tv.tv_sec 
            && mq_time.msec == msec)
    {
        pthread_mutex_unlock(&mq_time_mutex);
        return;
    }

    slot = (mq_time.slot + 1) % MQ_TIME_SLOTS;

    if (mq_time.sec == tv.tv_sec) {
        memcpy(mq_time_str[slot], mq_time_str[mq_time.slot], MQ_TIME_SIZE_PART);
        sprintf(mq_time_str[slot] + MQ_TIME_SIZE_PART, "%03ld ", msec);
    } else {
        ptm = localtime(&tv.tv_sec);
        if (ptm == NULL) {
            /* TODO: log errno */
            pthread_mutex_unlock(&mq_time_mutex);
            return;
        }
        snprintf(mq_time_str[slot], MQ_TIME_SIZE, mq_time_fmt,
                ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
                ptm->tm_hour, ptm->tm_min, ptm->tm_sec, msec);
    }

    /* memory barrier */
    __sync_synchronize();

    mq_time.sec = tv.tv_sec;
    mq_time.msec = msec;
    mq_time.cur = mq_time.sec * 1000 + mq_time.msec;
    mq_time.slot = slot;

    pthread_mutex_unlock(&mq_time_mutex);
}

inline time_t mq_cur_sec()
{
    return mq_time.sec;
}

inline time_t mq_cur_msec()
{
    return mq_time.msec;
}

inline time_t mq_current()
{
    return mq_time.cur;
}

inline const char *mq_cur_time_str()
{
    return mq_time_str[mq_time.slot];
}

