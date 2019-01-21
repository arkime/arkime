/* wise.c  -- With Intelligence See Everything
 *
 *  Simple plugin that queries the wise service for
 *  ips, domains, email, and md5s which can use various
 *  services to return data.  It caches all the results.
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

extern MolochConfig_t        config;

LOCAL void                 *wiseService;

LOCAL uint32_t              maxConns;
LOCAL uint32_t              maxRequests;
LOCAL uint32_t              maxCache;
LOCAL uint32_t              cacheSecs;
LOCAL char                  tcpTuple;
LOCAL char                  udpTuple;
LOCAL uint32_t              logEvery;


LOCAL int                   protocolField;

LOCAL uint32_t              fieldsTS;

#define FIELDS_MAP_MAX      21
LOCAL int                   fieldsMap[FIELDS_MAP_MAX][MOLOCH_FIELDS_DB_MAX];
LOCAL char                 *fieldsMapHash[FIELDS_MAP_MAX];
LOCAL int                   fieldsMapCnt;

LOCAL uint32_t              inflight;

LOCAL char                **wiseExcludeDomains;
LOCAL int                  *wiseExcludeDomainsLen;
LOCAL int                   wiseExcludeDomainsNum;

LOCAL char                 *wiseURL;
LOCAL int                   wisePort;
LOCAL char                 *wiseHost;

LOCAL char                  wiseGetURI[4096];

LOCAL const int validDNS[256] = {
    ['-'] = 1,
    ['_'] = 1,
    ['a' ... 'z'] = 1,
    ['A' ... 'Z'] = 1,
    ['0' ... '9'] = 1
};

#define INTEL_TYPE_IP      0
#define INTEL_TYPE_DOMAIN  1
#define INTEL_TYPE_MD5     2
#define INTEL_TYPE_EMAIL   3
#define INTEL_TYPE_URL     4
#define INTEL_TYPE_TUPLE   5
#define INTEL_TYPE_JA3     6
#define INTEL_TYPE_SHA256  7
#define INTEL_TYPE_NUM_PRE 8
#define INTEL_TYPE_SIZE    32

#define INTEL_TYPE_MAX_FIELDS  32

#define INTEL_STAT_LOOKUP     0
#define INTEL_STAT_CACHE      1
#define INTEL_STAT_REQUEST    2
#define INTEL_STAT_INPROGRESS 3
#define INTEL_STAT_FAIL       4
#define INTEL_STAT_SIZE       5

LOCAL uint32_t stats[INTEL_TYPE_SIZE][INTEL_STAT_SIZE];
/******************************************************************************/
typedef struct wiseitem {
    struct wiseitem      *wih_next, *wih_prev;
    struct wiseitem      *wil_next, *wil_prev;
    uint32_t              wih_bucket;
    uint32_t              wih_hash;

    MolochFieldOps_t      ops;
    MolochSession_t     **sessions;
    char                 *key;

    uint32_t              loadTime;
    uint16_t              sessionsSize;
    uint16_t              numSessions;
    char                  type;
} WiseItem_t;

typedef struct wiseitem_head {
    struct wiseitem      *wih_next, *wih_prev;
    struct wiseitem      *wil_next, *wil_prev;
    short                 wih_bucket;
    uint32_t              wih_count;
    uint32_t              wil_count;
} WiseItemHead_t;

#define WISE_MAX_REQUEST_ITEMS 512
typedef struct wiserequest {
    BSB          bsb;
    WiseItem_t  *items[WISE_MAX_REQUEST_ITEMS];
    int          numItems;
} WiseRequest_t;

typedef HASH_VAR(h_, WiseItemHash_t, WiseItemHead_t, 199337);

struct {
    char           *name;
    WiseItemHash_t  itemHash;
    WiseItemHead_t  itemList;
    int             fields[INTEL_TYPE_MAX_FIELDS];
    char            fieldsLen;
    char            nameLen;
} types[INTEL_TYPE_SIZE];

int            numTypes = INTEL_TYPE_NUM_PRE;

LOCAL MOLOCH_LOCK_DEFINE(item);

/******************************************************************************/
LOCAL WiseRequest_t *iRequest = 0;
LOCAL MOLOCH_LOCK_DEFINE(iRequest);
LOCAL char          *iBuf = 0;

