/******************************************************************************/
/* config.c  -- Functions dealing with the config file
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include "moloch.h"

extern MolochConfig_t        config;

static GKeyFile             *molochKeyFile;

/******************************************************************************/
gchar *moloch_config_str(GKeyFile *keyfile, char *key, char *d)
{
    if (!keyfile)
        keyfile = molochKeyFile;

    if (g_key_file_has_key(keyfile, config.nodeName, key, NULL)) {
        return g_key_file_get_string(keyfile, config.nodeName, key, NULL);
    }

    if (config.nodeClass && g_key_file_has_key(keyfile, config.nodeClass, key, NULL)) {
        return g_key_file_get_string(keyfile, config.nodeClass, key, NULL);
    }

    if (g_key_file_has_key(keyfile, "default", key, NULL)) {
        return g_key_file_get_string(keyfile, "default", key, NULL);
    }

    if (!d)
        return NULL;

    return g_strdup(d);
}

/******************************************************************************/
gchar **moloch_config_raw_str_list(GKeyFile *keyfile, char *key, char *d)
{
    if (!keyfile)
        keyfile = molochKeyFile;

    if (g_key_file_has_key(keyfile, config.nodeName, key, NULL)) {
        return g_key_file_get_string_list(keyfile, config.nodeName, key, NULL, NULL);
    }

    if (config.nodeClass && g_key_file_has_key(keyfile, config.nodeClass, key, NULL)) {
        return g_key_file_get_string_list(keyfile, config.nodeClass, key, NULL, NULL);
    }

    if (g_key_file_has_key(keyfile, "default", key, NULL)) {
        return g_key_file_get_string_list(keyfile, "default", key, NULL, NULL);
    }

    if (!d)
        return NULL;

    return g_strsplit(d, ";", 0);
}

