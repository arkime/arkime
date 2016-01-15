/******************************************************************************/
/* writer-null.c  -- writer that doesn't do anything
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "moloch.h"

extern MolochConfig_t        config;

static uint32_t              outputFilePos = 24;


/******************************************************************************/
uint32_t writer_null_queue_length()
{
    return 0;
}
/******************************************************************************/
void writer_null_flush(gboolean UNUSED(all))
{
}
/******************************************************************************/
void writer_null_exit()
{
}
/******************************************************************************/
void
writer_null_write(const struct pcap_pkthdr *h, const u_char *UNUSED(sp), uint32_t *fileNum, uint64_t *filePos)
{
    *fileNum = 0;
    *filePos = outputFilePos;
    outputFilePos += 16 + h->caplen;
}
/******************************************************************************/
char *
writer_null_name() {
    return "null";
}
/******************************************************************************/
void writer_null_init(char *UNUSED(name))
{
    moloch_writer_queue_length = writer_null_queue_length;
    moloch_writer_flush        = writer_null_flush;
    moloch_writer_exit         = writer_null_exit;
    moloch_writer_write        = writer_null_write;
    moloch_writer_name         = writer_null_name;
}