/******************************************************************************/
LOCAL int wise_item_cmp(const void *keyv, const void *elementv)
{
    char *key = (char*)keyv;
    WiseItem_t *element = (WiseItem_t *)elementv;

    return strcmp(key, element->key) == 0;
}
/******************************************************************************/
LOCAL void wise_print_stats()
{
    for (int i = 0; i < numTypes; i++) {
        LOG("%8s lookups:%7d cache:%7d requests:%7d inprogress:%7d fail:%7d hash:%7d list:%7d",
            types[i].name,
            stats[i][0],
            stats[i][1],
            stats[i][2],
            stats[i][3],
            stats[i][4],
            HASH_COUNT(wih_, types[i].itemHash),
            DLL_COUNT(wil_, &types[i].itemList));
    }
}
/******************************************************************************/
LOCAL void wise_load_fields()
{
    char                key[500];
    int                 key_len;

    memset(fieldsMap[0], -1, sizeof(fieldsMap[0]));

    key_len = snprintf(key, sizeof(key), "/fields?ver=1");
    size_t         data_len;
    unsigned char *data = moloch_http_send_sync(wiseService, "GET", key, key_len, NULL, 0, NULL, &data_len);;

    BSB bsb;
    BSB_INIT(bsb, data, data_len);

    int ver = -1, cnt = 0;
    BSB_IMPORT_u32(bsb, fieldsTS);
    BSB_IMPORT_u32(bsb, ver);

    if (ver < 0 || ver > 1) {
        if (wiseURL) {
            LOGEXIT("Verify wiseURL value of `%s` version: %d - %s",
                    wiseURL, ver,
                    (ver == -1?"Couldn't connect to WISE":"Unsupported version"));
        } else {
            LOGEXIT("Verify wiseHost:wisePort value of `%s:%d` version: %d - %s",
                    wiseHost, wisePort, ver,
                    (ver == -1?"Couldn't connect to WISE":"Unsupported version"));
        }
    }

    if (ver == 0) {
        BSB_IMPORT_u08(bsb, cnt);
    } else if (ver == 1) {
        BSB_IMPORT_u16(bsb, cnt);
    }

    if (cnt > MOLOCH_FIELDS_DB_MAX) {
        LOGEXIT("Wise server is returning too many fields %d > %d", cnt, MOLOCH_FIELDS_DB_MAX);
    }

    for (int i = 0; i < cnt; i++) {
        int len = 0;
        BSB_IMPORT_u16(bsb, len); // len includes NULL terminated
        fieldsMap[0][i] = moloch_field_define_text((char*)BSB_WORK_PTR(bsb), NULL);
        if (fieldsMap[0][i] == -1) {
            fieldsTS = 0;
            if (config.debug)
                LOG("Couldn't define field - %d %d %s", i, fieldsMap[0][i], BSB_WORK_PTR(bsb));
        }
        BSB_IMPORT_skip(bsb, len);
    }
    free(data);
}
/******************************************************************************/
LOCAL void wise_session_cmd_cb(MolochSession_t *session, gpointer uw1, gpointer UNUSED(uw2))
{
    WiseItem_t    *wi = uw1;

    if (wi) {
        moloch_field_ops_run(session, &wi->ops);
    }
    moloch_session_decr_outstanding(session);
}
/******************************************************************************/
LOCAL void wise_free_item(WiseItem_t *wi)
{
    g_free(wi->key);
    moloch_field_ops_free(&wi->ops);
    MOLOCH_TYPE_FREE(WiseItem_t, wi);
}
/******************************************************************************/
LOCAL void wise_remove_item_locked(WiseItem_t *wi)
{
    HASH_REMOVE(wih_, types[(int)wi->type].itemHash, wi);
    if (wi->sessions) {
        for (int i = 0; i < wi->numSessions; i++) {
            moloch_session_add_cmd(wi->sessions[i], MOLOCH_SES_CMD_FUNC, NULL, NULL, wise_session_cmd_cb);
        }
        g_free(wi->sessions);
        wi->sessions = 0;
    }
    moloch_free_later(wi, (GDestroyNotify) wise_free_item);
}
/******************************************************************************/
LOCAL void wise_cb(int UNUSED(code), unsigned char *data, int data_len, gpointer uw)
{

    BSB             bsb;
    WiseRequest_t *request = uw;
    int             i;

    inflight -= request->numItems;

    BSB_INIT(bsb, data, data_len);

    uint32_t fts = 0, ver = 0xffffffff;
    BSB_IMPORT_u32(bsb, fts);
    BSB_IMPORT_u32(bsb, ver);

    if (BSB_IS_ERROR(bsb) || (ver != 0 && ver != 2)) {
        MOLOCH_LOCK(item);
        for (i = 0; i < request->numItems; i++) {
            wise_remove_item_locked(request->items[i]);
        }
        MOLOCH_UNLOCK(item);
        MOLOCH_TYPE_FREE(WiseRequest_t, request);
        return;
    }

    if (ver == 0 && fts != fieldsTS)
        wise_load_fields();

    int hashPos = 0;
    if (ver == 2) {
        unsigned char *hash;
        BSB_IMPORT_ptr(bsb, hash, 32);

        int cnt = 0;
        BSB_IMPORT_u16(bsb, cnt);

        MOLOCH_LOCK(item);
        for (hashPos = 0; hashPos < fieldsMapCnt; hashPos++) {
            if (memcmp(hash, fieldsMapHash[hashPos], 32) == 0)
                break;
        }

        if (config.debug)
            LOG("WISE Response %32.32s cnt %d pos %d", hash, cnt, hashPos);

        if (hashPos == FIELDS_MAP_MAX)
            LOGEXIT("Too many unique wise hashs");

        if (hashPos == fieldsMapCnt) {
            fieldsMapHash[hashPos] = g_strndup((gchar*)hash, 32);
            fieldsMapCnt++;
            sprintf(wiseGetURI, "/get?ver=2");
            if (fieldsMapCnt > 0) {
                strcat(wiseGetURI, "&hashes=");
                strcat(wiseGetURI, fieldsMapHash[0]);
                for (i = 1; i < fieldsMapCnt; i++) {
                    strcat(wiseGetURI, ",");
                    strcat(wiseGetURI, fieldsMapHash[i]);
                }
            }
        }

        if (cnt)
            memset(fieldsMap[hashPos], -1, sizeof(fieldsMap[hashPos]));

        for (i = 0; i < cnt; i++) {
            int len = 0;
            BSB_IMPORT_u16(bsb, len); // len includes NULL terminated
            fieldsMap[0][i] = moloch_field_define_text((char*)BSB_WORK_PTR(bsb), NULL);
            if (fieldsMap[0][i] == -1)
                fieldsTS = 0;
            if (config.debug)
                LOG("Couldn't define field - %d %d %s", i, fieldsMap[0][i], BSB_WORK_PTR(bsb));
            BSB_IMPORT_skip(bsb, len);
        }
        MOLOCH_UNLOCK(item);
    }

    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    for (i = 0; i < request->numItems; i++) {
        MOLOCH_LOCK(item);
        WiseItem_t    *wi = request->items[i];
        int numOps = 0;
        BSB_IMPORT_u08(bsb, numOps);

        moloch_field_ops_init(&wi->ops, numOps, MOLOCH_FIELD_OPS_FLAGS_COPY);
        for (int o = 0; o < numOps; o++) {

            int rfield = 0;
            BSB_IMPORT_u08(bsb, rfield);
            int fieldPos = fieldsMap[hashPos][rfield];

            int len = 0;
            BSB_IMPORT_u08(bsb, len);
            char *str = (char*)BSB_WORK_PTR(bsb);
            BSB_IMPORT_skip(bsb, len);

            if (fieldPos == -1) {
                LOG("Couldn't find pos %d", rfield);
                continue;
            }

            moloch_field_ops_add(&wi->ops, fieldPos, str, len - 1);
        }

        wi->loadTime = currentTime.tv_sec;

        // Schedule updates on waiting sessions
        int s;
        for (s = 0; s < wi->numSessions; s++) {
            moloch_session_add_cmd(wi->sessions[s], MOLOCH_SES_CMD_FUNC, wi, NULL, wise_session_cmd_cb);
        }
        g_free(wi->sessions);
        wi->sessions = 0;
        wi->numSessions = 0;

        DLL_PUSH_HEAD(wil_, &types[(int)wi->type].itemList, wi);
        // Cache needs to be reduced
        if (types[(int)wi->type].itemList.wil_count > maxCache) {
            DLL_POP_TAIL(wil_, &types[(int)wi->type].itemList, wi);
            wise_remove_item_locked(wi);
        }
        MOLOCH_UNLOCK(item);
    }
    MOLOCH_TYPE_FREE(WiseRequest_t, request);
}
/******************************************************************************/
LOCAL void wise_lookup(MolochSession_t *session, WiseRequest_t *request, char *value, int type)
{

    if (*value == 0)
        return;

    if (request->numItems >= WISE_MAX_REQUEST_ITEMS)
        return;

    static int lookups = 0;

    lookups++;
    if (logEvery != 0 && (lookups % logEvery) == 0)
        wise_print_stats();

    stats[type][INTEL_STAT_LOOKUP]++;

    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    MOLOCH_LOCK(item);
    WiseItem_t *wi;
    HASH_FIND(wih_, types[type].itemHash, value, wi);

    if (wi) {
        // Already being looked up
        if (wi->sessions) {
            if (wi->numSessions >= 4096) {
                stats[type][INTEL_STAT_FAIL]++;
                goto cleanup;
            }

            if (wi->numSessions >= wi->sessionsSize) {
                wi->sessionsSize = MIN(wi->sessionsSize*2, 4096);
                wi->sessions = realloc(wi->sessions, sizeof(MolochSession_t *) * wi->sessionsSize);
            }
            wi->sessions[wi->numSessions++] = session;
            moloch_session_incr_outstanding(session);
            stats[type][INTEL_STAT_INPROGRESS]++;
            goto cleanup;
        }

        if (wi->loadTime + cacheSecs > currentTime.tv_sec) {
            moloch_field_ops_run(session, &wi->ops);
            stats[type][INTEL_STAT_CACHE]++;
            goto cleanup;
        }

        /* Had it in cache, but it is too old */
        DLL_REMOVE(wil_, &types[type].itemList, wi);
        moloch_field_ops_free(&wi->ops);
    } else {
        // Know nothing about it
        wi = MOLOCH_TYPE_ALLOC0(WiseItem_t);
        wi->key          = g_strdup(value);
        wi->type         = type;
        wi->sessionsSize = 4;
        HASH_ADD(wih_, types[type].itemHash, wi->key, wi);
    }

    wi->sessions = malloc(sizeof(MolochSession_t *) * wi->sessionsSize);
    wi->sessions[wi->numSessions++] = session;
    moloch_session_incr_outstanding(session);

    stats[type][INTEL_STAT_REQUEST]++;

    if (type < INTEL_TYPE_NUM_PRE) {
        BSB_EXPORT_u08(request->bsb, type);
    } else {
        BSB_EXPORT_u08(request->bsb, types[type].nameLen | 0x80);
        BSB_EXPORT_ptr(request->bsb, types[type].name, types[type].nameLen);
    }
    int len = strlen(value);
    BSB_EXPORT_u16(request->bsb, len);
    BSB_EXPORT_ptr(request->bsb, value, len);

    request->items[request->numItems++] = wi;

cleanup:
    MOLOCH_UNLOCK(item);
}
/******************************************************************************/
LOCAL void wise_lookup_domain(MolochSession_t *session, WiseRequest_t *request, char *domain)
{
    // Skip leading http
    if (*domain == 'h') {
        if (strncmp(domain, "http://", 7) == 0)
            domain += 7;
        else if (strncmp(domain, "https://", 8) == 0)
            domain += 8;
    }

    unsigned char *end = (unsigned char*)domain;
    unsigned char *colon = 0;
    int            period = 0;

    while (*end) {
        if (!validDNS[*end]) {
            if (*end == '.') {
                period++;
                end++;
                continue;
            }
            if (*end == ':') {
                colon = end;
                *colon = 0;
                break;
            }
            if (config.debug) {
                LOG("Invalid DNS: %s", domain);
            }
            return;
        }
        end++;
    }

    if (period == 0) {
        if (config.debug) {
            LOG("Invalid DNS: %s", domain);
        }
        return;
    }

    // Last character is digit, can't be a domain, so either ip or bogus
    if (isdigit(*(end-1))) {
        struct in_addr addr;
        if (inet_pton(AF_INET, domain, &addr) == 1) {
            wise_lookup(session, request, domain, INTEL_TYPE_IP);
        }
        return;
    }

    int l = strlen(domain);
    for (int i = 0; i < wiseExcludeDomainsNum; i++) {
        if (l > wiseExcludeDomainsLen[i] && memcmp(domain + l - wiseExcludeDomainsLen[i], wiseExcludeDomains[i], wiseExcludeDomainsLen[i]) == 0) {
            goto cleanup;
        }
    }

    wise_lookup(session, request, domain, INTEL_TYPE_DOMAIN);

cleanup:
    if (colon)
        *colon = ':';
}
/******************************************************************************/
void wise_lookup_ip(MolochSession_t *session, WiseRequest_t *request, struct in6_addr *ip6)
{
    char ipstr[INET6_ADDRSTRLEN];

    if (IN6_IS_ADDR_V4MAPPED(ip6)) {
        uint32_t ip = MOLOCH_V6_TO_V4(*ip6);
        snprintf(ipstr, sizeof(ipstr), "%u.%u.%u.%u", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff);
    } else {
        inet_ntop(AF_INET6, ip6, ipstr, sizeof(ipstr));
    }

    wise_lookup(session, request, ipstr, INTEL_TYPE_IP);
}
/******************************************************************************/
void wise_lookup_tuple(MolochSession_t *session, WiseRequest_t *request)
{
    char    str[1000];
    BSB     bsb;

    BSB_INIT(bsb, str, sizeof(str));
    BSB_EXPORT_sprintf(bsb, "%ld;", session->firstPacket.tv_sec);

    int first = 1;
    MolochString_t *hstring;
    MolochStringHashStd_t *shash = session->fields[protocolField]->shash;
    HASH_FORALL(s_, *shash, hstring,
        if (first) {
            first = 0;
        } else {
            BSB_EXPORT_u08(bsb, ',');
        }
        BSB_EXPORT_ptr(bsb, hstring->str, hstring->len);
    );
    
    if (IN6_IS_ADDR_V4MAPPED(&session->addr1)) {
      
      uint32_t ip1 = MOLOCH_V6_TO_V4(session->addr1);
      uint32_t ip2 = MOLOCH_V6_TO_V4(session->addr2);

      BSB_EXPORT_sprintf(bsb, ";%u.%u.%u.%u;%u;%u.%u.%u.%u;%u",
                         ip1 & 0xff, (ip1 >> 8) & 0xff, (ip1 >> 16) & 0xff, (ip1 >> 24) & 0xff,
                         session->port1,
                         ip2 & 0xff, (ip2 >> 8) & 0xff, (ip2 >> 16) & 0xff, (ip2 >> 24) & 0xff,
                         session->port2
                        );
    } else {
      // inet_ntop(AF_INET6, ip6, ipstr, sizeof(ipstr));
      char ipstr1[INET6_ADDRSTRLEN];
      char ipstr2[INET6_ADDRSTRLEN];
      
      inet_ntop(AF_INET6, &session->addr1, ipstr1, sizeof(ipstr1));
      inet_ntop(AF_INET6, &session->addr2, ipstr2, sizeof(ipstr2));
      
      BSB_EXPORT_sprintf(bsb, ";%s;%u;%s;%u",
                         ipstr1,
                         session->port1,
                         ipstr2,
                         session->port2
                        );
      
      
    }
    wise_lookup(session, request, str, INTEL_TYPE_TUPLE);
}
/******************************************************************************/
void wise_lookup_url(MolochSession_t *session, WiseRequest_t *request, char *url)
{
    // Skip leading http
    if (*url == 'h') {
        if (memcmp(url, "http://", 7) == 0)
            url += 7;
        else if (memcmp(url, "https://", 8) == 0)
            url += 8;
    }

    char *question = strchr(url, '?');
    if (question) {
        *question = 0;
        wise_lookup(session, request, url, INTEL_TYPE_URL);
        *question = '?';
    } else {
        wise_lookup(session, request, url, INTEL_TYPE_URL);
    }
}
/******************************************************************************/
LOCAL void wise_flush_locked()
{
    if (!iRequest || iRequest->numItems == 0)
        return;

    inflight += iRequest->numItems;
    if (moloch_http_send(wiseService, "POST", wiseGetURI, -1, iBuf, BSB_LENGTH(iRequest->bsb), NULL, TRUE, wise_cb, iRequest) != 0) {
        LOG("Wise - request failed %p for %d items", iRequest, iRequest->numItems);
        wise_cb(500, NULL, 0, iRequest);
    }

    iRequest = 0;
    iBuf     = 0;
}
/******************************************************************************/
LOCAL gboolean wise_flush(gpointer UNUSED(user_data))
{
    MOLOCH_LOCK(iRequest);
    wise_flush_locked();
    MOLOCH_UNLOCK(iRequest);
    return TRUE;
}
/******************************************************************************/

