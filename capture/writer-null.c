/******************************************************************************/
/* writer-null.c  -- writer that doesn't do anything
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

LOCAL  uint64_t              outputFilePos = 24;

/******************************************************************************/
LOCAL uint32_t writer_null_queue_length()
{
    return 0;
}
/******************************************************************************/
LOCAL void writer_null_exit()
{
}
/******************************************************************************/
LOCAL void writer_null_write(const ArkimeSession_t * const UNUSED(session), ArkimePacket_t * const packet)
{
    packet->writerFileNum = 0;
    packet->writerFilePos = outputFilePos;
    outputFilePos += 16 + packet->pktlen;
}
/******************************************************************************/
void writer_null_init(char *UNUSED(name))
{
    arkime_writer_queue_length = writer_null_queue_length;
    arkime_writer_exit         = writer_null_exit;
    arkime_writer_write        = writer_null_write;
}
