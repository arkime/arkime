/******************************************************************************/
/* writer-inplace.c  -- Writer that doesn't actually write pcap instead using
 *                      location of reading
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define _FILE_OFFSET_BITS 64
#include "arkime.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>

extern ArkimeConfig_t        config;


LOCAL ARKIME_LOCK_DEFINE(filePtr2Id);

extern char                *readerFileName[256];
extern uint32_t             readerOutputIds[256];

/******************************************************************************/
LOCAL uint32_t writer_inplace_queue_length()
{
    return 0;
}
/******************************************************************************/
LOCAL void writer_inplace_exit()
{
}
/******************************************************************************/
LOCAL long writer_inplace_create(ArkimePacket_t * const packet)
{
    struct stat st;
    const char *readerName = readerFileName[packet->readerPos];

    stat(readerName, &st);

    uint32_t outputId;
    if (config.pcapReprocess) {
        arkime_db_file_exists(readerName, &outputId);
    } else {
        char *filename;
        if (config.gapPacketPos)
            filename = arkime_db_create_file_full(packet->ts.tv_sec, readerName, st.st_size, !config.noLockPcap, &outputId,
                                                  "packetPosEncoding", "gap0",
                                                  (char *)NULL);
        else
            filename = arkime_db_create_file(packet->ts.tv_sec, readerName, st.st_size, !config.noLockPcap, &outputId);

        g_free(filename);
    }
    readerOutputIds[packet->readerPos] = outputId;
    return outputId;
}

/******************************************************************************/
LOCAL void writer_inplace_write(const ArkimeSession_t * const UNUSED(session), ArkimePacket_t * const packet)
{
    // Need to lock since multiple packet threads for the same readerPos are running and only want to create once
    ARKIME_LOCK(filePtr2Id);
    long outputId = readerOutputIds[packet->readerPos];
    if (!outputId)
        outputId = writer_inplace_create(packet);
    ARKIME_UNLOCK(filePtr2Id);

    packet->writerFileNum = outputId;
    packet->writerFilePos = packet->readerFilePos;
}
/******************************************************************************/
LOCAL void writer_inplace_write_dryrun(const ArkimeSession_t * const UNUSED(session), ArkimePacket_t * const packet)
{
    packet->writerFilePos = packet->readerFilePos;
}
/******************************************************************************/
void writer_inplace_init(char *UNUSED(name))
{
    config.gapPacketPos        = arkime_config_boolean(NULL, "gapPacketPos", TRUE);
    arkime_writer_queue_length = writer_inplace_queue_length;
    arkime_writer_exit         = writer_inplace_exit;
    if (config.dryRun)
        arkime_writer_write    = writer_inplace_write_dryrun;
    else
        arkime_writer_write    = writer_inplace_write;
}