void wise_plugin_pre_save(MolochSession_t *session, int UNUSED(final))
{
    MolochString_t *hstring = NULL;

    MOLOCH_LOCK(iRequest);
    if (!iRequest) {
        iRequest = MOLOCH_TYPE_ALLOC(WiseRequest_t);
        iBuf = moloch_http_get_buffer(0xffff);
        BSB_INIT(iRequest->bsb, iBuf, 0xffff);
        iRequest->numItems = 0;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
    //IPs
    wise_lookup_ip(session, iRequest, &session->addr1);
    wise_lookup_ip(session, iRequest, &session->addr2);

#pragma GCC diagnostic pop

    int type, i;
    for (type = 0; type < numTypes; type++) {
        for (i = 0; i < types[type].fieldsLen; i++) {
            int pos = types[type].fields[i];
            if (!session->fields[pos])
                continue;

            MolochStringHashStd_t *shash;
            gpointer               ikey;
            GHashTable            *ghash;
            GHashTableIter         iter;
            char                   buf[20];

            switch(config.fields[pos]->type) {
            case MOLOCH_FIELD_TYPE_INT:
                snprintf(buf, sizeof(buf), "%d", session->fields[pos]->i);
                wise_lookup(session, iRequest, buf, type);
                break;
            case MOLOCH_FIELD_TYPE_INT_GHASH:
                ghash = session->fields[pos]->ghash;
                g_hash_table_iter_init (&iter, ghash);
                while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
                    snprintf(buf, sizeof(buf), "%d", (int)(long)ikey);
                    wise_lookup(session, iRequest, buf, type);
                }
                break;
            case MOLOCH_FIELD_TYPE_IP:
                wise_lookup_ip(session, iRequest, (struct in6_addr *)session->fields[pos]->ip);
                break;
            case MOLOCH_FIELD_TYPE_IP_GHASH:
                ghash = session->fields[pos]->ghash;
                g_hash_table_iter_init (&iter, ghash);
                while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
                    wise_lookup_ip(session, iRequest, (struct in6_addr *)ikey);
                }
                break;
            case MOLOCH_FIELD_TYPE_STR:
                if (type == INTEL_TYPE_DOMAIN)
                    wise_lookup_domain(session, iRequest, session->fields[pos]->str);
                else
                    wise_lookup(session, iRequest, session->fields[pos]->str, type);
                break;
            case MOLOCH_FIELD_TYPE_STR_HASH:
                shash = session->fields[pos]->shash;
                HASH_FORALL(s_, *shash, hstring,
                    if (type == INTEL_TYPE_DOMAIN)
                        wise_lookup_domain(session, iRequest, hstring->str);
                    else if (type == INTEL_TYPE_URL)
                        wise_lookup_url(session, iRequest, hstring->str);
                    else if (hstring->uw) {
                        char str[1000];
                        snprintf(str, sizeof(str), "%s;%s", hstring->str, (char*)hstring->uw);
                        wise_lookup(session, iRequest, str, type);
                    } else {
                        wise_lookup(session, iRequest, hstring->str, type);
                    }
                );
                break;
            case MOLOCH_FIELD_TYPE_STR_GHASH:
                ghash = session->fields[pos]->ghash;
                g_hash_table_iter_init (&iter, ghash);
                while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
                    if (type == INTEL_TYPE_DOMAIN)
                        wise_lookup_domain(session, iRequest, hstring->str);
                    else if (type == INTEL_TYPE_URL)
                        wise_lookup_url(session, iRequest, hstring->str);
                    else
                        wise_lookup(session, iRequest, ikey, type);
                }
            } /* switch */
        }
    }

    // Tuples
    if ((tcpTuple && session->ses == SESSION_TCP) ||
        (udpTuple && session->ses == SESSION_UDP)) {
        wise_lookup_tuple(session, iRequest);
    }

    if (iRequest->numItems > WISE_MAX_REQUEST_ITEMS/2) {
        wise_flush_locked();
    }
    MOLOCH_UNLOCK(iRequest);
}
/******************************************************************************/
LOCAL void wise_plugin_exit()
{
    MOLOCH_LOCK(item);
    for (int type = 0; type < INTEL_TYPE_SIZE; type++) {
        WiseItem_t *wi;
        while (DLL_POP_TAIL(wil_, &types[type].itemList, wi)) {
            wise_remove_item_locked(wi);
        }
    }

    if (wiseHost)
        g_free(wiseHost);

    if (wiseURL)
        g_free(wiseURL);

    moloch_http_free_server(wiseService);
    MOLOCH_UNLOCK(item);
}
/******************************************************************************/
LOCAL uint32_t wise_plugin_outstanding()
{
    int count;
    MOLOCH_LOCK(iRequest);
    count = inflight + (iRequest?iRequest->numItems:0) + moloch_http_queue_length(wiseService);
    MOLOCH_UNLOCK(iRequest);
    LOG("wise: %d", count);
    return count;
}
/******************************************************************************/
LOCAL void wise_load_config()
{
    gsize keys_len;
    int   i, type;
    char *wiseStrings[INTEL_TYPE_NUM_PRE] = {"ip", "domain", "md5", "email", "url", "tuple", "ja3", "sha256"};

    for (type = 0; type < INTEL_TYPE_NUM_PRE; type++) {
        types[type].name = wiseStrings[type];
        types[type].nameLen = strlen(wiseStrings[type]);
    }

    // Defaults unless replaced below
    types[INTEL_TYPE_IP].fields[0] = moloch_field_by_db("http.xffIp");
    types[INTEL_TYPE_IP].fieldsLen = 1;

    types[INTEL_TYPE_URL].fields[0] = moloch_field_by_db("http.uri");
    types[INTEL_TYPE_URL].fieldsLen = 1;

    types[INTEL_TYPE_DOMAIN].fields[0] = moloch_field_by_db("http.host");
    types[INTEL_TYPE_DOMAIN].fields[1] = moloch_field_by_db("dns.host");

    if (config.parseDNSRecordAll) {
        types[INTEL_TYPE_DOMAIN].fields[2] = moloch_field_by_db("dns.mailserverHost");
        // Not sending nameserver for now
        types[INTEL_TYPE_DOMAIN].fieldsLen = 3;
    } else {
        types[INTEL_TYPE_DOMAIN].fieldsLen = 2;
    }

    types[INTEL_TYPE_MD5].fields[0] = moloch_field_by_db("http.md5");
    types[INTEL_TYPE_MD5].fields[1] = moloch_field_by_db("email.md5");
    types[INTEL_TYPE_MD5].fieldsLen = 2;

    if (config.supportSha256) {
        types[INTEL_TYPE_SHA256].fields[0] = moloch_field_by_db("http.sha256");
        types[INTEL_TYPE_SHA256].fields[1] = moloch_field_by_db("email.sha256");
        types[INTEL_TYPE_SHA256].fieldsLen = 2;
    }

    types[INTEL_TYPE_EMAIL].fields[0] = moloch_field_by_db("email.src");
    types[INTEL_TYPE_EMAIL].fields[1] = moloch_field_by_db("email.dst");
    types[INTEL_TYPE_EMAIL].fieldsLen = 2;

    types[INTEL_TYPE_JA3].fields[0] = moloch_field_by_db("tls.ja3");
    types[INTEL_TYPE_JA3].fieldsLen = 1;

    // Load user config
    gchar **keys = moloch_config_section_keys(NULL, "wise-types", &keys_len);

    if (!keys)
        return;

    for (i = 0; i < (int)keys_len; i++) {
        gchar **values = moloch_config_section_str_list(NULL, "wise-types", keys[i], NULL);

        if (strcmp(keys[i], "ip") == 0)
            type = INTEL_TYPE_IP;
        else if (strcmp(keys[i], "url") == 0)
            type = INTEL_TYPE_URL;
        else if (strcmp(keys[i], "domain") == 0)
            type = INTEL_TYPE_DOMAIN;
        else if (strcmp(keys[i], "md5") == 0)
            type = INTEL_TYPE_MD5;
        else if (strcmp(keys[i], "sha256") == 0)
            type = INTEL_TYPE_SHA256;
        else if (strcmp(keys[i], "email") == 0)
            type = INTEL_TYPE_EMAIL;
        else if (strcmp(keys[i], "ja3") == 0)
            type = INTEL_TYPE_JA3;
        else {
            if (numTypes == INTEL_TYPE_SIZE) {
                LOGEXIT("Too many wise-types, can only have %d custom types", INTEL_TYPE_SIZE - INTEL_TYPE_NUM_PRE);
            }
            type = numTypes++;
            types[type].nameLen = strlen(keys[i]);
            types[type].name = g_ascii_strdown(keys[i], types[type].nameLen);

            if (types[type].nameLen > 12)
                LOGEXIT("wise-types '%s' too long, max 12 chars", keys[i]);
        }

        types[type].fieldsLen = 0;
        int v;
        for (v = 0; values[v]; v++) {
            if (types[type].fieldsLen == INTEL_TYPE_MAX_FIELDS)
                LOGEXIT("wise-types '%s' has too man fields, max %d", keys[i], INTEL_TYPE_MAX_FIELDS);

            int pos;
            if (strncmp("db:", values[v], 3) == 0)
                pos = moloch_field_by_db(values[v] + 3);
            else
                pos = moloch_field_by_exp(values[v]);
            types[type].fields[(int)types[type].fieldsLen] = pos;
            types[type].fieldsLen++;
        }
        g_strfreev(values);
    }

    g_strfreev(keys);
}


