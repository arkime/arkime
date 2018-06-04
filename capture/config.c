/******************************************************************************/
/* config.c  -- Functions dealing with the config file
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>

extern MolochConfig_t        config;

LOCAL GKeyFile             *molochKeyFile;

/******************************************************************************/
gchar *moloch_config_section_str(GKeyFile *keyfile, char *section, char *key, char *d)
{
    char *result;
    if (!keyfile)
        keyfile = molochKeyFile;

    if (g_key_file_has_key(keyfile, section, key, NULL)) {
        result = g_key_file_get_string(keyfile, section, key, NULL);
    } else if (d) {
        result = g_strdup(d);
    } else {
        result = NULL;
    }

    if (config.debug) {
        LOG("%s.%s=%s", section, key, result?result:"(null)");
    }

    return result;
}
/******************************************************************************/
gchar **moloch_config_section_keys(GKeyFile *keyfile, char *section, gsize *keys_len)
{
    if (!keyfile)
        keyfile = molochKeyFile;

    if (!g_key_file_has_group(keyfile, section)) {
        *keys_len = 0;
        return NULL;
    }

    GError *error = 0;
    gchar **keys = g_key_file_get_keys (keyfile, section, keys_len, &error);
    if (error) {
        *keys_len = 0;
        return NULL;
    }
    return keys;
}

/******************************************************************************/
gchar *moloch_config_str(GKeyFile *keyfile, char *key, char *d)
{
    char *result;

    if (!keyfile)
        keyfile = molochKeyFile;

    if (g_key_file_has_key(keyfile, config.nodeName, key, NULL)) {
        result = g_key_file_get_string(keyfile, config.nodeName, key, NULL);
    } else if (config.nodeClass && g_key_file_has_key(keyfile, config.nodeClass, key, NULL)) {
        result = g_key_file_get_string(keyfile, config.nodeClass, key, NULL);
    } else if (g_key_file_has_key(keyfile, "default", key, NULL)) {
        result = g_key_file_get_string(keyfile, "default", key, NULL);
    } else if (d) {
        result = g_strdup(d);
    } else {
        result = NULL;
    }

    if (config.debug) {
        LOG("%s=%s", key, result?result:"(null)");
    }

    return result;
}

/******************************************************************************/
gchar **moloch_config_raw_str_list(GKeyFile *keyfile, char *key, char *d)
{
    gchar **result;

    if (!keyfile)
        keyfile = molochKeyFile;

    if (g_key_file_has_key(keyfile, config.nodeName, key, NULL)) {
        result = g_key_file_get_string_list(keyfile, config.nodeName, key, NULL, NULL);
    } else if (config.nodeClass && g_key_file_has_key(keyfile, config.nodeClass, key, NULL)) {
        result = g_key_file_get_string_list(keyfile, config.nodeClass, key, NULL, NULL);
    } else if (g_key_file_has_key(keyfile, "default", key, NULL)) {
        result = g_key_file_get_string_list(keyfile, "default", key, NULL, NULL);
    } else if (d) {
        result = g_strsplit(d, ";", 0);
    } else {
        result = NULL;
    }

    return result;
}

/******************************************************************************/
gchar **moloch_config_str_list(GKeyFile *keyfile, char *key, char *d)
{
    gchar **strs = moloch_config_raw_str_list(keyfile, key, d);
    if (!strs) {
        if (config.debug) {
            LOG("%s=(null)", key);
        }
        return strs;
    }

    int i, j;
    for (i = j = 0; strs[i]; i++) {
        char *str = strs[i];

        /* Remove leading and trailing spaces */
        while (isspace(*str))
            str++;
        g_strchomp(str);

        /* Empty string */
        if (*str == 0) {
            g_free(strs[i]);
            continue;
        }

        /* Moved front of string, need to realloc so g_strfreev doesn't blow */
        if (str != strs[i]) {
            str = g_strdup(str);
            g_free(strs[i]);
        }

        /* Save string back */
        strs[j] = str;
        j++;
    }

    /* NULL anything at the end that was moved forward */
    for (; j < i; j++)
        strs[j] = NULL;

    if (config.debug) {
        gchar *str = g_strjoinv(";", strs);
        LOG("%s=%s", key, str);
        g_free(str);
    }
    return strs;
}

