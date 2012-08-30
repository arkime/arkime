/******************************************************************************/
/* db.c  -- Functions dealing with database queries and updates
 *
 * Copyright 2012 The AOL Moloch Authors.  All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <string.h>
#include <netdb.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/statvfs.h>
#include "glib.h"
#include "moloch.h"
#include "GeoIP.h"

extern char          *connectString; 
extern char          *nodeName; 
extern char          *hostName; 
extern uint64_t       totalPackets;
extern uint64_t       totalBytes;
extern uint64_t       totalSessions;
static uint16_t       myPid;
static time_t         dbLastSave;

struct timeval        startTime;
static GeoIP         *gi = 0;
static GeoIP         *giASN = 0;

/******************************************************************************/
extern MolochConfig_t config;

/******************************************************************************/
typedef struct moloch_tag {
    struct moloch_tag *tag_next, *tag_prev;
    char              *tagName;
    int                tagValue;
    short              tag_bucket;
    short              tag_count;
} MolochTag_t;

HASH_VAR(tag_, tags, MolochTag_t, 9337);

/******************************************************************************/
uint32_t moloch_db_tag_hash(const void *key)
{
    char *p = (char *)key;
    uint32_t n = 0;
    while (*p) {
        n = (n << 5) - n + *p;
        p++;
    }
    return n;
}

/******************************************************************************/
int moloch_db_tag_cmp(const void *keyv, const void *elementv)
{
    char *key = (char*)keyv;
    MolochTag_t *element = (MolochTag_t *)elementv;

    return strcmp(key, element->tagName) == 0;
}

/******************************************************************************/
int moloch_db_js0n_str(char *str, unsigned char *in)
{
    char *out = str;

    *(out++) = '"';
    while (*in) {
        switch(*in) {
        case '\b':
            *(out++) = '\\';
            *(out++) = 'b';
            break;
        case '\n':
            *(out++) = '\\';
            *(out++) = 'n';
            break;
        case '\r':
            *(out++) = '\\';
            *(out++) = 'r';
            break;
        case '\f':
            *(out++) = '\\';
            *(out++) = 'f';
            break;
        case '\t':
            *(out++) = '\\';
            *(out++) = 't';
            break;
        case '"':
            *(out++) = '\\';
            *(out++) = '"';
            break;
        case '\\':
            *(out++) = '\\';
            *(out++) = '\\';
            break;
        case '/':
            *(out++) = '\\';
            *(out++) = '/';
            break;
        default:
            if(*in < 32) 
                out += sprintf(out, "\\u%04x", *in);
            else if(*in & 0x80) {
                *(out++) = (0xc0 | (*in >> 6));
                *(out++) = (0x80 | (*in & 0x3f));
            } else
                *(out++) = *in;
            break;
        }
        in++;
    }


    *(out++) = '"';
    *out = 0;
    return out-str;
}
/******************************************************************************/
static char *sJson = 0;
static char *sJPtr;

