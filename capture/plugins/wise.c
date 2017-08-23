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

LOCAL int                   httpHostField;
LOCAL int                   httpXffField;
LOCAL int                   httpMd5Field;
LOCAL int                   emailMd5Field;
LOCAL int                   emailSrcField;
LOCAL int                   emailDstField;
LOCAL int                   dnsHostField;
LOCAL int                   tagsField;
LOCAL int                   httpUrlField;
LOCAL int                   protocolField;
LOCAL int                   ja3Field;

LOCAL uint32_t              fieldsTS;
LOCAL int                   fieldsMap[256];

LOCAL uint32_t              inflight;

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
#define INTEL_TYPE_SIZE    7

LOCAL char *wiseStrings[] = {"ip", "domain", "md5", "email", "url", "tuple", "ja3"};

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

typedef struct wiserequest {
    BSB          bsb;
    WiseItem_t  *items[256];
    int          numItems;
} WiseRequest_t;

typedef HASH_VAR(h_, WiseItemHash_t, WiseItemHead_t, 199337);

WiseItemHash_t itemHash[INTEL_TYPE_SIZE];
WiseItemHead_t itemList[INTEL_TYPE_SIZE];

LOCAL MOLOCH_LOCK_DEFINE(item);

/******************************************************************************/
LOCAL WiseRequest_t *iRequest = 0;
LOCAL MOLOCH_LOCK_DEFINE(iRequest);
LOCAL char          *iBuf = 0;

