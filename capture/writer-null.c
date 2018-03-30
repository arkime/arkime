/******************************************************************************/
/* writer-null.c  -- writer that doesn't do anything
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

extern MolochConfig_t        config;

LOCAL  uint32_t              outputFilePos = 24;

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
LOCAL void writer_null_write(const MolochSession_t * const UNUSED(session), MolochPacket_t * const packet)
{
    packet->writerFileNum = 0;
    packet->writerFilePos = outputFilePos;
    outputFilePos += 16 + packet->pktlen;
}
/******************************************************************************/
void writer_null_init(char *UNUSED(name))
{
    moloch_writer_queue_length = writer_null_queue_length;
    moloch_writer_exit         = writer_null_exit;
    moloch_writer_write        = writer_null_write;
}