void moloch_db_save_session(MolochSession_t *session)
{
    GHashTableIter  iter;
    gpointer        keyp, valuep;
    uint32_t        i;
    char            id[100];
    char            key[100];
    int             key_len;
    uuid_t          uuid;

    /* No Packets */
    if (!session->filePosArray->len)
        return;

    if (!sJson) {
        sJPtr = sJson = moloch_es_get_buffer(MOLOCH_ES_BUFFER_SIZE_L);
    }

    static char     prefix[100];
    static time_t   prefix_time = 0;

    if (prefix_time != session->lastPacket) {
        prefix_time = session->lastPacket;
        struct tm *tmp = gmtime(&prefix_time);

        snprintf(prefix, sizeof(prefix), "%02d%02d%02d", tmp->tm_year%100, tmp->tm_mon+1, tmp->tm_mday);
    }
    uint32_t id_len = snprintf(id, sizeof(id), "%s-", prefix);

    uuid_generate(uuid);
    gint state = 0, save = 0;
    id_len += g_base64_encode_step((guchar*)&myPid, 2, FALSE, id + id_len, &state, &save);
    id_len += g_base64_encode_step(uuid, sizeof(uuid_t), FALSE, id + id_len, &state, &save);
    id_len += g_base64_encode_close(FALSE, id + id_len, &state, &save);
    id[id_len] = 0;

    for (i = 0; i < id_len; i++) {
        if (id[i] == '+') id[i] = '-';
        else if (id[i] == '/') id[i] = '_';
    }

    key_len = snprintf(key, sizeof(key), "/_bulk");
    
    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "{\"index\": {\"_index\": \"sessions-%s\", \"_type\": \"session\", \"_id\": \"%s\"}}\n", prefix, id);

    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "{");
    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"fp\":%u,", session->firstPacket);
    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"lp\":%u,", session->lastPacket);
    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"a1\":%u,", htonl(session->addr1));
    if (gi) {
        const char *g1 = GeoIP_country_code3_by_ipnum(gi, htonl(session->addr1));
        if (g1)
            sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"g1\":\"%s\",", g1);
    }
    if (giASN) {
        char *as1 = GeoIP_name_by_ipnum(giASN, htonl(session->addr1));
        if (as1) {
            sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"as1\":");
            sJPtr += moloch_db_js0n_str(sJPtr, (unsigned char*)as1);
            *(sJPtr++) = ',';
            free(as1);
        }
    }
    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"p1\":%u,", session->port1);
    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"a2\":%u,", htonl(session->addr2));
    if (gi) {
        const char *g2 = GeoIP_country_code3_by_ipnum(gi, htonl(session->addr2));
        if (g2)
            sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"g2\":\"%s\",", g2);
    }
    if (giASN) {
        char *as2 = GeoIP_name_by_ipnum(giASN, htonl(session->addr2));
        if (as2) {
            sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"as2\":");
            sJPtr += moloch_db_js0n_str(sJPtr, (unsigned char*)as2);
            *(sJPtr++) = ',';
            free(as2);
        }
    }
    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"p2\":%u,", session->port2);
    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"pr\":%u,", session->protocol);
    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"pa\":%u,", session->filePosArray->len);
    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"by\":%lu,", session->bytes);
    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"db\":%lu,", session->databytes);
    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"no\":\"%s\",", nodeName);
    if (session->rootId) {
        if (session->rootId[0] == 'R')
            session->rootId = g_strdup(id);
        sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"ro\":\"%s\",", session->rootId);
    }
    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"ps\":[");
    for(i = 0; i < session->filePosArray->len; i++) {
        if (i == 0)
            sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "%lu", (uint64_t)g_array_index(session->filePosArray, uint64_t, i));
        else
            sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), ",%lu", (uint64_t)g_array_index(session->filePosArray, uint64_t, i));
    }
    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "],");
    if (session->urlArray->len) {
        sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"us\":[");
        for(i = 0; i < session->urlArray->len; i++) {
            if (i != 0)
                *(sJPtr++) = ',';
            sJPtr += moloch_db_js0n_str(sJPtr, g_ptr_array_index(session->urlArray, i));
        }
        sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "],");
    }

    if (HASH_COUNT(s_, session->hosts)) {
        sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"ho\":[");
        i = 0;

        MolochString_t *hstring;
        HASH_FORALL_POP_HEAD(s_, session->hosts, hstring, 
            if (i != 0)
                *(sJPtr++) = ',';
            sJPtr += moloch_db_js0n_str(sJPtr, (unsigned char *)hstring->str);
            free(hstring->str);
            free(hstring);
            i++;
        );

        sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "],");
    }

    if (HASH_COUNT(s_, session->userAgents)) {
        sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"ua\":[");
        i = 0;

        MolochString_t *ustring;
        HASH_FORALL_POP_HEAD(s_, session->userAgents, ustring, 
            if (i != 0)
                *(sJPtr++) = ',';
            sJPtr += moloch_db_js0n_str(sJPtr, (unsigned char *)ustring->str);
            free(ustring->str);
            free(ustring);
            i++;
        );

        sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "],");
    }

    if (HASH_COUNT(i_, session->xffs)) {

        MolochInt_t *xff;

        if (gi) {
            sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"gxff\":[");
            i = 0;
            HASH_FORALL(i_, session->xffs, xff, 
                if (i != 0)
                    *(sJPtr++) = ',';
                const char *g = GeoIP_country_code3_by_ipnum(gi, htonl(xff->i));
                if (g)
                    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"%s\"", g);
                else
                    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"---\"");
                i++;
            );
            sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "],");
        }

        if (giASN) {
            sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"asxff\":[");
            i = 0;
            HASH_FORALL(i_, session->xffs, xff, 
                if (i != 0)
                    *(sJPtr++) = ',';
                char *as = GeoIP_name_by_ipnum(giASN, htonl(xff->i));
                if (as) {
                    sJPtr += moloch_db_js0n_str(sJPtr, (unsigned char*)as);
                    free(as);
                } else
                    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"---\"");
                i++;
            );
            sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "],");
        }


        sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"xff\":[");
        i = 0;
        HASH_FORALL_POP_HEAD(i_, session->xffs, xff, 
            if (i != 0)
                *(sJPtr++) = ',';
            sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "%u", htonl(xff->i));
            free(xff);
            i++;
        );

        sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "],");
    }

    if (g_hash_table_size(session->tags[0])) {
        sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"ta\":[");
        g_hash_table_iter_init (&iter, session->tags[0]);
        i = 0;
        while (g_hash_table_iter_next (&iter, &keyp, &valuep)) {
            if (i != 0)
                *(sJPtr++) = ',';
            sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "%lu", (uint64_t)keyp);
            i++;
        }
        sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "],");
    }

    if (g_hash_table_size(session->tags[1])) {
        sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"hh\":[");
        g_hash_table_iter_init (&iter, session->tags[1]);
        i = 0;
        while (g_hash_table_iter_next (&iter, &keyp, &valuep)) {
            if (i != 0)
                *(sJPtr++) = ',';
            sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "%lu", (uint64_t)keyp);
            i++;
        }
        sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "],");
    }


    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "\"fs\":[");
    for(i = 0; i < session->fileNumArray->len; i++) {
        if (i == 0)
            sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "%u", (uint32_t)g_array_index(session->fileNumArray, uint32_t, i));
        else
            sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), ",%u", (uint32_t)g_array_index(session->fileNumArray, uint32_t, i));
    }
    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "]");
    sJPtr += snprintf(sJPtr, MOLOCH_ES_BUFFER_SIZE_L - (sJPtr-sJson), "}\n");

    if (sJPtr - sJson >= MOLOCH_ES_BUFFER_SIZE_L) {
        LOG("MEMORY OUT OF BOUNDS ERROR - %ld", (long)(sJPtr - sJson));
    }


    if (sJPtr - sJson > config.dbBulkSize) {
        //printf("Sending: %ld %ld\n", sJPtr - sessionJson, totalSessions);
        //printf("%.*s", sJPtr - sessionJson, sessionJson);
        moloch_es_set(key, key_len, sJson, sJPtr - sJson, NULL, NULL);
        sJson = 0;

        struct timeval currentTime;
        gettimeofday(&currentTime, NULL);
        dbLastSave = currentTime.tv_sec;
    }
    totalSessions++;
}
/******************************************************************************/
long long zero_atoll(char *v) {
    if (v)
        return atoll(v);
    return 0;
}