/******************************************************************************/
int wise_item_cmp(const void *keyv, const void *elementv)
{
    char *key = (char*)keyv;
    WiseItem_t *element = (WiseItem_t *)elementv;

    return strcmp(key, element->key) == 0;
}
/******************************************************************************/
void wise_print_stats()
{
    int i;
    for (i = 0; i < INTEL_TYPE_SIZE; i++) {
        LOG("%8s lookups:%7d cache:%7d requests:%7d inprogress:%7d fail:%7d hash:%7d list:%7d",
            wiseStrings[i],
            stats[i][0],
            stats[i][1],
            stats[i][2],
            stats[i][3],
            stats[i][4],
            HASH_COUNT(wih_, itemHash[i]),
            DLL_COUNT(wil_, &itemList[i]));
    }
}
/******************************************************************************/
void wise_load_fields()
{
    char                key[500];
    int                 key_len;

    memset(fieldsMap, -1, sizeof(fieldsMap));

    key_len = snprintf(key, sizeof(key), "/fields");
    size_t         data_len;
    unsigned char *data = moloch_http_send_sync(wiseService, "GET", key, key_len, NULL, 0, NULL, &data_len);;

    BSB bsb;
    BSB_INIT(bsb, data, data_len);

    int ver, cnt = 0;
    BSB_IMPORT_u32(bsb, fieldsTS);
    BSB_IMPORT_u32(bsb, ver);
    BSB_IMPORT_u08(bsb, cnt);

    int i;
    for (i = 0; i < cnt; i++) {
        int len = 0;
        BSB_IMPORT_u16(bsb, len); // len includes NULL terminated
        fieldsMap[i] = moloch_field_define_text((char*)BSB_WORK_PTR(bsb), NULL);
        if (fieldsMap[i] == -1)
            fieldsTS = 0;
        if (config.debug)
            LOG("%d %d %s", i, fieldsMap[i], BSB_WORK_PTR(bsb));
        BSB_IMPORT_skip(bsb, len);
    }
    free(data);
}
/******************************************************************************/
void wise_session_cmd_cb(MolochSession_t *session, gpointer uw1, gpointer UNUSED(uw2))
{
    WiseItem_t    *wi = uw1;

    if (wi) {
        moloch_field_ops_run(session, &wi->ops);
    }
    moloch_session_decr_outstanding(session);
}
/******************************************************************************/
void wise_free_item_unlocked(WiseItem_t *wi)
{
    int i;
    HASH_REMOVE(wih_, itemHash[(int)wi->type], wi);
    if (wi->sessions) {
        for (i = 0; i < wi->numSessions; i++) {
            moloch_session_add_cmd(wi->sessions[i], MOLOCH_SES_CMD_FUNC, NULL, NULL, wise_session_cmd_cb);
        }
        g_free(wi->sessions);
        wi->sessions = 0;
    }
    g_free(wi->key);
    moloch_field_ops_free(&wi->ops);
    MOLOCH_TYPE_FREE(WiseItem_t, wi);
}
/******************************************************************************/
void wise_cb(int UNUSED(code), unsigned char *data, int data_len, gpointer uw)
{

    BSB             bsb;
    WiseRequest_t *request = uw;
    int             i;

    inflight -= request->numItems;

    BSB_INIT(bsb, data, data_len);

    uint32_t fts = 0, ver = 0;
    BSB_IMPORT_u32(bsb, fts);
    BSB_IMPORT_u32(bsb, ver);

    if (BSB_IS_ERROR(bsb) || ver != 0) {
        MOLOCH_LOCK(item);
        for (i = 0; i < request->numItems; i++) {
            wise_free_item_unlocked(request->items[i]);
        }
        MOLOCH_UNLOCK(item);
        MOLOCH_TYPE_FREE(WiseRequest_t, request);
        return;
    }

    if (fts != fieldsTS)
        wise_load_fields();

    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    for (i = 0; i < request->numItems; i++) {
        WiseItem_t    *wi = request->items[i];
        int numOps = 0;
        BSB_IMPORT_u08(bsb, numOps);

        moloch_field_ops_init(&wi->ops, numOps, MOLOCH_FIELD_OPS_FLAGS_COPY);
        if (numOps > 0) {
            int i;
            for (i = 0; i < numOps; i++) {

                int rfield = 0;
                BSB_IMPORT_u08(bsb, rfield);
                int fieldPos = fieldsMap[rfield];

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
        }

        wi->loadTime = currentTime.tv_sec;

        MOLOCH_LOCK(item);
        int s;
        for (s = 0; s < wi->numSessions; s++) {
            moloch_session_add_cmd(wi->sessions[s], MOLOCH_SES_CMD_FUNC, wi, NULL, wise_session_cmd_cb);
        }
        g_free(wi->sessions);
        wi->sessions = 0;
        wi->numSessions = 0;

        DLL_PUSH_HEAD(wil_, &itemList[(int)wi->type], wi);
        // Cache needs to be reduced
        if (itemList[(int)wi->type].wil_count > maxCache) {
            DLL_POP_TAIL(wil_, &itemList[(int)wi->type], wi);
            wise_free_item_unlocked(wi);
        }
        MOLOCH_UNLOCK(item);
    }
    MOLOCH_TYPE_FREE(WiseRequest_t, request);
}
/******************************************************************************/
void wise_lookup(MolochSession_t *session, WiseRequest_t *request, char *value, int type)
{

    if (*value == 0)
        return;

    if (request->numItems >= 256)
        return;

    MOLOCH_LOCK(item);

    static int lookups = 0;
    WiseItem_t *wi;

    lookups++;
    if ((lookups % 10000) == 0)
        wise_print_stats();

    stats[type][INTEL_STAT_LOOKUP]++;

    HASH_FIND(wih_, itemHash[type], value, wi);

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

        struct timeval currentTime;
        gettimeofday(&currentTime, NULL);

        if (wi->loadTime + cacheSecs > currentTime.tv_sec) {
            moloch_field_ops_run(session, &wi->ops);
            stats[type][INTEL_STAT_CACHE]++;
            goto cleanup;
        }

        /* Had it in cache, but it is too old */
        DLL_REMOVE(wil_, &itemList[type], wi);
        moloch_field_ops_free(&wi->ops);
    } else {
        // Know nothing about it
        wi = MOLOCH_TYPE_ALLOC0(WiseItem_t);
        wi->key          = g_strdup(value);
        wi->type         = type;
        wi->sessionsSize = 4;
        HASH_ADD(wih_, itemHash[type], wi->key, wi);
    }

    wi->sessions = malloc(sizeof(MolochSession_t *) * wi->sessionsSize);
    wi->sessions[wi->numSessions++] = session;
    moloch_session_incr_outstanding(session);

    stats[type][INTEL_STAT_REQUEST]++;

    BSB_EXPORT_u08(request->bsb, type);
    int len = strlen(value);
    BSB_EXPORT_u16(request->bsb, len);
    BSB_EXPORT_ptr(request->bsb, value, len);

    request->items[request->numItems++] = wi;

cleanup:
    MOLOCH_UNLOCK(item);
}
/******************************************************************************/
void wise_lookup_domain(MolochSession_t *session, WiseRequest_t *request, char *domain)
{
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

    wise_lookup(session, request, domain, INTEL_TYPE_DOMAIN);

    if (colon)
        *colon = ':';
}
/******************************************************************************/
void wise_lookup_ip(MolochSession_t *session, WiseRequest_t *request, uint32_t ip)
{
    char ipstr[18];

    snprintf(ipstr, sizeof(ipstr), "%d.%d.%d.%d", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff);

    wise_lookup(session, request, ipstr, INTEL_TYPE_IP);
}
/******************************************************************************/
void wise_lookup_tuple(MolochSession_t *session, WiseRequest_t *request)
{
    char    str[1000];
    BSB     bsb;

    BSB_INIT(bsb, str, sizeof(str));

    uint32_t ip1 = MOLOCH_V6_TO_V4(session->addr1);
    uint32_t ip2 = MOLOCH_V6_TO_V4(session->addr2);

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

    BSB_EXPORT_sprintf(bsb, ";%d.%d.%d.%d;%d;%d.%d.%d.%d;%d",
                       ip1 & 0xff, (ip1 >> 8) & 0xff, (ip1 >> 16) & 0xff, (ip1 >> 24) & 0xff,
                       session->port1,
                       ip2 & 0xff, (ip2 >> 8) & 0xff, (ip2 >> 16) & 0xff, (ip2 >> 24) & 0xff,
                       session->port2
                      );

    wise_lookup(session, request, str, INTEL_TYPE_TUPLE);
}
/******************************************************************************/
void wise_lookup_url(MolochSession_t *session, WiseRequest_t *request, char *url)
{
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
    if (moloch_http_send(wiseService, "POST", "/get", 4, iBuf, BSB_LENGTH(iRequest->bsb), NULL, TRUE, wise_cb, iRequest) != 0) {
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
    MolochString_t *hstring;
    uint32_t        i;

    MOLOCH_LOCK(iRequest);
    if (!iRequest) {
        iRequest = MOLOCH_TYPE_ALLOC(WiseRequest_t);
        iBuf = moloch_http_get_buffer(0xffff);
        BSB_INIT(iRequest->bsb, iBuf, 0xffff);
        iRequest->numItems = 0;
    }

    //IPs
    //ALW Fix - when wise supports v6
    if (IN6_IS_ADDR_V4MAPPED(&session->addr1)) {
        wise_lookup_ip(session, iRequest, MOLOCH_V6_TO_V4(session->addr1));
    }

    if (IN6_IS_ADDR_V4MAPPED(&session->addr2)) {
        wise_lookup_ip(session, iRequest, MOLOCH_V6_TO_V4(session->addr2));
    }


    //Domains
    if (session->fields[httpHostField]) {
        MolochStringHashStd_t *shash = session->fields[httpHostField]->shash;
        HASH_FORALL(s_, *shash, hstring,
            if (hstring->str[0] == 'h') {
                if (memcmp(hstring->str, "http://", 7) == 0)
                    wise_lookup_domain(session, iRequest, hstring->str+7);
                else if (memcmp(hstring->str, "https://", 8) == 0)
                    wise_lookup_domain(session, iRequest, hstring->str+8);
                else
                    wise_lookup_domain(session, iRequest, hstring->str);
            } else
                wise_lookup_domain(session, iRequest, hstring->str);
        );
    }
    if (session->fields[dnsHostField]) {
        MolochStringHashStd_t *shash = session->fields[dnsHostField]->shash;
        HASH_FORALL(s_, *shash, hstring,
            if (hstring->str[0] == '<')
                continue;
            wise_lookup_domain(session, iRequest, hstring->str);
        );
    }

    //MD5s
    if (session->fields[httpMd5Field]) {
        MolochStringHashStd_t *shash = session->fields[httpMd5Field]->shash;
        HASH_FORALL(s_, *shash, hstring,
            if (hstring->uw) {
                char str[1000];
                snprintf(str, sizeof(str), "%s;%s", hstring->str, hstring->uw);
                wise_lookup(session, iRequest, str, INTEL_TYPE_MD5);
            } else {
                wise_lookup(session, iRequest, hstring->str, INTEL_TYPE_MD5);
            }
        );
    }

    if (session->fields[emailMd5Field]) {
        MolochStringHashStd_t *shash = session->fields[emailMd5Field]->shash;
        HASH_FORALL(s_, *shash, hstring,
            wise_lookup(session, iRequest, hstring->str, INTEL_TYPE_MD5);
        );
    }

    //Email
    if (session->fields[emailSrcField]) {
        MolochStringHashStd_t *shash = session->fields[emailSrcField]->shash;
        HASH_FORALL(s_, *shash, hstring,
            wise_lookup(session, iRequest, hstring->str, INTEL_TYPE_EMAIL);
        );
    }

    if (session->fields[emailDstField]) {
        MolochStringHashStd_t *shash = session->fields[emailDstField]->shash;
        HASH_FORALL(s_, *shash, hstring,
            wise_lookup(session, iRequest, hstring->str, INTEL_TYPE_EMAIL);
        );
    }

    //URLs
    if (session->fields[httpUrlField]) {
        GPtrArray *sarray =  session->fields[httpUrlField]->sarray;

        for(i = 0; i < sarray->len; i++) {
            char *str = g_ptr_array_index(sarray, i);

            if (str[0] == '/') {
                wise_lookup_url(session, iRequest, str+2);
            } else if (str[0] == 'h' && memcmp("http://", str, 7) == 0) {
                wise_lookup_url(session, iRequest, str+7);
            } else
                wise_lookup_url(session, iRequest, str);
        }
    }

    // Tuples
    if ((tcpTuple && session->ses == SESSION_TCP) ||
        (udpTuple && session->ses == SESSION_UDP)) {
        wise_lookup_tuple(session, iRequest);
    }

    // JA3
    if (session->fields[ja3Field]) {
        MolochStringHashStd_t *shash = session->fields[ja3Field]->shash;
        HASH_FORALL(s_, *shash, hstring,
            wise_lookup(session, iRequest, hstring->str, INTEL_TYPE_JA3);
        );
    }

    if (iRequest->numItems > 128) {
        wise_flush_locked();
    }
    MOLOCH_UNLOCK(iRequest);
}
/******************************************************************************/
void wise_plugin_exit()
{
    MOLOCH_LOCK(item);
    int h;
    WiseItem_t *wi;
    for (h = 0; h < INTEL_TYPE_SIZE; h++) {
        while (DLL_POP_TAIL(wil_, &itemList[h], wi)) {
            wise_free_item_unlocked(wi);
        }
    }

    moloch_http_free_server(wiseService);
    MOLOCH_UNLOCK(item);
}
/******************************************************************************/
uint32_t wise_plugin_outstanding()
{
    int count;
    MOLOCH_LOCK(iRequest);
    count = inflight + (iRequest?iRequest->numItems:0) + moloch_http_queue_length(wiseService);
    MOLOCH_UNLOCK(iRequest);
    LOG("wise: %d", count);
    return count;
}
/******************************************************************************/
void moloch_plugin_init()
{

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

    int   port = moloch_config_int(NULL, "wisePort", 8081, 1, 0xffff);
    char *host = moloch_config_str(NULL, "wiseHost", "127.0.0.1");

    httpHostField  = moloch_field_by_db("ho");
    httpXffField   = moloch_field_by_db("xff");
    httpMd5Field   = moloch_field_by_db("hmd5");
    emailMd5Field  = moloch_field_by_db("emd5");
    emailSrcField  = moloch_field_by_db("esrc");
    emailDstField  = moloch_field_by_db("edst");
    dnsHostField   = moloch_field_by_db("dnsho");
    tagsField      = moloch_field_by_db("ta");
    httpUrlField   = moloch_field_by_db("us");
    protocolField  = moloch_field_by_db("prot-term");
    ja3Field       = moloch_field_by_db("tlsja3-term");

    char hoststr[200];
    snprintf(hoststr, sizeof(hoststr), "http://%s:%d", host, port);
    wiseService = moloch_http_create_server(hoststr, maxConns, maxRequests, 0);
    g_free(host);

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

    int h;
    for (h = 0; h < INTEL_TYPE_SIZE; h++) {
        HASH_INIT(wih_, itemHash[h], moloch_string_hash, wise_item_cmp);
        DLL_INIT(wil_, &itemList[h]);
    }
    g_timeout_add_seconds(1, wise_flush, 0);
    wise_load_fields();
}
