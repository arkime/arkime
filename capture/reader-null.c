/******************************************************************************/
/* reader-null.c
 *
 * Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"

extern ArkimeConfig_t        config;


/******************************************************************************/
int reader_null_stats(ArkimeReaderStats_t *stats)
{
    stats->dropped = 0;
    stats->total = 0;
    return 0;
}
/******************************************************************************/
LOCAL void reader_null_start()
{
}
/******************************************************************************/
void reader_null_init(char *UNUSED(name))
{
    arkime_reader_start         = reader_null_start;
    arkime_reader_stats         = reader_null_stats;
}