/******************************************************************************/
static uint64_t dbTotalPackets = 0;
static uint64_t dbTotalK = 0;
static uint64_t dbTotalSessions = 0;
static uint64_t dbTotalDropped = 0;
static struct timeval dbLastTime;

static char     stats_key[200];
static int      stats_key_len = 0;

void moloch_db_load_stats()
{
    size_t             datalen;
    uint32_t           len;
    uint32_t           source_len;
    unsigned char     *source = 0;

    stats_key_len = snprintf(stats_key, sizeof(stats_key), "/stats/stat/%s", nodeName);

    unsigned char     *data = moloch_es_get(stats_key, stats_key_len, &datalen);

    source = moloch_js0n_get(data, datalen, "_source", &source_len);
    if (source) {
        dbTotalPackets = zero_atoll((char*)moloch_js0n_get(source, source_len, "totalPackets", &len));
        dbTotalK = zero_atoll((char*)moloch_js0n_get(source, source_len, "totalK", &len));
        dbTotalSessions = zero_atoll((char*)moloch_js0n_get(source, source_len, "totalSessions", &len));
        dbTotalDropped = zero_atoll((char*)moloch_js0n_get(source, source_len, "totalDropped", &len));
    }
    gettimeofday(&dbLastTime, 0);
}
/******************************************************************************/
void moloch_db_update_stats()
{
    static uint64_t lastPackets = 0;
    static uint64_t lastBytes = 0;
    static uint64_t lastSessions = 0;
    static uint64_t lastDropped = 0;

    char *json = moloch_es_get_buffer(MOLOCH_ES_BUFFER_SIZE_S);
    struct timeval currentTime;

    gettimeofday(&currentTime, NULL);

    if (currentTime.tv_sec == dbLastTime.tv_sec)
        return;

    uint64_t totalDropped = moloch_nids_dropped_packets();

    struct statvfs vfs;
    statvfs(config.pcapDir, &vfs);

    dbTotalPackets += (totalPackets - lastPackets);
    dbTotalSessions += (totalSessions - lastSessions);
    dbTotalDropped += (totalDropped - lastDropped);
    dbTotalK += (totalBytes - lastBytes)/1024;
    int json_len = snprintf(json, MOLOCH_ES_BUFFER_SIZE_S, 
        "{"
        "\"hostname\": \"%s\", "
        "\"currentTime\": %" PRIu64 ", "
        "\"freeSpaceM\": %" PRIu64 ", "
        "\"monitoring\": %u, "
        "\"totalPackets\": %" PRIu64 ", "
        "\"totalK\": %" PRIu64 ", "
        "\"totalSessions\": %" PRIu64 ", "
        "\"totalDropped\": %" PRIu64 ", "
        "\"deltaPackets\": %" PRIu64 ", "
        "\"deltaBytes\": %" PRIu64 ", "
        "\"deltaSessions\": %" PRIu64 ", "
        "\"deltaDropped\": %" PRIu64 ", "
        "\"deltaMS\": %" PRIu64 
        "}",
        hostName,
        currentTime.tv_sec,
        (uint64_t)(vfs.f_frsize/1024.0*vfs.f_bavail/1024.0),
        moloch_nids_monitoring_sessions(),
        dbTotalPackets,
        dbTotalK,
        dbTotalSessions,
        dbTotalDropped,
        (totalPackets - lastPackets), 
        (totalBytes - lastBytes), 
        (totalSessions - lastSessions), 
        (totalDropped - lastDropped), 
        (currentTime.tv_sec - dbLastTime.tv_sec)*1000 + (currentTime.tv_usec/1000 - dbLastTime.tv_usec/1000));

    dbLastTime   = currentTime;
    lastBytes    = totalBytes;
    lastPackets  = totalPackets;
    lastSessions = totalSessions;
    lastDropped  = totalDropped;
    moloch_es_set(stats_key, stats_key_len, json, json_len, NULL, NULL);
}

