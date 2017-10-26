/******************************************************************************/
/* db.c  -- Functions dealing with database queries and updates
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
#include "molochconfig.h"
#include <uuid/uuid.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include "patricia.h"
#include "GeoIP.h"

#define MOLOCH_MIN_DB_VERSION 34

extern uint64_t         totalPackets;
extern uint64_t         totalBytes;
extern uint64_t         totalSessions;
static uint16_t         myPid;
extern uint32_t         pluginsCbs;

LOCAL struct timeval    startTime;
LOCAL GeoIP            *gi = 0;
LOCAL GeoIP            *giASN = 0;
LOCAL GeoIP            *gi6 = 0;
LOCAL GeoIP            *giASN6 = 0;
LOCAL char             *rirs[256];

void *                  esServer = 0;

LOCAL patricia_tree_t  *ipTree = 0;

extern char            *moloch_char_to_hex;
extern unsigned char    moloch_char_to_hexstr[256][3];
extern unsigned char    moloch_hex_to_char[256][256];

LOCAL uint32_t          nextFileNum;
LOCAL MOLOCH_LOCK_DEFINE(nextFileNum);

/******************************************************************************/
extern MolochConfig_t        config;

/******************************************************************************/
typedef struct moloch_tag {
    struct moloch_tag *tag_next, *tag_prev;
    char              *tagName;
    uint32_t           tag_hash;
    int                tagValue;
    short              tag_bucket;
    short              tag_count;
} MolochTag_t;

HASH_VAR(tag_, tags, MolochTag_t, 19991);

/******************************************************************************/
void moloch_db_add_local_ip(char *str, MolochIpInfo_t *ii)
{
    patricia_node_t *node;
    if (!ipTree) {
        ipTree = New_Patricia(128);
    }
    node = make_and_lookup(ipTree, str);
    node->data = ii;
}
/******************************************************************************/
void moloch_db_free_local_ip(MolochIpInfo_t *ii)
{
    if (ii->country)
        g_free(ii->country);
    if (ii->asn)
        g_free(ii->asn);
    if (ii->rir)
        g_free(ii->rir);
    MOLOCH_TYPE_FREE(MolochIpInfo_t, ii);
}
/******************************************************************************/
MolochIpInfo_t *moloch_db_get_local_ip6(MolochSession_t *session, struct in6_addr *ip)
{
    prefix_t prefix;
    patricia_node_t *node;

    if (IN6_IS_ADDR_V4MAPPED(ip)) {
        prefix.family = AF_INET;
        prefix.bitlen = 32;
        prefix.add.sin.s_addr = ((uint32_t *)ip->s6_addr)[3];
    } else {
        prefix.family = AF_INET6;
        prefix.bitlen = 128;
        memcpy(&prefix.add.sin6.s6_addr, ip, 16);
    }

    if ((node = patricia_search_best2 (ipTree, &prefix, 1)) == NULL)
        return 0;

    MolochIpInfo_t *ii = node->data;
    int t;

    for (t = 0; t < ii->numtags; t++) {
        moloch_field_int_add(config.tagsField, session, ii->tags[t]);
        moloch_field_string_add(config.tagsStringField, session, ii->tagsStr[t], -1, TRUE);
    }

    return ii;
}
/******************************************************************************/
MolochIpInfo_t *moloch_db_get_local_ip4(MolochSession_t *session, uint32_t ip)
{
    prefix_t prefix;
    patricia_node_t *node;

    prefix.family = AF_INET;
    prefix.bitlen = 32;
    prefix.add.sin.s_addr = ip;

    if ((node = patricia_search_best2 (ipTree, &prefix, 1)) == NULL)
        return 0;

    MolochIpInfo_t *ii = node->data;
    int t;

    for (t = 0; t < ii->numtags; t++) {
        moloch_field_int_add(config.tagsField, session, ii->tags[t]);
        moloch_field_string_add(config.tagsStringField, session, ii->tagsStr[t], -1, TRUE);
    }

    return ii;
}
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
void moloch_db_js0n_str(BSB *bsb, unsigned char *in, gboolean utf8)
{
    BSB_EXPORT_u08(*bsb, '"');
    while (*in) {
        switch(*in) {
        case '\b':
            BSB_EXPORT_cstr(*bsb, "\\b");
            break;
        case '\n':
            BSB_EXPORT_cstr(*bsb, "\\n");
            break;
        case '\r':
            BSB_EXPORT_cstr(*bsb, "\\r");
            break;
        case '\f':
            BSB_EXPORT_cstr(*bsb, "\\f");
            break;
        case '\t':
            BSB_EXPORT_cstr(*bsb, "\\t");
            break;
        case '"':
            BSB_EXPORT_cstr(*bsb, "\\\"");
            break;
        case '\\':
            BSB_EXPORT_cstr(*bsb, "\\\\");
            break;
        case '/':
            BSB_EXPORT_cstr(*bsb, "\\/");
            break;
        default:
            if(*in < 32) {
                BSB_EXPORT_sprintf(*bsb, "\\u%04x", *in);
            } else if (utf8) {
                if ((*in & 0xf0) == 0xf0) {
                    BSB_EXPORT_u08(*bsb, *(in++));
                    BSB_EXPORT_u08(*bsb, *(in++));
                    BSB_EXPORT_u08(*bsb, *(in++));
                    BSB_EXPORT_u08(*bsb, *in);
                } else if ((*in & 0xf0) == 0xe0) {
                    BSB_EXPORT_u08(*bsb, *(in++));
                    BSB_EXPORT_u08(*bsb, *(in++));
                    BSB_EXPORT_u08(*bsb, *in);
                } else if ((*in & 0xf0) == 0xd0) {
                    BSB_EXPORT_u08(*bsb, *(in++));
                    BSB_EXPORT_u08(*bsb, *in);
                } else {
                    BSB_EXPORT_u08(*bsb, *in);
                }
            } else {
                if(*in & 0x80) {
                    BSB_EXPORT_u08(*bsb, (0xc0 | (*in >> 6)));
                    BSB_EXPORT_u08(*bsb, (0x80 | (*in & 0x3f)));
                } else {
                    BSB_EXPORT_u08(*bsb, *in);
                }
            }
            break;
        }
        in++;
    }

    BSB_EXPORT_u08(*bsb, '"');
}

/******************************************************************************/
void moloch_db_geo_lookup6(MolochSession_t *session, struct in6_addr addr, char **g, char **as, char **rir, int *asFree)
{
    MolochIpInfo_t *ii = 0;
    *g = *as = *rir = 0;
    *asFree = 0;

    if (ipTree) {
        if ((ii = moloch_db_get_local_ip6(session, &addr))) {
            *g = ii->country;
            *as = ii->asn;
            *rir = ii->rir;
        }
    }

    if (IN6_IS_ADDR_V4MAPPED(&addr)) {
        if (!*g && gi) {
            *g = (char *)GeoIP_country_code3_by_ipnum(gi, htonl(MOLOCH_V6_TO_V4(addr)));
        }

        if (!*as && giASN) {
            *as = GeoIP_name_by_ipnum(giASN, htonl(MOLOCH_V6_TO_V4(addr)));
            if (*as) {
                *asFree = 1;
            }
        }

        if (!*rir) {
            *rir = rirs[MOLOCH_V6_TO_V4(addr) & 0xff];
        }
    } else {
        if (!*g && gi6) {
            *g = (char *)GeoIP_country_code3_by_ipnum_v6(gi6, addr);
        }

        if (!*as && giASN6) {
            *as = GeoIP_name_by_ipnum_v6(giASN6, addr);
            if (*as) {
                *asFree = 1;
            }
        }
    }
}
/******************************************************************************/
void moloch_db_geo_lookup4(MolochSession_t *session, uint32_t addr, char **g, char **as, char **rir, int *asFree)
{
    MolochIpInfo_t *ii = 0;
    *g = *as = *rir = 0;
    *asFree = 0;

    if (ipTree) {
        if ((ii = moloch_db_get_local_ip4(session, addr))) {
            *g = ii->country;
            *as = ii->asn;
            *rir = ii->rir;
        }
    }

    if (!*g && gi) {
        *g = (char *)GeoIP_country_code3_by_ipnum(gi, htonl(addr));
    }

    if (!*as && giASN) {
        *as = GeoIP_name_by_ipnum(giASN, htonl(addr));
        if (*as) {
            *asFree = 1;
        }
    }

    if (!*rir) {
        *rir = rirs[addr & 0xff];
    }
}
/******************************************************************************/
LOCAL void moloch_db_send_bulk(char *json, int len)
{
    moloch_http_set(esServer, "/_bulk", 6, json, len, NULL, NULL);
}
LOCAL MolochDbSendBulkFunc sendBulkFunc = moloch_db_send_bulk;
/******************************************************************************/
void moloch_db_set_send_bulk(MolochDbSendBulkFunc func)
{
    sendBulkFunc = func;
}
/******************************************************************************/
LOCAL struct {
    char   *json;
    BSB     bsb;
    time_t  lastSave;
    char    prefix[100];
    time_t  prefixTime;
    MOLOCH_LOCK_EXTERN(lock);
} dbInfo[MOLOCH_MAX_PACKET_THREADS];

#define MAX_IPS 2000

