/******************************************************************************/
/* writers.c  -- Functions dealing with writers
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <inttypes.h>
#include <errno.h>

ArkimeWriterQueueLength arkime_writer_queue_length;
ArkimeWriterWrite arkime_writer_write;
ArkimeWriterExit arkime_writer_exit;
ArkimeWriterIndex arkime_writer_index;

/******************************************************************************/
extern ArkimeConfig_t        config;

LOCAL  ArkimeStringHashStd_t writersHash;

/******************************************************************************/
void arkime_writers_start(char *name) {
    ArkimeString_t *str;
    char *freeIt = NULL;

    if (!name)
        name = freeIt = arkime_config_str(NULL, "pcapWriteMethod", "simple");


    HASH_FIND(s_, writersHash, name, str);
    if (!str) {
        CONFIGEXIT("Couldn't find pcapWriteMethod %s implementation", name);
    }
    ArkimeWriterInit func = str->uw;
    func(name);
    arkime_add_can_quit((ArkimeCanQuitFunc)arkime_writer_queue_length, "writer queue length");
    g_free(freeIt);
}
/******************************************************************************/
void arkime_writers_add(char *name, ArkimeWriterInit func) {
    arkime_string_add(&writersHash, name, func, TRUE);
}
/******************************************************************************/
void writer_disk_init(char *);
void writer_null_init(char *);
void writer_inplace_init(char *);
void writer_simple_init(char *);

void arkime_writers_init()
{
    HASH_INIT(s_, writersHash, arkime_string_hash, arkime_string_cmp);
    arkime_writers_add("null", writer_null_init);
    arkime_writers_add("inplace", writer_inplace_init);
    arkime_writers_add("simple", writer_simple_init);
    arkime_writers_add("simple-nodirect", writer_simple_init);
}