/******************************************************************************/
void moloch_db_update_dstats(int n)
{
    static uint64_t lastPackets[2] = {0, 0};
    static uint64_t lastBytes[2] = {0, 0};
    static uint64_t lastSessions[2] = {0, 0};
    static uint64_t lastDropped[2] = {0, 0};
    static struct timeval lastTime[2];
    static int      intervals[2] = {5, 60};
    char key[200];
    int  key_len = 0;

    char *json = moloch_es_get_buffer(MOLOCH_ES_BUFFER_SIZE_S);
    struct timeval currentTime;

    gettimeofday(&currentTime, NULL);

    key_len = snprintf(key, sizeof(key), "/dstats/dstat/%s-%d-%d", nodeName, (int)(currentTime.tv_sec/intervals[n])%1440, intervals[n]);
    if (lastPackets[n] == 0) {
        lastTime[n] = startTime;
    }

    uint64_t totalDropped = moloch_nids_dropped_packets();

    struct statvfs vfs;
    statvfs(config.pcapDir, &vfs);

    int json_len = snprintf(json, MOLOCH_ES_BUFFER_SIZE_S, 
        "{"
        "\"nodeName\": \"%s\", "
        "\"interval\": %d, "
        "\"currentTime\": %" PRIu64 ", "
        "\"freeSpaceM\": %" PRIu64 ", "
        "\"monitoring\": %u, "
        "\"deltaPackets\": %" PRIu64 ", "
        "\"deltaBytes\": %" PRIu64 ", "
        "\"deltaSessions\": %" PRIu64 ", "
        "\"deltaDropped\": %" PRIu64 ", "
        "\"deltaMS\": %" PRIu64 
        "}",
        nodeName,
        intervals[n],
        currentTime.tv_sec,
        (uint64_t)(vfs.f_frsize/1024.0*vfs.f_bavail/1024.0),
        moloch_nids_monitoring_sessions(),
        (totalPackets - lastPackets[n]), 
        (totalBytes - lastBytes[n]), 
        (totalSessions - lastSessions[n]), 
        (totalDropped - lastDropped[n]), 
        (currentTime.tv_sec - lastTime[n].tv_sec)*1000 + (currentTime.tv_usec/1000 - lastTime[n].tv_usec/1000));

    lastTime[n]     = currentTime;
    lastBytes[n]    = totalBytes;
    lastPackets[n]  = totalPackets;
    lastSessions[n] = totalSessions;
    lastDropped[n]  = totalDropped;
    moloch_es_set(key, key_len, json, json_len, NULL, NULL);
}
/******************************************************************************/
gboolean moloch_db_update_stats_gfunc (gpointer user_data)
{
    if (user_data == (gpointer)0)
        moloch_db_update_stats();
    else if (user_data == (gpointer)1)
        moloch_db_update_dstats(0);
    else if (user_data == (gpointer)2)
        moloch_db_update_dstats(1);

    return TRUE;
}
/******************************************************************************/
gboolean moloch_db_flush_gfunc (gpointer user_data )
{
    char            key[100];
    int             key_len;

    if (!sJson || sJPtr == sJson)
        return TRUE;

    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    if (user_data == 0 && (currentTime.tv_sec - dbLastSave) < 5)
        return TRUE;

    key_len = snprintf(key, sizeof(key), "/_bulk");
    moloch_es_set(key, key_len, sJson, sJPtr - sJson, NULL, NULL);
    sJson = 0;
    dbLastSave = currentTime.tv_sec;

    return TRUE;
}
/******************************************************************************/
typedef struct moloch_seq_request {
    MolochSeqNum_cb     func;
    gpointer            uw;

} MolochSeqRequest_t;