void moloch_db_save_session(MolochSession_t *session, int final)
{
    uint32_t               i;
    char                   id[100];
    uuid_t                 uuid;
    MolochString_t        *hstring;
    MolochInt_t           *hint;
    MolochStringHashStd_t *shash;
    MolochIntHashStd_t    *ihash;
    GHashTable            *ghash;
    GHashTableIter         iter;
    unsigned char         *startPtr;
    unsigned char         *dataPtr;
    uint32_t               jsonSize;
    int                    pos;
    gpointer               ikey;

    /* Let the plugins finish */
    if (pluginsCbs & MOLOCH_PLUGIN_SAVE)
        moloch_plugins_cb_save(session, final);

    /* Don't save spi data for session */
    if (session->stopSPI)
        return;

    /* No Packets */
    if (!config.dryRun && !session->filePosArray->len)
        return;

    /* Not enough packets */
    if (session->packets[0] + session->packets[1] < session->minSaving) {
        return;
    }

    /* jsonSize is an estimate of how much space it will take to encode the session */
    jsonSize = 1100 + session->filePosArray->len*12 + 10*session->fileNumArray->len + 10*session->fileLenArray->len;
    for (pos = 0; pos < session->maxFields; pos++) {
        if (session->fields[pos]) {
            jsonSize += session->fields[pos]->jsonSize;
        }
    }

    totalSessions++;
    session->segments++;

    const int thread = session->thread;

    if (dbInfo[thread].prefixTime != session->lastPacket.tv_sec) {
        dbInfo[thread].prefixTime = session->lastPacket.tv_sec;

        struct tm tmp;
        gmtime_r(&dbInfo[thread].prefixTime, &tmp);

        switch(config.rotate) {
        case MOLOCH_ROTATE_HOURLY:
            snprintf(dbInfo[thread].prefix, sizeof(dbInfo[thread].prefix), "%02d%02d%02dh%02d", tmp.tm_year%100, tmp.tm_mon+1, tmp.tm_mday, tmp.tm_hour);
            break;
        case MOLOCH_ROTATE_DAILY:
            snprintf(dbInfo[thread].prefix, sizeof(dbInfo[thread].prefix), "%02d%02d%02d", tmp.tm_year%100, tmp.tm_mon+1, tmp.tm_mday);
            break;
        case MOLOCH_ROTATE_WEEKLY:
            snprintf(dbInfo[thread].prefix, sizeof(dbInfo[thread].prefix), "%02dw%02d", tmp.tm_year%100, tmp.tm_yday/7);
            break;
        case MOLOCH_ROTATE_MONTHLY:
            snprintf(dbInfo[thread].prefix, sizeof(dbInfo[thread].prefix), "%02dm%02d", tmp.tm_year%100, tmp.tm_mon+1);
            break;
        }
    }
    uint32_t id_len = snprintf(id, sizeof(id), "%s-", dbInfo[thread].prefix);

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

    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    MOLOCH_LOCK(dbInfo[thread].lock);
    /* If no room left to add, send the buffer */
    if (dbInfo[thread].json && (uint32_t)BSB_REMAINING(dbInfo[thread].bsb) < jsonSize) {
        if (BSB_LENGTH(dbInfo[thread].bsb) > 0) {
            sendBulkFunc(dbInfo[thread].json, BSB_LENGTH(dbInfo[thread].bsb));
        } else {
            moloch_http_free_buffer(dbInfo[thread].json);
        }
        dbInfo[thread].json = 0;
        dbInfo[thread].lastSave = currentTime.tv_sec;
    }

    /* Allocate a new buffer using the max of the bulk size or estimated size. */
    if (!dbInfo[thread].json) {
        const int size = MAX(config.dbBulkSize, jsonSize);
        dbInfo[thread].json = moloch_http_get_buffer(size);
        BSB_INIT(dbInfo[thread].bsb, dbInfo[thread].json, size);
    }

    uint32_t timediff = (session->lastPacket.tv_sec - session->firstPacket.tv_sec)*1000 +
                        (session->lastPacket.tv_usec - session->firstPacket.tv_usec)/1000;

    BSB jbsb = dbInfo[thread].bsb;

    startPtr = BSB_WORK_PTR(jbsb);
    BSB_EXPORT_sprintf(jbsb, "{\"index\": {\"_index\": \"%ssessions-%s\", \"_type\": \"session\", \"_id\": \"%s\"}}\n", config.prefix, dbInfo[thread].prefix, id);

    dataPtr = BSB_WORK_PTR(jbsb);

    BSB_EXPORT_sprintf(jbsb,
                      "{\"fp\":%u,"
                      "\"lp\":%u,"
                      "\"fpd\":%" PRIu64 ","
                      "\"lpd\":%" PRIu64 ","
                      "\"sl\":%u,"
                      "\"a1\":%u,"
                      "\"p1\":%u,"
                      "\"a2\":%u,"
                      "\"p2\":%u,"
                      "\"pr\":%u,",
                      (uint32_t)session->firstPacket.tv_sec,
                      (uint32_t)session->lastPacket.tv_sec,
                      ((uint64_t)session->firstPacket.tv_sec)*1000 + ((uint64_t)session->firstPacket.tv_usec)/1000,
                      ((uint64_t)session->lastPacket.tv_sec)*1000 + ((uint64_t)session->lastPacket.tv_usec)/1000,
                      timediff,
                      htonl(MOLOCH_V6_TO_V4(session->addr1)),
                      session->port1,
                      htonl(MOLOCH_V6_TO_V4(session->addr2)),
                      session->port2,
                      session->protocol);

    if (session->protocol == IPPROTO_TCP) {
        BSB_EXPORT_sprintf(jbsb,
                           "\"tcpflags\":{"
                           "\"syn\": %d,"
                           "\"syn-ack\": %d,"
                           "\"ack\": %d,"
                           "\"psh\": %d,"
                           "\"fin\": %d,"
                           "\"rst\": %d,"
                           "\"urg\": %d"
                           "},",
                           session->tcpFlagCnt[MOLOCH_TCPFLAG_SYN],
                           session->tcpFlagCnt[MOLOCH_TCPFLAG_SYN_ACK],
                           session->tcpFlagCnt[MOLOCH_TCPFLAG_ACK],
                           session->tcpFlagCnt[MOLOCH_TCPFLAG_PSH],
                           session->tcpFlagCnt[MOLOCH_TCPFLAG_FIN],
                           session->tcpFlagCnt[MOLOCH_TCPFLAG_RST],
                           session->tcpFlagCnt[MOLOCH_TCPFLAG_URG]);
    }

    if (session->firstBytesLen[0] > 0) {
        int i;
        BSB_EXPORT_cstr(jbsb, "\"fb1\":\"");
        for (i = 0; i < session->firstBytesLen[0]; i++) {
            BSB_EXPORT_ptr(jbsb, moloch_char_to_hexstr[(unsigned char)session->firstBytes[0][i]], 2);
        }
        BSB_EXPORT_cstr(jbsb, "\",");
    }

    if (session->firstBytesLen[1] > 0) {
        BSB_EXPORT_cstr(jbsb, "\"fb2\":\"");
        for (i = 0; i < session->firstBytesLen[1]; i++) {
            BSB_EXPORT_ptr(jbsb, moloch_char_to_hexstr[(unsigned char)session->firstBytes[1][i]], 2);
        }
        BSB_EXPORT_cstr(jbsb, "\",");
    }

    char ipsrc[INET6_ADDRSTRLEN];
    char ipdst[INET6_ADDRSTRLEN];
    if (IN6_IS_ADDR_V4MAPPED(&session->addr1)) {
        uint32_t ip = MOLOCH_V6_TO_V4(session->addr1);
        snprintf(ipsrc, sizeof(ipsrc), "%d.%d.%d.%d", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff);
        ip = MOLOCH_V6_TO_V4(session->addr2);
        snprintf(ipdst, sizeof(ipdst), "%d.%d.%d.%d", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff);
    } else {
        strcpy(ipsrc, "0.0.0.0");
        strcpy(ipdst, "0.0.0.0");
        /* - Requires ES 5
        inet_ntop(AF_INET6, &session->addr1, ipsrc, sizeof(ipsrc));
        inet_ntop(AF_INET6, &session->addr2, ipdst, sizeof(ipdst));
        */

        BSB_EXPORT_cstr(jbsb, "\"tipv61-term\":\"");
        for (i = 0; i < 16; i++) {
            BSB_EXPORT_ptr(jbsb, moloch_char_to_hexstr[(unsigned char)session->addr1.s6_addr[i]], 2);
        }
        BSB_EXPORT_cstr(jbsb, "\",");

        BSB_EXPORT_cstr(jbsb, "\"tipv62-term\":\"");
        for (i = 0; i < 16; i++) {
            BSB_EXPORT_ptr(jbsb, moloch_char_to_hexstr[(unsigned char)session->addr2.s6_addr[i]], 2);
        }
        BSB_EXPORT_cstr(jbsb, "\",");
    }
    BSB_EXPORT_sprintf(jbsb,
                      "\"timestamp\":%" PRIu64 ","
                      "\"firstPacket\":%" PRIu64 ","
                      "\"lastPacket\":%" PRIu64 ","
                      "\"ipSrc\":\"%s\","
                      "\"portSrc\":%u,"
                      "\"ipDst\":\"%s\","
                      "\"portDst\":%u,",
                      ((uint64_t)currentTime.tv_sec)*1000 + ((uint64_t)currentTime.tv_usec)/1000,
                      ((uint64_t)session->firstPacket.tv_sec)*1000 + ((uint64_t)session->firstPacket.tv_usec)/1000,
                      ((uint64_t)session->lastPacket.tv_sec)*1000 + ((uint64_t)session->lastPacket.tv_usec)/1000,
                      ipsrc,
                      session->port1,
                      ipdst,
                      session->port2);


    char *g1, *g2, *as1, *as2, *rir1, *rir2;
    int asFree1, asFree2;

    moloch_db_geo_lookup6(session, session->addr1, &g1, &as1, &rir1, &asFree1);
    moloch_db_geo_lookup6(session, session->addr2, &g2, &as2, &rir2, &asFree2);

    if (g1)
        BSB_EXPORT_sprintf(jbsb, "\"g1\":\"%s\",", g1);
    if (g2)
        BSB_EXPORT_sprintf(jbsb, "\"g2\":\"%s\",", g2);


    if (as1) {
        BSB_EXPORT_cstr(jbsb, "\"as1\":");
        moloch_db_js0n_str(&jbsb, (unsigned char*)as1, TRUE);
        BSB_EXPORT_u08(jbsb, ',');
        if (asFree1)
            free(as1);
    }

    if (as2) {
        BSB_EXPORT_cstr(jbsb, "\"as2\":");
        moloch_db_js0n_str(&jbsb, (unsigned char*)as2, TRUE);
        BSB_EXPORT_u08(jbsb, ',');
        if (asFree2)
            free(as2);
    }


    if (rir1)
        BSB_EXPORT_sprintf(jbsb, "\"rir1\":\"%s\",", rir1);

    if (rir2)
        BSB_EXPORT_sprintf(jbsb, "\"rir2\":\"%s\",", rir2);

    BSB_EXPORT_sprintf(jbsb,
                      "\"pa\":%u,"
                      "\"pa1\":%u,"
                      "\"pa2\":%u,"
                      "\"by\":%" PRIu64 ","
                      "\"by1\":%" PRIu64 ","
                      "\"by2\":%" PRIu64 ","
                      "\"db\":%" PRIu64 ","
                      "\"db1\":%" PRIu64 ","
                      "\"db2\":%" PRIu64 ","
                      "\"ss\":%u,"
                      "\"no\":\"%s\",",
                      session->packets[0] + session->packets[1],
                      session->packets[0],
                      session->packets[1],
                      session->bytes[0] + session->bytes[1],
                      session->bytes[0],
                      session->bytes[1],
                      session->databytes[0] + session->databytes[1],
                      session->databytes[0],
                      session->databytes[1],
                      session->segments,
                      config.nodeName);

    if (session->rootId) {
        if (session->rootId[0] == 'R')
            session->rootId = g_strdup(id);
        BSB_EXPORT_sprintf(jbsb, "\"ro\":\"%s\",", session->rootId);
    }
    BSB_EXPORT_cstr(jbsb, "\"ps\":[");
    for(i = 0; i < session->filePosArray->len; i++) {
        if (i != 0)
            BSB_EXPORT_u08(jbsb, ',');
        BSB_EXPORT_sprintf(jbsb, "%" PRId64, (uint64_t)g_array_index(session->filePosArray, uint64_t, i));
    }
    BSB_EXPORT_cstr(jbsb, "],");

    BSB_EXPORT_cstr(jbsb, "\"psl\":[");
    for(i = 0; i < session->fileLenArray->len; i++) {
        if (i != 0)
            BSB_EXPORT_u08(jbsb, ',');
        BSB_EXPORT_sprintf(jbsb, "%u", (uint16_t)g_array_index(session->fileLenArray, uint16_t, i));
    }
    BSB_EXPORT_cstr(jbsb, "],");

    BSB_EXPORT_cstr(jbsb, "\"fs\":[");
    for(i = 0; i < session->fileNumArray->len; i++) {
        if (i == 0)
            BSB_EXPORT_sprintf(jbsb, "%u", (uint32_t)g_array_index(session->fileNumArray, uint32_t, i));
        else
            BSB_EXPORT_sprintf(jbsb, ",%u", (uint32_t)g_array_index(session->fileNumArray, uint32_t, i));
    }
    BSB_EXPORT_cstr(jbsb, "],");

    int inGroupNum = 0;
    for (pos = 0; pos < session->maxFields; pos++) {
        const int flags = config.fields[pos]->flags;
        if (!session->fields[pos] || flags & MOLOCH_FIELD_FLAG_DISABLED)
            continue;

        const int freeField = final || ((flags & MOLOCH_FIELD_FLAG_LINKED_SESSIONS) == 0);

        if (inGroupNum != config.fields[pos]->dbGroupNum) {
            if (inGroupNum != 0) {
                BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
                BSB_EXPORT_cstr(jbsb, "},");
            }
            inGroupNum = config.fields[pos]->dbGroupNum;

            if (inGroupNum) {
                BSB_EXPORT_sprintf(jbsb, "\"%.*s\": {", config.fields[pos]->dbGroupLen, config.fields[pos]->dbGroup);
            }
        }

        switch(config.fields[pos]->type) {
        case MOLOCH_FIELD_TYPE_INT:
            BSB_EXPORT_sprintf(jbsb, "\"%s\":%d", config.fields[pos]->dbField, session->fields[pos]->i);
            BSB_EXPORT_u08(jbsb, ',');
            break;
        case MOLOCH_FIELD_TYPE_STR:
            BSB_EXPORT_sprintf(jbsb, "\"%s\":", config.fields[pos]->dbField);
            moloch_db_js0n_str(&jbsb,
                               (unsigned char *)session->fields[pos]->str,
                               flags & MOLOCH_FIELD_FLAG_FORCE_UTF8);
            BSB_EXPORT_u08(jbsb, ',');
            if (freeField) {
                g_free(session->fields[pos]->str);
            }
            break;
        case MOLOCH_FIELD_TYPE_STR_ARRAY:
            if (flags & MOLOCH_FIELD_FLAG_CNT) {
                BSB_EXPORT_sprintf(jbsb, "\"%scnt\":%d,", config.fields[pos]->dbField, session->fields[pos]->sarray->len);
            } else if (flags & MOLOCH_FIELD_FLAG_COUNT) {
                BSB_EXPORT_sprintf(jbsb, "\"%s-cnt\":%d,", config.fields[pos]->dbField, session->fields[pos]->sarray->len);
            }
            BSB_EXPORT_sprintf(jbsb, "\"%s\":[", config.fields[pos]->dbField);
            for(i = 0; i < session->fields[pos]->sarray->len; i++) {
                moloch_db_js0n_str(&jbsb,
                                   g_ptr_array_index(session->fields[pos]->sarray, i),
                                   flags & MOLOCH_FIELD_FLAG_FORCE_UTF8);
                BSB_EXPORT_u08(jbsb, ',');
            }
            BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
            BSB_EXPORT_cstr(jbsb, "],");
            if (freeField) {
                g_ptr_array_free(session->fields[pos]->sarray, TRUE);
            }
            break;
        case MOLOCH_FIELD_TYPE_STR_HASH:
            shash = session->fields[pos]->shash;
            if (flags & MOLOCH_FIELD_FLAG_CNT) {
                BSB_EXPORT_sprintf(jbsb, "\"%scnt\":%d,", config.fields[pos]->dbField, HASH_COUNT(s_, *shash));
            } else if (flags & MOLOCH_FIELD_FLAG_COUNT) {
                BSB_EXPORT_sprintf(jbsb, "\"%s-cnt\":%d,", config.fields[pos]->dbField, HASH_COUNT(s_, *shash));
            }
            BSB_EXPORT_sprintf(jbsb, "\"%s\":[", config.fields[pos]->dbField);
            HASH_FORALL(s_, *shash, hstring,
                moloch_db_js0n_str(&jbsb, (unsigned char *)hstring->str, hstring->utf8 || flags & MOLOCH_FIELD_FLAG_FORCE_UTF8);
                BSB_EXPORT_u08(jbsb, ',');
            );
            if (freeField) {
                HASH_FORALL_POP_HEAD(s_, *shash, hstring,
                    g_free(hstring->str);
                    MOLOCH_TYPE_FREE(MolochString_t, hstring);
                );
                MOLOCH_TYPE_FREE(MolochStringHashStd_t, shash);
            }
            BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
            BSB_EXPORT_cstr(jbsb, "],");
            break;
        case MOLOCH_FIELD_TYPE_INT_HASH:
            ihash = session->fields[pos]->ihash;
            if (flags & MOLOCH_FIELD_FLAG_CNT) {
                BSB_EXPORT_sprintf(jbsb, "\"%scnt\": %d,", config.fields[pos]->dbField, HASH_COUNT(i_, *ihash));
            } else if (flags & MOLOCH_FIELD_FLAG_COUNT) {
                BSB_EXPORT_sprintf(jbsb, "\"%s-cnt\": %d,", config.fields[pos]->dbField, HASH_COUNT(i_, *ihash));
            }
            BSB_EXPORT_sprintf(jbsb, "\"%s\":[", config.fields[pos]->dbField);
            HASH_FORALL(i_, *ihash, hint,
                BSB_EXPORT_sprintf(jbsb, "%u", hint->i_hash);
                BSB_EXPORT_u08(jbsb, ',');
            );
            if (freeField) {
                HASH_FORALL_POP_HEAD(i_, *ihash, hint,
                    MOLOCH_TYPE_FREE(MolochInt_t, hint);
                );
                MOLOCH_TYPE_FREE(MolochIntHashStd_t, ihash);
            }
            BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
            BSB_EXPORT_cstr(jbsb, "],");
            break;
        case MOLOCH_FIELD_TYPE_INT_GHASH:
            ghash = session->fields[pos]->ghash;
            if (flags & MOLOCH_FIELD_FLAG_CNT) {
                BSB_EXPORT_sprintf(jbsb, "\"%scnt\": %d,", config.fields[pos]->dbField, g_hash_table_size(ghash));
            } else if (flags & MOLOCH_FIELD_FLAG_COUNT) {
                BSB_EXPORT_sprintf(jbsb, "\"%s-cnt\": %d,", config.fields[pos]->dbField, g_hash_table_size(ghash));
            }
            BSB_EXPORT_sprintf(jbsb, "\"%s\":[", config.fields[pos]->dbField);
            g_hash_table_iter_init (&iter, ghash);
            while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
                BSB_EXPORT_sprintf(jbsb, "%u", (int)(long)ikey);
                BSB_EXPORT_u08(jbsb, ',');
            }

            if (freeField) {
                g_hash_table_destroy(ghash);
            }
            BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
            BSB_EXPORT_cstr(jbsb, "],");
            break;
        case MOLOCH_FIELD_TYPE_IP: {
            const int             value = session->fields[pos]->i;
            char                 *as;
            char                 *g;
            char                 *rir;
            int                   asFree;
            const int             post = (flags & MOLOCH_FIELD_FLAG_IPPRE) == 0;

            moloch_db_geo_lookup4(session, value, &g, &as, &rir, &asFree);
            if (g) {
                if (post)
                    BSB_EXPORT_sprintf(jbsb, "\"%s-geo\":\"%s\",", config.fields[pos]->dbField, g);
                else
                    BSB_EXPORT_sprintf(jbsb, "\"g%s\":\"%s\",", config.fields[pos]->dbField, g);
            }

            if (as) {
                if (post)
                    BSB_EXPORT_sprintf(jbsb, "\"%s-asn\":", config.fields[pos]->dbField);
                else
                    BSB_EXPORT_sprintf(jbsb, "\"as%s\":", config.fields[pos]->dbField);
                moloch_db_js0n_str(&jbsb, (unsigned char*)as, TRUE);
                if (asFree) {
                    free(as);
                }
                BSB_EXPORT_u08(jbsb, ',');
            }

            if (rir) {
                if (post)
                    BSB_EXPORT_sprintf(jbsb, "\"%s-rir\":\"%s\",", config.fields[pos]->dbField, rir);
                else
                    BSB_EXPORT_sprintf(jbsb, "\"rir%s\":\"%s\",", config.fields[pos]->dbField, rir);
            }

            BSB_EXPORT_sprintf(jbsb, "\"%s\":%u,", config.fields[pos]->dbField, htonl(value));
            }
            break;
        case MOLOCH_FIELD_TYPE_IP_HASH: {
            const int post = (flags & MOLOCH_FIELD_FLAG_IPPRE) == 0;
            ihash = session->fields[pos]->ihash;
            if (flags & MOLOCH_FIELD_FLAG_CNT) {
                BSB_EXPORT_sprintf(jbsb, "\"%scnt\":%d,", config.fields[pos]->dbField, HASH_COUNT(i_, *ihash));
            } else if (flags & MOLOCH_FIELD_FLAG_COUNT) {
                BSB_EXPORT_sprintf(jbsb, "\"%s-cnt\":%d,", config.fields[pos]->dbField, HASH_COUNT(i_, *ihash));
            } else if (flags & MOLOCH_FIELD_FLAG_SCNT) {
                BSB_EXPORT_sprintf(jbsb, "\"%sscnt\":%d,", config.fields[pos]->dbField, HASH_COUNT(i_, *ihash));
            }

            char                 *as[MAX_IPS];
            char                 *g[MAX_IPS];
            char                 *rir[MAX_IPS];
            int                   asFree[MAX_IPS];
            int                   i;
            int                   cnt = 0;

            BSB_EXPORT_sprintf(jbsb, "\"%s\":[", config.fields[pos]->dbField);
            HASH_FORALL(i_, *ihash, hint,
                moloch_db_geo_lookup4(session, hint->i_hash, &g[cnt], &as[cnt], &rir[cnt], &asFree[cnt]);
                cnt++;
                if (cnt >= MAX_IPS)
                    break;
                BSB_EXPORT_sprintf(jbsb, "%u,", htonl(hint->i_hash));
            );
            BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
            BSB_EXPORT_cstr(jbsb, "],");

            if (post)
                BSB_EXPORT_sprintf(jbsb, "\"%s-geo\":[", config.fields[pos]->dbField);
            else
                BSB_EXPORT_sprintf(jbsb, "\"g%s\":[", config.fields[pos]->dbField);
            for (i = 0; i < cnt; i++) {
                if (g[i]) {
                    BSB_EXPORT_sprintf(jbsb, "\"%s\",", g[i]);
                } else {
                    BSB_EXPORT_cstr(jbsb, "\"---\",");
                }
            }
            BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
            BSB_EXPORT_cstr(jbsb, "],");

            if (post)
                BSB_EXPORT_sprintf(jbsb, "\"%s-asn\":[", config.fields[pos]->dbField);
            else
                BSB_EXPORT_sprintf(jbsb, "\"as%s\":[", config.fields[pos]->dbField);
            for (i = 0; i < cnt; i++) {
                if (as[i]) {
                    moloch_db_js0n_str(&jbsb, (unsigned char*)as[i], TRUE);
                    BSB_EXPORT_u08(jbsb, ',');
                    if(asFree[i])
                        free(as[i]);
                } else {
                    BSB_EXPORT_cstr(jbsb, "\"---\",");
                }
            }
            BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
            BSB_EXPORT_cstr(jbsb, "],");

            if (post)
                BSB_EXPORT_sprintf(jbsb, "\"%s-rir\":[", config.fields[pos]->dbField);
            else
                BSB_EXPORT_sprintf(jbsb, "\"rir%s\":[", config.fields[pos]->dbField);
            for (i = 0; i < cnt; i++) {
                if (rir[i]) {
                    BSB_EXPORT_sprintf(jbsb, "\"%s\",", rir[i]);
                } else {
                    BSB_EXPORT_cstr(jbsb, "\"\",");
                }
            }
            BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
            BSB_EXPORT_cstr(jbsb, "],");

            if (freeField) {
                HASH_FORALL_POP_HEAD(i_, *ihash, hint,
                    MOLOCH_TYPE_FREE(MolochInt_t, hint);
                );
                MOLOCH_TYPE_FREE(MolochIntHashStd_t, ihash);
            }
            break;
        }
        case MOLOCH_FIELD_TYPE_IP_GHASH: {
            const int post = (flags & MOLOCH_FIELD_FLAG_IPPRE) == 0;
            ghash = session->fields[pos]->ghash;
            if (flags & MOLOCH_FIELD_FLAG_CNT) {
                BSB_EXPORT_sprintf(jbsb, "\"%scnt\":%d,", config.fields[pos]->dbField, g_hash_table_size(ghash));
            } else if (flags & MOLOCH_FIELD_FLAG_COUNT) {
                BSB_EXPORT_sprintf(jbsb, "\"%s-cnt\":%d,", config.fields[pos]->dbField, g_hash_table_size(ghash));
            } else if (flags & MOLOCH_FIELD_FLAG_SCNT) {
                BSB_EXPORT_sprintf(jbsb, "\"%sscnt\":%d,", config.fields[pos]->dbField, g_hash_table_size(ghash));
            }

            char                 *as[MAX_IPS];
            char                 *g[MAX_IPS];
            char                 *rir[MAX_IPS];
            int                   asFree[MAX_IPS];
            int                   i;
            int                   cnt = 0;

            BSB_EXPORT_sprintf(jbsb, "\"%s\":[", config.fields[pos]->dbField);
            g_hash_table_iter_init (&iter, ghash);
            while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
                moloch_db_geo_lookup4(session, (int)(long)ikey, &g[cnt], &as[cnt], &rir[cnt], &asFree[cnt]);
                cnt++;
                if (cnt >= MAX_IPS)
                    break;

                BSB_EXPORT_sprintf(jbsb, "%u,", htonl((int)(long)ikey));
            }
            BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
            BSB_EXPORT_cstr(jbsb, "],");

            if (post)
                BSB_EXPORT_sprintf(jbsb, "\"%s-geo\":[", config.fields[pos]->dbField);
            else
                BSB_EXPORT_sprintf(jbsb, "\"g%s\":[", config.fields[pos]->dbField);
            for (i = 0; i < cnt; i++) {
                if (g[i]) {
                    BSB_EXPORT_sprintf(jbsb, "\"%s\",", g[i]);
                } else {
                    BSB_EXPORT_cstr(jbsb, "\"---\",");
                }
            }
            BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
            BSB_EXPORT_cstr(jbsb, "],");

            if (post)
                BSB_EXPORT_sprintf(jbsb, "\"%s-asn\":[", config.fields[pos]->dbField);
            else
                BSB_EXPORT_sprintf(jbsb, "\"as%s\":[", config.fields[pos]->dbField);
            for (i = 0; i < cnt; i++) {
                if (as[i]) {
                    moloch_db_js0n_str(&jbsb, (unsigned char*)as[i], TRUE);
                    BSB_EXPORT_u08(jbsb, ',');
                    if(asFree[i])
                        free(as[i]);
                } else {
                    BSB_EXPORT_cstr(jbsb, "\"---\",");
                }
            }
            BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
            BSB_EXPORT_cstr(jbsb, "],");

            if (post)
                BSB_EXPORT_sprintf(jbsb, "\"%s-rir\":[", config.fields[pos]->dbField);
            else
                BSB_EXPORT_sprintf(jbsb, "\"rir%s\":[", config.fields[pos]->dbField);
            for (i = 0; i < cnt; i++) {
                if (rir[i]) {
                    BSB_EXPORT_sprintf(jbsb, "\"%s\",", rir[i]);
                } else {
                    BSB_EXPORT_cstr(jbsb, "\"\",");
                }
            }
            BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
            BSB_EXPORT_cstr(jbsb, "],");

            if (freeField) {
                g_hash_table_destroy(ghash);
            }

            break;
        }
        case MOLOCH_FIELD_TYPE_CERTSINFO: {
            MolochCertsInfoHashStd_t *cihash = session->fields[pos]->cihash;

            BSB_EXPORT_sprintf(jbsb, "\"tlscnt\":%d,", HASH_COUNT(t_, *cihash));
            BSB_EXPORT_cstr(jbsb, "\"tls\":[");

            MolochCertsInfo_t *certs;
            MolochString_t *string;

            HASH_FORALL_POP_HEAD(t_, *cihash, certs,
                BSB_EXPORT_u08(jbsb, '{');

                if (certs->issuer.commonName.s_count > 0) {
                    BSB_EXPORT_cstr(jbsb, "\"iCn\":[");
                    while (certs->issuer.commonName.s_count > 0) {
                        DLL_POP_HEAD(s_, &certs->issuer.commonName, string);
                        moloch_db_js0n_str(&jbsb, (unsigned char *)string->str, string->utf8);
                        BSB_EXPORT_u08(jbsb, ',');
                        g_free(string->str);
                        MOLOCH_TYPE_FREE(MolochString_t, string);
                    }
                    BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
                    BSB_EXPORT_u08(jbsb, ']');
                    BSB_EXPORT_u08(jbsb, ',');
                }

                BSB_EXPORT_sprintf(jbsb, "\"hash\":\"%s\",", certs->hash);

                if (certs->issuer.orgName) {
                    BSB_EXPORT_cstr(jbsb, "\"iOn\":");
                    moloch_db_js0n_str(&jbsb, (unsigned char *)certs->issuer.orgName, certs->issuer.orgUtf8);
                    BSB_EXPORT_u08(jbsb, ',');
                }

                if (certs->subject.commonName.s_count) {
                    BSB_EXPORT_cstr(jbsb, "\"sCn\":[");
                    while (certs->subject.commonName.s_count > 0) {
                        DLL_POP_HEAD(s_, &certs->subject.commonName, string);
                        moloch_db_js0n_str(&jbsb, (unsigned char *)string->str, string->utf8);
                        BSB_EXPORT_u08(jbsb, ',');
                        g_free(string->str);
                        MOLOCH_TYPE_FREE(MolochString_t, string);
                    }
                    BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
                    BSB_EXPORT_u08(jbsb, ']');
                    BSB_EXPORT_u08(jbsb, ',');
                }

                if (certs->subject.orgName) {
                    BSB_EXPORT_cstr(jbsb, "\"sOn\":");
                    moloch_db_js0n_str(&jbsb, (unsigned char *)certs->subject.orgName, certs->subject.orgUtf8);
                    BSB_EXPORT_u08(jbsb, ',');
                }

                if (certs->serialNumber) {
                    int k;
                    BSB_EXPORT_cstr(jbsb, "\"sn\":\"");
                    for (k = 0; k < certs->serialNumberLen; k++) {
                        BSB_EXPORT_sprintf(jbsb, "%02x", certs->serialNumber[k]);
                    }
                    BSB_EXPORT_u08(jbsb, '"');
                    BSB_EXPORT_u08(jbsb, ',');
                }

                if (certs->alt.s_count) {
                    BSB_EXPORT_sprintf(jbsb, "\"altcnt\":%d,", certs->alt.s_count);
                    BSB_EXPORT_cstr(jbsb, "\"alt\":[");
                    while (certs->alt.s_count > 0) {
                        DLL_POP_HEAD(s_, &certs->alt, string);
                        moloch_db_js0n_str(&jbsb, (unsigned char *)string->str, TRUE);
                        BSB_EXPORT_u08(jbsb, ',');
                        g_free(string->str);
                        MOLOCH_TYPE_FREE(MolochString_t, string);
                    }
                    BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
                    BSB_EXPORT_u08(jbsb, ']');
                    BSB_EXPORT_u08(jbsb, ',');
                }

                BSB_EXPORT_sprintf(jbsb, "\"notBefore\": %" PRId64 ",", certs->notBefore);
                BSB_EXPORT_sprintf(jbsb, "\"notAfter\": %" PRId64 ",", certs->notAfter);
                if (certs->notAfter >= certs->notBefore)
                    BSB_EXPORT_sprintf(jbsb, "\"diffDays\": %" PRId64 ",", (certs->notAfter - certs->notBefore)/(60*60*24));

                BSB_EXPORT_rewind(jbsb, 1); // Remove last comma

                moloch_field_certsinfo_free(certs);
                i++;

                BSB_EXPORT_u08(jbsb, '}');
                BSB_EXPORT_u08(jbsb, ',');
            );
            MOLOCH_TYPE_FREE(MolochCertsInfoHashStd_t, cihash);

            BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
            BSB_EXPORT_cstr(jbsb, "],");
        }
        } /* switch */
        if (freeField) {
            MOLOCH_TYPE_FREE(MolochField_t, session->fields[pos]);
            session->fields[pos] = 0;
        }
    }

    if (inGroupNum) {
        BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
        BSB_EXPORT_cstr(jbsb, "},");
    }

    BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
    BSB_EXPORT_cstr(jbsb, "}\n");

    if (BSB_IS_ERROR(jbsb)) {
        LOG("ERROR - Ran out of memory creating DB record supposed to be %d", jsonSize);
        goto cleanup;
    }

    if (config.dryRun) {
        if (config.tests) {
            static int outputed;
            static MOLOCH_LOCK_DEFINE(outputed);

            MOLOCH_LOCK(outputed);
            outputed++;
            const int hlen = dataPtr - startPtr;
            fprintf(stderr, "  %s{\"header\":%.*s,\n  \"body\":%.*s}\n", (outputed==1 ? "":","), hlen-1, dbInfo[thread].json, (int)(BSB_LENGTH(jbsb)-hlen-1), dbInfo[thread].json+hlen);
            MOLOCH_UNLOCK(outputed);
        } else if (config.debug) {
            LOG("%.*s\n", (int)BSB_LENGTH(jbsb), dbInfo[thread].json);
        }
        BSB_INIT(jbsb, dbInfo[thread].json, BSB_SIZE(jbsb));
        goto cleanup;
    }

    if (config.noSPI) {
        BSB_INIT(jbsb, dbInfo[thread].json, BSB_SIZE(jbsb));
        goto cleanup;
    }

    if (jsonSize < (uint32_t)(BSB_WORK_PTR(jbsb) - startPtr)) {
        LOG("WARNING - %s BIGGER then expected json %d %d\n", id, jsonSize,  (int)(BSB_WORK_PTR(jbsb) - startPtr));
        if (config.debug)
            LOG("Data:\n%.*s\n", (int)(BSB_WORK_PTR(jbsb) - startPtr), startPtr);
    }
