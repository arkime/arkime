/******************************************************************************/
/* db.es.c -- Elasticsearch/OpenSearch sessions db and cluster db for capture's
 *            stats, dstats, files, fields and sequence, the default when dbUrl isn't set
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include "arkimeconfig.h"
#include <inttypes.h>

extern ArkimeConfig_t config;
// The sessions ES, NULL when the sessions db isn't OpenSearch/Elasticsearch
LOCAL void          *esServer;
// esServer unless dbUrl is a different OpenSearch/Elasticsearch
LOCAL void          *clusterServer;
LOCAL const char    *clusterServerName;

LOCAL char          *esBulkQuery;
LOCAL int            esBulkQueryLen;
LOCAL struct timespec startHealthCheck;
LOCAL uint64_t       esHealthMS;

/******************************************************************************/
// An OpenSearch/Elasticsearch server with the elasticsearch auth, retry and client cert settings
LOCAL void *arkime_db_create_es_server(const char *url)
{
    void *server = arkime_http_create_server(url, config.maxESConns, config.maxESRequests, config.compressES);

    // The sessions and cluster servers share them, http servers don't free their headers
    static char *headers[4];
    if (!headers[0]) {
        headers[0] = "Content-Type: application/json";
        headers[1] = "Expect:";

        const char *elasticsearchAPIKey = arkime_config_str(NULL, "elasticsearchAPIKey", NULL);
        const char *elasticsearchBasicAuth = arkime_config_str(NULL, "elasticsearchBasicAuth", NULL);
        if (elasticsearchAPIKey) {
            headers[2] = g_strdup_printf("Authorization: ApiKey %s", elasticsearchAPIKey);
        } else if (elasticsearchBasicAuth) {
            if (strchr(elasticsearchBasicAuth, ':') != NULL) {
                gchar *b64 = g_base64_encode((uint8_t *)elasticsearchBasicAuth, strlen(elasticsearchBasicAuth));
                headers[2] = g_strdup_printf("Authorization: Basic %s", b64);
                g_free(b64);
            } else {
                headers[2] = g_strdup_printf("Authorization: Basic %s", elasticsearchBasicAuth);
            }
        }
    }

    arkime_http_set_headers(server, headers);
    arkime_http_set_print_errors(server);

    int maxRetries = arkime_config_int(NULL, "esMaxRetries", 2, 0, 10);
    arkime_http_set_retries(server, maxRetries);

    char *clientCert = arkime_config_str(NULL, "esClientCert", NULL);
    char *clientKey = arkime_config_str(NULL, "esClientKey", NULL);
    char *clientKeyPass = arkime_config_str(NULL, "esClientKeyPass", NULL);
    arkime_http_set_client_cert(server, clientCert, clientKey, clientKeyPass);

    return server;
}
/******************************************************************************/
// Field changes are batched into a bulk, everything else is its own request
LOCAL BSB   fieldBSB;
LOCAL int   fieldBSBTimeout;
LOCAL gboolean arkime_db_es_fieldbsb_timeout(gpointer user_data)
{
    if (fieldBSB.buf && BSB_LENGTH(fieldBSB) > 0) {
        if (user_data == 0)
            arkime_http_schedule(clusterServer, "POST", "/_bulk", 6, (char *)fieldBSB.buf, BSB_LENGTH(fieldBSB), NULL, ARKIME_HTTP_PRIORITY_BEST, NULL, NULL);
        else {
            uint8_t       *data = arkime_http_send_sync(clusterServer, "POST", "/_bulk", 6, (char *)fieldBSB.buf, BSB_LENGTH(fieldBSB), NULL, NULL, NULL);
            arkime_http_free_buffer(fieldBSB.buf);
            if (data)
                free(data);
        }
        BSB_INIT(fieldBSB, arkime_http_get_buffer(config.dbBulkSize), config.dbBulkSize);
    }
    fieldBSBTimeout = 0;
    return G_SOURCE_REMOVE;
}
/******************************************************************************/
LOCAL void arkime_db_es_fieldbsb_make(int len)
{
    if (!fieldBSB.buf) {
        BSB_INIT(fieldBSB, arkime_http_get_buffer(config.dbBulkSize), config.dbBulkSize);
        fieldBSBTimeout = g_timeout_add_seconds(1, arkime_db_es_fieldbsb_timeout, 0);
    } else if (BSB_REMAINING(fieldBSB) < len + 1000) {
        g_source_remove(fieldBSBTimeout);
        arkime_db_es_fieldbsb_timeout(0);
        fieldBSBTimeout = g_timeout_add_seconds(1, arkime_db_es_fieldbsb_timeout, 0);
    }
}
/******************************************************************************/
// ES drops an update that gets there before its doc's create, so a files update waits for the create
typedef struct {
    char *key;
    char *buf;
    int   key_len;
    int   buf_len;
    int   priority;
} ArkimeDbEsDeferred_t;

