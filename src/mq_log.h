#ifndef __MQ_LOGGER_H__
#define __MQ_LOGGER_H__

#include <stdarg.h>
#include <sys/types.h>

typedef enum {
    MQ_LOG_DEBUG,
    MQ_LOG_INFO,
    MQ_LOG_ERROR,
} mq_log_type_t;

int mq_log_init(mq_log_type_t type, const char *path);
void __mq_log_fmt(mq_log_type_t type, const char *file, size_t line, const char *fmt, ...);

#define mq_log_debug(fmt, ...) __mq_log_fmt(MQ_LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define mq_log_info(fmt, ...) __mq_log_fmt(MQ_LOG_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define mq_log_error(fmt, ...) __mq_log_fmt(MQ_LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif
