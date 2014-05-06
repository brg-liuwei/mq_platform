#include "mq_log.h"
#include "mq_timer.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define MQLOGMAXLINE 4096

static int mq_debug_log_fd;
static int mq_info_log_fd;
static int mq_err_log_fd;

int mq_log_init(mq_log_type_t type, const char *path)
{
    int *p;

    assert(path != NULL);

    switch (type) {
        case MQ_LOG_DEBUG:
            p = &mq_debug_log_fd;
            break;
        case MQ_LOG_INFO:
            p = &mq_info_log_fd;
            break;
        case MQ_LOG_ERROR:
            p = &mq_err_log_fd;
            break;
        default:
            printf("%s:%d init log type error, type = %d\n",
                    __FILE__, __LINE__, type);
            return -1;
    }
    *p = open(path, O_APPEND | O_CREAT | O_WRONLY, 0644);
    return *p;
}

void __mq_log_fmt(mq_log_type_t type, const char *file, size_t line, const char *fmt, ...)
{
    int      n;
    int      log_fd;
    char     content[MQLOGMAXLINE];
    va_list  ap;

    if (type == MQ_LOG_ERROR) {
        n = snprintf(content, MQLOGMAXLINE, "%s%s:%ld ", mq_cur_time_str(), file, line);
    } else {
        n = snprintf(content, MQLOGMAXLINE, "%s", mq_cur_time_str());
    }

    va_start(ap, fmt);
    n += vsnprintf(content + n, MQLOGMAXLINE - n, fmt, ap);
    va_end(ap);

    switch (type) {
        case MQ_LOG_DEBUG:
            log_fd = mq_debug_log_fd;
            break;
        case MQ_LOG_INFO:
            log_fd = mq_info_log_fd;
            break;
        case MQ_LOG_ERROR:
            log_fd = mq_err_log_fd;
            break;
        default:
            __mq_log_fmt(MQ_LOG_ERROR, __FILE__, __LINE__, "error log type = %d\n", type);
            abort();
    }
    write(log_fd, content, n);
}