typedef struct {
    int        creates; // still in flight
    GPtrArray *deferred;
} ArkimeDbEsPending_t;

LOCAL GHashTable *pendingFiles; // id -> ArkimeDbEsPending_t
LOCAL ARKIME_LOCK_DEFINE(pendingFiles);

LOCAL void arkime_db_es_file_created(char *id)
{
    GPtrArray *deferred = NULL;
    ARKIME_LOCK(pendingFiles);
    ArkimeDbEsPending_t *pending = g_hash_table_lookup(pendingFiles, id);
    if (pending && --pending->creates == 0) {
        deferred = pending->deferred;
        g_hash_table_remove(pendingFiles, id);
        ARKIME_TYPE_FREE(ArkimeDbEsPending_t, pending);
    }
    ARKIME_UNLOCK(pendingFiles);

    for (guint i = 0; deferred && i < deferred->len; i++) {
        ArkimeDbEsDeferred_t *d = g_ptr_array_index(deferred, i);
        arkime_http_schedule(clusterServer, "POST", d->key, d->key_len, d->buf, d->buf_len, NULL, d->priority, NULL, NULL);
        g_free(d->key);
        ARKIME_TYPE_FREE(ArkimeDbEsDeferred_t, d);
    }
    if (deferred)
        g_ptr_array_free(deferred, TRUE);
    g_free(id);
}
/******************************************************************************/
LOCAL void arkime_db_es_file_created_cb(int UNUSED(code), uint8_t *UNUSED(data), int UNUSED(data_len), gpointer uw)
{
    arkime_db_es_file_created(uw);
}
/******************************************************************************/
LOCAL void arkime_db_es_send(const char *key, int key_len, const char *json, int len, gboolean sync, int priority, const char *fileId)
{
    char *buf = arkime_http_get_buffer(len);
    memcpy(buf, json, len);

    if (sync) {
        uint8_t *data = arkime_http_send_sync(clusterServer, "POST", key, key_len, buf, len, NULL, NULL, NULL);
        if (data)
            free(data);
        arkime_http_free_buffer(buf);
    } else if (fileId) {
        char *id = g_strdup(fileId);
        ARKIME_LOCK(pendingFiles);
        ArkimeDbEsPending_t *pending = g_hash_table_lookup(pendingFiles, fileId);
        if (!pending) {
            pending = ARKIME_TYPE_ALLOC0(ArkimeDbEsPending_t);
            pending->deferred = g_ptr_array_new();
            g_hash_table_insert(pendingFiles, g_strdup(fileId), pending);
        }
        pending->creates++;
        ARKIME_UNLOCK(pendingFiles);
        if (arkime_http_schedule(clusterServer, "POST", key, key_len, buf, len, NULL, priority, arkime_db_es_file_created_cb, id))
            arkime_db_es_file_created(id);
    } else {
        arkime_http_schedule(clusterServer, "POST", key, key_len, buf, len, NULL, priority, NULL, NULL);
    }
}
/******************************************************************************/
LOCAL void arkime_db_es_put(const char *table, const char *id, const char *json, int len, gboolean sync, int priority)
{
    if (strcmp(table, "fields") == 0) {
        arkime_db_es_fieldbsb_make(len);
        BSB_EXPORT_sprintf(fieldBSB, "{\"index\": {\"_index\": \"%sfields\", \"_id\": \"%s\"}}\n", config.clusterPrefix, id);
        BSB_EXPORT_ptr(fieldBSB, json, len);
        BSB_EXPORT_u08(fieldBSB, '\n');
        return;
    }

    char key[300];
    const gboolean files = strcmp(table, "files") == 0;
    int  key_len = arkime_snprintf_len(key, sizeof(key), "/%s%s/_doc/%s%s", config.clusterPrefix, table, id, files ? "?refresh=true" : "");
    arkime_db_es_send(key, key_len, json, len, sync, priority, files ? id : NULL);
}
/******************************************************************************/
// Live captures use an external version so an out of order stats doc is rejected
LOCAL uint64_t esStatsVersion;
LOCAL void arkime_db_es_put_stats(const char *json, int len, gboolean sync, int priority)
{
    char key[300];
    int  key_len;
    if (config.pcapReadOffline) {
        key_len = arkime_snprintf_len(key, sizeof(key), "/%sstats/_doc/%s", config.clusterPrefix, config.nodeName);
    } else {
        esStatsVersion++;
        key_len = arkime_snprintf_len(key, sizeof(key), "/%sstats/_doc/%s?version_type=external&version=%" PRIu64, config.clusterPrefix, config.nodeName, esStatsVersion);
    }
    arkime_db_es_send(key, key_len, json, len, sync, priority, NULL);
}
/******************************************************************************/
LOCAL void arkime_db_es_update(const char *table, const char *id, const char *json, int len, int priority)
{
    if (strcmp(table, "fields") == 0) {
        arkime_db_es_fieldbsb_make(len);
        BSB_EXPORT_sprintf(fieldBSB, "{\"update\": {\"_index\": \"%sfields\", \"_id\": \"%s\"}}\n{\"doc\": ", config.clusterPrefix, id);
        BSB_EXPORT_ptr(fieldBSB, json, len);
        BSB_EXPORT_cstr(fieldBSB, "}\n");
        return;
    }

    char key[300];
    int  key_len = arkime_snprintf_len(key, sizeof(key), "/%s%s/_update/%s", config.clusterPrefix, table, id);

    int   buf_len = len + 9;
    char *buf = arkime_http_get_buffer(buf_len);
    memcpy(buf, "{\"doc\": ", 8);
    memcpy(buf + 8, json, len);
    buf[buf_len - 1] = '}';

    if (strcmp(table, "files") == 0) {
        ARKIME_LOCK(pendingFiles);
        ArkimeDbEsPending_t *pending = g_hash_table_lookup(pendingFiles, id);
        if (pending) {
            ArkimeDbEsDeferred_t *d = ARKIME_TYPE_ALLOC(ArkimeDbEsDeferred_t);
            d->key = g_strndup(key, key_len);
            d->key_len = key_len;
            d->buf = buf;
            d->buf_len = buf_len;
            d->priority = priority;
            g_ptr_array_add(pending->deferred, d);
            ARKIME_UNLOCK(pendingFiles);
            return;
        }
        ARKIME_UNLOCK(pendingFiles);
    }
    arkime_http_schedule(clusterServer, "POST", key, key_len, buf, buf_len, NULL, priority, NULL, NULL);
}
/******************************************************************************/
LOCAL void arkime_db_es_delete(const char *table, const char *id)
{
    if (strcmp(table, "fields") == 0) {
        arkime_db_es_fieldbsb_make(0);
        BSB_EXPORT_sprintf(fieldBSB, "{\"delete\": {\"_index\": \"%sfields\", \"_id\": \"%s\"}}\n", config.clusterPrefix, id);
        return;
    }

    char key[300];
    int  key_len = arkime_snprintf_len(key, sizeof(key), "/%s%s/_doc/%s", config.clusterPrefix, table, id);
    arkime_http_schedule(clusterServer, "DELETE", key, key_len, NULL, 0, NULL, ARKIME_HTTP_PRIORITY_BEST, NULL, NULL);
}
/******************************************************************************/
typedef struct arkime_seq_request {
    char               *name;
    ArkimeSeqNum_cb     func;
    gpointer            uw;
} ArkimeSeqRequest_t;