void moloch_db_get_sequence_number_cb(unsigned char *data, int data_len, gpointer uw)
{
    MolochSeqRequest_t *r = uw;
    uint32_t            version_len;

    unsigned char *version = moloch_js0n_get(data, data_len, "_version", &version_len);

    if (!version_len || !version) {
        if (r->func)
            r->func(1, r->uw);
    } else {
        if (r->func)
            r->func(atoi((char*)version), r->uw);
    }
    free(r);
}
/******************************************************************************/
void moloch_db_get_sequence_number(char *name, MolochSeqNum_cb func, gpointer uw) 
{
    char                key[100];
    int                 key_len;
    MolochSeqRequest_t *r = malloc(sizeof(MolochSeqRequest_t));
    char               *json = moloch_es_get_buffer(MOLOCH_ES_BUFFER_SIZE_S);

    r->func = func;
    r->uw   = uw;

    key_len = snprintf(key, sizeof(key), "/sequence/sequence/%s", name);
    int json_len = snprintf(json, MOLOCH_ES_BUFFER_SIZE_S, "{}");
    moloch_es_set(key, key_len, json, json_len, moloch_db_get_sequence_number_cb, r);
}

/******************************************************************************/
static int         lastFileNum = 0;
void moloch_db_load_file_num()
{
    char               key[200];
    int                key_len;
    size_t             data_len;
    uint32_t           len;
    unsigned char     *value;

    lastFileNum = 0;

    key_len = snprintf(key, sizeof(key), "/files/file/_search?size=1&sort=num:desc&q=node:%s", nodeName);

    unsigned char     *data = moloch_es_get(key, key_len, &data_len);

    uint32_t           hits_len;
    unsigned char     *hits = moloch_js0n_get(data, data_len, "hits", &hits_len);

    if (!hits_len || !hits)
        return;

    uint32_t           hit_len;
    unsigned char     *hit = moloch_js0n_get(hits, hits_len, "hits", &hit_len);

    if (!hit_len || !hit)
        return;

    /* Remove array wrapper */
    uint32_t           source_len;
    unsigned char     *source = moloch_js0n_get(hit+1, hit_len-2, "_source", &source_len);

    if (!source_len || !source)
        return;

    if ((value = moloch_js0n_get(source, source_len, "num", &len))) {
        lastFileNum = atoi((char*)value);
    } else {
        LOG("ERROR - No num field in %.*s", source_len, source);
        exit (0);
    }
}
/******************************************************************************/
char *moloch_db_create_file(time_t firstPacket, uint32_t *id)
{
    char               key[100];
    int                key_len;
    int                num = ++lastFileNum;
    static char        filename[1024];
    struct tm         *tmp;
    char              *json = moloch_es_get_buffer(MOLOCH_ES_BUFFER_SIZE_S);
    
    tmp = localtime(&firstPacket);

    strcpy(filename, config.pcapDir);
    if (filename[strlen(filename)-1] != '/')
        strcat(filename, "/");
    sprintf(filename+strlen(filename), "%s-%02d%02d%02d-%08d.pcap", nodeName, tmp->tm_year%100, tmp->tm_mon+1, tmp->tm_mday, num);

    int json_len = snprintf(json, MOLOCH_ES_BUFFER_SIZE_S, "{\"num\":%d, \"name\":\"%s\", \"first\":%ld, \"node\":\"%s\"}", num, filename, firstPacket, nodeName);
    key_len = snprintf(key, sizeof(key), "/files/file/%s-%d", nodeName,num);

    moloch_es_set(key, key_len, json, json_len, NULL, NULL);

    if (config.logFileCreation)
        LOG("Creating file %d with key >%s< using >%s<", num, key, json);

    *id = num;
    return filename;
}

