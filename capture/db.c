/******************************************************************************/
/* db.c  -- Functions dealing with database queries and updates
 *
 * Copyright 2012-2013 AOL Inc. All rights reserved.
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/statvfs.h>
#include "glib.h"
#include "moloch.h"
#include "bsb.h"
#include "patricia.h"
#include "GeoIP.h"

#define MOLOCH_MIN_DB_VERSION 12

extern uint64_t         totalPackets;
extern uint64_t         totalBytes;
extern uint64_t         totalSessions;
static uint16_t         myPid;
static time_t           dbLastSave;

struct timeval          startTime;
static GeoIP           *gi = 0;
static GeoIP           *giASN = 0;

void *                  esServer = 0;

static patricia_tree_t *ipTree;

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
#define int_ntoa(x)     inet_ntoa(*((struct in_addr *)(int*)&x))
MolochIpInfo_t *moloch_db_get_local_ip(MolochSession_t *session, uint32_t ip) 
{
    prefix_t prefix;
    patricia_node_t *node;

    prefix.family = AF_INET;
    prefix.bitlen = 32;
    prefix.add.sin.s_addr = ip;

    if ((node = patricia_search_best (ipTree, &prefix)) == NULL)
        return 0;

    MolochIpInfo_t *ii = node->data;
    int t;

    for (t = 0; t < ii->numtags; t++) {
        moloch_field_int_add(MOLOCH_FIELD_TAGS, session, ii->tags[t]);
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
            BSB_EXPORT_u08(*bsb, '\\');
            BSB_EXPORT_u08(*bsb, 'b');
            break;
        case '\n':
            BSB_EXPORT_u08(*bsb, '\\');
            BSB_EXPORT_u08(*bsb, 'n');
            break;
        case '\r':
            BSB_EXPORT_u08(*bsb, '\\');
            BSB_EXPORT_u08(*bsb, 'r');
            break;
        case '\f':
            BSB_EXPORT_u08(*bsb, '\\');
            BSB_EXPORT_u08(*bsb, 'f');
            break;
        case '\t':
            BSB_EXPORT_u08(*bsb, '\\');
            BSB_EXPORT_u08(*bsb, 't');
            break;
        case '"':
            BSB_EXPORT_u08(*bsb, '\\');
            BSB_EXPORT_u08(*bsb, '"');
            break;
        case '\\':
            BSB_EXPORT_u08(*bsb, '\\');
            BSB_EXPORT_u08(*bsb, '\\');
            break;
        case '/':
            BSB_EXPORT_u08(*bsb, '\\');
            BSB_EXPORT_u08(*bsb, '/');
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
    uint32_t               jsonSize;
    int                    pos;

    /* Let the plugins finish */
    moloch_plugins_cb_save(session, final);


    /* jsonSize is an estimate of how much space it will take to encode the session */
    jsonSize = 1000 + session->filePosArray->len*12 + 10*session->fileNumArray->len + session->certJsonSize;
    for (pos = 0; pos < config.maxField; pos++) {
        if (session->fields[pos]) {
            jsonSize += session->fields[pos]->jsonSize;
        }
    }

    /* No Packets */
    if (!config.dryRun && !session->filePosArray->len)
        return;

    totalSessions++;

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
        moloch_http_set(esServer, key, key_len, sJson, BSB_LENGTH(jbsb), NULL, NULL);
        sJson = 0;

        struct timeval currentTime;
        gettimeofday(&currentTime, NULL);
        dbLastSave = currentTime.tv_sec;
    }

    /* Allocate a new buffer using the max of the bulk size or estimated size. */
    if (!sJson) {
        int size = MAX(config.dbBulkSize, jsonSize);
        sJson = moloch_http_get_buffer(size);
        BSB_INIT(jbsb, sJson, size);
    }

    startPtr = BSB_WORK_PTR(jbsb);

    BSB_EXPORT_sprintf(jbsb, "{\"index\": {\"_index\": \"sessions-%s\", \"_type\": \"session\", \"_id\": \"%s\"}}\n", prefix, id);

    BSB_EXPORT_sprintf(jbsb, 
                      "{\"fp\":%u,"
                      "\"lp\":%u,"
                      "\"fpd\":%" PRIu64 ","
                      "\"lpd\":%" PRIu64 ","
                      "\"a1\":%u,"
                      "\"p1\":%u,"
                      "\"a2\":%u,"
                      "\"p2\":%u,"
                      "\"pr\":%u,",
                      (uint32_t)session->firstPacket.tv_sec,
                      (uint32_t)session->lastPacket.tv_sec,
                      ((uint64_t)session->firstPacket.tv_sec)*1000 + ((uint64_t)session->firstPacket.tv_usec)/1000,
                      ((uint64_t)session->lastPacket.tv_sec)*1000 + ((uint64_t)session->lastPacket.tv_usec)/1000,
                      htonl(session->addr1),
                      session->port1,
                      htonl(session->addr2),
                      session->port2,
                      session->protocol);

    MolochIpInfo_t *ii1 = 0, *ii2 = 0;
    char *g1 = 0, *g2 = 0, *as1 = 0, *as2 = 0;
    if (ipTree) {
        if ((ii1 = moloch_db_get_local_ip(session, session->addr1))) {
            g1 = ii1->country;
            as1 = ii1->asn;
        }

        if ((ii2 = moloch_db_get_local_ip(session, session->addr2))) {
            g2 = ii2->country;
            as2 = ii2->asn;
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
        BSB_EXPORT_sprintf(jbsb, "\"as1\":");
        moloch_db_js0n_str(&jbsb, (unsigned char*)as1, TRUE);
        BSB_EXPORT_u08(jbsb, ',');
        if (!ii1 || !ii1->asn)
            free(as1);
    }

    if (as2) {
        BSB_EXPORT_sprintf(jbsb, "\"as2\":");
        moloch_db_js0n_str(&jbsb, (unsigned char*)as2, TRUE);
        BSB_EXPORT_u08(jbsb, ',');
        if (!ii2 || !ii2->asn)
            free(as2);
    }

    BSB_EXPORT_sprintf(jbsb, 
                      "\"pa\":%u,"
                      "\"by\":%" PRIu64 ","
                      "\"db\":%" PRIu64 ","
                      "\"no\":\"%s\",",
                      session->packets,
                      session->bytes,
                      session->databytes,
                      config.nodeName);
    
    if (session->rootId) {
        if (session->rootId[0] == 'R')
            session->rootId = g_strdup(id);
        BSB_EXPORT_sprintf(jbsb, "\"ro\":\"%s\",", session->rootId);
    }
    BSB_EXPORT_sprintf(jbsb, "\"ps\":[");
    for(i = 0; i < session->filePosArray->len; i++) {
        if (i != 0)
            BSB_EXPORT_u08(jbsb, ',');
        BSB_EXPORT_sprintf(jbsb, "%" PRId64, (uint64_t)g_array_index(session->filePosArray, uint64_t, i));
    }
    BSB_EXPORT_sprintf(jbsb, "],");

    if (HASH_COUNT(t_, session->certs)) {
        BSB_EXPORT_sprintf(jbsb, "\"tlscnt\":%d,", HASH_COUNT(t_, session->certs));
        BSB_EXPORT_sprintf(jbsb, "\"tls\":[");

        MolochCertsInfo_t *certs;
        MolochString_t *string;

        HASH_FORALL_POP_HEAD(t_, session->certs, certs,
            BSB_EXPORT_u08(jbsb, '{');

            if (certs->issuer.commonName.s_count > 0) {
                BSB_EXPORT_sprintf(jbsb, "\"iCn\":[");
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
                BSB_EXPORT_sprintf(jbsb, "\"iOn\":");
                moloch_db_js0n_str(&jbsb, (unsigned char *)certs->issuer.orgName, certs->issuer.orgUtf8);
                BSB_EXPORT_u08(jbsb, ',');
            }

            if (certs->subject.commonName.s_count) {
                BSB_EXPORT_sprintf(jbsb, "\"sCn\":[");
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
                BSB_EXPORT_sprintf(jbsb, "\"sOn\":");
                moloch_db_js0n_str(&jbsb, (unsigned char *)certs->subject.orgName, certs->subject.orgUtf8);
                BSB_EXPORT_u08(jbsb, ',');
            }

            if (certs->serialNumber) {
                int k;
                BSB_EXPORT_sprintf(jbsb, "\"sn\":\"");
                for (k = 0; k < certs->serialNumberLen; k++) {
                    BSB_EXPORT_sprintf(jbsb, "%02x", certs->serialNumber[k]);
                }
                BSB_EXPORT_u08(jbsb, '"');
                BSB_EXPORT_u08(jbsb, ',');
            }

            if (certs->alt.s_count) {
                BSB_EXPORT_sprintf(jbsb, "\"altcnt\":%d,", certs->alt.s_count);
                BSB_EXPORT_sprintf(jbsb, "\"alt\":[");
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

            BSB_EXPORT_rewind(jbsb, 1); // Remove last comma

            moloch_nids_certs_free(certs);
            i++;

            BSB_EXPORT_u08(jbsb, '}');
            BSB_EXPORT_u08(jbsb, ',');
        );

        BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
        BSB_EXPORT_sprintf(jbsb, "],");
    }


    BSB_EXPORT_sprintf(jbsb, "\"fs\":[");
    for(i = 0; i < session->fileNumArray->len; i++) {
        if (i == 0)
            BSB_EXPORT_sprintf(jbsb, "%u", (uint32_t)g_array_index(session->fileNumArray, uint32_t, i));
        else
            BSB_EXPORT_sprintf(jbsb, ",%u", (uint32_t)g_array_index(session->fileNumArray, uint32_t, i));
    }
    BSB_EXPORT_sprintf(jbsb, "],");

    int inHeaders = 0;
    for (pos = 0; pos < config.maxField; pos++) {
        if (session->fields[pos]) {
            int freeField = final || ((config.fields[pos]->flags & MOLOCH_FIELD_FLAG_CONTINUE) == 0);

            if (!inHeaders && config.fields[pos]->flags & MOLOCH_FIELD_FLAG_HEADERS) {
                BSB_EXPORT_sprintf(jbsb, "\"hdrs\": {");
                inHeaders = 1;
            } else if (inHeaders && (config.fields[pos]->flags & MOLOCH_FIELD_FLAG_HEADERS) == 0) {
                inHeaders = 0;
                BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
                BSB_EXPORT_sprintf(jbsb, "},");
            }

            switch(config.fields[pos]->type) {
            case MOLOCH_FIELD_TYPE_STR:
                BSB_EXPORT_sprintf(jbsb, "\"%s\":", config.fields[pos]->name);
                moloch_db_js0n_str(&jbsb, 
                                   (unsigned char *)session->fields[pos]->str, 
                                   config.fields[pos]->flags & MOLOCH_FIELD_FLAG_FORCE_UTF8);
                BSB_EXPORT_u08(jbsb, ',');
                if (freeField) {
                    g_free(session->fields[pos]->str);
                }
                break;
            case MOLOCH_FIELD_TYPE_STR_ARRAY:
                if (config.fields[pos]->flags & MOLOCH_FIELD_FLAG_CNT) {
                    BSB_EXPORT_sprintf(jbsb, "\"%scnt\":%d,", config.fields[pos]->name, session->fields[pos]->sarray->len);
                }
                BSB_EXPORT_sprintf(jbsb, "\"%s\":[", config.fields[pos]->name);
                for(i = 0; i < session->fields[pos]->sarray->len; i++) {
                    moloch_db_js0n_str(&jbsb, 
                                       g_ptr_array_index(session->fields[pos]->sarray, i), 
                                       config.fields[pos]->flags & MOLOCH_FIELD_FLAG_FORCE_UTF8);
                    BSB_EXPORT_u08(jbsb, ',');
                }
                BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
                BSB_EXPORT_sprintf(jbsb, "],");
                if (freeField) {
                    g_ptr_array_free(session->fields[pos]->sarray, TRUE);
                }
                break;
            case MOLOCH_FIELD_TYPE_STR_HASH:
                shash = session->fields[pos]->shash;
                if (config.fields[pos]->flags & MOLOCH_FIELD_FLAG_CNT) {
                    BSB_EXPORT_sprintf(jbsb, "\"%scnt\":%d,", config.fields[pos]->name, HASH_COUNT(s_, *shash));
                }
                BSB_EXPORT_sprintf(jbsb, "\"%s\":[", config.fields[pos]->name);
                HASH_FORALL(s_, *shash, hstring,
                    moloch_db_js0n_str(&jbsb, (unsigned char *)hstring->str, hstring->utf8 || config.fields[pos]->flags & MOLOCH_FIELD_FLAG_FORCE_UTF8);
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
                BSB_EXPORT_sprintf(jbsb, "],");
                break;
            case MOLOCH_FIELD_TYPE_INT_HASH:
                ihash = session->fields[pos]->ihash;
                if (config.fields[pos]->flags & MOLOCH_FIELD_FLAG_CNT) {
                    BSB_EXPORT_sprintf(jbsb, "\"%scnt\": %d,", config.fields[pos]->name, HASH_COUNT(i_, *ihash));
                }
                BSB_EXPORT_sprintf(jbsb, "\"%s\":[", config.fields[pos]->name);
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
                BSB_EXPORT_sprintf(jbsb, "],");
                break;
            case MOLOCH_FIELD_TYPE_IP_HASH:
                ihash = session->fields[pos]->ihash;
                if (config.fields[pos]->flags & MOLOCH_FIELD_FLAG_CNT) {
                    BSB_EXPORT_sprintf(jbsb, "\"%scnt\":%d,", config.fields[pos]->name, HASH_COUNT(i_, *ihash));
                }
                if (config.fields[pos]->flags & MOLOCH_FIELD_FLAG_SCNT) {
                    BSB_EXPORT_sprintf(jbsb, "\"%sscnt\":%d,", config.fields[pos]->name, HASH_COUNT(i_, *ihash));
                }

                if (gi || ipTree) {
                    MolochIpInfo_t *ii;

                    BSB_EXPORT_sprintf(jbsb, "\"g%s\":[", config.fields[pos]->name);
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
                            BSB_EXPORT_sprintf(jbsb, "\"---\"");
                        }
                        BSB_EXPORT_u08(jbsb, ',');
                    );
                    BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
                    BSB_EXPORT_sprintf(jbsb, "],");
                }

                if (giASN || ipTree) {
                    MolochIpInfo_t *ii = 0;

                    BSB_EXPORT_sprintf(jbsb, "\"as%s\":[", config.fields[pos]->name);
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
                            BSB_EXPORT_sprintf(jbsb, "\"---\"");
                        }
                        BSB_EXPORT_u08(jbsb, ',');
                    );
                    BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
                    BSB_EXPORT_sprintf(jbsb, "],");
                }


                BSB_EXPORT_sprintf(jbsb, "\"%s\":[", config.fields[pos]->name);
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

                BSB_EXPORT_sprintf(jbsb, "],");
            }
            if (freeField) {
                MOLOCH_TYPE_FREE(MolochField_t, session->fields[pos]);
                session->fields[pos] = 0;
            }
        }
    }

    if (inHeaders) {
        BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
        BSB_EXPORT_sprintf(jbsb, "},");
    }

    BSB_EXPORT_rewind(jbsb, 1); // Remove last comma
    BSB_EXPORT_sprintf(jbsb, "}\n");

    if (BSB_IS_ERROR(jbsb)) {
        LOG("ERROR - Ran out of memory creating DB record supposed to be %d", jsonSize);
        return;
    }

    if (config.dryRun) {
        if (config.debug)
            LOG("%.*s\n", (int)BSB_LENGTH(jbsb), sJson);
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
    size_t             datalen;
    uint32_t           len;
    uint32_t           source_len;
    unsigned char     *source = 0;

    stats_key_len = snprintf(stats_key, sizeof(stats_key), "/stats/stat/%s", config.nodeName);

    unsigned char     *data = moloch_http_get(esServer, stats_key, stats_key_len, &datalen);

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

    char *json = moloch_http_get_buffer(MOLOCH_HTTP_BUFFER_SIZE);
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
    uint64_t mem = (uint64_t)sbrk(0);
    int diffms = (currentTime.tv_sec - dbLastTime.tv_sec)*1000 + (currentTime.tv_usec/1000 - dbLastTime.tv_usec/1000);
    int json_len = snprintf(json, MOLOCH_HTTP_BUFFER_SIZE,
        "{"
        "\"hostname\": \"%s\", "
        "\"currentTime\": %u, "
        "\"freeSpaceM\": %" PRIu64 ", "
        "\"monitoring\": %u, "
        "\"memory\": %" PRIu64 ", "
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
        (uint64_t)(vfs.f_frsize/1024.0*vfs.f_bavail/1024.0),
        moloch_nids_monitoring_sessions(),
        mem,
        moloch_nids_disk_queue(),
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
    moloch_http_set(esServer, stats_key, stats_key_len, json, json_len, NULL, NULL);
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

    char *json = moloch_http_get_buffer(MOLOCH_HTTP_BUFFER_SIZE);
    struct timeval currentTime;

    gettimeofday(&currentTime, NULL);

    key_len = snprintf(key, sizeof(key), "/dstats/dstat/%s-%d-%d", config.nodeName, (int)(currentTime.tv_sec/intervals[n])%1440, intervals[n]);
    if (lastPackets[n] == 0) {
        lastTime[n] = startTime;
    }

    uint64_t totalDropped = moloch_nids_dropped_packets();

    struct statvfs vfs;
    statvfs(config.pcapDir, &vfs);

    const uint64_t cursec = currentTime.tv_sec;
    const uint64_t diffms = (currentTime.tv_sec - lastTime[n].tv_sec)*1000 + (currentTime.tv_usec/1000 - lastTime[n].tv_usec/1000);
    uint64_t mem = (uint64_t)sbrk(0);

    int json_len = snprintf(json, MOLOCH_HTTP_BUFFER_SIZE,
        "{"
        "\"nodeName\": \"%s\", "
        "\"interval\": %d, "
        "\"currentTime\": %" PRIu64 ", "
        "\"freeSpaceM\": %" PRIu64 ", "
        "\"monitoring\": %u, "
        "\"memory\": %" PRIu64 ", "
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
        (uint64_t)(vfs.f_frsize/1024.0*vfs.f_bavail/1024.0),
        moloch_nids_monitoring_sessions(),
        mem,
        moloch_nids_disk_queue(),
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
void moloch_db_get_sequence_number_cb(unsigned char *data, int data_len, gpointer uw)
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

    key_len = snprintf(key, sizeof(key), "/sequence/sequence/%s", name);
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

    key_len = snprintf(key, sizeof(key), "/sequence/sequence/%s", name);

    data = moloch_http_send_sync(esServer, "POST", key, key_len, "{}", 2, &data_len);
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
    uint32_t           exists_len;
    unsigned char     *exists = 0;

    /* First see if we have the new style number or not */
    key_len = snprintf(key, sizeof(key), "/sequence/sequence/fn-%s", config.nodeName);
    data = moloch_http_get(esServer, key, key_len, &data_len);

    exists = moloch_js0n_get(data, data_len, "exists", &exists_len);
    if (exists && memcmp("true", exists, 4) == 0) {
        goto fetch_file_num;
        return;
    }


    /* Don't have new style numbers, go create them */
    key_len = snprintf(key, sizeof(key), "/files/file/_search?size=1&sort=num:desc&q=node:%s", config.nodeName);

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
    key_len = snprintf(key, sizeof(key), "/sequence/sequence/fn-%s?version_type=external&version=%d", config.nodeName, fileNum + 100);
    moloch_http_send_sync(esServer, "POST", key, key_len, "{}", 2, NULL);

fetch_file_num:
    if (!(config.pcapReadFile || config.pcapReadDir)) {
        /* If doing a live file create a file number now */
        snprintf(key, sizeof(key), "fn-%s", config.nodeName);
        nextFileNum = moloch_db_get_sequence_number_sync(key);
    }
}
/******************************************************************************/
char *moloch_db_create_file(time_t firstPacket, char *name, uint64_t size, uint32_t *id)
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
        json_len = snprintf(json, MOLOCH_HTTP_BUFFER_SIZE, "{\"num\":%d, \"name\":\"%s\", \"first\":%" PRIu64 ", \"node\":\"%s\", \"filesize\":%" PRIu64 ", \"locked\":1}", num, name, fp, config.nodeName, size);
        key_len = snprintf(key, sizeof(key), "/files/file/%s-%d?refresh=true", config.nodeName,num);
    } else {
        tmp = localtime(&firstPacket);

        strcpy(filename, config.pcapDir);
        if (filename[strlen(filename)-1] != '/')
            strcat(filename, "/");
        snprintf(filename+strlen(filename), sizeof(filename) - strlen(filename), "%s-%02d%02d%02d-%08d.pcap", config.nodeName, tmp->tm_year%100, tmp->tm_mon+1, tmp->tm_mday, num);

        json_len = snprintf(json, MOLOCH_HTTP_BUFFER_SIZE, "{\"num\":%d, \"name\":\"%s\", \"first\":%" PRIu64 ", \"node\":\"%s\", \"locked\":0}", num, filename, fp, config.nodeName);
        key_len = snprintf(key, sizeof(key), "/files/file/%s-%d?refresh=true", config.nodeName,num);
    }

    moloch_http_set(esServer, key, key_len, json, json_len, NULL, NULL);

    if (config.logFileCreation)
        LOG("Creating file %d with key >%s< using >%s<", num, key, json);

    *id = num;

    if (name)
        return name;

    return filename;
}
/******************************************************************************/
void moloch_db_check()
{
    size_t             datalen;
    char               key[100];
    int                key_len;
    unsigned char     *data;

    key_len = snprintf(key, sizeof(key), "/dstats/version/version/_source");
    data = moloch_http_get(esServer, key, key_len, &datalen);

    if (!data) {
        LOG("ERROR - Couldn't load version information, database might out down or out of date.  Run \"db/db.pl host:port update\"");
        exit(1);
    }

    uint32_t           version_len;
    unsigned char     *version = 0;

    version = moloch_js0n_get(data, datalen, "version", &version_len);

    if (atoi((char*)version) < MOLOCH_MIN_DB_VERSION) {
        LOG("ERROR - Database version (%.*s) too old, needs to be at least (%d), run \"db/db.pl host:port update\"", version_len, version, MOLOCH_MIN_DB_VERSION);
        exit(1);
    }

    if (config.compressES) {
        key_len = snprintf(key, sizeof(key), "/_nodes/_local?settings&process");
        data = moloch_http_get(esServer, key, key_len, &datalen);
        if (strstr((char *)data, "\"http.compression\":\"true\"") == NULL) {
            LOG("ERROR - need to add \"http.compression: true\" to elasticsearch yml file since \"compressES = true\" is set in moloch config");
            exit(1);
        }
    }
}

