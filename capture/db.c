/******************************************************************************/
/* db.c  -- Functions dealing with database queries and updates
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
#include <uuid/uuid.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/statvfs.h>
#include "moloch.h"
#include "bsb.h"
#include "glib.h"
#include "patricia.h"
#include "GeoIP.h"

#define MOLOCH_MIN_DB_VERSION 23

extern uint64_t         totalPackets;
extern uint64_t         totalBytes;
extern uint64_t         totalSessions;
static uint16_t         myPid;
static time_t           dbLastSave;
extern uint32_t         pluginsCbs;

struct timeval          startTime;
static GeoIP           *gi = 0;
static GeoIP           *giASN = 0;
static char            *rirs[256];

void *                  esServer = 0;

patricia_tree_t        *ipTree = 0;

extern char            *moloch_char_to_hex;
extern unsigned char    moloch_char_to_hexstr[256][3];
extern unsigned char    moloch_hex_to_char[256][256];

static int tagsField = -1;

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

HASH_VAR(tag_, tags, MolochTag_t, 9337);

/******************************************************************************/
void moloch_db_add_local_ip(char *str, MolochIpInfo_t *ii)
{
    patricia_node_t *node;
    if (!ipTree) {
        ipTree = New_Patricia(32);
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
}
/******************************************************************************/
#define int_ntoa(x)     inet_ntoa(*((struct in_addr *)(int*)&x))
MolochIpInfo_t *moloch_db_get_local_ip(MolochSession_t *session, uint32_t ip)
{
    prefix_t prefix;
    patricia_node_t *node;

    prefix.family = AF_INET;
    prefix.bitlen = 32;
    prefix.add.sin.s_addr = ip;

    if ((node = patricia_search_best2 (ipTree, &prefix, 1)) == NULL)
        return 0;

    if (tagsField == -1)
        tagsField = moloch_field_by_db("ta");

    MolochIpInfo_t *ii = node->data;
    int t;

    for (t = 0; t < ii->numtags; t++) {
        moloch_field_int_add(tagsField, session, ii->tags[t]);
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
static char *sJson = 0;
static BSB jbsb;

void moloch_db_save_session(MolochSession_t *session, int final)
{
    uint32_t               i;
    char                   id[100];
    char                   key[100];
    int                    key_len;
    uuid_t                 uuid;
    MolochString_t        *hstring;
    MolochInt_t           *hint;
    MolochStringHashStd_t *shash;
    MolochIntHashStd_t    *ihash;
    unsigned char         *startPtr;
    unsigned char         *dataPtr;
    uint32_t               jsonSize;
    int                    pos;

    /* Let the plugins finish */
    if (pluginsCbs & MOLOCH_PLUGIN_SAVE)
        moloch_plugins_cb_save(session, final);

    /* jsonSize is an estimate of how much space it will take to encode the session */
    jsonSize = 1100 + session->filePosArray->len*12 + 10*session->fileNumArray->len + 10*session->fileLenArray->len;
    for (pos = 0; pos < session->maxFields; pos++) {
        if (session->fields[pos]) {
            jsonSize += session->fields[pos]->jsonSize;
        }
    }

    /* No Packets */
    if (!config.dryRun && !session->filePosArray->len)
        return;

    totalSessions++;
    session->segments++;

    static char     prefix[100];
    static time_t   prefix_time = 0;

    if (prefix_time != session->lastPacket.tv_sec) {
        prefix_time = session->lastPacket.tv_sec;
        struct tm *tmp = gmtime(&prefix_time);

        switch(config.rotate) {
        case MOLOCH_ROTATE_HOURLY:
            snprintf(prefix, sizeof(prefix), "%02d%02d%02dh%02d", tmp->tm_year%100, tmp->tm_mon+1, tmp->tm_mday, tmp->tm_hour);
            break;
        case MOLOCH_ROTATE_DAILY:
            snprintf(prefix, sizeof(prefix), "%02d%02d%02d", tmp->tm_year%100, tmp->tm_mon+1, tmp->tm_mday);
            break;
        case MOLOCH_ROTATE_WEEKLY:
            snprintf(prefix, sizeof(prefix), "%02dw%02d", tmp->tm_year%100, tmp->tm_yday/7);
            break;
        case MOLOCH_ROTATE_MONTHLY:
            snprintf(prefix, sizeof(prefix), "%02dm%02d", tmp->tm_year%100, tmp->tm_mon+1);
            break;
        }
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

    /* If no room left to add, send the buffer */
    if (sJson && (uint32_t)BSB_REMAINING(jbsb) < jsonSize) {
        if (BSB_LENGTH(jbsb) > 0) {
            moloch_http_set(esServer, key, key_len, sJson, BSB_LENGTH(jbsb), NULL, NULL);
        }
        sJson = 0;

        struct timeval currentTime;
        gettimeofday(&currentTime, NULL);
        dbLastSave = currentTime.tv_sec;
    }

    /* Allocate a new buffer using the max of the bulk size or estimated size. */
    if (!sJson) {
        const int size = MAX(config.dbBulkSize, jsonSize);
        sJson = moloch_http_get_buffer(size);
        BSB_INIT(jbsb, sJson, size);
    }

    uint32_t timediff = (session->lastPacket.tv_sec - session->firstPacket.tv_sec)*1000 +
                        (session->lastPacket.tv_usec - session->firstPacket.tv_usec)/1000;

    startPtr = BSB_WORK_PTR(jbsb);
    BSB_EXPORT_sprintf(jbsb, "{\"index\": {\"_index\": \"%ssessions-%s\", \"_type\": \"session\", \"_id\": \"%s\"}}\n", config.prefix, prefix, id);

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
                      htonl(session->addr1),
                      session->port1,
                      htonl(session->addr2),
                      session->port2,
                      session->protocol);

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

    MolochIpInfo_t *ii1 = 0, *ii2 = 0;
    char *g1 = 0, *g2 = 0, *as1 = 0, *as2 = 0, *rir1 = 0, *rir2 = 0;

    if (ipTree) {
        if ((ii1 = moloch_db_get_local_ip(session, session->addr1))) {
            g1 = ii1->country;
            as1 = ii1->asn;
            rir1 = ii1->rir;
        }

        if ((ii2 = moloch_db_get_local_ip(session, session->addr2))) {
            g2 = ii2->country;
            as2 = ii2->asn;
            rir2 = ii2->rir;
        }
    }

    if (gi) {
        if (!g1)
            g1 = (char *)GeoIP_country_code3_by_ipnum(gi, htonl(session->addr1));

        if (!g2)
            g2 = (char *)GeoIP_country_code3_by_ipnum(gi, htonl(session->addr2));
    }

    if (g1)
        BSB_EXPORT_sprintf(jbsb, "\"g1\":\"%s\",", g1);
    if (g2)
        BSB_EXPORT_sprintf(jbsb, "\"g2\":\"%s\",", g2);

    if (giASN) {
        if (!as1) {
            as1 = GeoIP_name_by_ipnum(giASN, htonl(session->addr1));
        }

        if (!as2) {
            as2 = GeoIP_name_by_ipnum(giASN, htonl(session->addr2));
        }
    }
    if (as1) {
        BSB_EXPORT_cstr(jbsb, "\"as1\":");
        moloch_db_js0n_str(&jbsb, (unsigned char*)as1, TRUE);
        BSB_EXPORT_u08(jbsb, ',');
        if (!ii1 || !ii1->asn)
            free(as1);
    }

    if (as2) {
        BSB_EXPORT_cstr(jbsb, "\"as2\":");
        moloch_db_js0n_str(&jbsb, (unsigned char*)as2, TRUE);
        BSB_EXPORT_u08(jbsb, ',');
        if (!ii2 || !ii2->asn)
            free(as2);
    }

    if (!rir1)
        rir1 = rirs[session->addr1 & 0xff];

    if (rir1)
        BSB_EXPORT_sprintf(jbsb, "\"rir1\":\"%s\",", rir1);

    if (!rir2)
        rir2 = rirs[session->addr2 & 0xff];

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
        case MOLOCH_FIELD_TYPE_IP: {
            const int             value = session->fields[pos]->i;
            const MolochIpInfo_t *ii = ipTree?moloch_db_get_local_ip(session, value):0;
            char                 *as = NULL;
            const char           *g = NULL;
            const char           *rir = NULL;
            const int             post = (flags & MOLOCH_FIELD_FLAG_IPPRE) == 0;

            if (ii) {
                g = ii->country;
                as = ii->asn;
                rir = ii->rir;
            }

            if (gi || g) {

                if (!g) {
                    g = GeoIP_country_code3_by_ipnum(gi, htonl(value));
                }

                if (g) {
                    if (post)
                        BSB_EXPORT_sprintf(jbsb, "\"%s-geo\":\"%s\",", config.fields[pos]->dbField, g);
                    else
                        BSB_EXPORT_sprintf(jbsb, "\"g%s\":\"%s\",", config.fields[pos]->dbField, g);
                }
            }

            if (giASN || as) {
                if (!as) {
                    as = GeoIP_name_by_ipnum(giASN, htonl(value));
                }

                if (as) {
                    if (post)
                        BSB_EXPORT_sprintf(jbsb, "\"%s-asn\":", config.fields[pos]->dbField);
                    else
                        BSB_EXPORT_sprintf(jbsb, "\"as%s\":", config.fields[pos]->dbField);
                    moloch_db_js0n_str(&jbsb, (unsigned char*)as, TRUE);
                    if (!ii || !ii->asn) {
                        free(as);
                    }
                    BSB_EXPORT_u08(jbsb, ',');
                }
            }

            if (config.rirFile || rir) {
                if (!rir) {
                    rir = rirs[value & 0xff];
                }

                if (rir) {
                    if (post)
                        BSB_EXPORT_sprintf(jbsb, "\"%s-rir\":\"%s\",", config.fields[pos]->dbField, rir);
                    else
                        BSB_EXPORT_sprintf(jbsb, "\"rir%s\":\"%s\",", config.fields[pos]->dbField, rir);
                }
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

            if (gi || ipTree) {
                const MolochIpInfo_t *ii;

                if (post)
                    BSB_EXPORT_sprintf(jbsb, "\"%s-geo\":[", config.fields[pos]->dbField);
                else
                    BSB_EXPORT_sprintf(jbsb, "\"g%s\":[", config.fields[pos]->dbField);
                HASH_FORALL(i_, *ihash, hint,
                    const char *g = NULL;
                    if (ipTree && (ii = moloch_db_get_local_ip(session, hint->i_hash))) {
                        g = ii->country;
                    }

                    if (!g) {
                        g = GeoIP_country_code3_by_ipnum(gi, htonl(hint->i_hash));
                    }

                    if (g) {
                        BSB_EXPORT_sprintf(jbsb, "\"%s\"", g);
                    } else {
                        BSB_EXPORT_cstr(jbsb, "\"---\"");
                    }
                    BSB_EXPORT_u08(jbsb, ',');
                );
                BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
                BSB_EXPORT_cstr(jbsb, "],");
            }

            if (giASN || ipTree) {
                const MolochIpInfo_t *ii = 0;

                if (post)
                    BSB_EXPORT_sprintf(jbsb, "\"%s-asn\":[", config.fields[pos]->dbField);
                else
                    BSB_EXPORT_sprintf(jbsb, "\"as%s\":[", config.fields[pos]->dbField);
                HASH_FORALL(i_, *ihash, hint,
                    char *as = NULL;

                    if (ipTree && (ii = moloch_db_get_local_ip(session, hint->i_hash))) {
                        as = ii->asn;
                    }

                    if (!as) {
                        as = GeoIP_name_by_ipnum(giASN, htonl(hint->i_hash));
                    }

                    if (as) {
                        moloch_db_js0n_str(&jbsb, (unsigned char*)as, TRUE);
                        if (!ii || !ii->asn) {
                            free(as);
                        }
                    } else {
                        BSB_EXPORT_cstr(jbsb, "\"---\"");
                    }
                    BSB_EXPORT_u08(jbsb, ',');
                );
                BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
                BSB_EXPORT_cstr(jbsb, "],");
            }

            if (config.rirFile || ipTree) {
                const MolochIpInfo_t *ii = 0;

                if (post)
                    BSB_EXPORT_sprintf(jbsb, "\"%s-rir\":[", config.fields[pos]->dbField);
                else
                    BSB_EXPORT_sprintf(jbsb, "\"rir%s\":[", config.fields[pos]->dbField);
                HASH_FORALL(i_, *ihash, hint,
                    char *rir = NULL;

                    if (ipTree && (ii = moloch_db_get_local_ip(session, hint->i_hash))) {
                        rir = ii->rir;
                    }

                    if (!rir) {
                        rir = rirs[hint->i_hash & 0xff];
                    }

                    if (rir) {
                        BSB_EXPORT_sprintf(jbsb, "\"%s\",", rir);
                    } else {
                        BSB_EXPORT_cstr(jbsb, "\"\",");
                    }
                );
                BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
                BSB_EXPORT_cstr(jbsb, "],");
            }


            BSB_EXPORT_sprintf(jbsb, "\"%s\":[", config.fields[pos]->dbField);
            HASH_FORALL(i_, *ihash, hint,
                BSB_EXPORT_sprintf(jbsb, "%u", htonl(hint->i_hash));
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
        return;
    }

    if (config.dryRun) {
        if (config.tests) {
            const int hlen = dataPtr - startPtr;
            fprintf(stderr, "  %s{\"header\":%.*s,\n  \"body\":%.*s}\n", (totalSessions==1 ? "":","), hlen-1, sJson, (int)(BSB_LENGTH(jbsb)-hlen-1), sJson+hlen);
        } else if (config.debug) {
            LOG("%.*s\n", (int)BSB_LENGTH(jbsb), sJson);
        }
        BSB_INIT(jbsb, sJson, BSB_SIZE(jbsb));
        return;
    }

    if (config.noSPI) {
        BSB_INIT(jbsb, sJson, BSB_SIZE(jbsb));
        return;
    }

    if (jsonSize < (uint32_t)(BSB_WORK_PTR(jbsb) - startPtr)) {
        LOG("WARNING - BIGGER then expected json %d %d\n", jsonSize,  (int)(BSB_WORK_PTR(jbsb) - startPtr));
        if (config.debug)
            LOG("Data:\n%.*s\n", (int)(BSB_WORK_PTR(jbsb) - startPtr), startPtr);
    }
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
    size_t             data_len;
    uint32_t           len;
    uint32_t           source_len;
    unsigned char     *source = 0;

    stats_key_len = snprintf(stats_key, sizeof(stats_key), "/%sstats/stat/%s", config.prefix, config.nodeName);

    unsigned char     *data = moloch_http_get(esServer, stats_key, stats_key_len, &data_len);

    source = moloch_js0n_get(data, data_len, "_source", &source_len);
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
    static uint64_t       lastPackets = 0;
    static uint64_t       lastBytes = 0;
    static uint64_t       lastSessions = 0;
    static uint64_t       lastDropped = 0;
    uint64_t              freeSpaceM = 0;
    static struct rusage  lastUsage;
    int                   i;

    struct timeval currentTime;

    gettimeofday(&currentTime, NULL);

    if (lastPackets != 0 && currentTime.tv_sec == dbLastTime.tv_sec)
        return;

    char *json = moloch_http_get_buffer(MOLOCH_HTTP_BUFFER_SIZE);

    uint64_t totalDropped = moloch_nids_dropped_packets();

    for (i = 0; config.pcapDir[i]; i++) {
        struct statvfs vfs;
        statvfs(config.pcapDir[i], &vfs);
        freeSpaceM += (uint64_t)(vfs.f_frsize/1024.0*vfs.f_bavail/1024.0);
    }

    dbTotalPackets += (totalPackets - lastPackets);
    dbTotalSessions += (totalSessions - lastSessions);
    dbTotalDropped += (totalDropped - lastDropped);
    dbTotalK += (totalBytes - lastBytes)/1024;

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    int diffms = (currentTime.tv_sec - dbLastTime.tv_sec)*1000 + (currentTime.tv_usec/1000 - dbLastTime.tv_usec/1000);
    uint64_t diffusage = (usage.ru_utime.tv_sec - lastUsage.ru_utime.tv_sec)*1000 + (usage.ru_utime.tv_usec/1000 - lastUsage.ru_utime.tv_usec/1000) +
                         (usage.ru_stime.tv_sec - lastUsage.ru_stime.tv_sec)*1000 + (usage.ru_stime.tv_usec/1000 - lastUsage.ru_stime.tv_usec/1000);

    int json_len = snprintf(json, MOLOCH_HTTP_BUFFER_SIZE,
        "{"
        "\"hostname\": \"%s\", "
        "\"currentTime\": %u, "
        "\"freeSpaceM\": %" PRIu64 ", "
        "\"monitoring\": %u, "
#if defined(__APPLE__) && defined(__MACH__)
        "\"memory\": %ld, "
#else
        "\"memory\": %" PRIu64 ", "
#endif
        "\"cpu\": %" PRIu64 ", "
        "\"diskQueue\": %u, "
        "\"totalPackets\": %" PRIu64 ", "
        "\"totalK\": %" PRIu64 ", "
        "\"totalSessions\": %" PRIu64 ", "
        "\"totalDropped\": %" PRIu64 ", "
        "\"deltaPackets\": %" PRIu64 ", "
        "\"deltaBytes\": %" PRIu64 ", "
        "\"deltaSessions\": %" PRIu64 ", "
        "\"deltaDropped\": %" PRIu64 ", "
        "\"deltaMS\": %u"
        "}",
        config.hostName,
        (uint32_t)currentTime.tv_sec,
        freeSpaceM,
        moloch_nids_monitoring_sessions(),
#if defined(__APPLE__) && defined(__MACH__)
        usage.ru_maxrss,
#else
        usage.ru_maxrss * 1024UL,
#endif
        diffusage*10000/diffms,
        moloch_writer_queue_length?moloch_writer_queue_length():0,
        dbTotalPackets,
        dbTotalK,
        dbTotalSessions,
        dbTotalDropped,
        (totalPackets - lastPackets),
        (totalBytes - lastBytes),
        (totalSessions - lastSessions),
        (totalDropped - lastDropped),
        diffms);

    dbLastTime   = currentTime;
    lastBytes    = totalBytes;
    lastPackets  = totalPackets;
    lastSessions = totalSessions;
    lastDropped  = totalDropped;
    lastUsage    = usage;

    moloch_http_set(esServer, stats_key, stats_key_len, json, json_len, NULL, NULL);
}

/******************************************************************************/
void moloch_db_update_dstats(int n)
{
    static uint64_t       lastPackets[2] = {0, 0};
    static uint64_t       lastBytes[2] = {0, 0};
    static uint64_t       lastSessions[2] = {0, 0};
    static uint64_t       lastDropped[2] = {0, 0};
    static struct rusage  lastUsage[2];
    static struct timeval lastTime[2];
    static int            intervals[2] = {5, 60};
    uint64_t              freeSpaceM = 0;
    int                   i;
    char                  key[200];
    int                   key_len = 0;

    char *json = moloch_http_get_buffer(MOLOCH_HTTP_BUFFER_SIZE);
    struct timeval currentTime;

    gettimeofday(&currentTime, NULL);

    key_len = snprintf(key, sizeof(key), "/%sdstats/dstat/%s-%d-%d", config.prefix, config.nodeName, (int)(currentTime.tv_sec/intervals[n])%1440, intervals[n]);
    if (lastPackets[n] == 0) {
        lastTime[n] = startTime;
    }

    uint64_t totalDropped = moloch_nids_dropped_packets();

    for (i = 0; config.pcapDir[i]; i++) {
        struct statvfs vfs;
        statvfs(config.pcapDir[i], &vfs);
        freeSpaceM += (uint64_t)(vfs.f_frsize/1024.0*vfs.f_bavail/1024.0);
    }

    const uint64_t cursec = currentTime.tv_sec;
    const uint64_t diffms = (currentTime.tv_sec - lastTime[n].tv_sec)*1000 + (currentTime.tv_usec/1000 - lastTime[n].tv_usec/1000);

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    uint64_t diffusage = (usage.ru_utime.tv_sec - lastUsage[n].ru_utime.tv_sec)*1000 + (usage.ru_utime.tv_usec/1000 - lastUsage[n].ru_utime.tv_usec/1000) +
                         (usage.ru_stime.tv_sec - lastUsage[n].ru_stime.tv_sec)*1000 + (usage.ru_stime.tv_usec/1000 - lastUsage[n].ru_stime.tv_usec/1000);

    int json_len = snprintf(json, MOLOCH_HTTP_BUFFER_SIZE,
        "{"
        "\"nodeName\": \"%s\", "
        "\"interval\": %d, "
        "\"currentTime\": %" PRIu64 ", "
        "\"freeSpaceM\": %" PRIu64 ", "
        "\"monitoring\": %u, "
#if defined(__APPLE__) && defined(__MACH__)
        "\"memory\": %ld, "
#else
        "\"memory\": %" PRIu64 ", "
#endif
        "\"cpu\": %" PRIu64 ", "
        "\"diskQueue\": %u, "
        "\"deltaPackets\": %" PRIu64 ", "
        "\"deltaBytes\": %" PRIu64 ", "
        "\"deltaSessions\": %" PRIu64 ", "
        "\"deltaDropped\": %" PRIu64 ", "
        "\"deltaMS\": %" PRIu64
        "}",
        config.nodeName,
        intervals[n],
        cursec,
        freeSpaceM,
        moloch_nids_monitoring_sessions(),
#if defined(__APPLE__) && defined(__MACH__)
        usage.ru_maxrss,
#else
        usage.ru_maxrss * 1024UL,
#endif
        diffusage*10000/diffms,
        moloch_writer_queue_length?moloch_writer_queue_length():0,
        (totalPackets - lastPackets[n]),
        (totalBytes - lastBytes[n]),
        (totalSessions - lastSessions[n]),
        (totalDropped - lastDropped[n]),
        diffms);

    lastTime[n]     = currentTime;
    lastBytes[n]    = totalBytes;
    lastPackets[n]  = totalPackets;
    lastSessions[n] = totalSessions;
    lastDropped[n]  = totalDropped;
    lastUsage[n]    = usage;
    moloch_http_set(esServer, key, key_len, json, json_len, NULL, NULL);
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

    if (!sJson || BSB_LENGTH(jbsb) == 0)
        return TRUE;

    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    if (user_data == 0 && (currentTime.tv_sec - dbLastSave) < config.dbFlushTimeout)
        return TRUE;

    key_len = snprintf(key, sizeof(key), "/_bulk");
    moloch_http_set(esServer, key, key_len, sJson, BSB_LENGTH(jbsb), NULL, NULL);
    sJson = 0;
    dbLastSave = currentTime.tv_sec;

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

    key_len = snprintf(key, sizeof(key), "/%ssequence/sequence/%s", config.prefix, name);

    data = moloch_http_send_sync(esServer, "POST", key, key_len, "{}", 2, NULL, &data_len);
    version = moloch_js0n_get(data, data_len, "_version", &version_len);

    if (!version_len || !version) {
        LOG("ERROR - Couldn't fetch sequence: %.*s", (int)data_len, data);
        return moloch_db_get_sequence_number_sync(name);
    } else {
        return atoi((char *)version);
    }
}
/******************************************************************************/
uint32_t nextFileNum;
void moloch_db_fn_seq_cb(uint32_t newSeq, gpointer UNUSED(uw))
{
    nextFileNum = newSeq;
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
        LOG("ERROR - No num field in %.*s", source_len, source);
        exit (0);
    }

    /* Now create the new style */
    key_len = snprintf(key, sizeof(key), "/%ssequence/sequence/fn-%s?version_type=external&version=%d", config.prefix, config.nodeName, fileNum + 100);
    moloch_http_send_sync(esServer, "POST", key, key_len, "{}", 2, NULL, NULL);

fetch_file_num:
    if (!config.pcapReadOffline) {
        /* If doing a live file create a file number now */
        snprintf(key, sizeof(key), "fn-%s", config.nodeName);
        nextFileNum = moloch_db_get_sequence_number_sync(key);
    }
}
/******************************************************************************/
char *moloch_db_create_file(time_t firstPacket, char *name, uint64_t size, int locked, uint32_t *id)
{
    char               key[100];
    int                key_len;
    uint32_t           num;
    static char        filename[1024];
    struct tm         *tmp;
    char              *json = moloch_http_get_buffer(MOLOCH_HTTP_BUFFER_SIZE);
    int                json_len;
    const uint64_t     fp = firstPacket;


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

        json_len = snprintf(json, MOLOCH_HTTP_BUFFER_SIZE, "{\"num\":%d, \"name\":\"%s\", \"first\":%" PRIu64 ", \"node\":\"%s\", \"filesize\":%" PRIu64 ", \"locked\":%d}", num, name, fp, config.nodeName, size, locked);
        key_len = snprintf(key, sizeof(key), "/%sfiles/file/%s-%d?refresh=true", config.prefix, config.nodeName,num);
    } else {
        tmp = localtime(&firstPacket);

        strcpy(filename, config.pcapDir[config.pcapDirPos++]);
        if (!config.pcapDir[config.pcapDirPos])
            config.pcapDirPos = 0;

        if (filename[strlen(filename)-1] != '/')
            strcat(filename, "/");
        snprintf(filename+strlen(filename), sizeof(filename) - strlen(filename), "%s-%02d%02d%02d-%08d.pcap", config.nodeName, tmp->tm_year%100, tmp->tm_mon+1, tmp->tm_mday, num);

        json_len = snprintf(json, MOLOCH_HTTP_BUFFER_SIZE, "{\"num\":%d, \"name\":\"%s\", \"first\":%" PRIu64 ", \"node\":\"%s\", \"locked\":%d}", num, filename, fp, config.nodeName, locked);
        key_len = snprintf(key, sizeof(key), "/%sfiles/file/%s-%d?refresh=true", config.prefix, config.nodeName,num);
    }

    moloch_http_set(esServer, key, key_len, json, json_len, NULL, NULL);

    if (config.logFileCreation)
        LOG("Creating file %d with key >%s< using >%s<", num, key, json);

    *id = num;

    if (name)
        return name;

    return g_strdup(filename);
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
        LOG("ERROR - Couldn't load version information, database might be down or out of date.  Run \"db/db.pl host:port upgrade\"");
        exit(1);
    }

    uint32_t           version_len;
    unsigned char     *version = 0;

    version = moloch_js0n_get(data, data_len, "version", &version_len);

    if (!version || atoi((char*)version) < MOLOCH_MIN_DB_VERSION) {
        LOG("ERROR - Database version '%.*s' is too old, needs to be at least (%d), run \"db/db.pl host:port upgrade\"", version_len, version, MOLOCH_MIN_DB_VERSION);
        exit(1);
    }

    if (config.compressES) {
        key_len = snprintf(key, sizeof(key), "/_nodes/_local?settings&process&flat_settings");
        data = moloch_http_get(esServer, key, key_len, &data_len);
        if (strstr((char *)data, "\"http.compression\":\"true\"") == NULL) {
            LOG("ERROR - need to add \"http.compression: true\" to elasticsearch yml file since \"compressES = true\" is set in moloch config");
            exit(1);
        }
    }
}

/******************************************************************************/
void moloch_db_load_tags()
{
    size_t             data_len;
    char               key[100];
    int                key_len;

    key_len = snprintf(key, sizeof(key), "/%stags/tag/_search?size=3000&fields=n", config.prefix);
    unsigned char     *data = moloch_http_get(esServer, key, key_len, &data_len);

    if (!data) {
        return;
    }

    uint32_t           hits_len;
    unsigned char     *hits = 0;
    hits = moloch_js0n_get(data, data_len, "hits", &hits_len);
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

static int                     outstandingTagRequests = 0;
static MolochTagRequest_t      tagRequests;

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

        DLL_POP_HEAD(t_, &tagRequests, r);

        MolochTag_t *tag;
        HASH_FIND(tag_, tags, r->tag, tag);

        if (tag) {
            if (r->func)
                r->func(r->uw, r->tagtype, r->tag, tag->tagValue);
            g_free(r->escaped);
            free(r->tag);
            MOLOCH_TYPE_FREE(MolochTagRequest_t, r);
            continue;
        }

        key_len = snprintf(key, sizeof(key), "/%stags/tag/%s?fields=n", config.prefix, r->escaped);
        moloch_http_send(esServer, "GET", key, key_len, NULL, 0, NULL, FALSE, moloch_db_tag_cb, r);
        outstandingTagRequests++;
        break;
    }
}
/******************************************************************************/

void moloch_db_tag_create_cb(int UNUSED(code), unsigned char *data, int UNUSED(data_len), gpointer uw)
{
    MolochTagRequest_t *r = uw;
    char                key[500];
    int                 key_len;

    if (strstr((char *)data, "{\"error\":") != 0) {
        key_len = snprintf(key, sizeof(key), "/%stags/tag/%s?fields=n", config.prefix, r->escaped);
        moloch_http_send(esServer, "GET", key, key_len, NULL, 0, NULL, FALSE, moloch_db_tag_cb, r);
        return;
    }

    MolochTag_t *tag = MOLOCH_TYPE_ALLOC(MolochTag_t);
    tag->tagName = g_strdup(r->tag);
    tag->tagValue = r->newSeq;
    HASH_ADD(tag_, tags, tag->tagName, tag);

    if (r->func)
        r->func(r->uw, r->tagtype, r->tag, r->newSeq);
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
            r->func(r->uw, r->tagtype, r->tag, 0);
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

        MolochTag_t *tag = MOLOCH_TYPE_ALLOC(MolochTag_t);
        tag->tagName = g_strdup(r->tag);
        if (*n == '[')
            tag->tagValue = atol((char*)n+1);
        else
            tag->tagValue = atol((char*)n);
        HASH_ADD(tag_, tags, tag->tagName, tag);

        if (r->func)
            r->func(r->uw, r->tagtype, r->tag, tag->tagValue);
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
            func(uw, tagtype, tagname, tag->tagValue);
        return;
    }

    if (config.dryRun) {
        static int tagNum = 1;
        MolochTag_t *tag = MOLOCH_TYPE_ALLOC(MolochTag_t);
        tag->tagName = g_strdup(tagname);
        tag->tagValue = tagNum++;
        HASH_ADD(tag_, tags, tag->tagName, tag);

        if (func)
            func(uw, tagtype, tagname, tag->tagValue);
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

        key_len = snprintf(key, sizeof(key), "/%stags/tag/%s?fields=n", config.prefix, r->escaped);
        moloch_http_send(esServer, "GET", key, key_len, NULL, 0, NULL, FALSE, moloch_db_tag_cb, r);
        outstandingTagRequests++;
    } else {
        DLL_PUSH_TAIL(t_, &tagRequests, r);
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
        fields = moloch_js0n_get(ahits+out[i], out[i+1], "_source", &fields_len);
        if (!fields) {
            continue;
        }

        moloch_field_define_json(id, id_len, fields, fields_len);
    }
}
/******************************************************************************/
void moloch_db_add_field(char *group, char *kind, char *expression, char *friendlyName, char *dbField, char *help, va_list ap)
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

    if (ap) {
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
    char                   key[100];
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
gboolean moloch_db_file_exists(char *filename)
{
    size_t                 data_len;
    char                   key[2000];
    int                    key_len;

    key_len = snprintf(key, sizeof(key), "/%sfiles/file/_search?size=1&sort=num:desc&q=node:%s+AND+name:\"%s\"", config.prefix, config.nodeName, filename);

    LOG("query: %s", key);
    unsigned char *data = moloch_http_get(esServer, key, key_len, &data_len);
    LOG("data: %.*s", (int)data_len, data);

    uint32_t           hits_len;
    unsigned char     *hits = moloch_js0n_get(data, data_len, "hits", &hits_len);

    if (!hits_len || !hits)
        return FALSE;

    uint32_t           total_len;
    unsigned char     *total = moloch_js0n_get(hits, hits_len, "total", &total_len);

    if (!total_len || !total)
        return FALSE;

    if (*total != '0')
        return TRUE;

    return FALSE;
}
/******************************************************************************/
guint timers[5];
void moloch_db_init()
{
    if (config.tests) {
        fprintf(stderr, "{\"packets\": [\n");
    }
    if (!config.dryRun) {
        esServer = moloch_http_create_server(config.elasticsearch, 9200, config.maxESConns, config.maxESRequests, config.compressES);
    }
    DLL_INIT(t_, &tagRequests);
    HASH_INIT(tag_, tags, moloch_db_tag_hash, moloch_db_tag_cmp);
    myPid = getpid();
    gettimeofday(&startTime, NULL);
    if (!config.dryRun) {
        moloch_db_check();
        moloch_db_load_file_num();
        moloch_db_load_tags();
        moloch_db_load_stats();
        moloch_db_load_fields();
    }

    if (config.geoipFile) {
        gi = GeoIP_open(config.geoipFile, GEOIP_MEMORY_CACHE);
        if (!gi) {
            printf("Couldn't initialize GeoIP %s from %s", strerror(errno), config.geoipFile);
            exit(1);
        }
        GeoIP_set_charset(gi, GEOIP_CHARSET_UTF8);
    }

    if (config.geoipASNFile) {
        giASN = GeoIP_open(config.geoipASNFile, GEOIP_MEMORY_CACHE);
        if (!giASN) {
            printf("Couldn't initialize GeoIP ASN %s from %s", strerror(errno), config.geoipASNFile);
            exit(1);
        }
        GeoIP_set_charset(giASN, GEOIP_CHARSET_UTF8);
    }

    moloch_db_load_rir();

    if (!config.dryRun) {
        timers[0] = g_timeout_add_seconds( 2, moloch_db_update_stats_gfunc, 0);
        timers[1] = g_timeout_add_seconds( 5, moloch_db_update_stats_gfunc, (gpointer)1);
        timers[2] = g_timeout_add_seconds(60, moloch_db_update_stats_gfunc, (gpointer)2);
        timers[3] = g_timeout_add_seconds( 1, moloch_db_flush_gfunc, 0);
    }
}
/******************************************************************************/
void moloch_db_exit()
{
    int i;

    if (!config.dryRun) {
        for (i = 0; i < 4; i++) {
            g_source_remove(timers[i]);
        }

        moloch_db_flush_gfunc((gpointer)1);
        moloch_db_update_stats();
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
        Clear_Patricia(ipTree, moloch_db_free_local_ip);
    }

    MolochTag_t *tag;
    HASH_FORALL_POP_HEAD(tag_, tags, tag,
        g_free(tag->tagName);
        MOLOCH_TYPE_FREE(MolochTag_t, tag);
    );
}