/******************************************************************************/
void moloch_db_load_tags() 
{
    size_t             datalen;
    char               key[100];
    int                key_len;

    key_len = snprintf(key, sizeof(key), "/tags/tag/_search?size=3000&fields=n");
    unsigned char     *data = moloch_es_get(key, key_len, &datalen);

    if (!data) {
        return;
    }

    uint32_t           hits_len;
    unsigned char     *hits = 0;
    hits = moloch_js0n_get(data, datalen, "hits", &hits_len);
    if (!hits) {
        return;
    }

    uint32_t           ahits_len;
    unsigned char     *ahits = 0;
    ahits = moloch_js0n_get(hits, hits_len, "hits", &ahits_len);
    
    if (!ahits)
        return;

    uint32_t out[2*8000];
    memset(out, 0, sizeof(out));
    js0n(ahits, ahits_len, out);
    int i;
    for (i = 0; out[i]; i+= 2) {
        uint32_t           id_len;
        unsigned char     *id = 0;
        id = moloch_js0n_get(ahits+out[i], out[i+1], "_id", &id_len);

        uint32_t           fields_len;
        unsigned char     *fields = 0;
        fields = moloch_js0n_get(ahits+out[i], out[i+1], "fields", &fields_len);
        if (!fields) {
            continue;
        }

        uint32_t           n_len;
        unsigned char     *n = 0;
        n = moloch_js0n_get(fields, fields_len, "n", &n_len);


        if (id && n) {
            MolochTag_t *tag = malloc(sizeof(MolochTag_t));
            tag->tagName = g_strndup((char*)id, (int)id_len);
            tag->tagValue = atol((char*)n);
            HASH_ADD(tag_, tags, tag->tagName, tag);
        } else {
            LOG ("ERROR - Could not load %.*s", out[i+1], ahits+out[i]);
        }
    }
}
typedef struct moloch_tag_request {
    struct moloch_tag_request *t_next, *t_prev;
    int                        t_count;
    MolochSession_t           *session;
    MolochTag_cb               func;
    int                        tagtype;
    char                      *tag;
    char                      *escaped;
    uint32_t                   newSeq;
} MolochTagRequest_t;

