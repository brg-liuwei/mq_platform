#include "mq_io.h"
#include "mq_log.h"

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#define MQ_FILE_FMT "/tmp/mq-%d-%u.msg"

static off_t mq_write_off;
static uint32_t mq_write_seq;
static uint32_t mq_read_seq;
static int mq_write_fd = -1;
static int mq_read_fd = -1;

static uint64_t mq_write_records = 0;
static uint64_t mq_read_records = 0;

static int mq_open_write_file()
{
    char path[256];
    close(mq_write_fd);
    ++mq_write_seq;
    snprintf(path, sizeof path, MQ_FILE_FMT, getpid(), mq_write_seq);
    mq_write_fd = open(path, O_CREAT | O_APPEND | O_WRONLY, 0666);
    if (mq_write_fd == -1) {
        mq_log_error("open path %s errno: %d, errmsg: %s\n",
                path, errno, strerror(errno));
        exit(21);
    }
    mq_write_off = (off_t)0;
    return mq_write_fd;
}

static int mq_open_read_file()
{
    char path[256];

    close(mq_read_fd);
    ++mq_read_seq;
    snprintf(path, sizeof path, MQ_FILE_FMT, getpid(), mq_read_seq);
    mq_read_fd = open(path, O_RDONLY);
    if (mq_read_fd == -1) {
        mq_log_error("open path %s errno: %d, errmsg: %s\n",
                path, errno, strerror(errno));
        exit(20);
    }
    unlink(path);
    return mq_read_fd;
}

int mq_io_init()
{
    mq_open_write_file();
    mq_open_read_file();
    return 0;
}

void mq_write_msg(const char *buf, size_t buf_size)
{
    int rc;
    mq_msg_hdr_t  hdr;

    hdr.pos = mq_write_off;
    hdr.msg_size = buf_size;

    rc = write(mq_write_fd, &hdr, sizeof hdr);
    if (rc == -1) {
        mq_log_error("FATAL: write msg hdr errno: %d, errmsg: %s\n",
                errno, strerror(errno));
        return;
    }
    rc = write(mq_write_fd, buf, buf_size);
    if (rc == -1) {
        mq_log_error("write msg body errno: %d, errmsg: %s\n",
                errno, strerror(errno));
        exit(22);
    }
    ++mq_write_records;

    mq_write_off += sizeof(hdr) + buf_size;

    if (mq_write_off > (64L * 1024L * 1024L)) {
        /* 4 MB */
        hdr.pos = -1;
        hdr.msg_size = 0;

        /* write end msg */
        ++mq_write_records;
        write(mq_write_fd, &hdr, sizeof hdr);

        mq_open_write_file();
    }
}

/*  some bugs here: SIGBUS */

//mq_msg_hdr_t *mq_read_msg()
//{
//    static void          *addr = NULL;
//    static void          *end_addr;
//    static mq_msg_hdr_t  *phdr;
//    static mq_msg_hdr_t   fake = {0, 0, };
//    static int            pg_size = 0;
//    static size_t         mm_size;
//    off_t                 next_off, map_off;
//
//    if (mq_read_records >= mq_write_records) {
//        return NULL;
//    }
//
//    if (pg_size == 0) {
//        pg_size = getpagesize();
//        mm_size = 32L * pg_size;
//    }
//
//    ++mq_read_records;
//
//    if (addr == NULL) {
//
//        addr = mmap(NULL, mm_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, mq_read_fd, 0);
//        end_addr = addr +  mm_size;
//
//        if (addr == MAP_FAILED) {
//            mq_log_error("mmap fail, errno: %d, errmsg: %s\n",
//                    errno, strerror(errno));
//            exit(23);
//        }
//
//        phdr = addr;
//        if ((void *)phdr + sizeof(mq_msg_hdr_t) + phdr->msg_size > end_addr) {
//            mq_log_error("msg too long\n");
//            return &fake;
//        }
//
//        goto check_file_end;
//
//    }
//
//    /* addr to return */
//    next_off = phdr->pos + sizeof(mq_msg_hdr_t) + phdr->msg_size;
//    phdr = (void *)phdr + sizeof(mq_msg_hdr_t) + phdr->msg_size;
//
//    if ((void *)phdr + sizeof(mq_msg_hdr_t) >= end_addr
//            || (void *)phdr + sizeof(mq_msg_hdr_t) + phdr->msg_size >= end_addr)
//    {
//
//        munmap(addr, mm_size);
//
//        map_off = next_off / pg_size * pg_size;
//
//        /* re-mmap */
//        addr = mmap(NULL, mm_size, PROT_READ, MAP_PRIVATE, mq_read_fd, map_off);
//        end_addr = addr +  mm_size;
//
//        if (addr == MAP_FAILED) {
//            mq_log_error("mmap fail, errno: %d, errmsg: %s\n",
//                    errno, strerror(errno));
//            exit(23);
//        }
//
//        phdr = addr + next_off - map_off;
//    }
//
//check_file_end:
//
//    if (phdr->pos == -1 && phdr->msg_size == 0) {
//        /* end of file */
//        munmap(addr, mm_size);
//        addr = NULL;
//        mq_open_read_file();
//
//        return mq_read_msg();
//    }
//
//    return phdr;
//}

mq_msg_hdr_t *mq_read_msg()
{
    mq_msg_hdr_t   hdr;
    static mq_msg_hdr_t  *p = NULL;

    if (p != NULL) {
        free(p);
        p = NULL;
    }

    if (mq_read_records >= mq_write_records) {
        return NULL;
    }
    ++mq_read_records;
    read(mq_read_fd, &hdr, sizeof hdr);

    if (hdr.pos == -1 && hdr.msg_size == 0) {
        /* end of file */
        mq_open_read_file();
        return mq_read_msg();
    }

    p = (mq_msg_hdr_t *)calloc(1, sizeof(mq_msg_hdr_t) + hdr.msg_size + 1);
    memcpy(p, &hdr, sizeof hdr);
    read(mq_read_fd, (void *)p + sizeof(hdr), hdr.msg_size);
    return p;
}