cleanup:
    dbInfo[thread].bsb = jbsb;
    MOLOCH_UNLOCK(dbInfo[thread].lock);
}
/******************************************************************************/
long long zero_atoll(char *v) {
    if (v)
        return atoll(v);
    return 0;
}

/******************************************************************************/
#define NUMBER_OF_STATS 4
static uint64_t dbTotalPackets[NUMBER_OF_STATS];
static uint64_t dbTotalK[NUMBER_OF_STATS];
static uint64_t dbTotalSessions[NUMBER_OF_STATS];
static uint64_t dbTotalDropped[NUMBER_OF_STATS];

static char     stats_key[200];
static int      stats_key_len = 0;

void moloch_db_load_stats()
{
    size_t             data_len;
    uint32_t           len;
    uint32_t           source_len;
    unsigned char     *source = 0;

    stats_key_len = snprintf(stats_key, sizeof(stats_key), "/%sstats/stat/%s", config.prefix, config.nodeName);

    unsigned char     *data = moloch_http_get(esServer, stats_key, stats_key_len, &data_len);

    source = moloch_js0n_get(data, data_len, "_source", &source_len);
    if (source) {
        dbTotalPackets[0]  = zero_atoll((char*)moloch_js0n_get(source, source_len, "totalPackets", &len));
        dbTotalK[0]        = zero_atoll((char*)moloch_js0n_get(source, source_len, "totalK", &len));
        dbTotalSessions[0] = dbTotalSessions[2] = zero_atoll((char*)moloch_js0n_get(source, source_len, "totalSessions", &len));
        dbTotalDropped[0]  = zero_atoll((char*)moloch_js0n_get(source, source_len, "totalDropped", &len));

        int i;
        for (i = 1; i < NUMBER_OF_STATS; i++) {
            dbTotalPackets[i]  = dbTotalPackets[0];
            dbTotalK[i]        = dbTotalK[0];
            dbTotalSessions[i] = dbTotalSessions[0];
            dbTotalDropped[i]  = dbTotalDropped[0];
        }
    }
    free(data);
}
/******************************************************************************/
#if defined(__APPLE__) && defined(__MACH__)
uint64_t moloch_db_memory_size()
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;
}
#elif  defined(__linux__)
uint64_t moloch_db_memory_size()
{
    int fd = open("/proc/self/statm", O_RDONLY, 0);
    if (fd == -1)
        return 0;

    char buf[1024];
    int len = read(fd, buf, sizeof(buf));
    close(fd);

    if (len <= 10) {
        LOG("/proc/self/statm file too small - %d '%.*s'", len, len, buf);

        return 0;
    }

    buf[len] = 0;

    uint64_t size;
    sscanf(buf, "%ld", &size);

    if (size == 0) {
        LOG("/proc/self/statm size 0 - %d '%.*s'", len, len, buf);
    }

    return getpagesize() * size;
}
#else
uint64_t moloch_db_memory_size()
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss * 1024UL;
}
#endif
/******************************************************************************/
uint64_t moloch_db_memory_max()
{
    return (uint64_t)sysconf (_SC_PHYS_PAGES) * (uint64_t)sysconf (_SC_PAGESIZE);
}

/******************************************************************************/
void moloch_db_update_stats(int n, gboolean sync)
{
    static uint64_t       lastPackets[NUMBER_OF_STATS];
    static uint64_t       lastBytes[NUMBER_OF_STATS];
    static uint64_t       lastSessions[NUMBER_OF_STATS];
    static uint64_t       lastDropped[NUMBER_OF_STATS];
    static uint64_t       lastFragsDropped[NUMBER_OF_STATS];
    static uint64_t       lastOverloadDropped[NUMBER_OF_STATS];
    static uint64_t       lastESDropped[NUMBER_OF_STATS];
    static struct rusage  lastUsage[NUMBER_OF_STATS];
    static struct timeval lastTime[NUMBER_OF_STATS];
    static int            intervals[NUMBER_OF_STATS] = {1, 5, 60, 600};
    uint64_t              freeSpaceM = 0;
    uint64_t              totalSpaceM = 0;
    int                   i;
    char                  key[200];
    int                   key_len = 0;

    char *json = moloch_http_get_buffer(MOLOCH_HTTP_BUFFER_SIZE);
    struct timeval currentTime;

    gettimeofday(&currentTime, NULL);

    if (lastPackets[n] == 0) {
        lastTime[n] = startTime;
    }

    uint64_t overloadDropped = moloch_packet_dropped_overload();
    uint64_t totalDropped    = moloch_packet_dropped_packets();
    uint64_t fragsDropped    = moloch_packet_dropped_frags();
    uint64_t esDropped       = moloch_http_dropped_count(esServer);

    for (i = 0; config.pcapDir[i]; i++) {
        struct statvfs vfs;
        statvfs(config.pcapDir[i], &vfs);
        freeSpaceM += (uint64_t)(vfs.f_frsize/1024.0*vfs.f_bavail/1024.0);
        totalSpaceM += (uint64_t)(vfs.f_frsize/1024.0*vfs.f_blocks/1024.0);
    }

    const uint64_t cursec = currentTime.tv_sec;
    uint64_t diffms = (currentTime.tv_sec - lastTime[n].tv_sec)*1000 + (currentTime.tv_usec/1000 - lastTime[n].tv_usec/1000);

    // Prevent FPE
    if (diffms == 0)
        diffms = 1;

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    uint64_t diffusage = (usage.ru_utime.tv_sec - lastUsage[n].ru_utime.tv_sec)*1000 + (usage.ru_utime.tv_usec/1000 - lastUsage[n].ru_utime.tv_usec/1000) +
                         (usage.ru_stime.tv_sec - lastUsage[n].ru_stime.tv_sec)*1000 + (usage.ru_stime.tv_usec/1000 - lastUsage[n].ru_stime.tv_usec/1000);

    dbTotalPackets[n] += (totalPackets - lastPackets[n]);
    dbTotalSessions[n] += (totalSessions - lastSessions[n]);
    dbTotalDropped[n] += (totalDropped - lastDropped[n]);
    dbTotalK[n] += (totalBytes - lastBytes[n])/1024;

    uint64_t mem = moloch_db_memory_size();
    double   memMax = moloch_db_memory_max();
    float    memUse = mem/memMax*100.0;

    if (memUse > config.maxMemPercentage) {
        LOG("Aborting, max memory percentage reached: %.2f > %d", memUse, config.maxMemPercentage);
        fflush(stdout);
        fflush(stderr);
        kill(getpid(), SIGSEGV);
    }

    int json_len = snprintf(json, MOLOCH_HTTP_BUFFER_SIZE,
        "{"
        "\"ver\": \"%s\", "
        "\"nodeName\": \"%s\", "
        "\"hostname\": \"%s\", "
        "\"interval\": %d, "
        "\"currentTime\": %" PRIu64 ", "
        "\"freeSpaceM\": %" PRIu64 ", "
        "\"freeSpaceP\": %.2f, "
        "\"monitoring\": %u, "
        "\"memory\": %" PRIu64 ", "
        "\"memoryP\": %.2f, "
        "\"cpu\": %" PRIu64 ", "
        "\"diskQueue\": %u, "
        "\"esQueue\": %u, "
        "\"packetQueue\": %u, "
        "\"fragsQueue\": %u, "
        "\"frags\": %u, "
        "\"needSave\": %u, "
        "\"closeQueue\": %u, "
        "\"totalPackets\": %" PRIu64 ", "
        "\"totalK\": %" PRIu64 ", "
        "\"totalSessions\": %" PRIu64 ", "
        "\"totalDropped\": %" PRIu64 ", "
        "\"tcpSessions\": %u, "
        "\"udpSessions\": %u, "
        "\"icmpSessions\": %u, "
        "\"deltaPackets\": %" PRIu64 ", "
        "\"deltaBytes\": %" PRIu64 ", "
        "\"deltaSessions\": %" PRIu64 ", "
        "\"deltaDropped\": %" PRIu64 ", "
        "\"deltaFragsDropped\": %" PRIu64 ", "
        "\"deltaOverloadDropped\": %" PRIu64 ", "
        "\"deltaESDropped\": %" PRIu64 ", "
        "\"deltaMS\": %" PRIu64
        "}",
        VERSION,
        config.nodeName,
        config.hostName,
        intervals[n],
        cursec,
        freeSpaceM,
        freeSpaceM*100.0/totalSpaceM,
        moloch_session_monitoring(),
        moloch_db_memory_size(),
        memUse,
        diffusage*10000/diffms,
        moloch_writer_queue_length?moloch_writer_queue_length():0,
        moloch_http_queue_length(esServer),
        moloch_packet_outstanding(),
        moloch_packet_frags_outstanding(),
        moloch_packet_frags_size(),
        moloch_session_need_save_outstanding(),
        moloch_session_close_outstanding(),
        dbTotalPackets[n],
        dbTotalK[n],
        dbTotalSessions[n],
        dbTotalDropped[n],
        moloch_session_watch_count(SESSION_TCP),
        moloch_session_watch_count(SESSION_UDP),
        moloch_session_watch_count(SESSION_ICMP),
        (totalPackets - lastPackets[n]),
        (totalBytes - lastBytes[n]),
        (totalSessions - lastSessions[n]),
        (totalDropped - lastDropped[n]),
        (fragsDropped - lastFragsDropped[n]),
        (overloadDropped - lastOverloadDropped[n]),
        (esDropped - lastESDropped[n]),
        diffms);

    lastTime[n]            = currentTime;
    lastBytes[n]           = totalBytes;
    lastPackets[n]         = totalPackets;
    lastSessions[n]        = totalSessions;
    lastDropped[n]         = totalDropped;
    lastFragsDropped[n]    = fragsDropped;
    lastOverloadDropped[n] = overloadDropped;
    lastESDropped[n]       = esDropped;
    lastUsage[n]           = usage;

    if (n == 0) {
        if (sync)
            moloch_http_send_sync(esServer, "POST", stats_key, stats_key_len, json, json_len, NULL, NULL);
        else
            moloch_http_set(esServer, stats_key, stats_key_len, json, json_len, NULL, NULL);
    } else {
        key_len = snprintf(key, sizeof(key), "/%sdstats/dstat/%s-%d-%d", config.prefix, config.nodeName, (int)(currentTime.tv_sec/intervals[n])%1440, intervals[n]);
        moloch_http_set(esServer, key, key_len, json, json_len, NULL, NULL);
    }
}
/******************************************************************************/
gboolean moloch_db_update_stats_gfunc (gpointer user_data)
{
    moloch_db_update_stats((long)user_data, 0);

    return TRUE;
}
/******************************************************************************/
// Runs on main thread
gboolean moloch_db_flush_gfunc (gpointer user_data )
{
    int             thread;
    struct timeval  currentTime;

    gettimeofday(&currentTime, NULL);

    for (thread = 0; thread < config.packetThreads; thread++) {
        MOLOCH_LOCK(dbInfo[thread].lock);
        if (dbInfo[thread].json && BSB_LENGTH(dbInfo[thread].bsb) > 0 &&
            ((currentTime.tv_sec - dbInfo[thread].lastSave) >= config.dbFlushTimeout || user_data == (gpointer)1)) {

            char   *json = dbInfo[thread].json;
            int     len = BSB_LENGTH(dbInfo[thread].bsb);

            dbInfo[thread].json = 0;
            dbInfo[thread].lastSave = currentTime.tv_sec;
            MOLOCH_UNLOCK(dbInfo[thread].lock);
            // Unlock and then send buffer
            sendBulkFunc(json, len);
        } else {
            MOLOCH_UNLOCK(dbInfo[thread].lock);
        }
    }

    return TRUE;
}
/******************************************************************************/
typedef struct moloch_seq_request {
    char               *name;
    MolochSeqNum_cb     func;
    gpointer            uw;
} MolochSeqRequest_t;

