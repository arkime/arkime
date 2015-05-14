/******************************************************************************/
/* writer-inplace.c  -- Writer that doesn't actually write pcap instead using
 *                      location of reading
 *
 * Copyright 2012-2015 AOL Inc. All rights reserved.
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
#include <sys/stat.h>
#include "moloch.h"

extern MolochConfig_t        config;


static char                 *outputFileName;
static uint32_t              outputId;
static FILE                 *inputFile;
static char                  inputFilename[PATH_MAX+1];

/******************************************************************************/
uint32_t writer_inplace_queue_length()
{
    return 0;
}
/******************************************************************************/
void writer_inplace_flush(gboolean UNUSED(all))
{
}
/******************************************************************************/
void writer_inplace_exit()
{
}
/******************************************************************************/
void writer_inplace_create(char *filename)
{
    if (config.dryRun) {
        outputFileName = "dryrun.pcap";
        return;
    }

    struct stat st;

    fstat(fileno(inputFile), &st);

    outputFileName = moloch_db_create_file(nids_last_pcap_header->ts.tv_sec, filename, st.st_size, 1, &outputId);
}

/******************************************************************************/
void
writer_inplace_write(const struct pcap_pkthdr *h, const u_char *UNUSED(sp), uint32_t *fileNum, uint64_t *filePos)
{
    if (!outputFileName)
        writer_inplace_create(inputFilename);

    *fileNum = outputId;
    *filePos = ftell(inputFile) - 16 - h->caplen;
}
/******************************************************************************/
char *
writer_inplace_name() {
    return inputFilename;
}
/******************************************************************************/
void
writer_inplace_next_input(FILE *file, char *filename) {
    inputFile = file;
    strcpy(inputFilename, filename);
    if (!config.dryRun) {
        g_free(outputFileName);
    }
    outputFileName = 0;
}
/******************************************************************************/
void writer_inplace_init(char *UNUSED(name))
{
    moloch_writer_queue_length = writer_inplace_queue_length;
    moloch_writer_flush        = writer_inplace_flush;
    moloch_writer_exit         = writer_inplace_exit;
    moloch_writer_write        = writer_inplace_write;
    moloch_writer_next_input   = writer_inplace_next_input;
    moloch_writer_name         = writer_inplace_name;
}