/******************************************************************************/
uint32_t moloch_config_int(GKeyFile *keyfile, char *key, uint32_t d, uint32_t min, uint32_t max)
{
    uint32_t value = d;

    if (!keyfile)
        keyfile = molochKeyFile;

    if (g_key_file_has_key(keyfile, config.nodeName, key, NULL)) {
        value = g_key_file_get_integer(keyfile, config.nodeName, key, NULL);
    } else if (config.nodeClass && g_key_file_has_key(keyfile, config.nodeClass, key, NULL)) {
        value = g_key_file_get_integer(keyfile, config.nodeClass, key, NULL);
    } else if (g_key_file_has_key(keyfile, "default", key, NULL)) {
        value = g_key_file_get_integer(keyfile, "default", key, NULL);
    }

    if (value < min) {
        LOG ("INFO: Reseting %s since %u is less then the min %u", key, value, min);
        value = min;
    }
    if (value > max) {
        LOG ("INFO: Reseting %s since %u is greater then the max %u", key, value, max);
        value = max;
    }

    if (config.debug) {
        LOG("%s=%d", key, value);
    }

    return value;
}

/******************************************************************************/
double moloch_config_double(GKeyFile *keyfile, char *key, double d, double min, double max)
{
    double value = d;

    if (!keyfile)
        keyfile = molochKeyFile;

    if (g_key_file_has_key(keyfile, config.nodeName, key, NULL)) {
        value = g_key_file_get_double(keyfile, config.nodeName, key, NULL);
    } else if (config.nodeClass && g_key_file_has_key(keyfile, config.nodeClass, key, NULL)) {
        value = g_key_file_get_double(keyfile, config.nodeClass, key, NULL);
    } else if (g_key_file_has_key(keyfile, "default", key, NULL)) {
        value = g_key_file_get_double(keyfile, "default", key, NULL);
    }

    if (value < min)
        value = min;
    if (value > max)
        value = max;

    if (config.debug) {
        LOG("%s=%lf", key, value);
    }

    return value;
}

