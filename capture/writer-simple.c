/******************************************************************************/
/* writer-simple.c  -- Simple Writer Plugin
 *
 * This writer just creates a file per packet thread and queues buffers
 * to be written to disk in a single output thread.
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

typedef struct molochsimple {
    struct molochsimple *simple_next, *simple_prev;
    int                  simple_count;
    char                *buf;
    uint64_t             pos;
    uint32_t             id;
    int                  fd;
    uint32_t             bufpos;
    int                  closing;
} MolochSimple_t;

static MolochSimple_t    simpleQ;
static MOLOCH_LOCK_DEFINE(simpleQ);
static MOLOCH_COND_DEFINE(simpleQ);

LOCAL MolochSimple_t        *currentInfo[MOLOCH_MAX_PACKET_THREADS];
LOCAL int                    pageSize;

/******************************************************************************/
uint32_t writer_simple_queue_length()
{
    return DLL_COUNT(simple_, &simpleQ);
}
/******************************************************************************/
MolochSimple_t *writer_simple_alloc(MolochSimple_t *previous)
{
    MolochSimple_t *info;

    info = MOLOCH_TYPE_ALLOC0(MolochSimple_t);
    info->buf = mmap (0, config.pcapWriteSize + MOLOCH_PACKET_MAX_LEN, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
    if (previous) {
        info->fd = previous->fd;
        info->pos = previous->pos;
        info->id = previous->id;
    }
    return info;
}
/******************************************************************************/
void writer_simple_free(MolochSimple_t *info)
{
    munmap(info->buf, config.pcapWriteSize + MOLOCH_PACKET_MAX_LEN);
    MOLOCH_TYPE_FREE(MolochSimple_t, info);
}

/******************************************************************************/
void writer_simple_process_buf(int thread, int closing)
{
    MolochSimple_t *info = currentInfo[thread];

    info->closing = closing;
    if (!closing) {
        currentInfo[thread] = writer_simple_alloc(info);
        memcpy(currentInfo[thread]->buf, info->buf + config.pcapWriteSize, info->bufpos - config.pcapWriteSize);
        currentInfo[thread]->bufpos = info->bufpos - config.pcapWriteSize;
    } else {
        currentInfo[thread] = NULL;
    }
    MOLOCH_LOCK(simpleQ);
    DLL_PUSH_TAIL(simple_, &simpleQ, info);
    MOLOCH_COND_SIGNAL(simpleQ);
    MOLOCH_UNLOCK(simpleQ);
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

    if (!currentInfo[thread]) {
        currentInfo[thread] = writer_simple_alloc(NULL);
        char *name = moloch_db_create_file(packet->ts.tv_sec, NULL, 0, 0, &currentInfo[thread]->id);
        int options = O_NOATIME | O_WRONLY | O_CREAT | O_TRUNC;
#ifdef O_DIRECT
        options |= O_DIRECT;
#else
        LOG("No O_DIRECT");
#endif
        currentInfo[thread]->fd = open(name,  options, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        if (currentInfo[thread]->fd < 0) {
            LOG("ERROR - pcap open failed - Couldn't open file: '%s' with %s  (%d)", name, strerror(errno), errno);
            exit(2);
        }
        currentInfo[thread]->pos = currentInfo[thread]->bufpos = 24;
        memcpy(currentInfo[thread]->buf, &pcapFileHeader, 24);
        if (config.debug)
            LOG("opened %d %s %d", thread, name, currentInfo[thread]->fd);
        g_free(name);
    }

    packet->writerFileNum = currentInfo[thread]->id;
    packet->writerFilePos = currentInfo[thread]->pos;

    struct pcap_sf_pkthdr hdr;

    hdr.ts.tv_sec  = packet->ts.tv_sec;
    hdr.ts.tv_usec = packet->ts.tv_usec;
    hdr.caplen     = packet->pktlen;
    hdr.pktlen     = packet->pktlen;

    memcpy(currentInfo[thread]->buf+currentInfo[thread]->bufpos, &hdr, 16);
    currentInfo[thread]->bufpos += 16;
    memcpy(currentInfo[thread]->buf+currentInfo[thread]->bufpos, packet->pkt, packet->pktlen);
    currentInfo[thread]->bufpos += packet->pktlen;
    currentInfo[thread]->pos += 16 + packet->pktlen;

    if (currentInfo[thread]->bufpos > config.pcapWriteSize) {
        writer_simple_process_buf(thread, 0);
    } else if (currentInfo[thread]->pos >= config.maxFileSizeB) {
        writer_simple_process_buf(thread, 1);
    }
}
/******************************************************************************/
void *writer_simple_thread(void *UNUSED(arg))
{
    MolochSimple_t *info;

    LOG("THREAD %p", (gpointer)pthread_self());
    while (1) {
        MOLOCH_LOCK(simpleQ);
        while (DLL_COUNT(simple_, &simpleQ) == 0) {
            MOLOCH_COND_WAIT(simpleQ);
        }
        DLL_POP_HEAD(simple_, &simpleQ, info);
        MOLOCH_UNLOCK(simpleQ);

        uint32_t pos = 0;
        uint32_t total;
        if (info->closing) {
            total = info->bufpos;
            if (total % pageSize != 0) {
                total = (total - (total % pageSize) + pageSize);
            }
        } else {
            total = config.pcapWriteSize;
        }

        while (pos < total) {
            int len = write(info->fd, info->buf + pos, total - pos);
            if (len >= 0) {
                pos += len;
            } else {
                LOG("ERROR writing - %d %s", len, strerror(errno));
                exit(0);
            }
        }
        if (info->closing) {
            ftruncate(info->fd, info->pos);
            close(info->fd);
        }

        writer_simple_free(info);
    }
    return NULL;
}
/******************************************************************************/
void writer_simple_exit()
{
    int thread;

    for (thread = 0; thread < config.packetThreads; thread++) {
        if (currentInfo[thread]) {
            writer_simple_process_buf(thread, 1);
        }
    }

    // Pause the main thread until our thread finishes
    while (writer_simple_queue_length() > 0) {
        usleep(10000);
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

    DLL_INIT(simple_, &simpleQ);
    g_thread_new("moloch-simple", &writer_simple_thread, NULL);
}