/******************************************************************************/
gchar **moloch_config_str_list(GKeyFile *keyfile, char *key, char *d)
{
    gchar **strs = moloch_config_raw_str_list(keyfile, key, d);
    if (!strs)
        return strs;

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

    if (value < min)
        value = min;
    if (value > max)
        value = max;

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

    config.dontSaveBPFs     = moloch_config_str_list(keyfile, "dontSaveBPFs", NULL);
    if (config.dontSaveBPFs) {
        for (i = 0; config.dontSaveBPFs[i]; i++);
        config.dontSaveBPFsNum = i;
        config.dontSaveBPFsStop = malloc(config.dontSaveBPFsNum*sizeof(int));

        GRegex     *regex = g_regex_new(":\\s*(\\d+)\\s*$", 0, 0, 0);
        GMatchInfo *match_info;
        for (i = 0; config.dontSaveBPFs[i]; i++) {
            g_regex_match(regex, config.dontSaveBPFs[i], 0, &match_info);
            if (g_match_info_matches(match_info)) {
                config.dontSaveBPFsStop[i] = atoi(g_match_info_fetch(match_info, 1));
                gint pos;
                g_match_info_fetch_pos(match_info, 0, &pos, NULL);
                config.dontSaveBPFs[i][pos] = 0;
            } else {
                config.dontSaveBPFsStop[i] = 1;
            }
            g_match_info_free(match_info);
        }
        g_regex_unref(regex);
    }

    config.plugins          = moloch_config_str_list(keyfile, "plugins", NULL);
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
    config.interface        = moloch_config_str(keyfile, "interface", NULL);
    config.pcapDir          = moloch_config_str_list(keyfile, "pcapDir", NULL);
    config.bpf              = moloch_config_str(keyfile, "bpf", NULL);
    config.yara             = moloch_config_str(keyfile, "yara", NULL);
    config.emailYara        = moloch_config_str(keyfile, "emailYara", NULL);
    config.geoipFile        = moloch_config_str(keyfile, "geoipFile", NULL);
    config.rirFile          = moloch_config_str(keyfile, "rirFile", NULL);
    config.geoipASNFile     = moloch_config_str(keyfile, "geoipASNFile", NULL);
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

    config.maxFileSizeG          = moloch_config_double(keyfile, "maxFileSizeG", 4, 0.01, 1024);
    config.maxFileSizeB          = config.maxFileSizeG*1024LL*1024LL*1024LL;
    config.maxFileTimeM          = moloch_config_int(keyfile, "maxFileTimeM", 0, 0, 0xffff);
    config.icmpTimeout           = moloch_config_int(keyfile, "icmpTimeout", 10, 1, 0xffff);
    config.udpTimeout            = moloch_config_int(keyfile, "udpTimeout", 60, 1, 0xffff);
    config.tcpTimeout            = moloch_config_int(keyfile, "tcpTimeout", 60*8, 10, 0xffff);
    config.tcpSaveTimeout        = moloch_config_int(keyfile, "tcpSaveTimeout", 60*8, 10, 60*120);
    config.maxStreams            = moloch_config_int(keyfile, "maxStreams", 1500000, 1, 16777215);
    config.maxPackets            = moloch_config_int(keyfile, "maxPackets", 10000, 1, 1000000);
    config.minFreeSpaceG         = moloch_config_int(keyfile, "freeSpaceG", 100, 1, 100000);
    config.dbBulkSize            = moloch_config_int(keyfile, "dbBulkSize", 200000, MOLOCH_HTTP_BUFFER_SIZE*2, 1000000);
    config.dbFlushTimeout        = moloch_config_int(keyfile, "dbFlushTimeout", 5, 1, 60*30);
    config.maxESConns            = moloch_config_int(keyfile, "maxESConns", 20, 5, 1000);
    config.maxESRequests         = moloch_config_int(keyfile, "maxESRequests", 500, 10, 5000);
    config.logEveryXPackets      = moloch_config_int(keyfile, "logEveryXPackets", 50000, 1000, 1000000);
    config.packetsPerPoll        = moloch_config_int(keyfile, "packetsPerPoll", 50000, 1000, 1000000);
    config.pcapBufferSize        = moloch_config_int(keyfile, "pcapBufferSize", 300000000, 100000, 0xffffffff);
    config.pcapWriteSize         = moloch_config_int(keyfile, "pcapWriteSize", 0x40000, 0x40000, 0x800000);
    config.maxFreeOutputBuffers  = moloch_config_int(keyfile, "maxFreeOutputBuffers", 50, 0, 0xffff);


    config.logUnknownProtocols   = moloch_config_boolean(keyfile, "logUnknownProtocols", config.debug);
    config.logESRequests         = moloch_config_boolean(keyfile, "logESRequests", config.debug);
    config.logFileCreation       = moloch_config_boolean(keyfile, "logFileCreation", config.debug);
    config.parseSMTP             = moloch_config_boolean(keyfile, "parseSMTP", TRUE);
    config.parseSMB              = moloch_config_boolean(keyfile, "parseSMB", TRUE);
    config.parseQSValue          = moloch_config_boolean(keyfile, "parseQSValue", FALSE);
    config.parseCookieValue      = moloch_config_boolean(keyfile, "parseCookieValue", FALSE);
    config.compressES            = moloch_config_boolean(keyfile, "compressES", FALSE);
    config.antiSynDrop           = moloch_config_boolean(keyfile, "antiSynDrop", TRUE);

}
/******************************************************************************/
void moloch_config_get_tag_cb(MolochIpInfo_t *ii, int UNUSED(tagtype), const char UNUSED(*tagName), uint32_t tag)
{
    if (ii->numtags >= 10) return;

    ii->tags[ii->numtags++] = tag;
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
        LOG("Error with override-ips: %s", error->message);
        exit(1);
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
                moloch_db_get_tag(ii, 0, values[v]+4, (MolochTag_cb)moloch_config_get_tag_cb);
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
        LOG("Error with %s: %s", section, error->message);
        exit(1);
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
            kind = "textfield";
            if (unique)
                t = MOLOCH_FIELD_TYPE_STR_HASH;
            else
                t = MOLOCH_FIELD_TYPE_STR_ARRAY;
            break;
        case 1:
            if (unique)
                t = MOLOCH_FIELD_TYPE_INT_HASH;
            else
                t = MOLOCH_FIELD_TYPE_INT_ARRAY;
            break;
        case 2:
            kind = "ip";
            t = MOLOCH_FIELD_TYPE_IP_HASH;
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
        char rawfield[100];
        char help[100];
        
        if (type == 0) {
            sprintf(expression, "%s%s", expBase, name);
            sprintf(field, "%s%s.snow", dbBase, name);
            sprintf(rawfield, "%s%s.raw", dbBase, name);
            sprintf(help, "%s%s", helpBase, name);
        } else {
            sprintf(expression, "%s%s", expBase, name);
            sprintf(field, "%s%s", dbBase, name);
            rawfield[0] = 0;
            sprintf(help, "%s%s", helpBase, name);
        }

        int pos;
        if (rawfield[0]) {
            pos = moloch_field_define(group, kind,
                    expression, expression, field,
                    help,
                    t, f, 
                    "rawField", rawfield, NULL);
        } else {
            pos = moloch_field_define(group, kind,
                    expression, expression, field,
                    help,
                    t, f, NULL);
        }
        moloch_config_add_header(hash, g_strdup(keys[k]), pos);
        g_strfreev(values);
    }
    g_strfreev(keys);
}
/******************************************************************************/
void moloch_config_init()
{
    int i;
    char *str;
    static char *rotates[] = {"hourly", "daily", "weekly", "monthly"};

    HASH_INIT(s_, config.dontSaveTags, moloch_string_hash, moloch_string_cmp);

    moloch_config_load();

    if (config.debug) {
        LOG("nodeClass: %s", config.nodeClass);
        LOG("elasticsearch: %s", config.elasticsearch);
        LOG("prefix: %s", config.prefix);
        LOG("interface: %s", config.interface);
        if (config.pcapDir) {
            str = g_strjoinv(";", config.pcapDir);
            LOG("pcapDir: %s", str);
            g_free(str);
        }
        LOG("bpf: %s", config.bpf);
        LOG("yara: %s", config.yara);
        LOG("geoipFile: %s", config.geoipFile);
        LOG("geoipASNFile: %s", config.geoipASNFile);
        LOG("rirFile: %s", config.rirFile);
        LOG("dropUser: %s", config.dropUser);
        LOG("dropGroup: %s", config.dropGroup);

        if (config.smtpIpHeaders) {
            str = g_strjoinv(";", config.smtpIpHeaders);
            LOG("smtpIpHeaders: %s", str);
            g_free(str);
        }

        if (config.pluginsDir) {
            str = g_strjoinv(";", config.pluginsDir);
            LOG("pluginsDir: %s", str);
            g_free(str);
        }

        if (config.plugins) {
            str = g_strjoinv(";", config.plugins);
            LOG("plugins: %s", str);
            g_free(str);
        }

        if (config.parsersDir) {
            str = g_strjoinv(";", config.parsersDir);
            LOG("parsersDir: %s", str);
            g_free(str);
        }

        LOG("maxFileSizeG: %lf", config.maxFileSizeG);
        LOG("maxFileSizeB: %ld", config.maxFileSizeB);
        LOG("maxFileTimeM: %u", config.maxFileTimeM);
        LOG("icmpTimeout: %u", config.icmpTimeout);
        LOG("udpTimeout: %u", config.udpTimeout);
        LOG("tcpTimeout: %u", config.tcpTimeout);
        LOG("tcpSaveTimeout: %u", config.tcpSaveTimeout);
        LOG("maxStreams: %u", config.maxStreams);
        LOG("maxPackets: %u", config.maxPackets);
        LOG("minFreeSpaceG: %u", config.minFreeSpaceG);
        LOG("dbBulkSize: %u", config.dbBulkSize);
        LOG("dbFlushTimeout: %u", config.dbFlushTimeout);
        LOG("maxESConns: %u", config.maxESConns);
        LOG("maxESRequests: %u", config.maxESRequests);
        LOG("logEveryXPackets: %u", config.logEveryXPackets);
        LOG("packetsPerPoll: %u", config.packetsPerPoll);
        LOG("pcapBufferSize: %u", config.pcapBufferSize);
        LOG("pcapWriteSize: %u", config.pcapWriteSize);
        LOG("maxFreeOutputBuffers: %u", config.maxFreeOutputBuffers);

        LOG("logUnknownProtocols: %s", (config.logUnknownProtocols?"true":"false"));
        LOG("logESRequests: %s", (config.logESRequests?"true":"false"));
        LOG("logFileCreation: %s", (config.logFileCreation?"true":"false"));
        LOG("parseSMTP: %s", (config.parseSMTP?"true":"false"));
        LOG("parseSMB: %s", (config.parseSMB?"true":"false"));
        LOG("parseQSValue: %s", (config.parseQSValue?"true":"false"));
        LOG("parseCookieValue: %s", (config.parseCookieValue?"true":"false"));
        LOG("compressES: %s", (config.compressES?"true":"false"));

        LOG("rotateIndex = %s", rotates[config.rotate]);
        LOG("offlineFilenameRegex: %s", g_regex_get_pattern(config.offlineRegex));

        MolochString_t *tstring;
        HASH_FORALL(s_, config.dontSaveTags, tstring,
          LOG("dontSaveTags: %s", tstring->str);
        );

        for (i = 0; i < config.dontSaveBPFsNum; i++) {
          LOG("dontSaveBPFs: %s:%d", config.dontSaveBPFs[i], config.dontSaveBPFsStop[i]);
        }
    }

    if (!config.interface && !config.pcapReadOffline) {
        printf("Need to set interface, pcap file (-r) or pcap directory (-R) \n");
        exit (1);
    }

    if (!config.pcapDir) {
        printf("Must set a pcapDir to save files to\n");
        exit(1);
    }
}
/******************************************************************************/
void moloch_config_exit()
{
    g_key_file_free(molochKeyFile);

    if (config.nodeClass)
        g_free(config.nodeClass);
    if (config.interface)
        g_free(config.interface);
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
    if (config.pluginsDir)
        g_strfreev(config.pluginsDir);
    if (config.parsersDir)
        g_strfreev(config.parsersDir);
    if (config.plugins)
        g_strfreev(config.plugins);
    if (config.smtpIpHeaders)
        g_strfreev(config.smtpIpHeaders);
}
