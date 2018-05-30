/******************************************************************************/
/* readers.c  -- Functions dealing with pcap readers
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

#include "moloch.h"

extern MolochConfig_t        config;

LOCAL  MolochStringHashStd_t readersHash;

void reader_libpcapfile_init(char*);
void reader_libpcap_init(char*);
void reader_tpacketv3_init(char*);

MolochReaderStart  moloch_reader_start;
MolochReaderStats  moloch_reader_stats;
MolochReaderStop   moloch_reader_stop;

char              *readerFileName[256];
MolochFieldOps_t   readerFieldOps[256];


/******************************************************************************/
void moloch_readers_set(char *name) {
    MolochString_t *str;
    if (!name)
        name = moloch_config_str(NULL, "pcapReadMethod", "libpcap");


    HASH_FIND(s_, readersHash, name, str);
    if (!str) {
        LOGEXIT("Couldn't find pcapReadMethod '%s' implementation", name);
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
    moloch_readers_add("tpacketv3", reader_tpacketv3_init);

    char **interfaceOps;
    interfaceOps = moloch_config_raw_str_list(NULL, "interfaceOps", "");

    int i, j;
    for (i = 0; interfaceOps[i]; i++) {
        if (!interfaceOps[i][0])
            continue;

        char **opsstr = g_strsplit(interfaceOps[i], ",", 0);
        for (j = 0; opsstr[j]; j++) { }

        moloch_field_ops_init(&readerFieldOps[i], j, MOLOCH_FIELD_OPS_FLAGS_COPY);

        for (j = 0; opsstr[j]; j++) {
            char *equal = strchr(opsstr[j], '=');
            if (!equal) {
                LOGEXIT("Must be FieldExpr=value, missing equal '%s'", opsstr[j]);
            }
            int len = strlen(equal+1);
            if (!len) {
                LOGEXIT("Must be FieldExpr=value, empty value for '%s'", opsstr[j]);
            }
            *equal = 0;
            int fieldPos = moloch_field_by_exp(opsstr[j]);
            if (fieldPos == -1) {
                LOGEXIT("Must be FieldExpr=value, Unknown field expression '%s'", opsstr[j]);
            }
            moloch_field_ops_add(&readerFieldOps[i], fieldPos, equal+1, len);
        }
        g_strfreev(opsstr);
    }
    g_strfreev(interfaceOps);
}
/******************************************************************************/
void moloch_readers_exit()
{
}
