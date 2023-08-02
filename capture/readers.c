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

#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL  ArkimeStringHashStd_t readersHash;

void reader_libpcapfile_init(char*);
void reader_libpcap_init(char*);
void reader_tpacketv3_init(char*);
void reader_null_init(char*);
void reader_pcapoverip_init(char*);
void reader_tzsp_init(char*);

ArkimeReaderStart  arkime_reader_start;
ArkimeReaderStats  arkime_reader_stats;
ArkimeReaderStop   arkime_reader_stop;
ArkimeReaderExit   arkime_reader_exit;

char              *readerFileName[256];
ArkimeFieldOps_t   readerFieldOps[256];
uint32_t           readerOutputIds[256];


/******************************************************************************/
void arkime_readers_set(char *name) {
    ArkimeString_t *str;
    char *freeIt = NULL;
    if (!name)
        name = freeIt = arkime_config_str(NULL, "pcapReadMethod", "libpcap");


    HASH_FIND(s_, readersHash, name, str);
    if (!str) {
        CONFIGEXIT("Couldn't find pcapReadMethod '%s' implementation", name);
    }
    ArkimeReaderInit func = str->uw;
    func(name);
    g_free(freeIt);
}
/******************************************************************************/
void arkime_readers_add(char *name, ArkimeReaderInit func) {
    arkime_string_add(&readersHash, name, func, TRUE);
}
/******************************************************************************/
void arkime_readers_init()
{
    HASH_INIT(s_, readersHash, arkime_string_hash, arkime_string_cmp);
    arkime_readers_add("libpcap-file", reader_libpcapfile_init);
    arkime_readers_add("libpcap", reader_libpcap_init);
    arkime_readers_add("tpacketv3", reader_tpacketv3_init);
    arkime_readers_add("afpacketv3", reader_tpacketv3_init);
    arkime_readers_add("null", reader_null_init);
    arkime_readers_add("pcapoveripclient", reader_pcapoverip_init);
    arkime_readers_add("pcap-over-ip-client", reader_pcapoverip_init);
    arkime_readers_add("pcapoveripserver", reader_pcapoverip_init);
    arkime_readers_add("pcap-over-ip-server", reader_pcapoverip_init);
    arkime_readers_add("tzsp", reader_tzsp_init);
}

/******************************************************************************/
void arkime_readers_start()
{
    char **interfaceOps;
    interfaceOps = arkime_config_raw_str_list(NULL, "interfaceOps", "");

    int i, j;
    for (i = 0; interfaceOps[i]; i++) {
        if (!interfaceOps[i][0])
            continue;

        char **opsstr = g_strsplit(interfaceOps[i], ",", 0);
        for (j = 0; opsstr[j]; j++) { }

        arkime_field_ops_init(&readerFieldOps[i], j, ARKIME_FIELD_OPS_FLAGS_COPY);

        for (j = 0; opsstr[j]; j++) {
            char *equal = strchr(opsstr[j], '=');
            if (!equal) {
                CONFIGEXIT("Must be FieldExpr=value, missing equal '%s'", opsstr[j]);
            }
            int len = strlen(equal+1);
            if (!len) {
                CONFIGEXIT("Must be FieldExpr=value, empty value for '%s'", opsstr[j]);
            }
            *equal = 0;
            int fieldPos = arkime_field_by_exp(opsstr[j]);
            if (fieldPos == -1) {
                CONFIGEXIT("Must be FieldExpr=value, Unknown field expression '%s'", opsstr[j]);
            }
            arkime_field_ops_add(&readerFieldOps[i], fieldPos, equal+1, len);
        }
        g_strfreev(opsstr);
    }
    g_strfreev(interfaceOps);
    arkime_reader_start();
}
/******************************************************************************/
void arkime_readers_exit()
{
    if (arkime_reader_exit)
        arkime_reader_exit();
}
