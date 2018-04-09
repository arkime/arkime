/******************************************************************************/
/* writer-disk.c  -- Default pcap disk writer
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
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
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <gio/gio.h>

#ifndef O_NOATIME
#define O_NOATIME 0
#endif

extern MolochConfig_t        config;
extern MolochPcapFileHdr_t   pcapFileHeader;


typedef struct moloch_output {
    struct moloch_output *mo_next, *mo_prev;
    uint16_t   mo_count;

    char      *name;
    char      *buf;
    uint64_t   max;
    uint64_t   pos;
    char       close;
    uint32_t   fileId;
} MolochDiskOutput_t;


LOCAL  MolochDiskOutput_t   *output;
LOCAL  MOLOCH_LOCK_DEFINE(output);

LOCAL  MolochDiskOutput_t    outputQ;
LOCAL  MOLOCH_LOCK_DEFINE(outputQ);
LOCAL  MOLOCH_COND_DEFINE(outputQ);

LOCAL  MolochIntHead_t       freeOutputBufs;
LOCAL  MOLOCH_LOCK_DEFINE(freeOutputBufs);

LOCAL  uint32_t              outputId;
LOCAL  char                 *outputFileName;
LOCAL  uint64_t              outputFilePos = 0;
LOCAL  struct timespec       outputFileTime;

#define MOLOCH_WRITE_NORMAL 0x00
#define MOLOCH_WRITE_DIRECT 0x01
#define MOLOCH_WRITE_MMAP   0x02
#define MOLOCH_WRITE_THREAD 0x04

LOCAL  int                   writeMethod;
LOCAL  int                   pageSize;

/******************************************************************************/
LOCAL uint32_t writer_disk_queue_length_thread()
{
    int count = DLL_COUNT(mo_, &outputQ);
    return count;
}
/******************************************************************************/
LOCAL uint32_t writer_disk_queue_length_nothread()
{
    return DLL_COUNT(mo_, &outputQ);
}
/******************************************************************************/
LOCAL void writer_disk_alloc_buf(MolochDiskOutput_t *out)
{
    if (writeMethod & MOLOCH_WRITE_THREAD)
        MOLOCH_LOCK(freeOutputBufs);

    if (freeOutputBufs.i_count > 0) {
        MolochInt_t *tmp;
        DLL_POP_HEAD(i_, &freeOutputBufs, tmp);
        out->buf = (void*)tmp;
    } else {
        out->buf = mmap (0, config.pcapWriteSize + MOLOCH_PACKET_MAX_LEN, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
        if (unlikely(out->buf == MAP_FAILED)) {
            LOGEXIT("ERROR - MMap failure in disk_alloc_buf, %d: %s",errno, strerror(errno));
        }
    }

    if (writeMethod & MOLOCH_WRITE_THREAD)
        MOLOCH_UNLOCK(freeOutputBufs);
}
/******************************************************************************/
LOCAL void writer_disk_free_buf(MolochDiskOutput_t *out)
{
    if (writeMethod & MOLOCH_WRITE_THREAD)
        MOLOCH_LOCK(freeOutputBufs);

    if (freeOutputBufs.i_count > (int)config.maxFreeOutputBuffers) {
        munmap(out->buf, config.pcapWriteSize + MOLOCH_PACKET_MAX_LEN);
    } else {
        MolochInt_t *tmp = (MolochInt_t *)out->buf;
        DLL_PUSH_HEAD(i_, &freeOutputBufs, tmp);
    }
    out->buf = 0;

    if (writeMethod & MOLOCH_WRITE_THREAD)
        MOLOCH_UNLOCK(freeOutputBufs);
}
/******************************************************************************/
/* Only used in non thread mode to write out data
 */
LOCAL gboolean writer_disk_output_cb(gint fd, GIOCondition UNUSED(cond), gpointer UNUSED(data))
{
    if (fd && config.quitting)
        return FALSE;

    static int outputFd = 0;

    MolochDiskOutput_t *out = DLL_PEEK_HEAD(mo_, &outputQ);
    if (!out)
        return DLL_COUNT(mo_, &outputQ) > 0;

    if (!outputFd) {
        LOG("Opening %s", out->name);
        int options = O_NOATIME | O_WRONLY | O_NONBLOCK | O_CREAT | O_TRUNC;
#ifdef O_DIRECT
        if (writeMethod & MOLOCH_WRITE_DIRECT)
            options |= O_DIRECT;
#endif
        outputFd = open(out->name,  options, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        if (outputFd < 0) {
            LOG("ERROR - pcap open failed - Couldn't open file: '%s' with %s  (%d)", out->name, strerror(errno), errno);
            if (config.dropUser) {
                LOG("   Verify that user '%s' set by configuration variable dropUser can write and the parent directory exists", config.dropUser);
            }
            exit (2);
        }
    }

    int len;
    uint64_t filelen = 0;
    if (writeMethod == MOLOCH_WRITE_NORMAL) {
        len = write(outputFd, out->buf+out->pos, (out->max - out->pos));
        if (len < 0) {
            LOGEXIT("ERROR - Write %d failed with %d %d\n", outputFd, len, errno);
        }
    } else {
        int wlen = (out->max - out->pos);
        if (out->close && wlen % pageSize != 0) {
            filelen = lseek(outputFd, 0, SEEK_CUR) + wlen;
            wlen = (wlen - (wlen % pageSize) + pageSize);
        }
        len = write(outputFd, out->buf+out->pos, wlen);
        if (len < 0) {
            LOGEXIT("ERROR - Write %d failed with %d %d\n", outputFd, len, errno);
        }
    }

    out->pos += len;

    // Still more to write out
    if (out->pos < out->max) {
        return TRUE;
    }

    // The last write for this fd
    if (out->close) {
        if (filelen) {
            (void)ftruncate(outputFd, filelen);
        } else {
            filelen = lseek(outputFd, 0, SEEK_CUR);
        }
        close(outputFd);
        outputFd = 0;
        free(out->name);
        moloch_db_update_filesize(out->fileId, filelen);
    }

    // Cleanup buffer
    DLL_REMOVE(mo_, &outputQ, out);
    writer_disk_free_buf(out);
    MOLOCH_TYPE_FREE(MolochDiskOutput_t, out);

    // More waiting to write on different fd, setup a new watch
    if (outputFd && !config.quitting && DLL_COUNT(mo_, &outputQ) > 0) {
        moloch_watch_fd(outputFd, MOLOCH_GIO_WRITE_COND, writer_disk_output_cb, NULL);
        return FALSE;
    }

    return DLL_COUNT(mo_, &outputQ) > 0;
}
/******************************************************************************/
LOCAL void *writer_disk_output_thread(void *UNUSED(arg))
{
    if (config.debug)
        LOG("THREAD %p", (gpointer)pthread_self());

    MolochDiskOutput_t *out;
    int outputFd = 0;

    while (1) {
        uint64_t filelen = 0;
        MOLOCH_LOCK(outputQ);
        while (DLL_COUNT(mo_, &outputQ) == 0) {
            MOLOCH_COND_WAIT(outputQ);
        }
        DLL_POP_HEAD(mo_, &outputQ, out);
        MOLOCH_UNLOCK(outputQ);

        if (!outputFd) {
            LOG("Opening %s", out->name);
            int options = O_NOATIME | O_WRONLY | O_NONBLOCK | O_CREAT | O_TRUNC;
#ifdef O_DIRECT
            if (writeMethod & MOLOCH_WRITE_DIRECT)
                options |= O_DIRECT;
#endif
            outputFd = open(out->name,  options, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
            if (outputFd < 0) {
                LOGEXIT("ERROR - pcap open failed - Couldn't open file: '%s' with %s  (%d)", out->name, strerror(errno), errno);
            }
        }

        while (out->pos < out->max) {
            int wlen = out->max - out->pos;

            if (out->close && (writeMethod & MOLOCH_WRITE_DIRECT) && ((wlen % pageSize) != 0)) {
                filelen = lseek(outputFd, 0, SEEK_CUR) + wlen;
                wlen = (wlen - (wlen % pageSize) + pageSize);
            }

            int len = write(outputFd, out->buf+out->pos, wlen);
            out->pos += len;
            if (len < 0) {
                LOGEXIT("ERROR - Write %d failed with %d %d\n", outputFd, len, errno);
            }
        }

        if (out->close) {
            if (filelen) {
                (void)ftruncate(outputFd, filelen);
            } else {
                filelen = lseek(outputFd, 0, SEEK_CUR);
            }
            close(outputFd);
            outputFd = 0;
            free(out->name);
            moloch_db_update_filesize(out->fileId, filelen);
        }
        writer_disk_free_buf(out);
        MOLOCH_TYPE_FREE(MolochDiskOutput_t, out);
    }
}
/******************************************************************************/
LOCAL void writer_disk_flush(gboolean all)
{
    if (unlikely(config.dryRun || !output)) {
        return;
    }

    output->close = all;
    output->name  = outputFileName;

    MolochDiskOutput_t *noutput = MOLOCH_TYPE_ALLOC0(MolochDiskOutput_t);
    noutput->max = config.pcapWriteSize;
    noutput->fileId = output->fileId;
    writer_disk_alloc_buf(noutput);


    all |= (output->pos <= output->max);

    if (all) {
        output->max = output->pos;
    } else {
        noutput->pos = output->pos - output->max;
        memmove(noutput->buf, output->buf + output->max, noutput->pos);
    }
    output->pos = 0;

    int count;
    if (writeMethod & MOLOCH_WRITE_THREAD) {
        MOLOCH_LOCK(outputQ);
        DLL_PUSH_TAIL(mo_, &outputQ, output);
        count = DLL_COUNT(mo_, &outputQ);
        MOLOCH_COND_SIGNAL(outputQ);
        MOLOCH_UNLOCK(outputQ);
    } else {
        DLL_PUSH_TAIL(mo_, &outputQ, output);
        count = DLL_COUNT(mo_, &outputQ);

        if (count == 1) {
            writer_disk_output_cb(0,0,0);
        }
    }

    if (count >= 100 && count % 50 == 0) {
        LOG("WARNING - %d output buffers waiting, disk IO system too slow?", count);
    }

    output = noutput;
}
/******************************************************************************/
LOCAL void writer_disk_exit()
{
    writer_disk_flush(TRUE);
    outputFileName = 0;
    if (writeMethod & MOLOCH_WRITE_THREAD) {
        while (writer_disk_queue_length_thread() >0) {
            usleep(10000);
        }
    } else {
        // Write out all the buffers
        while (DLL_COUNT(mo_, &outputQ) > 0) {
            writer_disk_output_cb(0, 0, 0);
        }
    }
}
/******************************************************************************/
LOCAL void writer_disk_create(MolochPacket_t * const packet)
{
    outputFileName = moloch_db_create_file(packet->ts.tv_sec, NULL, 0, 0, &outputId);
    outputFilePos = 24;

    output = MOLOCH_TYPE_ALLOC0(MolochDiskOutput_t);
    output->max = config.pcapWriteSize;
    writer_disk_alloc_buf(output);
    output->pos = 24;
    clock_gettime(CLOCK_REALTIME_COARSE, &outputFileTime);

    output->fileId = outputId;
    memcpy(output->buf, &pcapFileHeader, 24);
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
LOCAL void writer_disk_write(const MolochSession_t * const UNUSED(session), MolochPacket_t * const packet)
{
    struct pcap_sf_pkthdr hdr;

    hdr.ts.tv_sec  = packet->ts.tv_sec;
    hdr.ts.tv_usec = packet->ts.tv_usec;
    hdr.caplen     = packet->pktlen;
    hdr.pktlen     = packet->pktlen;

    MOLOCH_LOCK(output);
    if (!outputFileName) {
        writer_disk_create(packet);
    }

    memcpy(output->buf + output->pos, (char *)&hdr, sizeof(hdr));
    output->pos += sizeof(hdr);

    memcpy(output->buf + output->pos, packet->pkt, packet->pktlen);
    output->pos += packet->pktlen;

    if(output->pos > output->max) {
        writer_disk_flush(FALSE);
    }
    packet->writerFileNum = outputId;
    packet->writerFilePos = outputFilePos;
    outputFilePos += 16 + packet->pktlen;

    if (outputFilePos >= config.maxFileSizeB) {
        writer_disk_flush(TRUE);
        outputFileName = 0;
    }
    MOLOCH_UNLOCK(output);
}
/******************************************************************************/
LOCAL gboolean writer_disk_file_time_gfunc (gpointer UNUSED(user_data))
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME_COARSE, &ts);

    if (outputFileName && outputFilePos > 24 && (ts.tv_sec - outputFileTime.tv_sec) >= config.maxFileTimeM*60) {
        writer_disk_flush(TRUE);
        outputFileName = 0;
    }

    return TRUE;
}

/******************************************************************************/
void writer_disk_init(char *name)
{
    LOG("WARNING - pcapWriteMethod of %s is depreciated, please switch to pcapWriteMethod=simple", name);
    if (strcmp(name, "normal") == 0)
        writeMethod = MOLOCH_WRITE_NORMAL;
    else if (strcmp(name, "direct") == 0)
        writeMethod = MOLOCH_WRITE_DIRECT;
    else if (strcmp(name, "thread") == 0)
        writeMethod = MOLOCH_WRITE_THREAD | MOLOCH_WRITE_NORMAL;
    else if (strcmp(name, "thread-direct") == 0)
        writeMethod = MOLOCH_WRITE_THREAD | MOLOCH_WRITE_DIRECT;
    else {
        printf("Unknown pcapWriteMethod '%s'\n", name);
        exit(1);
    }

#ifndef O_DIRECT
    if (writeMethod & MOLOCH_WRITE_DIRECT) {
        printf("OS doesn't support direct write method\n");
        exit(1);
    }
#endif

    if (writeMethod & MOLOCH_WRITE_THREAD) {
        g_thread_new("moloch-output", &writer_disk_output_thread, NULL);
    }

    if ((writeMethod & MOLOCH_WRITE_DIRECT) && sizeof(off_t) == 4 && config.maxFileSizeG > 2)
        printf("WARNING - DIRECT mode on 32bit machines may not work with maxFileSizeG > 2");

    pageSize = getpagesize();
    if (writeMethod & MOLOCH_WRITE_DIRECT && (config.pcapWriteSize % pageSize != 0)) {
        printf("When using pcapWriteMethod of direct pcapWriteSize must be a multiple of %d", pageSize);
        exit (1);
    }

    DLL_INIT(mo_, &outputQ);
    DLL_INIT(i_, &freeOutputBufs);

    if (writeMethod & MOLOCH_WRITE_THREAD) {
        moloch_writer_queue_length = writer_disk_queue_length_thread;
    } else {
        moloch_writer_queue_length = writer_disk_queue_length_nothread;
    }

    moloch_writer_exit         = writer_disk_exit;
    moloch_writer_write        = writer_disk_write;

    if (config.maxFileTimeM > 0) {
        g_timeout_add_seconds( 30, writer_disk_file_time_gfunc, 0);
    }
}
