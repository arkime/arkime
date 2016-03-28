/******************************************************************************/
/* writer-simple.c  -- Simple Writer Plugin
 *
 * This writer just creates a file per thread and writes to it, no locks
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define _FILE_OFFSET_BITS 64
#include "moloch.h"
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

#ifndef O_NOATIME
#define O_NOATIME 0
#endif

extern MolochConfig_t        config;
extern MolochPcapFileHdr_t   pcapFileHeader;

typedef struct  {
    char      *buf;
    pthread_t  self;
    char      *name;
    uint64_t   pos;
    uint32_t   id;
    int        fd;
    uint32_t   bufpos;
} MolochSimple_t;

LOCAL MolochSimple_t         info[MOLOCH_MAX_PACKET_THREADS];
LOCAL int                    pageSize;

/******************************************************************************/
uint32_t writer_simple_queue_length()
{
    return 0;
}
/******************************************************************************/
/* If closing is true the buffer can be any size, if false then has to be pcapWriteSize */
void writer_simple_write_buf(int thread, int closing)
{
    uint32_t pos = 0;
    uint32_t total;
    if (closing) {
        total = info[thread].bufpos;
        if (total % pageSize != 0) {
            total = (total - (total % pageSize) + pageSize);
        }
    } else {
        total = config.pcapWriteSize;
    }

    while (pos < total) {
        int len = write(info[thread].fd, info[thread].buf + pos, total-pos);
        if (len >= 0) {
            pos += len;
        } else {
            LOG("ERROR writing - %d %s", len, strerror(errno));
        }
    }
    if (closing) {
        ftruncate(info[thread].fd, info[thread].pos);
        info[thread].bufpos = 0;
    } else {
        memmove(info[thread].buf, info[thread].buf + total, info[thread].bufpos - total);
        info[thread].bufpos = info[thread].bufpos - total;
    }
}
/******************************************************************************/
void writer_simple_exit()
{
    int thread;

    for (thread = 0; thread < config.packetThreads; thread++) {
        if (info[thread].fd) {
            writer_simple_write_buf(thread, 1);
            close(info[thread].fd);
            info[thread].fd = 0;
            g_free(info[thread].name);
        }
        munmap(info[thread].buf, config.pcapWriteSize + MOLOCH_PACKET_MAX_LEN);
    }
}
/******************************************************************************/
struct pcap_timeval {
    int32_t tv_sec;		/* seconds */
    int32_t tv_usec;		/* microseconds */
};
struct pcap_sf_pkthdr {
    struct pcap_timeval ts;	/* time stamp */
    uint32_t caplen;		/* length of portion present */
    uint32_t pktlen;		/* length this packet (off wire) */
};
void writer_simple_write(const MolochSession_t * const session, MolochPacket_t * const packet)
{
    int thread = session->thread;

    if (info[thread].self == 0)
        info[thread].self = pthread_self();

    if (info[thread].self != pthread_self()) {
        LOG("ERROR - Writing on wrong thread");
        exit(3);
    }

    if (!info[thread].fd) {
        info[thread].name = moloch_db_create_file(packet->ts.tv_sec, NULL, 0, 0, &info[thread].id);
        int options = O_NOATIME | O_WRONLY | O_CREAT | O_TRUNC;
#ifdef O_DIRECT
        options |= O_DIRECT;
#endif
        info[thread].fd = open(info[thread].name,  options, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        if (info[thread].fd < 0) {
            LOG("ERROR - pcap open failed - Couldn't open file: '%s' with %s  (%d)", info[thread].name, strerror(errno), errno);
            exit(2);
        }
        info[thread].pos = info[thread].bufpos = 24;
        memcpy(info[thread].buf, &pcapFileHeader, 24);
        if (config.debug)
            LOG("opened %d %s %d", thread, info[thread].name, info[thread].fd);
    }

    packet->writerFileNum = info[thread].id;
    packet->writerFilePos = info[thread].pos;

    struct pcap_sf_pkthdr hdr;

    hdr.ts.tv_sec  = packet->ts.tv_sec;
    hdr.ts.tv_usec = packet->ts.tv_usec;
    hdr.caplen     = packet->pktlen;
    hdr.pktlen     = packet->pktlen;

    memcpy(info[thread].buf+info[thread].bufpos, &hdr, 16);
    info[thread].bufpos += 16;
    memcpy(info[thread].buf+info[thread].bufpos, packet->pkt, packet->pktlen);
    info[thread].bufpos += packet->pktlen;
    info[thread].pos += 16 + packet->pktlen;

    if (info[thread].bufpos > config.pcapWriteSize) {
        writer_simple_write_buf(thread, 0);
    }

    if (info[thread].pos >= config.maxFileSizeB) {
        writer_simple_write_buf(thread, 1);
        close(info[thread].fd);
        info[thread].fd = 0;
        g_free(info[thread].name);
    }
}
/******************************************************************************/
void writer_simple_init(char *UNUSED(name))
{
    moloch_writer_queue_length = writer_simple_queue_length;
    moloch_writer_exit         = writer_simple_exit;
    moloch_writer_write        = writer_simple_write;

    pageSize = getpagesize();
    if (config.pcapWriteSize % pageSize != 0) {
        config.pcapWriteSize = ((config.pcapWriteSize + pageSize - 1) / pageSize) * pageSize;
        LOG ("INFO: Reseting pcapWriteSize to %u since it must be a multiple of %u", config.pcapWriteSize, pageSize);
    }

    int thread;
    for (thread = 0; thread < config.packetThreads; thread++) {
        info[thread].buf = mmap (0, config.pcapWriteSize + MOLOCH_PACKET_MAX_LEN, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
    }

}
