/******************************************************************************/
/* readers.c  -- Functions dealing with pcap readers
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

#include "moloch.h"

extern MolochConfig_t        config;

static MolochStringHashStd_t readersHash;

void reader_libpcapfile_init(char*);
void reader_libpcap_init(char*);

MolochReaderStart  moloch_reader_start;
MolochReaderStats  moloch_reader_stats;
MolochReaderFilter moloch_reader_should_filter;
MolochReaderStop   moloch_reader_stop;


/******************************************************************************/
void moloch_readers_set(char *name) {
    MolochString_t *str;
    if (!name)
        name = moloch_config_str(NULL, "pcapReadMethod", "libpcap");


    HASH_FIND(s_, readersHash, name, str);
    if (!str) {
        LOG("Couldn't find pcapReadMethod '%s' implementation", name);
        exit(0);
    }
    MolochReaderInit func = str->uw;
    func(name);
}
/******************************************************************************/
void moloch_readers_add(char *name, MolochReaderInit func) {
    moloch_string_add(&readersHash, name, func, TRUE);
}
/******************************************************************************/
void moloch_readers_init()
{
    HASH_INIT(s_, readersHash, moloch_string_hash, moloch_string_cmp);
    moloch_readers_add("libpcap-file", reader_libpcapfile_init);
    moloch_readers_add("libpcap", reader_libpcap_init);
}
/******************************************************************************/
void moloch_readers_exit()
{
}
