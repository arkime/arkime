/******************************************************************************/
/* readers.c  -- Functions dealing with pcap readers
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL  ArkimeStringHashStd_t readersHash;

void reader_libpcapfile_init(char *);
void reader_libpcap_init(char *);
void reader_tpacketv3_init(char *);
void reader_null_init(char *);
void reader_pcapoverip_init(char *);
void reader_tzsp_init(char *);
void arkime_reader_scheme_init(char *);

ArkimeReaderStart  arkime_reader_start;
ArkimeReaderStats  arkime_reader_stats;
ArkimeReaderStop   arkime_reader_stop;
ArkimeReaderExit   arkime_reader_exit;

char              *readerFileName[256];
ArkimeFieldOps_t   readerFieldOps[256];

ArkimeOfflineInfo_t  offlineInfo[256];

ArkimeFilenameOps_t  readerFilenameOps[256];
int                  readerFilenameOpsNum;


/******************************************************************************/
void arkime_readers_set(char *name)
{
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
void arkime_readers_add(char *name, ArkimeReaderInit func)
{
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
    arkime_readers_add("scheme", arkime_reader_scheme_init);
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
            int len = strlen(equal + 1);
            if (!len) {
                CONFIGEXIT("Must be FieldExpr=value, empty value for '%s'", opsstr[j]);
            }
            *equal = 0;
            int fieldPos = arkime_field_by_exp(opsstr[j]);
            if (fieldPos == -1) {
                CONFIGEXIT("Must be FieldExpr=value, Unknown field expression '%s'", opsstr[j]);
            }
            arkime_field_ops_add(&readerFieldOps[i], fieldPos, equal + 1, len);
        }
        g_strfreev(opsstr);
    }
    g_strfreev(interfaceOps);

    // Compile all the filename ops.  The formation is fieldexpr=value%value
    // value is expanded using the g_regex_replace rules (\1 being the first capture group)
    // https://developer.gnome.org/glib/stable/glib-Perl-compatible-regular-expressions.html#g-regex-replace
    char **filenameOpsStr;
    filenameOpsStr = arkime_config_str_list(NULL, "filenameOps", "");

    for (i = 0; filenameOpsStr && filenameOpsStr[i] && i < 100; i++) {
        if (!filenameOpsStr[i][0])
            continue;

        char *equal = strchr(filenameOpsStr[i], '=');
        if (!equal) {
            CONFIGEXIT("Must be FieldExpr=regex%%value, missing equal '%s'", filenameOpsStr[i]);
        }

        char *percent = strchr(equal + 1, '%');
        if (!percent) {
            CONFIGEXIT("Must be FieldExpr=regex%%value, missing percent '%s'", filenameOpsStr[i]);
        }

        *equal = 0;
        *percent = 0;

        int elen = strlen(equal + 1);
        if (!elen) {
            CONFIGEXIT("Must be FieldExpr=regex%%value, empty regex for '%s'", filenameOpsStr[i]);
        }

        int vlen = strlen(percent + 1);
        if (!vlen) {
            CONFIGEXIT("Must be FieldExpr=regex%%value, empty value for '%s'", filenameOpsStr[i]);
        }

        int fieldPos = arkime_field_by_exp(filenameOpsStr[i]);
        if (fieldPos == -1) {
            CONFIGEXIT("Must be FieldExpr=regex?value, Unknown field expression '%s'", filenameOpsStr[i]);
        }

        readerFilenameOps[readerFilenameOpsNum].regex = g_regex_new(equal + 1, 0, 0, 0);
        readerFilenameOps[readerFilenameOpsNum].expand = g_strdup(percent + 1);
        if (!readerFilenameOps[readerFilenameOpsNum].regex)
            CONFIGEXIT("Couldn't compile regex '%s'", equal + 1);
        readerFilenameOps[readerFilenameOpsNum].field = fieldPos;
        readerFilenameOpsNum++;
    }
    g_strfreev(filenameOpsStr);

    arkime_reader_start();
}
/******************************************************************************/
void arkime_readers_exit()
{
    if (arkime_reader_exit)
        arkime_reader_exit();
}