void moloch_db_get_sequence_number(char *name, MolochSeqNum_cb func, gpointer uw);
void moloch_db_get_sequence_number_cb(int UNUSED(code), unsigned char *data, int data_len, gpointer uw)
{
    MolochSeqRequest_t *r = uw;
    uint32_t            version_len;

    unsigned char *version = moloch_js0n_get(data, data_len, "_version", &version_len);

    if (!version_len || !version) {
        LOG("ERROR - Couldn't fetch sequence: %.*s", data_len, data);
        moloch_db_get_sequence_number(r->name, r->func, r->uw);
    } else {
        if (r->func)
            r->func(atoi((char*)version), r->uw);
    }
    MOLOCH_TYPE_FREE(MolochSeqRequest_t, r);
}
/******************************************************************************/
void moloch_db_get_sequence_number(char *name, MolochSeqNum_cb func, gpointer uw)
{
    char                key[100];
    int                 key_len;
    MolochSeqRequest_t *r = MOLOCH_TYPE_ALLOC(MolochSeqRequest_t);
    char               *json = moloch_http_get_buffer(MOLOCH_HTTP_BUFFER_SIZE);

    r->name = name;
    r->func = func;
    r->uw   = uw;

    key_len = snprintf(key, sizeof(key), "/%ssequence/sequence/%s", config.prefix, name);
    int json_len = snprintf(json, MOLOCH_HTTP_BUFFER_SIZE, "{}");
    moloch_http_set(esServer, key, key_len, json, json_len, moloch_db_get_sequence_number_cb, r);
}
/******************************************************************************/
uint32_t moloch_db_get_sequence_number_sync(char *name)
{
    char                key[100];
    int                 key_len;
    unsigned char      *data;
    size_t              data_len;
    unsigned char      *version;
    uint32_t            version_len;

    while (1) {
        key_len = snprintf(key, sizeof(key), "/%ssequence/sequence/%s", config.prefix, name);

        data = moloch_http_send_sync(esServer, "POST", key, key_len, "{}", 2, NULL, &data_len);
        version = moloch_js0n_get(data, data_len, "_version", &version_len);

        if (!version_len || !version) {
            LOG("ERROR - Couldn't fetch sequence: %d %.*s", (int)data_len, (int)data_len, data);
            free(data);
            continue;
        } else {
            uint32_t v = atoi((char *)version);
            free(data);
            return v;
        }
    }
}
/******************************************************************************/
void moloch_db_fn_seq_cb(uint32_t newSeq, gpointer UNUSED(uw))
{
    MOLOCH_LOCK(nextFileNum);
    nextFileNum = newSeq;
    MOLOCH_UNLOCK(nextFileNum);
}
/******************************************************************************/
void moloch_db_load_file_num()
{
    char               key[200];
    int                key_len;
    size_t             data_len;
    unsigned char     *data;
    uint32_t           len;
    unsigned char     *value;
    uint32_t           source_len;
    unsigned char     *source = 0;
    uint32_t           found_len;
    unsigned char     *found = 0;

    /* First see if we have the new style number or not */
    key_len = snprintf(key, sizeof(key), "/%ssequence/sequence/fn-%s", config.prefix, config.nodeName);
    data = moloch_http_get(esServer, key, key_len, &data_len);

    found = moloch_js0n_get(data, data_len, "found", &found_len);
    if (found && memcmp("true", found, 4) == 0) {
        goto fetch_file_num;
    }
    free(data);


    /* Don't have new style numbers, go create them */
    key_len = snprintf(key, sizeof(key), "/%sfiles/file/_search?size=1&sort=num:desc&q=node:%s", config.prefix, config.nodeName);

    data = moloch_http_get(esServer, key, key_len, &data_len);

    uint32_t           hits_len;
    unsigned char     *hits = moloch_js0n_get(data, data_len, "hits", &hits_len);

    if (!hits_len || !hits)
        goto fetch_file_num;

    uint32_t           hit_len;
    unsigned char     *hit = moloch_js0n_get(hits, hits_len, "hits", &hit_len);

    if (!hit_len || !hit)
        goto fetch_file_num;

    /* Remove array wrapper */
    source = moloch_js0n_get(hit+1, hit_len-2, "_source", &source_len);

    if (!source_len || !source)
        goto fetch_file_num;

    int fileNum;
    if ((value = moloch_js0n_get(source, source_len, "num", &len))) {
        fileNum = atoi((char*)value);
    } else {
        LOGEXIT("ERROR - No num field in %.*s", source_len, source);
    }
    free(data);

    /* Now create the new style */
    key_len = snprintf(key, sizeof(key), "/%ssequence/sequence/fn-%s?version_type=external&version=%d", config.prefix, config.nodeName, fileNum + 100);
    data = moloch_http_send_sync(esServer, "POST", key, key_len, "{}", 2, NULL, NULL);

fetch_file_num:
    if (data)
        free(data);

    if (!config.pcapReadOffline) {
        /* If doing a live file create a file number now */
        snprintf(key, sizeof(key), "fn-%s", config.nodeName);
        nextFileNum = moloch_db_get_sequence_number_sync(key);
    }
}
/******************************************************************************/
// Modified From https://github.com/phaag/nfdump/blob/master/bin/flist.c
// Copyright (c) 2014, Peter Haag
void moloch_db_mkpath(char *path)
{
    struct stat sb;
    char *slash = path;
    int done = 0;

    while (!done) {
        slash += strspn(slash, "/");
        slash += strcspn(slash, "/");

        done = (*slash == '\0');
        *slash = '\0';

        if (stat(path, &sb)) {
            if (config.debug) {
                LOG("mkdir(%s)", path);
            }
            if (errno != ENOENT || (mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP) && errno != EEXIST)) {
                LOGEXIT("mkdir() error for '%s': %s\n", path, strerror(errno));
            }
        } else if (!S_ISDIR(sb.st_mode)) {
            LOGEXIT("Path '%s': %s ", path, strerror(ENOTDIR));
        }

        if (!done)
            *slash = '/';
    }
}
/******************************************************************************/
char *moloch_db_create_file_full(time_t firstPacket, char *name, uint64_t size, int locked, uint32_t *id, ...)
{
    char               key[100];
    int                key_len;
    uint32_t           num;
    char               filename[1024];
    struct tm         *tmp;
    char              *json = moloch_http_get_buffer(MOLOCH_HTTP_BUFFER_SIZE);
    BSB                jbsb;
    const uint64_t     fp = firstPacket;
    double             maxFreeSpacePercent = 0;
    uint64_t           maxFreeSpaceBytes   = 0;
    int                i;


    BSB_INIT(jbsb, json, MOLOCH_HTTP_BUFFER_SIZE);

    MOLOCH_LOCK(nextFileNum);
    snprintf(key, sizeof(key), "fn-%s", config.nodeName);
    if (nextFileNum == 0) {
        /* If doing an offline file OR the last async call hasn't returned, just get a sync filenum */
        num = moloch_db_get_sequence_number_sync(key);
    } else {
        /* If doing a live file, use current file num and schedule the next one */
        num = nextFileNum;
        nextFileNum = 0; /* Don't reuse number */
        moloch_db_get_sequence_number(key, moloch_db_fn_seq_cb, 0);
    }


    if (name) {
        static GRegex     *numRegex;
        static GRegex     *numHexRegex;
        if (!numRegex) {
            numRegex = g_regex_new("#NUM#", 0, 0, 0);
            numHexRegex = g_regex_new("#NUMHEX#", 0, 0, 0);
        }
        char numstr[100];
        snprintf(numstr, sizeof(numstr), "%d", num);

        char *name1 = g_regex_replace_literal(numRegex, name, -1, 0, numstr, 0, NULL);
        name = g_regex_replace_literal(numHexRegex, name1, -1, 0, (char *)moloch_char_to_hexstr[num%256], 0, NULL);
        g_free(name1);

        BSB_EXPORT_sprintf(jbsb, "{\"num\":%d, \"name\":\"%s\", \"first\":%" PRIu64 ", \"node\":\"%s\", \"filesize\":%" PRIu64 ", \"locked\":%d", num, name, fp, config.nodeName, size, locked);
        key_len = snprintf(key, sizeof(key), "/%sfiles/file/%s-%d?refresh=true", config.prefix, config.nodeName,num);
    } else {

        uint16_t flen = strlen(config.pcapDir[config.pcapDirPos]);
        if (flen >= sizeof(filename)-1) {
            LOGEXIT("pcapDir %s is too large", config.pcapDir[config.pcapDirPos]);
        }

        strcpy(filename, config.pcapDir[config.pcapDirPos]);

        tmp = localtime(&firstPacket);

        if (config.pcapDirTemplate) {
            int tlen;

            // pcapDirTemplate must start with /, checked in config.c
            if (filename[flen-1] == '/')
                flen--;

            if ((tlen = strftime(filename+flen, sizeof(filename)-flen-1, config.pcapDirTemplate, tmp)) == 0) {
                LOGEXIT("Couldn't form filename: %s %s", config.pcapDir[config.pcapDirPos], config.pcapDirTemplate);
            }
            flen += tlen;
        }

        if (strcmp(config.pcapDirAlgorithm, "max-free-percent") == 0) {
            // Select the pcapDir with the highest percentage of free space
            for (i = 0; config.pcapDir[i]; i++) {
                struct statvfs vfs;
                statvfs(config.pcapDir[i], &vfs);
                LOG("%s has %0.2f%% free", config.pcapDir[i], 100 * ((double)vfs.f_bavail / (double)vfs.f_blocks));
                if ((double)vfs.f_bavail / (double)vfs.f_blocks >= maxFreeSpacePercent)
                {
                    maxFreeSpacePercent = (double)vfs.f_bavail / (double)vfs.f_blocks;
                    config.pcapDirPos = i;
                }
            }
            LOG("%s has the highest percentage of available disk space", config.pcapDir[config.pcapDirPos]);
        } else if (strcmp(config.pcapDirAlgorithm, "max-free-bytes") == 0) {
            // Select the pcapDir with the most bytes free
            for (i = 0; config.pcapDir[i]; i++) {
                struct statvfs vfs;
                statvfs(config.pcapDir[i], &vfs);
                LOG("%s has %" PRIu64 " megabytes available", config.pcapDir[i], (uint64_t)vfs.f_bavail * (uint64_t)vfs.f_frsize / 1024 / 1024);
                if ((uint64_t)vfs.f_bavail * (uint64_t)vfs.f_frsize >= maxFreeSpaceBytes)
                {
                    maxFreeSpaceBytes = (uint64_t)vfs.f_bavail * (uint64_t)vfs.f_frsize;
                    config.pcapDirPos = i;
                }
            }
            LOG("%s has the most available space", config.pcapDir[config.pcapDirPos]);
        } else {
            // Select pcapDir by round robin
            config.pcapDirPos++;
            if (!config.pcapDir[config.pcapDirPos])
                config.pcapDirPos = 0;
        }

        if (filename[flen-1] == '/') {
            flen--;
        }

        struct stat sb;
        if (stat(filename, &sb)) {
            moloch_db_mkpath(filename);
        }

        snprintf(filename+flen, sizeof(filename) - flen, "/%s-%02d%02d%02d-%08d.pcap", config.nodeName, tmp->tm_year%100, tmp->tm_mon+1, tmp->tm_mday, num);

        BSB_EXPORT_sprintf(jbsb, "{\"num\":%d, \"name\":\"%s\", \"first\":%" PRIu64 ", \"node\":\"%s\", \"locked\":%d", num, filename, fp, config.nodeName, locked);
        key_len = snprintf(key, sizeof(key), "/%sfiles/file/%s-%d?refresh=true", config.prefix, config.nodeName,num);
    }

    char    *field, *value;
    va_list  args;
    va_start(args, id);
    while (1) {
        field = va_arg(args, char *);
        if (!field)
            break;

        value = va_arg(args, char *);
        if (!value)
            break;

        BSB_EXPORT_sprintf(jbsb, ", \"%s\": ", field);
        if (*value == '{' || *value == '[')
            BSB_EXPORT_sprintf(jbsb, "%s", value);
        else
            BSB_EXPORT_sprintf(jbsb, "\"%s\"", value);
    }
    va_end(args);

    BSB_EXPORT_u08(jbsb, '}');

    moloch_http_set(esServer, key, key_len, json, BSB_LENGTH(jbsb), NULL, NULL);

    MOLOCH_UNLOCK(nextFileNum);

    if (config.logFileCreation)
        LOG("Creating file %d with key >%s< using >%.*s<", num, key, (int)BSB_LENGTH(jbsb), json);

    *id = num;

    if (name)
        return name;

    return g_strdup(filename);
}
/******************************************************************************/
char *moloch_db_create_file(time_t firstPacket, char *name, uint64_t size, int locked, uint32_t *id)
{
    return moloch_db_create_file_full(firstPacket, name, size, locked, id, NULL);
}
/******************************************************************************/
void moloch_db_check()
{
    size_t             data_len;
    char               key[100];
    int                key_len;
    unsigned char     *data;

    key_len = snprintf(key, sizeof(key), "/%sdstats/version/version/_source", config.prefix);
    data = moloch_http_get(esServer, key, key_len, &data_len);

    if (!data || data_len == 0) {
        LOGEXIT("ERROR - Couldn't load version information, database might be down or out of date.  Run \"db/db.pl host:port upgrade\"");
    }

    uint32_t           version_len;
    unsigned char     *version = 0;

    version = moloch_js0n_get(data, data_len, "version", &version_len);

    if (!version || atoi((char*)version) < MOLOCH_MIN_DB_VERSION) {
        LOGEXIT("ERROR - Database version '%.*s' is too old, needs to be at least (%d), run \"db/db.pl host:port upgrade\"", version_len, version, MOLOCH_MIN_DB_VERSION);
    }
    free(data);

    if (config.compressES) {
        key_len = snprintf(key, sizeof(key), "/_nodes/_local?settings&process&flat_settings");
        data = moloch_http_get(esServer, key, key_len, &data_len);
        if (strstr((char *)data, "\"http.compression\":\"true\"") == NULL) {
            LOGEXIT("ERROR - need to add \"http.compression: true\" to elasticsearch yml file since \"compressES = true\" is set in moloch config");
        }
        free(data);
    }
}