/******************************************************************************/
char moloch_config_boolean(GKeyFile *keyfile, char *key, char d)
{
    gboolean value = d;

    if (!keyfile)
        keyfile = molochKeyFile;

    if (g_key_file_has_key(keyfile, config.nodeName, key, NULL)) {
        value = g_key_file_get_boolean(keyfile, config.nodeName, key, NULL);
    } else if (config.nodeClass && g_key_file_has_key(keyfile, config.nodeClass, key, NULL)) {
        value = g_key_file_get_boolean(keyfile, config.nodeClass, key, NULL);
    } else if (g_key_file_has_key(keyfile, "default", key, NULL)) {
        value = g_key_file_get_boolean(keyfile, "default", key, NULL);
    }

    if (config.debug) {
        LOG("%s=%s", key, value?"true": "false");
    }

    return value;
}
/******************************************************************************/
void moloch_config_load_includes(char **includes)
{
    int       i, g, k;

    for (i = 0; includes[i]; i++) {
        GKeyFile *keyFile = g_key_file_new();
        GError *error = 0;
        gboolean status = g_key_file_load_from_file(keyFile, includes[i], G_KEY_FILE_NONE, &error);
        if (!status || error) {
            printf("Couldn't load config includes file (%s) %s\n", includes[i], (error?error->message:""));
            exit(1);
        }

        gchar **groups = g_key_file_get_groups (keyFile, NULL);
        for (g = 0; groups[g]; g++) {
            gchar **keys = g_key_file_get_keys (keyFile, groups[g], NULL, NULL);
            for (k = 0; keys[k]; k++) {
                char *value = g_key_file_get_value(keyFile, groups[g], keys[k], NULL);
                if (value && !error) {
                    g_key_file_set_value(molochKeyFile, groups[g], keys[k], value);
                    g_free(value);
                }
            }
            g_strfreev(keys);
        }
        g_strfreev(groups);
        g_key_file_free(keyFile);
    }
}
/******************************************************************************/
void moloch_config_load()
{

    gboolean  status;
    GError   *error = 0;
    GKeyFile *keyfile;
    int       i;

    keyfile = molochKeyFile = g_key_file_new();

    status = g_key_file_load_from_file(keyfile, config.configFile, G_KEY_FILE_NONE, &error);
    if (!status || error) {
        printf("Couldn't load config file (%s) %s\n", config.configFile, (error?error->message:""));
        exit(1);
    }

    char **includes = moloch_config_str_list(keyfile, "includes", NULL);
    if (includes) {
        moloch_config_load_includes(includes);
        g_strfreev(includes);
        //LOG("KEYFILE:\n%s", g_key_file_to_data(molochKeyFile, NULL, NULL));
    }


    char *rotateIndex       = moloch_config_str(keyfile, "rotateIndex", "daily");

    if (strcmp(rotateIndex, "hourly") == 0)
        config.rotate = MOLOCH_ROTATE_HOURLY;
    else if (strcmp(rotateIndex, "hourly6") == 0)
        config.rotate = MOLOCH_ROTATE_HOURLY6;
    else if (strcmp(rotateIndex, "daily") == 0)
        config.rotate = MOLOCH_ROTATE_DAILY;
    else if (strcmp(rotateIndex, "weekly") == 0)
        config.rotate = MOLOCH_ROTATE_WEEKLY;
    else if (strcmp(rotateIndex, "monthly") == 0)
        config.rotate = MOLOCH_ROTATE_MONTHLY;
    else {
        printf("Unknown rotateIndex '%s'\n", rotateIndex);
        exit(1);
    }
    g_free(rotateIndex);

    config.nodeClass        = moloch_config_str(keyfile, "nodeClass", NULL);
    gchar **tags            = moloch_config_str_list(keyfile, "dontSaveTags", NULL);
    if (tags) {
        for (i = 0; tags[i]; i++) {
            if (!(*tags[i]))
                continue;
            int num = 1;
            char *colon = strchr(tags[i], ':');
            if (colon) {
                *colon = 0;
                num = atoi(colon+1);
                if (num < 1)
                    num = 1;
                if (num > 0xffff)
                    num = 0xffff;
            }
            moloch_string_add((MolochStringHash_t *)(char*)&config.dontSaveTags, tags[i], (gpointer)(long)num, TRUE);
        }
        g_strfreev(tags);
    }

    config.plugins          = moloch_config_str_list(keyfile, "plugins", NULL);
    config.rootPlugins      = moloch_config_str_list(keyfile, "rootPlugins", NULL);
    config.smtpIpHeaders    = moloch_config_str_list(keyfile, "smtpIpHeaders", NULL);

    if (config.smtpIpHeaders) {
        for (i = 0; config.smtpIpHeaders[i]; i++) {
            int len = strlen(config.smtpIpHeaders[i]);
            char *lower = g_ascii_strdown(config.smtpIpHeaders[i], len);
            g_free(config.smtpIpHeaders[i]);
            config.smtpIpHeaders[i] = lower;
            if (lower[len-1] == ':')
                lower[len-1] = 0;
        }
    }

    config.prefix           = moloch_config_str(keyfile, "prefix", "");
    int len = strlen(config.prefix);
    if (len > 0 && config.prefix[len - 1] != '_') {
        char *tmp  = malloc(len + 2);
        memcpy(tmp, config.prefix, len);
        tmp[len] = '_';
        tmp[len+1] = 0;
        g_free(config.prefix);
        config.prefix = tmp;
    }

    config.elasticsearch    = moloch_config_str(keyfile, "elasticsearch", "localhost:9200");
    config.interface        = moloch_config_str_list(keyfile, "interface", NULL);
    config.pcapDir          = moloch_config_str_list(keyfile, "pcapDir", NULL);
    config.bpf              = moloch_config_str(keyfile, "bpf", NULL);
    config.yara             = moloch_config_str(keyfile, "yara", NULL);
    config.emailYara        = moloch_config_str(keyfile, "emailYara", NULL);
    config.rirFile          = moloch_config_str(keyfile, "rirFile", NULL);
    config.ouiFile          = moloch_config_str(keyfile, "ouiFile", NULL);
    config.geoLite2ASN      = moloch_config_str(keyfile, "geoLite2ASN", "/data/moloch/etc/GeoLite2-ASN.mmdb");
    config.geoLite2Country  = moloch_config_str(keyfile, "geoLite2Country", "/data/moloch/etc/GeoLite2-Country.mmdb");
    config.dropUser         = moloch_config_str(keyfile, "dropUser", NULL);
    config.dropGroup        = moloch_config_str(keyfile, "dropGroup", NULL);
    config.pluginsDir       = moloch_config_str_list(keyfile, "pluginsDir", NULL);
    config.parsersDir       = moloch_config_str_list(keyfile, "parsersDir", " /data/moloch/parsers ; ./parsers ");
    char *offlineRegex      = moloch_config_str(keyfile, "offlineFilenameRegex", "(?i)\\.(pcap|cap)$");

    config.offlineRegex     = g_regex_new(offlineRegex, 0, 0, &error);
    if (!config.offlineRegex || error) {
        printf("Couldn't parse offlineRegex (%s) %s\n", offlineRegex, (error?error->message:""));
        exit(1);
    }
    g_free(offlineRegex);

    config.pcapDirTemplate  = moloch_config_str(keyfile, "pcapDirTemplate", NULL);
    if (config.pcapDirTemplate && config.pcapDirTemplate[0] != '/') {
        printf("pcapDirTemplate MUST start with a / '%s'\n", config.pcapDirTemplate);
        exit(1);
    }

    config.pcapDirAlgorithm = moloch_config_str(keyfile, "pcapDirAlgorithm", "round-robin");
    if (strcmp(config.pcapDirAlgorithm, "round-robin") != 0
            && strcmp(config.pcapDirAlgorithm, "max-free-percent") != 0
            && strcmp(config.pcapDirAlgorithm, "max-free-bytes") != 0) {
        printf("'%s' is not a valid value for pcapDirAlgorithm.  Supported algorithms are round-robin, max-free-percent, and max-free-bytes.\n", config.pcapDirAlgorithm);
        exit(1);
    }

    config.maxFileSizeG          = moloch_config_double(keyfile, "maxFileSizeG", 4, 0.01, 1024);
    config.maxFileSizeB          = config.maxFileSizeG*1024LL*1024LL*1024LL;
    config.maxFileTimeM          = moloch_config_int(keyfile, "maxFileTimeM", 0, 0, 0xffff);
    config.timeouts[SESSION_ICMP]= moloch_config_int(keyfile, "icmpTimeout", 10, 1, 0xffff);
    config.timeouts[SESSION_UDP] = moloch_config_int(keyfile, "udpTimeout", 60, 1, 0xffff);
    config.timeouts[SESSION_TCP] = moloch_config_int(keyfile, "tcpTimeout", 60*8, 10, 0xffff);
    config.timeouts[SESSION_SCTP]= moloch_config_int(keyfile, "sctpTimeout", 60, 10, 0xffff);
    config.timeouts[SESSION_ESP] = moloch_config_int(keyfile, "espTimeout", 60*10, 10, 0xffff);
    config.tcpSaveTimeout        = moloch_config_int(keyfile, "tcpSaveTimeout", 60*8, 10, 60*120);
    int maxStreams               = moloch_config_int(keyfile, "maxStreams", 1500000, 1, 16777215);
    config.maxPackets            = moloch_config_int(keyfile, "maxPackets", 10000, 1, 0xffff);
    config.maxPacketsInQueue     = moloch_config_int(keyfile, "maxPacketsInQueue", 200000, 10000, 5000000);
    config.dbBulkSize            = moloch_config_int(keyfile, "dbBulkSize", 200000, MOLOCH_HTTP_BUFFER_SIZE*2, 1000000);
    config.dbFlushTimeout        = moloch_config_int(keyfile, "dbFlushTimeout", 5, 1, 60*30);
    config.maxESConns            = moloch_config_int(keyfile, "maxESConns", 20, 5, 1000);
    config.maxESRequests         = moloch_config_int(keyfile, "maxESRequests", 500, 10, 5000);
    config.logEveryXPackets      = moloch_config_int(keyfile, "logEveryXPackets", 50000, 1000, 0xffffffff);
    config.pcapBufferSize        = moloch_config_int(keyfile, "pcapBufferSize", 300000000, 100000, 0xffffffff);
    config.pcapWriteSize         = moloch_config_int(keyfile, "pcapWriteSize", 0x40000, 0x10000, 0x800000);
    config.maxFreeOutputBuffers  = moloch_config_int(keyfile, "maxFreeOutputBuffers", 50, 0, 0xffff);
    config.fragsTimeout          = moloch_config_int(keyfile, "fragsTimeout", 60*8, 60, 0xffff);
    config.maxFrags              = moloch_config_int(keyfile, "maxFrags", 10000, 100, 0xffffff);
    config.snapLen               = moloch_config_int(keyfile, "snapLen", 16384, 1, MOLOCH_PACKET_MAX_LEN);
    config.maxMemPercentage      = moloch_config_int(keyfile, "maxMemPercentage", 100, 5, 100);
    config.maxReqBody            = moloch_config_int(keyfile, "maxReqBody", 256, 0, 0x7fff);

    config.packetThreads         = moloch_config_int(keyfile, "packetThreads", 1, 1, MOLOCH_MAX_PACKET_THREADS);


    config.logUnknownProtocols   = moloch_config_boolean(keyfile, "logUnknownProtocols", config.debug);
    config.logESRequests         = moloch_config_boolean(keyfile, "logESRequests", config.debug);
    config.logFileCreation       = moloch_config_boolean(keyfile, "logFileCreation", config.debug);
    config.logHTTPConnections    = moloch_config_boolean(keyfile, "logHTTPConnections", TRUE);
    config.parseSMTP             = moloch_config_boolean(keyfile, "parseSMTP", TRUE);
    config.parseSMB              = moloch_config_boolean(keyfile, "parseSMB", TRUE);
    config.parseQSValue          = moloch_config_boolean(keyfile, "parseQSValue", FALSE);
    config.parseCookieValue      = moloch_config_boolean(keyfile, "parseCookieValue", FALSE);
    config.supportSha256         = moloch_config_boolean(keyfile, "supportSha256", FALSE);
    config.reqBodyOnlyUtf8       = moloch_config_boolean(keyfile, "reqBodyOnlyUtf8", TRUE);
    config.compressES            = moloch_config_boolean(keyfile, "compressES", FALSE);
    config.antiSynDrop           = moloch_config_boolean(keyfile, "antiSynDrop", TRUE);
    config.readTruncatedPackets  = moloch_config_boolean(keyfile, "readTruncatedPackets", FALSE);
    config.trackESP              = moloch_config_boolean(keyfile, "trackESP", FALSE);

    config.maxStreams[SESSION_TCP] = maxStreams/config.packetThreads*1.25;
    config.maxStreams[SESSION_UDP] = maxStreams/config.packetThreads/20;
    config.maxStreams[SESSION_SCTP] = maxStreams/config.packetThreads/20;
    config.maxStreams[SESSION_ICMP] = maxStreams/config.packetThreads/200;
    config.maxStreams[SESSION_ESP] = maxStreams/config.packetThreads/200;


    gchar **saveUnknownPackets     = moloch_config_str_list(keyfile, "saveUnknownPackets", NULL);
    if (saveUnknownPackets) {
        for (i = 0; saveUnknownPackets[i]; i++) {
            char *s = saveUnknownPackets[i];

            if (strcmp(s, "all") == 0) {
                memset(&config.etherSavePcap, 0xff, 1024);
                memset(&config.ipSavePcap, 0xff, 4);
            } else if (strcmp(s, "ip:all") == 0) {
                memset(&config.ipSavePcap, 0xff, 4);
            } else if (strcmp(s, "ether:all") == 0) {
                memset(&config.etherSavePcap, 0xff, 1024);
            } else if (strncmp(s, "ip:", 3) == 0) {
                int n = atoi(s+3);
                if (n < 0 || n > 0xff)
                    LOGEXIT("Bad value: %s", s);
                BIT_SET(n, config.ipSavePcap);
            } else if (strncmp(s, "-ip:", 4) == 0) {
                int n = atoi(s+4);
                if (n < 0 || n > 0xff)
                    LOGEXIT("Bad value: %s", s);
                BIT_CLR(n, config.ipSavePcap);
            } else if (strncmp(s, "ether:", 6) == 0) {
                int n = atoi(s+6);
                if (n < 0 || n > 0xffff)
                    LOGEXIT("Bad value: %s", s);
                BIT_SET(n, config.etherSavePcap);
            } else if (strncmp(s, "-ether:", 7) == 0) {
                int n = atoi(s+7);
                if (n < 0 || n > 0xffff)
                    LOGEXIT("Bad value: %s", s);
                BIT_CLR(n, config.etherSavePcap);
            } else {
                LOGEXIT("Not sure what %s is", s);
            }
        }
    }

}
/******************************************************************************/
void moloch_config_load_local_ips()
{
    GError   *error = 0;

    if (!g_key_file_has_group(molochKeyFile, "override-ips"))
        return;

    gsize keys_len;
    gchar **keys = g_key_file_get_keys (molochKeyFile, "override-ips", &keys_len, &error);
    if (error) {
        LOGEXIT("Error with override-ips: %s", error->message);
    }

    gsize k, v;
    for (k = 0 ; k < keys_len; k++) {
        gsize values_len;
        gchar **values = g_key_file_get_string_list(molochKeyFile,
                                                   "override-ips",
                                                   keys[k],
                                                  &values_len,
                                                   NULL);
        MolochIpInfo_t *ii = MOLOCH_TYPE_ALLOC0(MolochIpInfo_t);
        for (v = 0; v < values_len; v++) {
            if (strncmp(values[v], "asn:", 4) == 0) {
                ii->asn = g_strdup(values[v]+4);
            } else if (strncmp(values[v], "rir:", 4) == 0) {
                ii->rir = g_strdup(values[v]+4);
            } else if (strncmp(values[v], "tag:", 4) == 0) {
                if (ii->numtags < 10) {
                    ii->tagsStr[ii->numtags] = strdup(values[v]+4);
                    ii->numtags++;
                }
            } else if (strncmp(values[v], "country:", 8) == 0) {
                ii->country = g_strdup(values[v]+8);
            }
        }
        moloch_db_add_local_ip(keys[k], ii);
        g_strfreev(values);
    }
    g_strfreev(keys);
}
/******************************************************************************/
void moloch_config_load_packet_ips()
{
    GError   *error = 0;

    if (!g_key_file_has_group(molochKeyFile, "packet-drop-ips"))
        return;

    gsize keys_len;
    gchar **keys = g_key_file_get_keys (molochKeyFile, "packet-drop-ips", &keys_len, &error);
    if (error) {
        LOGEXIT("Error with packet-drop-ips: %s", error->message);
    }

    gsize k, v;
    for (k = 0 ; k < keys_len; k++) {
        gsize values_len;
        gchar **values = g_key_file_get_string_list(molochKeyFile,
                                                   "packet-drop-ips",
                                                   keys[k],
                                                  &values_len,
                                                   NULL);
        int mode = 0;
        for (v = 0; v < values_len; v++) {
            if (strncmp(values[v], "drop", 4) == 0) {

            } else if (strncmp(values[v], "allow", 4) == 0) {
                mode = 1;
            } else {
                LOGEXIT("Unknown argument to packet-drop-ips %s %s", keys[k], values[v]);
            }
        }
        moloch_packet_add_packet_ip(keys[k], mode);
        g_strfreev(values);
    }
    g_strfreev(keys);
}
/******************************************************************************/
void moloch_config_add_header(MolochStringHashStd_t *hash, char *key, int pos)
{
    MolochString_t *hstring;

    hstring = MOLOCH_TYPE_ALLOC0(MolochString_t);
    hstring->str = key;
    hstring->len = strlen(key);
    hstring->uw = (gpointer)(long)pos;
    HASH_ADD(s_, *hash, hstring->str, hstring);
}
/******************************************************************************/
void moloch_config_load_header(char *section, char *group, char *helpBase, char *expBase, char *dbBase, MolochStringHashStd_t *hash, int flags)
{
    GError   *error = 0;
    char      name[100];

    if (!g_key_file_has_group(molochKeyFile, section))
        return;

    gsize keys_len;
    gchar **keys = g_key_file_get_keys (molochKeyFile, section, &keys_len, &error);
    if (error) {
        LOGEXIT("Error with %s: %s", section, error->message);
    }

    gsize k, v;
    for (k = 0 ; k < keys_len; k++) {
        gsize values_len;
        gchar **values = g_key_file_get_string_list(molochKeyFile,
                                                   section,
                                                   keys[k],
                                                  &values_len,
                                                   NULL);
        snprintf(name, sizeof(name), "%s", keys[k]);
        int type = 0;
        int t = 0;
        int unique = 1;
        int count  = 0;
        char *kind = 0;
        for (v = 0; v < values_len; v++) {
            if (strcmp(values[v], "type:integer") == 0 ||
                strcmp(values[v], "type:seconds") == 0) {
                type = 1;
                kind = values[v] + 5;
            } else if (strcmp(values[v], "type:ip") == 0) {
                type = 2;
            } else if (strcmp(values[v], "unique:false") == 0) {
                unique = 0;
            } else if (strcmp(values[v], "count:true") == 0) {
                count = 1;
            }
        }

        int f = flags;

        if (count)
            f |= MOLOCH_FIELD_FLAG_CNT;

        switch (type) {
        case 0:
            kind = "termfield";
            if (unique)
                t = MOLOCH_FIELD_TYPE_STR_HASH;
            else
                t = MOLOCH_FIELD_TYPE_STR_ARRAY;
            break;
        case 1:
            if (unique)
                t = MOLOCH_FIELD_TYPE_INT_GHASH;
            else
                t = MOLOCH_FIELD_TYPE_INT_ARRAY;
            break;
        case 2:
            kind = "ip";
            t = MOLOCH_FIELD_TYPE_IP_GHASH;
            break;
        }



        MolochString_t *hstring;

        HASH_FIND(s_, *hash, keys[k], hstring);
        if (hstring) {
            LOG("WARNING - ignoring field %s for %s", keys[k], section);
            g_strfreev(values);
            continue;
        }

        char expression[100];
        char field[100];
        char help[100];

        snprintf(expression, sizeof(expression), "%s%s", expBase, name);
        snprintf(field, sizeof(field), "%s%s", dbBase, name);
        snprintf(help, sizeof(help), "%s%s", helpBase, name);

        int pos;
        pos = moloch_field_define(group, kind,
                expression, expression, field,
                help,
                t, f, NULL);
        moloch_config_add_header(hash, g_strdup(keys[k]), pos);
        g_strfreev(values);
    }
    g_strfreev(keys);
}