static int                     outstandingTagRequests = 0;
static MolochTagRequest_t      tagRequests;

void moloch_db_tag_cb(unsigned char *data, int data_len, gpointer uw);

/******************************************************************************/
void moloch_db_free_tag_request(MolochTagRequest_t *r)
{
    g_free(r->escaped);
    free(r->tag);
    free(r);
    outstandingTagRequests--;

    while (tagRequests.t_count > 0) {
        char               key[500];
        int                key_len;

        DLL_POP_HEAD(t_, &tagRequests, r);

        MolochTag_t *tag;
        HASH_FIND(tag_, tags, r->tag, tag);

        if (tag) {
            if (r->func)
                r->func(r->session, r->tagtype, tag->tagValue);
            continue;
        }

        key_len = snprintf(key, sizeof(key), "/tags/tag/%s?fields=n", r->escaped);
        moloch_es_send("GET", key, key_len, NULL, 0, 0, FALSE, moloch_db_tag_cb, r);
        outstandingTagRequests++;
        break;
    }
}
/******************************************************************************/

void moloch_db_tag_create_cb(unsigned char *data, int UNUSED(data_len), gpointer uw)
{
    MolochTagRequest_t *r = uw;
    char                key[500];
    int                 key_len;

    if (strstr((char *)data, "{\"error\":") != 0) {
        key_len = snprintf(key, sizeof(key), "/tags/tag/%s?fields=n", r->escaped);
        moloch_es_send("GET", key, key_len, NULL, 0, 0, FALSE, moloch_db_tag_cb, r);
        return;
    }

    MolochTag_t *tag = malloc(sizeof(MolochTag_t));
    tag->tagName = g_strdup(r->tag);
    tag->tagValue = r->newSeq;
    HASH_ADD(tag_, tags, tag->tagName, tag);

    if (r->func)
        r->func(r->session, r->tagtype, r->newSeq);
    moloch_db_free_tag_request(r);
}
/******************************************************************************/
void moloch_db_tag_seq_cb(uint32_t newSeq, gpointer uw)
{
    MolochTagRequest_t *r = uw;
    char                key[500];
    int                 key_len;
    char               *json = moloch_es_get_buffer(MOLOCH_ES_BUFFER_SIZE_S);

    r->newSeq = newSeq;

    key_len = snprintf(key, sizeof(key), "/tags/tag/%s?op_type=create", r->escaped);
    int json_len = snprintf(json, MOLOCH_ES_BUFFER_SIZE_S, "{\"n\":%u}", newSeq);

    moloch_es_set(key, key_len, json, json_len, moloch_db_tag_create_cb, r);
}
/******************************************************************************/
void moloch_db_tag_cb(unsigned char *data, int data_len, gpointer uw)
{
    MolochTagRequest_t *r = uw;

    if (!data) {
        if (r->func)
            r->func(r->session, r->tagtype, 0);
        moloch_db_free_tag_request(r);
        return;
    }

    uint32_t           fields_len;
    unsigned char     *fields = 0;
    fields = moloch_js0n_get(data, data_len, "fields", &fields_len);

    if (fields) {
        uint32_t           n_len;
        unsigned char     *n = 0;
        n = moloch_js0n_get(fields, fields_len, "n", &n_len);

        MolochTag_t *tag = malloc(sizeof(MolochTag_t));
        tag->tagName = g_strdup(r->tag);
        tag->tagValue = atol((char*)n);
        HASH_ADD(tag_, tags, tag->tagName, tag);

        if (r->func)
            r->func(r->session, r->tagtype, atol((char *)n));
        moloch_db_free_tag_request(r);
        return;
    }

    moloch_db_get_sequence_number("tags", moloch_db_tag_seq_cb, r) ;
}
/******************************************************************************/
void moloch_db_get_tag(MolochSession_t *session, int tagtype, const char *tagname, MolochTag_cb func)
{
    MolochTag_t *tag;
    HASH_FIND(tag_, tags, tagname, tag);

    if (tag) {
        if (func)
            func(session, tagtype, tag->tagValue);
        return;
    }

    MolochTagRequest_t *r = malloc(sizeof(MolochTagRequest_t));
    r->session = session;
    r->func    = func;
    r->tag     = strdup(tagname);
    r->tagtype = tagtype;
    r->escaped = g_uri_escape_string (tagname, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH_ELEMENT, 0);



    if (outstandingTagRequests == 0) {
        char               key[500];
        int                key_len;

        key_len = snprintf(key, sizeof(key), "/tags/tag/%s?fields=n", r->escaped);
        moloch_es_send("GET", key, key_len, NULL, 0, 0, FALSE, moloch_db_tag_cb, r);
        outstandingTagRequests++;
    } else {
        DLL_PUSH_TAIL(t_, &tagRequests, r);
    }
}
/******************************************************************************/
void moloch_db_init()
{
    DLL_INIT(t_, &tagRequests);
    HASH_INIT(tag_, tags, moloch_db_tag_hash, moloch_db_tag_cmp);
    moloch_db_load_file_num();
    myPid = getpid();
    gettimeofday(&startTime, NULL);
    moloch_db_load_tags();
    moloch_db_load_stats();

    if (config.geoipFile) {
        gi = GeoIP_open(config.geoipFile, GEOIP_MEMORY_CACHE);
        if (!gi) {
            printf("Couldn't initialize GeoIP %s from %s", strerror(errno), config.geoipFile);
            exit(1);
        }
    }

    if (config.geoipASNFile) {
        giASN = GeoIP_open(config.geoipASNFile, GEOIP_MEMORY_CACHE);
        if (!giASN) {
            printf("Couldn't initialize GeoIP ASN %s from %s", strerror(errno), config.geoipASNFile);
            exit(1);
        }
    }

    g_timeout_add_seconds( 2, moloch_db_update_stats_gfunc, 0);
    g_timeout_add_seconds( 5, moloch_db_update_stats_gfunc, (gpointer)1);
    g_timeout_add_seconds(60, moloch_db_update_stats_gfunc, (gpointer)2);
    g_timeout_add_seconds( 1, moloch_db_flush_gfunc, 0);
}
/******************************************************************************/
void moloch_db_exit()
{
    moloch_db_flush_gfunc((gpointer)1);
}
