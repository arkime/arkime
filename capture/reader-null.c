/******************************************************************************/
/* reader-null.c
 *
 * Copyright 2019 AOL Inc. All rights reserved.
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

#include "moloch.h"

extern MolochConfig_t        config;


/******************************************************************************/
int reader_null_stats(MolochReaderStats_t *stats)
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
    moloch_reader_start         = reader_null_start;
    moloch_reader_stats         = reader_null_stats;
}
