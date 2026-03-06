/******************************************************************************/
/* db.es.c  -- Elasticsearch database implementation
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include "arkimeconfig.h"
#include <inttypes.h>
#include <errno.h>

extern ArkimeConfig_t        config;

void                         *esServer = 0;
LOCAL char                   *esBulkQuery;
LOCAL int                     esBulkQueryLen;

LOCAL struct timespec          startHealthCheck;
LOCAL uint64_t                 esHealthMS;

LOCAL uint64_t zero_atoll(const char *v);
/******************************************************************************/
LOCAL void arkime_db_es_send_bulk_cb(int code, uint8_t *data, int data_len, gpointer UNUSED(uw))
{
    uint8_t *forbidden;

    if (code != 200)
        LOG("Bulk issue.  Code: %d\n%.*s", code, data_len, data);
    else if (config.debug > 4)
        LOG("Bulk Reply code:%d :>%.*s<", code, data_len, data);
    else if ((forbidden = (uint8_t *)strstr((char *)data, "FORBIDDEN")) != 0) {
        const uint8_t *end = forbidden + 10;
        while (forbidden > data && *forbidden != '{') {
            forbidden--;
        }
        while (end < data + data_len && *end != '}') {
            end++;
        }
        LOG("ERROR - OpenSearch/Elasticsearch is returning a FORBIDDEN error. This is mostly likely because: the index is closed, the index is read-only from ILM, or you've hit the disk water marks. See FAQ. %.*s", (int)(end - forbidden + 1), forbidden);
    }
}
/******************************************************************************/
LOCAL void arkime_db_es_send_bulk(char *json, int len)
{
    if (config.debug > 4)
        LOG("Sending Bulk:>%.*s<", len, json);
    arkime_http_schedule(esServer, "POST", esBulkQuery, esBulkQueryLen, json, len, NULL, ARKIME_HTTP_PRIORITY_NORMAL, arkime_db_es_send_bulk_cb, NULL);
}
/******************************************************************************/
LOCAL void arkime_db_es_load_stats(uint64_t *totalPackets, uint64_t *totalK, uint64_t *totalSessions, uint64_t *totalDropped, uint64_t *dbVersion)
{
    size_t             data_len;
    uint32_t           len;
    uint32_t           source_len;
    const uint8_t     *source = 0;

    char     stats_key[200];
    int      stats_key_len = 0;
    stats_key_len = arkime_snprintf_len(stats_key, sizeof(stats_key), "/%sstats/_doc/%s", config.prefix, config.nodeName);

    uint8_t           *data = arkime_http_get(esServer, stats_key, stats_key_len, &data_len);
    if (!data) {
        LOGEXIT("ERROR - Couldn't fetch stats: no data returned - %.*s", stats_key_len, stats_key);
    }

    uint32_t           version_len;
    const uint8_t     *version = arkime_js0n_get(data, data_len, "_version", &version_len);

    if (!version_len || !version) {
        *dbVersion = 0;
    } else {
        *dbVersion = atol((char *)version);
    }
    source = arkime_js0n_get(data, data_len, "_source", &source_len);
    if (source) {
        *totalPackets  = zero_atoll((char *)arkime_js0n_get(source, source_len, "totalPackets", &len));
        *totalK        = zero_atoll((char *)arkime_js0n_get(source, source_len, "totalK", &len));
        *totalSessions = zero_atoll((char *)arkime_js0n_get(source, source_len, "totalSessions", &len));
        *totalDropped  = zero_atoll((char *)arkime_js0n_get(source, source_len, "totalDropped", &len));
    }
    free(data);
}
/******************************************************************************/
LOCAL void arkime_db_es_send_stats(char *json, int json_len, int n, uint64_t currentTimeSec, uint64_t dbVersion, gboolean sync)
{
    if (n == 0) {
        char     stats_key[200];
        int      stats_key_len = 0;
        if (config.pcapReadOffline) {
            stats_key_len = arkime_snprintf_len(stats_key, sizeof(stats_key), "/%sstats/_doc/%s", config.prefix, config.nodeName);
        } else {
            stats_key_len = arkime_snprintf_len(stats_key, sizeof(stats_key), "/%sstats/_doc/%s?version_type=external&version=%" PRIu64, config.prefix, config.nodeName, dbVersion);
        }
        if (sync) {
            uint8_t *data = arkime_http_send_sync(esServer, "POST", stats_key, stats_key_len, json, json_len, NULL, NULL, NULL);
            if (data)
                free(data);
            arkime_http_free_buffer(json);
        } else {
            if ((currentTimeSec % 60) >= 2) {
                arkime_http_schedule(esServer, "POST", stats_key, stats_key_len, json, json_len, NULL, ARKIME_HTTP_PRIORITY_DROPABLE, NULL, NULL);
            } else {
                arkime_http_schedule(esServer, "POST", stats_key, stats_key_len, json, json_len, NULL, ARKIME_HTTP_PRIORITY_BEST, NULL, NULL);
            }
        }
    } else {
        static const int intervals[4] = {1, 5, 60, 600};
        char key[200];
        int key_len = arkime_snprintf_len(key, sizeof(key), "/%sdstats/_doc/%s-%d-%d", config.prefix, config.nodeName, (int)(currentTimeSec / intervals[n]) % 1440, intervals[n]);
        arkime_http_schedule(esServer, "POST", key, key_len, json, json_len, NULL, ARKIME_HTTP_PRIORITY_DROPABLE, NULL, NULL);
    }
}
/******************************************************************************/
LOCAL void arkime_db_es_check(ArkimeDbMode_t UNUSED(mode))
{
    size_t             data_len;
    char               key[1000];
    int                key_len;
    char               tname[100];
    uint8_t           *data;

    snprintf(tname, sizeof(tname), "%ssessions3_template", config.prefix);

    key_len = arkime_snprintf_len(key, sizeof(key), "/_template/%s?filter_path=**._meta", tname);
    data = arkime_http_get(esServer, key, key_len, &data_len);

    if (!data || data_len == 0) {
        LOGEXIT("ERROR - Couldn't load version information, database (%s) might be down or not initialized.", config.elasticsearch);
    }

    uint32_t           template_len;
    const uint8_t     *template = 0;

    template = arkime_js0n_get(data, data_len, tname, &template_len);
    if (!template || template_len == 0) {
        LOGEXIT("ERROR - Couldn't load version information, database might be down or out of date.  Run \"db/db.pl host:port upgrade\"");
    }

    uint32_t           mappings_len;
    const uint8_t     *mappings = 0;

    mappings = arkime_js0n_get(template, template_len, "mappings", &mappings_len);
    if (!mappings || mappings_len == 0) {
        LOGEXIT("ERROR - Couldn't load version information, database might be down or out of date.  Run \"db/db.pl host:port upgrade\"");
    }

    uint32_t           meta_len;
    const uint8_t     *meta = 0;

    meta = arkime_js0n_get(mappings, mappings_len, "_meta", &meta_len);
    if (!meta || meta_len == 0) {
        LOGEXIT("ERROR - Couldn't load version information, database might be down or out of date.  Run \"db/db.pl host:port upgrade\"");
    }

    uint32_t           version_len = 0;
    const uint8_t     *version = 0;

    version = arkime_js0n_get(meta, meta_len, "molochDbVersion", &version_len);

    if (!version)
        LOGEXIT("ERROR - Database version couldn't be found, have you run \"db/db.pl host:port init\"");

    extern int arkimeDbVersion;
    arkimeDbVersion = atoi((char *)version);
    if (arkimeDbVersion < 83) {
        LOGEXIT("ERROR - Database version '%.*s' is too old, needs to be at least (%d), run \"db/db.pl host:port upgrade\"", version_len, version, 83);
    }
    free(data);
}
/******************************************************************************/
typedef struct arkime_seq_request {
    char               *name;
    ArkimeSeqNum_cb     func;
    gpointer            uw;
} ArkimeSeqRequest_t;