LOCAL void arkime_db_es_sequence(const char *name, ArkimeSeqNum_cb func, gpointer uw);
LOCAL void arkime_db_es_sequence_cb(int UNUSED(code), uint8_t *data, int data_len, gpointer uw)
{
    ArkimeSeqRequest_t *r = uw;
    uint32_t            version_len;

    const uint8_t *version = arkime_js0n_get(data, data_len, "_version", &version_len);

    if (!version_len || !version) {
        LOG("ERROR - Couldn't fetch sequence: %.*s", data_len, data);
        arkime_db_es_sequence(r->name, r->func, r->uw);
    } else {
        if (r->func)
            r->func(atoi((char *)version), r->uw);
    }

    g_free(r->name);
    ARKIME_TYPE_FREE(ArkimeSeqRequest_t, r);
}
/******************************************************************************/
LOCAL void arkime_db_es_sequence(const char *name, ArkimeSeqNum_cb func, gpointer uw)
{
    char                key[200];
    int                 key_len;
    ArkimeSeqRequest_t *r = ARKIME_TYPE_ALLOC(ArkimeSeqRequest_t);
    char               *json = arkime_http_get_buffer(ARKIME_HTTP_BUFFER_SIZE);

    r->name = g_strdup(name);
    r->func = func;
    r->uw   = uw;

    key_len = arkime_snprintf_len(key, sizeof(key), "/%ssequence/_doc/%s", config.clusterPrefix, name);
    int json_len = arkime_snprintf_len(json, ARKIME_HTTP_BUFFER_SIZE, "{}");
    arkime_http_schedule(clusterServer, "POST", key, key_len, json, json_len, NULL, ARKIME_HTTP_PRIORITY_BEST, arkime_db_es_sequence_cb, r);
}
/******************************************************************************/
LOCAL uint32_t arkime_db_es_sequence_sync(const char *name)
{
    while (1) {
        char key[200];
        int key_len = arkime_snprintf_len(key, sizeof(key), "/%ssequence/_doc/%s", config.clusterPrefix, name);

        size_t data_len;
        uint8_t *data = arkime_http_send_sync(clusterServer, "POST", key, key_len, "{}", 2, NULL, &data_len, NULL);
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
LOCAL void arkime_db_es_each(const char *table, ArkimeDbRow_cb func)
{
    size_t                 data_len;
    char                   key[100];
    int                    key_len;

    key_len = arkime_snprintf_len(key, sizeof(key), "/%s%s/_search?size=3000", config.clusterPrefix, table);
    uint8_t           *data = arkime_http_get(clusterServer, key, key_len, &data_len);

    if (!data) {
        LOGEXIT("ERROR - Couldn't download %s%s, database (%s) might be down or not initialized.", config.clusterPrefix, table, clusterServerName);
    }

    uint32_t           hits_len;
    const uint8_t     *hits = arkime_js0n_get(data, data_len, "hits", &hits_len);
    uint32_t           ahits_len = 0;
    const uint8_t     *ahits = hits ? arkime_js0n_get(hits, hits_len, "hits", &ahits_len) : NULL;

    if (!ahits) {
        LOGEXIT("ERROR - Couldn't download %s%s, database (%s) might be down or not initialized.", config.clusterPrefix, table, clusterServerName);
    }

    uint32_t out[2 * 8000];
    memset(out, 0, sizeof(out));
    if (js0n(ahits, ahits_len, out, sizeof(out)) != 0)
        LOG("WARNING - Couldn't parse all of the %s%s response, some may be missing", config.clusterPrefix, table);
    for (int i = 0; out[i]; i += 2) {
        uint32_t           id_len;
        const uint8_t     *id = arkime_js0n_get(ahits + out[i], out[i + 1], "_id", &id_len);

        uint32_t           source_len;
        const uint8_t     *source = arkime_js0n_get(ahits + out[i], out[i + 1], "_source", &source_len);
        if (!source) {
            continue;
        }

        func(id, id_len, source, source_len);
    }
    free(data);
}
/******************************************************************************/
LOCAL gboolean arkime_db_es_file_exists(const char *node, const char *filename, uint32_t *outputId)
{
    size_t                 data_len;
    char                   key[200];
    int                    key_len;

    key_len = arkime_snprintf_len(key, sizeof(key), "/%sfiles/_search?rest_total_hits_as_int", config.clusterPrefix);

    // Escaping can expand each byte to 6
    int   size = 6 * (strlen(node) + strlen(filename)) + 200;
    char *body = g_malloc(size);
    BSB   bsb;
    BSB_INIT(bsb, body, size);
    BSB_EXPORT_cstr(bsb, "{\"size\":1,\"sort\":[{\"num\":\"desc\"}],\"query\":{\"bool\":{\"filter\":[{\"terms\":{\"node\":[");
    arkime_db_js0n_str(&bsb, (uint8_t *)node, TRUE);
    BSB_EXPORT_cstr(bsb, "]}},{\"term\":{\"name\":");
    arkime_db_js0n_str(&bsb, (uint8_t *)filename, TRUE);
    BSB_EXPORT_cstr(bsb, "}}]}}}");

    uint8_t *data = arkime_http_send_sync(clusterServer, "POST", key, key_len, body, BSB_LENGTH(bsb), NULL, &data_len, NULL);
    g_free(body);

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
LOCAL int arkime_db_es_queue_length_best()
{
    return arkime_http_queue_length_best(clusterServer);
}
/******************************************************************************/
LOCAL int arkime_db_es_queue_length()
{
    ARKIME_LOCK(pendingFiles);
    const int pending = g_hash_table_size(pendingFiles);
    ARKIME_UNLOCK(pendingFiles);
    return arkime_http_queue_length(clusterServer) + pending;
}
/******************************************************************************/
LOCAL void arkime_db_es_exit()
{
    if (fieldBSBTimeout)
        g_source_remove(fieldBSBTimeout);

    if (fieldBSB.buf && BSB_LENGTH(fieldBSB) > 0) {
        arkime_db_es_fieldbsb_timeout(GINT_TO_POINTER(1));
    }

    if (fieldBSB.buf) {
        arkime_http_free_buffer(fieldBSB.buf);
        fieldBSB.buf = 0;
    }

    // Just the cluster db indices, a /<prefix>*/ would be every index with an empty prefix
    if (!config.noRefresh && (clusterServer != esServer || strcmp(config.clusterPrefix, config.prefix) != 0)) {
        char path[400];
        const char *p = config.clusterPrefix;
        snprintf(path, sizeof(path), "/%sfiles,%sstats,%sdstats,%sfields,%ssequence/_refresh?ignore_unavailable=true", p, p, p, p, p);
        uint8_t *data = arkime_http_get(clusterServer, path, -1, NULL);
        if (data)
            free(data);
    }

    if (clusterServer != esServer) {
        arkime_http_free_server(clusterServer);
        clusterServer = esServer;
    }
}
/******************************************************************************/
LOCAL const ArkimeClusterDb_t esClusterDb = {
    .put           = arkime_db_es_put,
    .put_stats     = arkime_db_es_put_stats,
    .update        = arkime_db_es_update,
    .del           = arkime_db_es_delete,
    .sequence      = arkime_db_es_sequence,
    .sequence_sync = arkime_db_es_sequence_sync,
    .each          = arkime_db_es_each,
    .file_exists   = arkime_db_es_file_exists,
    .queue_length  = arkime_db_es_queue_length,
    .queue_length_best = arkime_db_es_queue_length_best,
    .exit          = arkime_db_es_exit
};
/******************************************************************************/
// The sessions ES server unless dbUrl is a different OpenSearch/Elasticsearch
LOCAL const ArkimeClusterDb_t *arkime_db_es_cluster_init(const char *url)
{
    pendingFiles = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    char *httpUrl = url ? arkime_config_es_url(url) : NULL;
    if (!httpUrl || (esServer && strcmp(httpUrl, config.elasticsearch) == 0)) {
        g_free(httpUrl);
        clusterServer = esServer;
        clusterServerName = config.elasticsearch;
    } else {
        clusterServer = arkime_db_create_es_server(httpUrl);
        clusterServerName = httpUrl;
    }

    // File numbers start at 101
    if (clusterServer) {
        char     key[200];
        int      key_len = arkime_snprintf_len(key, sizeof(key), "/%ssequence/_doc/fn-%s", config.clusterPrefix, config.nodeName);
        size_t   data_len;
        uint8_t *data = arkime_http_get(clusterServer, key, key_len, &data_len);
        if (!data)
            LOGEXIT("ERROR - Couldn't fetch sequence: no data returned - %.*s", key_len, key);

        uint32_t       found_len;
        const uint8_t *found = arkime_js0n_get(data, data_len, "found", &found_len);
        if (found && (found_len < 4 || memcmp("true", found, 4) != 0)) {
            free(data);
            key_len = arkime_snprintf_len(key, sizeof(key), "/%ssequence/_doc/fn-%s?version_type=external&version=100", config.clusterPrefix, config.nodeName);
            data = arkime_http_send_sync(clusterServer, "POST", key, key_len, "{}", 2, NULL, NULL, NULL);
        }
        if (data)
            free(data);
    }

    if (clusterServer && !config.pcapReadOffline && !config.dryRun) {
        char     key[300];
        int      key_len = arkime_snprintf_len(key, sizeof(key), "/%sstats/_doc/%s?_source=false", config.clusterPrefix, config.nodeName);
        size_t   data_len;
        uint8_t *data = arkime_http_get(clusterServer, key, key_len, &data_len);
        if (!data)
            LOGEXIT("ERROR - Couldn't fetch stats: no data returned - %.*s", key_len, key);

        uint32_t       version_len;
        const uint8_t *version = arkime_js0n_get(data, data_len, "_version", &version_len);
        if (version && version_len)
            esStatsVersion = atoll((char *)version);
        free(data);
    }
    return &esClusterDb;
}
/******************************************************************************/
LOCAL void arkime_db_es_send_bulk_cb(int code, uint8_t *data, int data_len, gpointer UNUSED(uw))
{
    uint8_t *forbidden;

    if (code != 200)
        LOG("Bulk issue.  Code: %d\n%.*s", code, data_len, data);
    else if (config.debug > 4)
        LOG("Bulk Reply code:%d :>%.*s<", code, data_len, data);
    else if (data && (forbidden = (uint8_t *)strstr((char *)data, "FORBIDDEN")) != 0) {
        const uint8_t *end = forbidden + 10;
        while (forbidden > data && *forbidden != '{') {
            forbidden--;
        }
        while (end < data + data_len && *end != '}') {
            end++;
        }
        LOG("ERROR - OpenSearch/Elasticsearch is returning a FORBIDDEN error. This is most likely because: the index is closed, the index is read-only from ILM, or you've hit the disk watermarks. %.*s", (int)(end - forbidden + 1), forbidden);
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
LOCAL void arkime_db_es_health_cb(int code, uint8_t *data, int data_len, gpointer uw)
{
    if (code != 200 || !data || data_len < 2) {
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
// Runs on main thread
LOCAL void arkime_db_es_health(gboolean first)
{
    arkime_http_schedule(esServer, "GET", "/_cat/health?format=json", -1, NULL, 0, NULL, ARKIME_HTTP_PRIORITY_DROPPABLE, arkime_db_es_health_cb, GINT_TO_POINTER(first));
    clock_gettime(CLOCK_MONOTONIC, &startHealthCheck);
}
/******************************************************************************/
LOCAL uint64_t arkime_db_es_health_ms()
{
    return esHealthMS;
}
/******************************************************************************/
LOCAL int arkime_db_es_check()
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

    const int dbVersion = atoi((char *)version);
    if (dbVersion < ARKIME_MIN_DB_VERSION) {
        LOGEXIT("ERROR - Database version '%.*s' is too old, needs to be at least (%d), run \"db/db.pl host:port upgrade\"", version_len, version, ARKIME_MIN_DB_VERSION);
    }
    free(data);
    return dbVersion;
}
/******************************************************************************/
LOCAL int arkime_db_es_sessions_queue_length()
{
    return arkime_http_queue_length(esServer);
}
/******************************************************************************/
LOCAL uint64_t arkime_db_es_dropped()
{
    return arkime_http_dropped_count(esServer);
}
/******************************************************************************/
LOCAL void arkime_db_es_sessions_exit()
{
    if (!config.noRefresh) {
        char path[100];
        snprintf(path, sizeof(path), "/%s*/_refresh", config.prefix);
        uint8_t *data = arkime_http_get(esServer, path, -1, NULL);
        if (data)
            free(data);
    }
    arkime_http_free_server(esServer);
    esServer = NULL;
}
/******************************************************************************/
LOCAL const ArkimeSessionsDb_t esSessionsDb = {
    .check        = arkime_db_es_check,
    .health       = arkime_db_es_health,
    .health_ms    = arkime_db_es_health_ms,
    .queue_length = arkime_db_es_sessions_queue_length,
    .dropped      = arkime_db_es_dropped,
    .exit         = arkime_db_es_sessions_exit
};
/******************************************************************************/
// The sessions ES, the cluster db shares it when dbUrl is the same ES
LOCAL const ArkimeSessionsDb_t *arkime_db_es_sessions_init(const char *url)
{
    g_free(config.elasticsearch);
    config.elasticsearch = arkime_config_es_url(url);
    esServer = arkime_db_create_es_server(config.elasticsearch);

    esBulkQuery = arkime_config_str(NULL, "esBulkQuery", "/_bulk");
    esBulkQueryLen = strlen(esBulkQuery);
    arkime_db_set_send_bulk2(arkime_db_es_send_bulk, TRUE, FALSE, 0xffff);
    return &esSessionsDb;
}
/******************************************************************************/
// The sessions ES, else an ES cluster db, for plugins like tagger that keep their own indices
void *arkime_db_es_server()
{
    return esServer ? esServer : clusterServer;
}
/******************************************************************************/
void arkime_db_es_init()
{
    const char *schemes[] = {"http", "https", "elasticsearch", "elasticsearchs", "opensearch", "opensearchs"};
    for (int i = 0; i < ARRAY_LEN(schemes); i++) {
        arkime_db_sessions_register((char *)schemes[i], arkime_db_es_sessions_init);
        arkime_db_cluster_register((char *)schemes[i], arkime_db_es_cluster_init);
    }
}
