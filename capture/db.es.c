/******************************************************************************/
/* db.es.c -- Elasticsearch/OpenSearch cluster db for capture's stats, dstats,
 *            files, fields and sequence, the default when clusterDbUrl isn't set
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include "arkimeconfig.h"
#include <inttypes.h>

extern ArkimeConfig_t config;
extern void          *esServer;

/******************************************************************************/
// Field changes are batched into a bulk, everything else is its own request
LOCAL BSB   fieldBSB;
LOCAL int   fieldBSBTimeout;
LOCAL gboolean arkime_db_es_fieldbsb_timeout(gpointer user_data)
{
    if (fieldBSB.buf && BSB_LENGTH(fieldBSB) > 0) {
        if (user_data == 0)
            arkime_http_schedule(esServer, "POST", "/_bulk", 6, (char *)fieldBSB.buf, BSB_LENGTH(fieldBSB), NULL, ARKIME_HTTP_PRIORITY_BEST, NULL, NULL);
        else {
            uint8_t       *data = arkime_http_send_sync(esServer, "POST", "/_bulk", 6, (char *)fieldBSB.buf, BSB_LENGTH(fieldBSB), NULL, NULL, NULL);
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
LOCAL void arkime_db_es_send(const char *key, int key_len, const char *json, int len, gboolean sync, int priority)
{
    char *buf = arkime_http_get_buffer(len);
    memcpy(buf, json, len);

    if (sync) {
        uint8_t *data = arkime_http_send_sync(esServer, "POST", key, key_len, buf, len, NULL, NULL, NULL);
        if (data)
            free(data);
        arkime_http_free_buffer(buf);
    } else {
        arkime_http_schedule(esServer, "POST", key, key_len, buf, len, NULL, priority, NULL, NULL);
    }
}
/******************************************************************************/
LOCAL void arkime_db_es_put(const char *table, const char *id, const char *json, int len, gboolean sync, int priority)
{
    if (strcmp(table, "fields") == 0) {
        arkime_db_es_fieldbsb_make(len);
        BSB_EXPORT_sprintf(fieldBSB, "{\"index\": {\"_index\": \"%sfields\", \"_id\": \"%s\"}}\n", config.prefix, id);
        BSB_EXPORT_ptr(fieldBSB, json, len);
        BSB_EXPORT_u08(fieldBSB, '\n');
        return;
    }

    char key[300];
    int  key_len = arkime_snprintf_len(key, sizeof(key), "/%s%s/_doc/%s%s", config.prefix, table, id, strcmp(table, "files") == 0 ? "?refresh=true" : "");
    arkime_db_es_send(key, key_len, json, len, sync, priority);
}
/******************************************************************************/
// Live captures use an external version so an out of order stats doc is rejected
LOCAL uint64_t esStatsVersion;
LOCAL void arkime_db_es_put_stats(const char *json, int len, gboolean sync, int priority)
{
    char key[300];
    int  key_len;
    if (config.pcapReadOffline) {
        key_len = arkime_snprintf_len(key, sizeof(key), "/%sstats/_doc/%s", config.prefix, config.nodeName);
    } else {
        esStatsVersion++;
        key_len = arkime_snprintf_len(key, sizeof(key), "/%sstats/_doc/%s?version_type=external&version=%" PRIu64, config.prefix, config.nodeName, esStatsVersion);
    }
    arkime_db_es_send(key, key_len, json, len, sync, priority);
}
/******************************************************************************/
LOCAL void arkime_db_es_update(const char *table, const char *id, const char *json, int len, int priority)
{
    if (strcmp(table, "fields") == 0) {
        arkime_db_es_fieldbsb_make(len);
        BSB_EXPORT_sprintf(fieldBSB, "{\"update\": {\"_index\": \"%sfields\", \"_id\": \"%s\"}}\n{\"doc\": ", config.prefix, id);
        BSB_EXPORT_ptr(fieldBSB, json, len);
        BSB_EXPORT_cstr(fieldBSB, "}\n");
        return;
    }

    char key[300];
    int  key_len = arkime_snprintf_len(key, sizeof(key), "/%s%s/_update/%s", config.prefix, table, id);

    int   buf_len = len + 9;
    char *buf = arkime_http_get_buffer(buf_len);
    memcpy(buf, "{\"doc\": ", 8);
    memcpy(buf + 8, json, len);
    buf[buf_len - 1] = '}';
    arkime_http_schedule(esServer, "POST", key, key_len, buf, buf_len, NULL, priority, NULL, NULL);
}
/******************************************************************************/
LOCAL void arkime_db_es_delete(const char *table, const char *id)
{
    if (strcmp(table, "fields") == 0) {
        arkime_db_es_fieldbsb_make(0);
        BSB_EXPORT_sprintf(fieldBSB, "{\"delete\": {\"_index\": \"%sfields\", \"_id\": \"%s\"}}\n", config.prefix, id);
        return;
    }

    char key[300];
    int  key_len = arkime_snprintf_len(key, sizeof(key), "/%s%s/_doc/%s", config.prefix, table, id);
    arkime_http_schedule(esServer, "DELETE", key, key_len, NULL, 0, NULL, ARKIME_HTTP_PRIORITY_BEST, NULL, NULL);
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

    key_len = arkime_snprintf_len(key, sizeof(key), "/%ssequence/_doc/%s", config.prefix, name);
    int json_len = arkime_snprintf_len(json, ARKIME_HTTP_BUFFER_SIZE, "{}");
    arkime_http_schedule(esServer, "POST", key, key_len, json, json_len, NULL, ARKIME_HTTP_PRIORITY_BEST, arkime_db_es_sequence_cb, r);
}
/******************************************************************************/
LOCAL uint32_t arkime_db_es_sequence_sync(const char *name)
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
LOCAL void arkime_db_es_each(const char *table, ArkimeDbRow_cb func)
{
    size_t                 data_len;
    char                   key[100];
    int                    key_len;

    key_len = arkime_snprintf_len(key, sizeof(key), "/%s%s/_search?size=3000", config.prefix, table);
    uint8_t           *data = arkime_http_get(esServer, key, key_len, &data_len);

    if (!data) {
        LOGEXIT("ERROR - Couldn't download %s%s, database (%s) might be down or not initialized.", config.prefix, table, config.elasticsearch);
    }

    uint32_t           hits_len;
    const uint8_t     *hits = arkime_js0n_get(data, data_len, "hits", &hits_len);
    uint32_t           ahits_len = 0;
    const uint8_t     *ahits = hits ? arkime_js0n_get(hits, hits_len, "hits", &ahits_len) : NULL;

    if (!ahits) {
        LOGEXIT("ERROR - Couldn't download %s%s, database (%s) might be down or not initialized.", config.prefix, table, config.elasticsearch);
    }

    uint32_t out[2 * 8000];
    memset(out, 0, sizeof(out));
    if (js0n(ahits, ahits_len, out, sizeof(out)) != 0)
        LOG("WARNING - Couldn't parse all of the %s%s response, some may be missing", config.prefix, table);
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

    key_len = arkime_snprintf_len(key, sizeof(key), "/%sfiles/_search?rest_total_hits_as_int", config.prefix);

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

    uint8_t *data = arkime_http_send_sync(esServer, "POST", key, key_len, body, BSB_LENGTH(bsb), NULL, &data_len, NULL);
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
LOCAL int arkime_db_es_queue_length()
{
    return arkime_http_queue_length(esServer);
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
    .exit          = arkime_db_es_exit
};
/******************************************************************************/
// Uses the sessions ES server, a different ES clusterDbUrl only matters to viewer
LOCAL const ArkimeClusterDb_t *arkime_db_es_cluster_init(const char *UNUSED(url))
{
    // File numbers start at 101
    if (esServer) {
        char     key[200];
        int      key_len = arkime_snprintf_len(key, sizeof(key), "/%ssequence/_doc/fn-%s", config.prefix, config.nodeName);
        size_t   data_len;
        uint8_t *data = arkime_http_get(esServer, key, key_len, &data_len);
        if (!data)
            LOGEXIT("ERROR - Couldn't fetch sequence: no data returned - %.*s", key_len, key);

        uint32_t       found_len;
        const uint8_t *found = arkime_js0n_get(data, data_len, "found", &found_len);
        if (found && (found_len < 4 || memcmp("true", found, 4) != 0)) {
            free(data);
            key_len = arkime_snprintf_len(key, sizeof(key), "/%ssequence/_doc/fn-%s?version_type=external&version=100", config.prefix, config.nodeName);
            data = arkime_http_send_sync(esServer, "POST", key, key_len, "{}", 2, NULL, NULL, NULL);
        }
        if (data)
            free(data);
    }

    if (esServer && !config.pcapReadOffline && !config.dryRun) {
        char     key[300];
        int      key_len = arkime_snprintf_len(key, sizeof(key), "/%sstats/_doc/%s?_source=false", config.prefix, config.nodeName);
        size_t   data_len;
        uint8_t *data = arkime_http_get(esServer, key, key_len, &data_len);
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
void arkime_db_es_init()
{
    const char *schemes[] = {"http", "https", "elasticsearch", "elasticsearchs", "opensearch", "opensearchs"};
    for (int i = 0; i < ARRAY_LEN(schemes); i++)
        arkime_db_cluster_register((char *)schemes[i], arkime_db_es_cluster_init);
}
