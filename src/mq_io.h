#ifndef __MQ_IO_H__
#define __MQ_IO_H__

#include <stdint.h>
#include <sys/types.h>

typedef struct mq_msg_hdr_s mq_msg_hdr_t;

#pragma pack(push)
#pragma pack(1)
struct mq_msg_hdr_s {
    off_t   pos;
    size_t  msg_size;
    char    msg[];
};
#pragma pack(pop)

int mq_io_init();
void mq_write_msg(const char *buf, size_t buf_size);
mq_msg_hdr_t *mq_read_msg();

#endif