LOCAL void arkime_db_es_get_sequence_number(const char *name, ArkimeSeqNum_cb func, gpointer uw);
LOCAL void arkime_db_es_get_sequence_number_cb(int UNUSED(code), uint8_t *data, int data_len, gpointer uw)
{
    ArkimeSeqRequest_t *r = uw;
    uint32_t            version_len;

    const uint8_t *version = arkime_js0n_get(data, data_len, "_version", &version_len);

    if (!version_len || !version) {
        LOG("ERROR - Couldn't fetch sequence: %.*s", data_len, data);
        arkime_db_es_get_sequence_number(r->name, r->func, r->uw);
    } else {
        if (r->func)
            r->func(atoi((char *)version), r->uw);
    }

    g_free(r->name);
    ARKIME_TYPE_FREE(ArkimeSeqRequest_t, r);
}
/******************************************************************************/
LOCAL void arkime_db_es_get_sequence_number(const char *name, ArkimeSeqNum_cb func, gpointer uw)
{
    char                key[200];
    int                 key_len;
    ArkimeSeqRequest_t *r = ARKIME_TYPE_ALLOC(ArkimeSeqRequest_t);
    char               *json = arkime_http_get_buffer(ARKIME_HTTP_BUFFER_SIZE);

    r->name = g_strdup(name);
    r->func = func;
    r->uw   = uw;

    key_len = arkime_snprintf_len(key, sizeof(key), "/%ssequence/_doc/%s", config.prefix, name);
    int json_len = arkime_snprintf_len(json, ARKIME_HTTP_BUFFER_SIZE, "{}");
    arkime_http_schedule(esServer, "POST", key, key_len, json, json_len, NULL, ARKIME_HTTP_PRIORITY_BEST, arkime_db_es_get_sequence_number_cb, r);
}
/******************************************************************************/
LOCAL uint32_t arkime_db_es_get_sequence_number_sync(const char *name)
{

    while (1) {
        char key[200];
        int key_len = arkime_snprintf_len(key, sizeof(key), "/%ssequence/_doc/%s", config.prefix, name);

        size_t data_len;
        uint8_t *data = arkime_http_send_sync(esServer, "POST", key, key_len, "{}", 2, NULL, &data_len, NULL);
        if (!data) {
            LOG("ERROR - Couldn't fetch sequence: no data returned - %.*s", key_len, key);
            continue;
        }

        uint32_t version_len;
        const uint8_t *version = arkime_js0n_get(data, data_len, "_version", &version_len);

        if (!version_len || !version) {
            LOG("ERROR - Couldn't fetch sequence: %d %.*s", (int)data_len, (int)data_len, data);

            if (strstr((char *)data, "FORBIDDEN") != 0) {
                LOG("ERROR - You have most likely run out of space on an elasticsearch node, see https://arkime.com/faq#recommended-elasticsearch-settings on setting disk watermarks and how to clear the elasticsearch error");
            }
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
LOCAL void arkime_db_es_create_file(char *json, int json_len, uint32_t num)
{
    char key[200];
    int key_len = arkime_snprintf_len(key, sizeof(key), "/%sfiles/_doc/%s-%u?refresh=true", config.prefix, config.nodeName, num);
    arkime_http_schedule(esServer, "POST", key, key_len, json, json_len, NULL, ARKIME_HTTP_PRIORITY_BEST, NULL, NULL);
}
/******************************************************************************/
LOCAL void arkime_db_es_update_file(char *json, int json_len, uint32_t fileid)
{
    char key[1000];
    int key_len = arkime_snprintf_len(key, sizeof(key), "/%sfiles/_update/%s-%u", config.prefix, config.nodeName, fileid);
    arkime_http_schedule(esServer, "POST", key, key_len, json, json_len, NULL, ARKIME_HTTP_PRIORITY_DROPABLE, NULL, NULL);
}
/******************************************************************************/
LOCAL gboolean arkime_db_es_file_exists(const char *filename, uint32_t *outputId)
{
    size_t                 data_len;
    char                   key[2000];
    int                    key_len;

    key_len = arkime_snprintf_len(key, sizeof(key), "/%sfiles/_search?rest_total_hits_as_int&size=1&sort=num:desc&q=node%%3A%%22%s%%22+AND+name%%3A%%22%s%%22", config.prefix, config.nodeName, filename);

    uint8_t *data = arkime_http_get(esServer, key, key_len, &data_len);

    if (!data) {
        return FALSE;
    }

    uint32_t           hits_len;
    const uint8_t     *hits = arkime_js0n_get(data, data_len, "hits", &hits_len);

    if (!hits_len || !hits) {
        free(data);
        return FALSE;
    }

    uint32_t           total_len;
    const uint8_t     *total = arkime_js0n_get(hits, hits_len, "total", &total_len);

    if (!total_len || !total) {
        free(data);
        return FALSE;
    }

    if (*total == '0') {
        free(data);
        return FALSE;
    }

    if (outputId) {
        hits = arkime_js0n_get(data, data_len, "hits", &hits_len);

        uint32_t           hit_len;
        const uint8_t     *hit = arkime_js0n_get(hits, hits_len, "hits", &hit_len);

        if (!hit || hit_len < 2) {
            free(data);
            return FALSE;
        }

        uint32_t           source_len;
        const uint8_t     *source = 0;

        /* Remove array wrapper */
        source = arkime_js0n_get(hit + 1, hit_len - 2, "_source", &source_len);

        uint32_t           len;
        const uint8_t     *value;

        if ((value = arkime_js0n_get(source, source_len, "num", &len))) {
            *outputId = atoi((char *)value);
        } else {
            LOGEXIT("ERROR - Files check has no num field in %.*s", source_len, source);
        }
    }

    free(data);
    return TRUE;
}
/******************************************************************************/
LOCAL void arkime_db_es_load_fields()
{
    size_t                 data_len;
    char                   key[100];
    int                    key_len;

    key_len = arkime_snprintf_len(key, sizeof(key), "/%sfields/_search?size=3000", config.prefix);
    uint8_t           *data = arkime_http_get(esServer, key, key_len, &data_len);

    if (!data) {
        LOGEXIT("ERROR - Couldn't download %sfields, database (%s) might be down or not initialized.", config.prefix, config.elasticsearch);
        return;
    }

    uint32_t           hits_len;
    const uint8_t     *hits = 0;
    hits = arkime_js0n_get(data, data_len, "hits", &hits_len);
    if (!hits) {
        LOGEXIT("ERROR - Couldn't download %sfields, database (%s) might be down or not initialized.", config.prefix, config.elasticsearch);
        free(data);
        return;
    }

    uint32_t           ahits_len;
    const uint8_t     *ahits = 0;
    ahits = arkime_js0n_get(hits, hits_len, "hits", &ahits_len);

    if (!ahits) {
        LOGEXIT("ERROR - Couldn't download %sfields, database (%s) might be down or not initialized.", config.prefix, config.elasticsearch);
        free(data);
        return;
    }

    uint32_t out[2 * 8000];
    memset(out, 0, sizeof(out));
    js0n(ahits, ahits_len, out, sizeof(out));
    for (int i = 0; out[i]; i += 2) {
        uint32_t           id_len;
        const uint8_t     *id = 0;
        id = arkime_js0n_get(ahits + out[i], out[i + 1], "_id", &id_len);

        uint32_t           source_len;
        const uint8_t     *source = 0;
        source = arkime_js0n_get(ahits + out[i], out[i + 1], "_source", &source_len);
        if (!source) {
            continue;
        }

        arkime_field_define_json(id, id_len, source, source_len);
    }
    free(data);
}
/******************************************************************************/
LOCAL void arkime_db_es_send_fields(char *json, int json_len, gboolean sync)
{
    if (!sync) {
        arkime_http_schedule(esServer, "POST", "/_bulk", 6, json, json_len, NULL, ARKIME_HTTP_PRIORITY_BEST, NULL, NULL);
    } else {
        uint8_t *data = arkime_http_send_sync(esServer, "POST", "/_bulk", 6, json, json_len, NULL, NULL, NULL);
        arkime_http_free_buffer(json);
        if (data)
            free(data);
    }
}
/******************************************************************************/
LOCAL int arkime_db_es_queue_length()
{
    return arkime_http_queue_length(esServer);
}
/******************************************************************************/
LOCAL int arkime_db_es_queue_length_best()
{
    return arkime_http_queue_length_best(esServer);
}
/******************************************************************************/
LOCAL uint64_t arkime_db_es_dropped_count()
{
    return arkime_http_dropped_count(esServer);
}
/******************************************************************************/
LOCAL void arkime_db_es_refresh()
{
    char path[100];
    snprintf(path, sizeof(path), "/%s*/_refresh", config.prefix);
    uint8_t *data = arkime_http_get(esServer, path, -1, NULL);
    if (data)
        free(data);
}
/******************************************************************************/
LOCAL void arkime_db_es_health_check_cb(int UNUSED(code), uint8_t *data, int data_len, gpointer uw)
{
    if (code != 200 || !data) {
        LOG("WARNING - Couldn't perform Elasticsearch health check");
        return;
    }

    uint32_t           status_len;
    const uint8_t     *status;
    struct timespec    stopHealthCheck;

    clock_gettime(CLOCK_MONOTONIC, &stopHealthCheck);

    esHealthMS = (stopHealthCheck.tv_sec - startHealthCheck.tv_sec) * 1000 +
                 (stopHealthCheck.tv_nsec - startHealthCheck.tv_nsec) / 1000000L;

    if (*data == '[')
        status = arkime_js0n_get(data + 1, data_len - 2, "status", &status_len);
    else
        status = arkime_js0n_get(data, data_len, "status", &status_len);

    if (!status) {
        LOG("WARNING - Couldn't find status in '%.*s'", data_len, data);
    } else if (esHealthMS > 20000) {
        LOG("WARNING - Elasticsearch health check took more than 20 seconds %" PRIu64 "ms", esHealthMS);
    } else if ((status[0] == 'y' && uw == GINT_TO_POINTER(1)) || (status[0] == 'r')) {
        LOG("WARNING - Elasticsearch is %.*s and took %" PRIu64 "ms to query health, this may cause issues.  See FAQ.", status_len, status, esHealthMS);
    }
}
/******************************************************************************/
LOCAL gboolean arkime_db_es_health_check(gpointer user_data)
{
    arkime_http_schedule(esServer, "GET", "/_cat/health?format=json", -1, NULL, 0, NULL, ARKIME_HTTP_PRIORITY_DROPABLE, arkime_db_es_health_check_cb, user_data);
    clock_gettime(CLOCK_MONOTONIC, &startHealthCheck);
    return G_SOURCE_CONTINUE;
}
/******************************************************************************/
LOCAL void arkime_db_es_exit()
{
    arkime_http_free_server(esServer);
}
/******************************************************************************/
LOCAL uint64_t zero_atoll(const char *v)
{
    if (v)
        return atoll(v);
    return 0;
}
/******************************************************************************/
LOCAL ArkimeDbImpl_t esImpl = {
    .init                    = NULL, /* set below after forward declaration */
    .exit                    = arkime_db_es_exit,
    .load_stats              = arkime_db_es_load_stats,
    .send_stats              = arkime_db_es_send_stats,
    .check                   = arkime_db_es_check,
    .get_sequence_number      = arkime_db_es_get_sequence_number,
    .get_sequence_number_sync = arkime_db_es_get_sequence_number_sync,
    .create_file             = arkime_db_es_create_file,
    .update_file             = arkime_db_es_update_file,
    .file_exists             = arkime_db_es_file_exists,
    .load_fields             = arkime_db_es_load_fields,
    .send_fields             = arkime_db_es_send_fields,
    .queue_length            = arkime_db_es_queue_length,
    .queue_length_best       = arkime_db_es_queue_length_best,
    .dropped_count           = arkime_db_es_dropped_count,
    .refresh                 = arkime_db_es_refresh,
};

LOCAL guint esHealthTimer;

/******************************************************************************/
LOCAL void arkime_db_es_init_impl(const char *url, ArkimeDbMode_t UNUSED(mode))
{
    const char *esUrl;

    if (url) {
        /* Replace elasticsearch:// or elasticsearchs:// with http:// or https:// */
        if (strncmp(url, "elasticsearchs://", 17) == 0) {
            char *newUrl = g_strdup_printf("https://%s", url + 17);
            esUrl = newUrl;
        } else if (strncmp(url, "elasticsearch://", 16) == 0) {
            char *newUrl = g_strdup_printf("http://%s", url + 16);
            esUrl = newUrl;
        } else {
            esUrl = url;
        }
    } else {
        esUrl = config.elasticsearch;
    }

    esServer = arkime_http_create_server(esUrl, config.maxESConns, config.maxESRequests, config.compressES);

    static char *headers[4] = {"Content-Type: application/json", "Expect:", NULL, NULL};

    const char *elasticsearchAPIKey = arkime_config_str(NULL, "elasticsearchAPIKey", NULL);
    const char *elasticsearchBasicAuth = arkime_config_str(NULL, "elasticsearchBasicAuth", NULL);
    if (elasticsearchAPIKey) {
        static char auth[1024];
        snprintf(auth, sizeof(auth), "Authorization: ApiKey %s", elasticsearchAPIKey);
        headers[2] = auth;
    } else if (elasticsearchBasicAuth) {
        static char auth[1024];
        if (strchr(elasticsearchBasicAuth, ':') != NULL) {
            gchar *b64 = g_base64_encode((uint8_t *)elasticsearchBasicAuth, strlen(elasticsearchBasicAuth));
            snprintf(auth, sizeof(auth), "Authorization: Basic %s", b64);
            g_free(b64);
        } else {
            snprintf(auth, sizeof(auth), "Authorization: Basic %s", elasticsearchBasicAuth);
        }
        headers[2] = auth;
    }

    arkime_http_set_headers(esServer, headers);
    arkime_http_set_print_errors(esServer);

    int maxRetries = arkime_config_int(NULL, "esMaxRetries", 2, 0, 10);
    arkime_http_set_retries(esServer, maxRetries);

    char *clientCert = arkime_config_str(NULL, "esClientCert", NULL);
    char *clientKey = arkime_config_str(NULL, "esClientKey", NULL);
    char *clientKeyPass = arkime_config_str(NULL, "esClientKeyPass", NULL);
    arkime_http_set_client_cert(esServer, clientCert, clientKey, clientKeyPass);

    esBulkQuery = arkime_config_str(NULL, "esBulkQuery", "/_bulk");
    esBulkQueryLen = strlen(esBulkQuery);

    arkime_db_es_health_check(GINT_TO_POINTER(1));

    if (arkime_config_boolean(NULL, "dbEsHealthCheck", TRUE)) {
        esHealthTimer = g_timeout_add_seconds(30, arkime_db_es_health_check, 0);
    }

    arkime_db_set_send_bulk(arkime_db_es_send_bulk);
}
/******************************************************************************/
void arkime_db_es_init()
{
    esImpl.init = arkime_db_es_init_impl;
    arkime_db_register("elasticsearch", &esImpl);
    arkime_db_register("elasticsearchs", &esImpl);
}