/******************************************************************************/
void moloch_db_load_tags()
{
    size_t             data_len;
    char               key[100];
    int                key_len;

    key_len = snprintf(key, sizeof(key), "/%stags/tag/_search?size=3000", config.prefix);
    unsigned char     *data = moloch_http_get(esServer, key, key_len, &data_len);

    if (!data) {
        return;
    }

    uint32_t           hits_len;
    unsigned char     *hits = 0;
    hits = moloch_js0n_get(data, data_len, "hits", &hits_len);
    if (!hits) {
        free(data);
        return;
    }

    uint32_t           ahits_len;
    unsigned char     *ahits = 0;
    ahits = moloch_js0n_get(hits, hits_len, "hits", &ahits_len);

    if (!ahits) {
        free(data);
        return;
    }

    uint32_t out[2*8000];
    memset(out, 0, sizeof(out));
    js0n(ahits, ahits_len, out);
    int i;
    for (i = 0; out[i]; i+= 2) {
        uint32_t           id_len;
        unsigned char     *id = 0;
        id = moloch_js0n_get(ahits+out[i], out[i+1], "_id", &id_len);

        uint32_t           source_len;
        unsigned char     *source = 0;
        source = moloch_js0n_get(ahits+out[i], out[i+1], "_source", &source_len);
        if (!source) {
            continue;
        }

        uint32_t           n_len;
        unsigned char     *n = 0;
        n = moloch_js0n_get(source, source_len, "n", &n_len);


        if (id && n) {
            MolochTag_t *tag = MOLOCH_TYPE_ALLOC(MolochTag_t);
            tag->tagName = g_strndup((char*)id, (int)id_len);
            if (*n == '[')
                tag->tagValue = atol((char*)n+1);
            else
                tag->tagValue = atol((char*)n);
            HASH_ADD(tag_, tags, tag->tagName, tag);
        } else {
            LOG ("ERROR - Could not load %.*s", out[i+1], ahits+out[i]);
        }
    }
    free(data);
}