/******************************************************************************/
#define MOLOCH_CONFIG_FILES 100
typedef struct {
    char                 *desc;
    int                   num;
    char                 *name[MOLOCH_CONFIG_FILES];
    MolochFileChange_cb   cb;
    MolochFilesChange_cb  cbs;
    off_t                 size[MOLOCH_CONFIG_FILES];
    int64_t               modify[MOLOCH_CONFIG_FILES];
    char                  freeOld;
} MolochFileChange_t;

LOCAL int                numFiles;
LOCAL MolochFileChange_t files[MOLOCH_CONFIG_FILES];
/******************************************************************************/
void moloch_config_monitor_file(char *desc, char *name, MolochFileChange_cb cb)
{
    struct stat     sb;

    if (numFiles >= MOLOCH_CONFIG_FILES)
        LOGEXIT("Couldn't monitor anymore files %s %s", desc, name);

    if (stat(name, &sb) != 0) {
        LOGEXIT("Couldn't stat %s file %s error %s", desc, name, strerror(errno));
    }

    files[numFiles].name[0] = g_strdup(name);
    files[numFiles].modify[0] = sb.st_mtime;

    files[numFiles].desc = g_strdup(desc);
    files[numFiles].cb = cb;
    files[numFiles].num = 1;

    numFiles++;
    cb(name);
}
/******************************************************************************/
void moloch_config_monitor_files(char *desc, char **names, MolochFilesChange_cb cb)
{
    struct stat     sb;
    int             i;

    if (numFiles >= MOLOCH_CONFIG_FILES)
        LOGEXIT("Couldn't monitor anymore files %s %s", desc, names[0]);

    for (i = 0; names[i] && i < MOLOCH_CONFIG_FILES; i++) {
        if (stat(names[i], &sb) != 0) {
            LOGEXIT("Couldn't stat %s file %s error %s", desc, names[i], strerror(errno));
        }

        files[numFiles].name[i] = g_strdup(names[i]);
        files[numFiles].modify[i] = sb.st_mtime;
    }

    files[numFiles].desc = g_strdup(desc);
    files[numFiles].cbs = cb;
    files[numFiles].num = i;

    numFiles++;
    cb(names);
}
/******************************************************************************/
gboolean moloch_config_reload_files (gpointer UNUSED(user_data))
{
    int             i, f;
    struct stat     sb[MOLOCH_CONFIG_FILES];

    for (i = 0; i < numFiles; i++) {
        if (files[i].freeOld) {
            if (config.debug)
                LOG("Free old %s %s %d", files[i].desc, files[i].name[0], files[i].num);
            if (files[i].cbs)
                files[i].cbs(NULL);
            else
                files[i].cb(NULL);
            files[i].freeOld = 0;
        }

        int changed = 0;
        for (f = 0; f < files[i].num; f++) {
            if (stat(files[i].name[f], &sb[f]) != 0) {
                LOG("Couldn't stat %s file %s error %s", files[i].desc, files[i].name[f], strerror(errno));
                changed = 0;
                break;
            }

            if (sb[f].st_size <= 1) { // Ignore tiny files for reloads
                changed = 0;
                break;
            }

            if (sb[f].st_mtime > files[i].modify[f]) {
                if (files[i].size[f] != sb[f].st_size) {
                    files[i].size[f] = sb[f].st_size;
                    changed = 0;
                    break;
                }
                if (config.debug)
                    LOG("Changed %s %s", files[i].desc, files[i].name[f]);
                changed = 1;
            }
        }

        // Something was changed
        if (changed) {
            if (files[i].cbs)
                files[i].cbs(files[i].name);
            else
                files[i].cb(files[i].name[0]);

            files[i].freeOld = 1;
            for (f = 0; f < files[i].num; f++) {
                files[i].size[f] = 0;
                files[i].modify[f] = sb[f].st_mtime;
            }
        }
    }

    return TRUE;
}
/******************************************************************************/
void moloch_config_init()
{
    HASH_INIT(s_, config.dontSaveTags, moloch_string_hash, moloch_string_cmp);

    moloch_config_load();

    if (config.debug) {
        LOG("maxFileSizeB: %" PRIu64, config.maxFileSizeB);
    }

    if (config.interface && !config.interface[0]) {
        printf("interface set in config file, but it is empty\n");
        exit (1);
    }

    if (!config.interface && !config.pcapReadOffline) {
        printf("Need to set interface, pcap file (-r) or pcap directory (-R) \n");
        exit (1);
    }

    if (!config.pcapDir) {
        printf("Must set a pcapDir to save files to\n");
        exit(1);
    }

    if (!config.dryRun) {
        g_timeout_add_seconds( 10, moloch_config_reload_files, 0);
    }
}
/******************************************************************************/
void moloch_config_exit()
{
    g_key_file_free(molochKeyFile);

    if (config.nodeClass)
        g_free(config.nodeClass);
    if (config.interface)
        g_strfreev(config.interface);
    if (config.elasticsearch)
        g_free(config.elasticsearch);
    if (config.bpf)
        g_free(config.bpf);
    if (config.yara)
        g_free(config.yara);
    if (config.emailYara)
        g_free(config.emailYara);
    if (config.pcapDir)
        g_strfreev(config.pcapDir);
    if (config.pcapDirTemplate)
        g_free(config.pcapDirTemplate);
    if (config.pluginsDir)
        g_strfreev(config.pluginsDir);
    if (config.parsersDir)
        g_strfreev(config.parsersDir);
    if (config.plugins)
        g_strfreev(config.plugins);
    if (config.rootPlugins)
        g_strfreev(config.rootPlugins);
    if (config.smtpIpHeaders)
        g_strfreev(config.smtpIpHeaders);
}