/******************************************************************************/
void moloch_plugin_init()
{
    int i;

    if (config.dryRun) {
        LOG("Not enabling in dryRun mode");
        return;
    }

    maxConns = moloch_config_int(NULL, "wiseMaxConns", 10, 1, 60);
    maxRequests = moloch_config_int(NULL, "wiseMaxRequests", 100, 1, 50000);
    maxCache = moloch_config_int(NULL, "wiseMaxCache", 100000, 1, 500000);
    cacheSecs = moloch_config_int(NULL, "wiseCacheSecs", 600, 1, 5000);
    tcpTuple = moloch_config_boolean(NULL, "wiseTcpTupleLookups", FALSE);
    udpTuple = moloch_config_boolean(NULL, "wiseUdpTupleLookups", FALSE);
    logEvery = moloch_config_int(NULL, "wiseLogEvery", 10000, 0, 10000000);

    wiseURL  = moloch_config_str(NULL, "wiseURL", NULL);
    wisePort = moloch_config_int(NULL, "wisePort", 8081, 1, 0xffff);
    wiseHost = moloch_config_str(NULL, "wiseHost", "127.0.0.1");

    protocolField    = moloch_field_by_db("protocol");

    wise_load_config();

    wiseExcludeDomains = moloch_config_str_list(NULL, "wiseExcludeDomains", ".in-addr.arpa;.ip6.arpa");
    for (i = 0; wiseExcludeDomains[i]; i++);
    wiseExcludeDomainsNum = i;
    wiseExcludeDomainsLen = malloc(sizeof(int) * wiseExcludeDomainsNum);

    for (i = 0; wiseExcludeDomains[i]; i++) {
        wiseExcludeDomainsLen[i] = strlen(wiseExcludeDomains[i]);
    }

    if (wiseURL) {
        wiseService = moloch_http_create_server(wiseURL, maxConns, maxRequests, 0);
    } else {
        char hoststr[200];
        snprintf(hoststr, sizeof(hoststr), "http://%s:%d", wiseHost, wisePort);
        wiseService = moloch_http_create_server(hoststr, maxConns, maxRequests, 0);
    }
    moloch_http_set_retries(wiseService, 1);

    moloch_plugins_register("wise", FALSE);

    moloch_plugins_set_cb("wise",
      NULL,
      NULL,
      NULL,
      wise_plugin_pre_save,
      NULL,
      NULL,
      wise_plugin_exit,
      NULL
    );

    moloch_plugins_set_outstanding_cb("wise", wise_plugin_outstanding);

    int type;
    for (type = 0; type < INTEL_TYPE_SIZE; type++) {
        HASH_INIT(wih_, types[type].itemHash, moloch_string_hash, wise_item_cmp);
        DLL_INIT(wil_, &types[type].itemList);
    }
    g_timeout_add_seconds(1, wise_flush, 0);
    wise_load_fields();

    sprintf(wiseGetURI, "/get?ver=2");
}