/******************************************************************************/
void moloch_db_load_tags()
{
    size_t             datalen;
    char               key[100];
    int                key_len;

    key_len = snprintf(key, sizeof(key), "/tags/tag/_search?size=3000&fields=n");
    unsigned char     *data = moloch_http_get(esServer, key, key_len, &datalen);

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
            MolochTag_t *tag = MOLOCH_TYPE_ALLOC(MolochTag_t);
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
    void                      *uw;
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
                r->func(r->uw, r->tagtype, tag->tagValue);
            g_free(r->escaped);
            free(r->tag);
            MOLOCH_TYPE_FREE(MolochTagRequest_t, r);
            continue;
        }

        key_len = snprintf(key, sizeof(key), "/tags/tag/%s?fields=n", r->escaped);
        moloch_http_send(esServer, "GET", key, key_len, NULL, 0, FALSE, moloch_db_tag_cb, r);
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
        moloch_http_send(esServer, "GET", key, key_len, NULL, 0, FALSE, moloch_db_tag_cb, r);
        return;
    }

    MolochTag_t *tag = MOLOCH_TYPE_ALLOC(MolochTag_t);
    tag->tagName = g_strdup(r->tag);
    tag->tagValue = r->newSeq;
    HASH_ADD(tag_, tags, tag->tagName, tag);

    if (r->func)
        r->func(r->uw, r->tagtype, r->newSeq);
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

    key_len = snprintf(key, sizeof(key), "/tags/tag/%s?op_type=create", r->escaped);
    int json_len = snprintf(json, MOLOCH_HTTP_BUFFER_SIZE, "{\"n\":%u}", newSeq);

    moloch_http_set(esServer, key, key_len, json, json_len, moloch_db_tag_create_cb, r);
}
/******************************************************************************/
void moloch_db_tag_cb(unsigned char *data, int data_len, gpointer uw)
{
    MolochTagRequest_t *r = uw;

    if (!data) {
        if (r->func)
            r->func(r->uw, r->tagtype, 0);
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
        tag->tagValue = atol((char*)n);
        HASH_ADD(tag_, tags, tag->tagName, tag);

        if (r->func)
            r->func(r->uw, r->tagtype, atol((char *)n));
        moloch_db_free_tag_request(r);
        return;
    }

    moloch_db_get_sequence_number("tags", moloch_db_tag_seq_cb, r) ;
}
/******************************************************************************/
void moloch_db_get_tag(void *uw, int tagtype, const char *tagname, MolochTag_cb func)
{
    MolochTag_t *tag;
    HASH_FIND(tag_, tags, tagname, tag);

    if (tag) {
        if (func)
            func(uw, tagtype, tag->tagValue);
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

        key_len = snprintf(key, sizeof(key), "/tags/tag/%s?fields=n", r->escaped);
        moloch_http_send(esServer, "GET", key, key_len, NULL, 0, FALSE, moloch_db_tag_cb, r);
        outstandingTagRequests++;
    } else {
        DLL_PUSH_TAIL(t_, &tagRequests, r);
    }
}
/******************************************************************************/
guint timers[5];
void moloch_db_init()
{
    esServer = moloch_http_create_server(config.elasticsearch, 9200, config.maxESConns, config.maxESRequests, config.compressES);
    DLL_INIT(t_, &tagRequests);
    HASH_INIT(tag_, tags, moloch_db_tag_hash, moloch_db_tag_cmp);
    moloch_db_load_file_num();
    myPid = getpid();
    gettimeofday(&startTime, NULL);
    moloch_db_check();
    moloch_db_load_tags();
    moloch_db_load_stats();

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
    }
    moloch_http_free_server(esServer);
}