typedef struct moloch_tag_request {
    struct moloch_tag_request *t_next, *t_prev;
    int                        t_count;
    void                      *uw;
    MolochTag_cb               func;
    int                        tagtype;
    char                      *tag;
    char                      *escaped;
    uint32_t                   newSeq;
} MolochTagRequest_t;

LOCAL int                     outstandingTagRequests = 0;
LOCAL MolochTagRequest_t      tagRequests;
LOCAL MOLOCH_LOCK_DEFINE(tagRequests);

void moloch_db_tag_cb(int code, unsigned char *data, int data_len, gpointer uw);

/******************************************************************************/
int moloch_db_tags_loading() {
    return outstandingTagRequests + tagRequests.t_count;
}
/******************************************************************************/
void moloch_db_free_tag_request(MolochTagRequest_t *r)
{
    g_free(r->escaped);
    free(r->tag);
    MOLOCH_TYPE_FREE(MolochTagRequest_t, r);
    outstandingTagRequests--;

    while (tagRequests.t_count > 0) {
        char               key[500];
        int                key_len;

        MOLOCH_LOCK(tagRequests);
        DLL_POP_HEAD(t_, &tagRequests, r);
        MOLOCH_UNLOCK(tagRequests);

        MolochTag_t *tag;
        HASH_FIND(tag_, tags, r->tag, tag);

        if (tag) {
            if (r->func)
                r->func(r->uw, r->tagtype, r->tag, tag->tagValue, TRUE);
            g_free(r->escaped);
            free(r->tag);
            MOLOCH_TYPE_FREE(MolochTagRequest_t, r);
            continue;
        }

        key_len = snprintf(key, sizeof(key), "/%stags/tag/%s", config.prefix, r->escaped);
        moloch_http_send(esServer, "GET", key, key_len, NULL, 0, NULL, FALSE, moloch_db_tag_cb, r);
        outstandingTagRequests++;
        break;
    }
}
/******************************************************************************/
void moloch_db_tag_seq_cb(uint32_t newSeq, gpointer uw);
void moloch_db_tag_create_cb(int code, unsigned char *data, int UNUSED(data_len), gpointer uw)
{
    MolochTagRequest_t *r = uw;
    char                key[500];
    int                 key_len;

    // Try again on error
    if (code == 0) {
        moloch_db_tag_seq_cb(r->newSeq, uw);
        return;
    }

    if (strstr((char *)data, "{\"error\":") != 0) {
        key_len = snprintf(key, sizeof(key), "/%stags/tag/%s", config.prefix, r->escaped);
        moloch_http_send(esServer, "GET", key, key_len, NULL, 0, NULL, FALSE, moloch_db_tag_cb, r);
        return;
    }

    MolochTag_t *tag = MOLOCH_TYPE_ALLOC(MolochTag_t);
    tag->tagName = g_strdup(r->tag);
    tag->tagValue = r->newSeq;
    HASH_ADD(tag_, tags, tag->tagName, tag);

    if (r->func)
        r->func(r->uw, r->tagtype, r->tag, r->newSeq, TRUE);
    moloch_db_free_tag_request(r);
}
/******************************************************************************/
void moloch_db_tag_seq_cb(uint32_t newSeq, gpointer uw)
{
    MolochTagRequest_t *r = uw;
    char                key[500];
    int                 key_len;
    char               *json = moloch_http_get_buffer(MOLOCH_HTTP_BUFFER_SIZE);

    r->newSeq = newSeq;

    key_len = snprintf(key, sizeof(key), "/%stags/tag/%s?op_type=create", config.prefix, r->escaped);
    int json_len = snprintf(json, MOLOCH_HTTP_BUFFER_SIZE, "{\"n\":%u}", newSeq);

    moloch_http_set(esServer, key, key_len, json, json_len, moloch_db_tag_create_cb, r);
}
/******************************************************************************/
void moloch_db_tag_cb(int UNUSED(code), unsigned char *data, int data_len, gpointer uw)
{
    MolochTagRequest_t *r = uw;

    if (!data) {
        if (r->func)
            r->func(r->uw, r->tagtype, r->tag, 0, TRUE);
        moloch_db_free_tag_request(r);
        return;
    }

    uint32_t           source_len;
    unsigned char     *source = 0;
    source = moloch_js0n_get(data, data_len, "_source", &source_len);

    if (source) {
        uint32_t           n_len;
        unsigned char     *n = 0;
        n = moloch_js0n_get(source, source_len, "n", &n_len);

        MolochTag_t *tag = MOLOCH_TYPE_ALLOC(MolochTag_t);
        tag->tagName = g_strdup(r->tag);
        if (*n == '[')
            tag->tagValue = atol((char*)n+1);
        else
            tag->tagValue = atol((char*)n);
        HASH_ADD(tag_, tags, tag->tagName, tag);

        if (r->func)
            r->func(r->uw, r->tagtype, r->tag, tag->tagValue, TRUE);
        moloch_db_free_tag_request(r);
        return;
    }

    moloch_db_get_sequence_number("tags", moloch_db_tag_seq_cb, r) ;
}
/******************************************************************************/
uint32_t moloch_db_peek_tag(const char *tagname)
{
    MolochTag_t *tag;
    HASH_FIND(tag_, tags, tagname, tag);

    if (!tag)
        return 0;
    return tag->tagValue;
}
/******************************************************************************/
void moloch_db_get_tag(void *uw, int tagtype, const char *tagname, MolochTag_cb func)
{
    MolochTag_t *tag;
    HASH_FIND(tag_, tags, tagname, tag);

    if (tag) {
        if (func)
            func(uw, tagtype, tagname, tag->tagValue, FALSE);
        return;
    }

    if (config.dryRun) {
        static int tagNum = 1;
        MOLOCH_LOCK(tagRequests);

        HASH_FIND(tag_, tags, tagname, tag);

        if (tag) {
            MOLOCH_UNLOCK(tagRequests);
            if (func)
                func(uw, tagtype, tagname, tag->tagValue, FALSE);
            return;
        }

        MolochTag_t *tag = MOLOCH_TYPE_ALLOC(MolochTag_t);
        tag->tagName = g_strdup(tagname);
        tag->tagValue = tagNum++;
        HASH_ADD(tag_, tags, tag->tagName, tag);
        MOLOCH_UNLOCK(tagRequests);

        if (func)
            func(uw, tagtype, tagname, tag->tagValue, FALSE);
        return;
    }

    MolochTagRequest_t *r = MOLOCH_TYPE_ALLOC(MolochTagRequest_t);
    r->uw = uw;
    r->func    = func;
    r->tag     = strdup(tagname);
    r->tagtype = tagtype;
    r->escaped = g_uri_escape_string (tagname, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH_ELEMENT, 0);



    if (outstandingTagRequests == 0) {
        char               key[500];
        int                key_len;

        key_len = snprintf(key, sizeof(key), "/%stags/tag/%s", config.prefix, r->escaped);
        moloch_http_send(esServer, "GET", key, key_len, NULL, 0, NULL, FALSE, moloch_db_tag_cb, r);
        outstandingTagRequests++;
    } else {
        MOLOCH_LOCK(tagRequests);
        DLL_PUSH_TAIL(t_, &tagRequests, r);
        MOLOCH_UNLOCK(tagRequests);
    }
}
/******************************************************************************/
void moloch_db_load_rir()
{
    memset(rirs, 0, sizeof(rirs));
    if (config.rirFile) {
        FILE *fp;
        char line[1000];
        if (!(fp = fopen(config.rirFile, "r"))) {
            printf("Couldn't open RIR from %s", config.rirFile);
            exit(1);
        }

        while(fgets(line, sizeof(line), fp)) {
            int   cnt = 0, quote = 0, num = 0;
            char *ptr, *start;

            for (start = ptr = line; *ptr != 0; ptr++) {
                if (*ptr == '"') {
                    quote = !quote;
                    continue;
                }

                if (quote || *ptr != ',')
                    continue;

                // We have comma outside of quotes
                *ptr = 0;
                if (cnt == 0) {
                    num = atoi(start);
                    if (num >= 255)
                        break;
                } else if (*start && cnt == 3) {
                    gchar **parts = g_strsplit(start, ".", 0);
                    if (parts[1] && *parts[1]) {
                        rirs[num] = g_ascii_strup(parts[1], -1);
                    }
                    g_strfreev(parts);

                    break;
                }

                cnt++;
                start = ptr+1;
            }
        }
        fclose(fp);
    }
}
/******************************************************************************/
void moloch_db_load_fields()
{
    size_t                 data_len;
    char                   key[100];
    int                    key_len;

    key_len = snprintf(key, sizeof(key), "/%sfields/field/_search?size=3000", config.prefix);
    unsigned char     *data = moloch_http_get(esServer, key, key_len, &data_len);

    if (!data) {
        return;
    }

    uint32_t           hits_len;
    unsigned char     *hits = 0;
    hits = moloch_js0n_get(data, data_len, "hits", &hits_len);
    if (!hits) {
        free(data);
        return;
    }

    uint32_t           ahits_len;
    unsigned char     *ahits = 0;
    ahits = moloch_js0n_get(hits, hits_len, "hits", &ahits_len);

    if (!ahits) {
        free(data);
        return;
    }

    uint32_t out[2*8000];
    memset(out, 0, sizeof(out));
    js0n(ahits, ahits_len, out);
    int i;
    for (i = 0; out[i]; i+= 2) {
        uint32_t           id_len;
        unsigned char     *id = 0;
        id = moloch_js0n_get(ahits+out[i], out[i+1], "_id", &id_len);

        uint32_t           source_len;
        unsigned char     *source = 0;
        source = moloch_js0n_get(ahits+out[i], out[i+1], "_source", &source_len);
        if (!source) {
            continue;
        }

        moloch_field_define_json(id, id_len, source, source_len);
    }
    free(data);
}
/******************************************************************************/
void moloch_db_add_field(char *group, char *kind, char *expression, char *friendlyName, char *dbField, char *help, int haveap, va_list ap)
{
    char                   key[100];
    int                    key_len;
    BSB                    bsb;
    char                  *field, *value;

    if (config.dryRun)
        return;

    char                  *json = moloch_http_get_buffer(10000);

    BSB_INIT(bsb, json, 10000);

    key_len = snprintf(key, sizeof(key), "/%sfields/field/%s", config.prefix, expression);

    BSB_EXPORT_sprintf(bsb, "{\"friendlyName\": \"%s\", \"group\": \"%s\", \"help\": \"%s\", \"dbField\": \"%s\", \"type\": \"%s\"",
             friendlyName,
             group,
             help,
             dbField,
             kind);

    if (haveap) {
        while (1) {
            field = va_arg(ap, char *);
            if (!field)
                break;

            value = va_arg(ap, char *);
            if (!value)
                break;

            BSB_EXPORT_sprintf(bsb, ", \"%s\": ", field);
            if (*value == '{' || *value == '[')
                BSB_EXPORT_sprintf(bsb, "%s", value);
            else
                BSB_EXPORT_sprintf(bsb, "\"%s\"", value);
        }
    }

    BSB_EXPORT_u08(bsb, '}');
    moloch_http_send(esServer, "POST", key, key_len, json, BSB_LENGTH(bsb), NULL, FALSE, NULL, NULL);
}
/******************************************************************************/
void moloch_db_update_field(char *expression, char *name, char *value)
{
    char                   key[1000];
    int                    key_len;
    BSB                    bsb;

    if (config.dryRun)
        return;

    char                  *json = moloch_http_get_buffer(1000);

    BSB_INIT(bsb, json, 1000);

    key_len = snprintf(key, sizeof(key), "/%sfields/field/%s/_update", config.prefix, expression);

    BSB_EXPORT_sprintf(bsb, "{\"doc\": {\"%s\":", name);
    if (*value == '[') {
        BSB_EXPORT_sprintf(bsb, "%s", value);
    } else {
        moloch_db_js0n_str(&bsb, (unsigned char*)value, TRUE);
    }
    BSB_EXPORT_sprintf(bsb, "}}");
    moloch_http_send(esServer, "POST", key, key_len, json, BSB_LENGTH(bsb), NULL, FALSE, NULL, NULL);
}
/******************************************************************************/
void moloch_db_update_filesize(uint32_t fileid, uint64_t filesize)
{
    char                   key[1000];
    int                    key_len;
    int                    json_len;

    if (config.dryRun)
        return;

    char                  *json = moloch_http_get_buffer(1000);

    key_len = snprintf(key, sizeof(key), "/%sfiles/file/%s-%d/_update", config.prefix, config.nodeName, fileid);

    json_len = snprintf(json, 1000, "{\"doc\": {\"filesize\": %" PRIu64 "}}", filesize);

    moloch_http_send(esServer, "POST", key, key_len, json, json_len, NULL, TRUE, NULL, NULL);
}
/******************************************************************************/
gboolean moloch_db_file_exists(char *filename)
{
    size_t                 data_len;
    char                   key[2000];
    int                    key_len;

    key_len = snprintf(key, sizeof(key), "/%sfiles/file/_search?size=1&sort=num:desc&q=node:%s+AND+name:\"%s\"", config.prefix, config.nodeName, filename);

    unsigned char *data = moloch_http_get(esServer, key, key_len, &data_len);

    uint32_t           hits_len;
    unsigned char     *hits = moloch_js0n_get(data, data_len, "hits", &hits_len);

    if (!hits_len || !hits) {
        free(data);
        return FALSE;
    }

    uint32_t           total_len;
    unsigned char     *total = moloch_js0n_get(hits, hits_len, "total", &total_len);

    if (!total_len || !total) {
        free(data);
        return FALSE;
    }

    if (*total != '0') {
        free(data);
        return TRUE;
    }

    free(data);
    return FALSE;
}
/******************************************************************************/
int moloch_db_can_quit()
{
    if (outstandingTagRequests > 0) {
        if (config.debug)
            LOG ("Can't quit, outstandingTagRequests %d", outstandingTagRequests);
        return 1;
    }

    if (tagRequests.t_count > 0) {
        if (config.debug)
            LOG ("Can't quit, tagRequests %d", tagRequests.t_count);
        return 1;
    }

    int thread;
    for (thread = 0; thread < config.packetThreads; thread++) {
        if (dbInfo[thread].json && BSB_LENGTH(dbInfo[thread].bsb) > 0) {
            moloch_db_flush_gfunc((gpointer)1);
            if (config.debug)
                LOG ("Can't quit, sJson[%d] %ld", thread, BSB_LENGTH(dbInfo[thread].bsb));
            return 1;
        }
    }

    if (moloch_http_queue_length(esServer) > 0) {
        if (config.debug)
            LOG ("Can't quit, moloch_http_queue_length(esServer) %d", moloch_http_queue_length(esServer));
        return 1;
    }

    return 0;
}
/******************************************************************************/
static guint timers[10];
void moloch_db_init()
{
    if (config.tests) {
        fprintf(stderr, "{\"sessions\": [\n");
    }
    if (!config.dryRun) {
        esServer = moloch_http_create_server(config.elasticsearch, config.maxESConns, config.maxESRequests, config.compressES);
    }
    DLL_INIT(t_, &tagRequests);
    HASH_INIT(tag_, tags, moloch_db_tag_hash, moloch_db_tag_cmp);
    myPid = getpid();
    gettimeofday(&startTime, NULL);
    if (!config.dryRun) {
        moloch_db_check();
        moloch_db_load_file_num();
        if (!config.noLoadTags)
            moloch_db_load_tags();
        moloch_db_load_stats();
        moloch_db_load_fields();
    }

    moloch_add_can_quit(moloch_db_can_quit, "DB");

    if (config.geoipFile) {
        gi = GeoIP_open(config.geoipFile, GEOIP_MEMORY_CACHE);
        if (!gi) {
            printf("Couldn't initialize GeoIP %s from %s", strerror(errno), config.geoipFile);
            exit(1);
        }
        GeoIP_set_charset(gi, GEOIP_CHARSET_UTF8);
    }

    if (config.geoip6File) {
        gi6 = GeoIP_open(config.geoip6File, GEOIP_MEMORY_CACHE);
        if (!gi6) {
            printf("Couldn't initialize GeoIP %s from %s", strerror(errno), config.geoip6File);
            exit(1);
        }
        GeoIP_set_charset(gi6, GEOIP_CHARSET_UTF8);
    }

    if (config.geoipASNFile) {
        giASN = GeoIP_open(config.geoipASNFile, GEOIP_MEMORY_CACHE);
        if (!giASN) {
            printf("Couldn't initialize GeoIP ASN %s from %s", strerror(errno), config.geoipASNFile);
            exit(1);
        }
        GeoIP_set_charset(giASN, GEOIP_CHARSET_UTF8);
    }

    if (config.geoipASN6File) {
        giASN6 = GeoIP_open(config.geoipASN6File, GEOIP_MEMORY_CACHE);
        if (!giASN6) {
            printf("Couldn't initialize GeoIP ASN 6 %s from %s", strerror(errno), config.geoipASN6File);
            exit(1);
        }
        GeoIP_set_charset(giASN6, GEOIP_CHARSET_UTF8);
    }

    moloch_db_load_rir();

    if (!config.dryRun) {
        timers[0] = g_timeout_add_seconds(  2, moloch_db_update_stats_gfunc, 0);
        timers[1] = g_timeout_add_seconds(  5, moloch_db_update_stats_gfunc, (gpointer)1);
        timers[2] = g_timeout_add_seconds( 60, moloch_db_update_stats_gfunc, (gpointer)2);
        timers[3] = g_timeout_add_seconds(600, moloch_db_update_stats_gfunc, (gpointer)3);
        timers[4] = g_timeout_add_seconds(  1, moloch_db_flush_gfunc, 0);
    }
    int thread;
    for (thread = 0; thread < config.packetThreads; thread++) {
        MOLOCH_LOCK_INIT(dbInfo[thread].lock);
    }
}
/******************************************************************************/
void moloch_db_exit()
{
    int i;

    if (!config.dryRun) {
        for (i = 0; timers[i]; i++) {
            g_source_remove(timers[i]);
        }

        moloch_db_flush_gfunc((gpointer)1);
        moloch_db_update_stats(0, 1);
        moloch_http_free_server(esServer);
    }

    if (config.tests) {
        int comma = 0;
        MolochTag_t *tag;
        fprintf(stderr, "], \"tags\": {\n");
        HASH_FORALL(tag_, tags, tag,
            if (comma)
                fprintf(stderr, ",\n");
            else {
                comma = 1;
                fprintf(stderr, "\n");
            }
            fprintf(stderr, "  \"%d\": \"%s\"", tag->tagValue, tag->tagName);
        );
        fprintf(stderr, "\n}}\n");
    }

    if (ipTree) {
        Destroy_Patricia(ipTree, moloch_db_free_local_ip);
        ipTree = 0;
    }

    MolochTag_t *tag;
    HASH_FORALL_POP_HEAD(tag_, tags, tag,
        g_free(tag->tagName);
        MOLOCH_TYPE_FREE(MolochTag_t, tag);
    );
}
