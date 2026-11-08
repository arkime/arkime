/******************************************************************************/
/* mongodb.c -- MongoDB writer for capture, a rootPlugins plugin. It can hold the
 *              sessions (sessionsDbUrl) and the cluster db (dbUrl: stats, dstats,
 *              files, fields and sequence), the same layout mongodb.js reads.
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <inttypes.h>
#include <mongoc/mongoc.h>
#include <arpa/inet.h>

extern ArkimeConfig_t config;

#define MG_MAX_DROPPABLE   500
// A files or fields write tries this many times, a second apart, before it is lost
#define MG_MAX_RETRIES     30
// A sessions bulk waits this many in the queue before new ones are dropped
#define MG_MAX_BULKS       200
// MIN_VERSION in mongodb.js
#define MG_MIN_MAJOR       9
#define MG_MIN_MINOR       0

enum {
    MG_OP_PUT,
    MG_OP_UPDATE,
    MG_OP_DELETE,
    MG_OP_SEQUENCE,
    MG_OP_BULK,
    MG_OP_EXIT
};

typedef struct {
    int              type;
    const char      *table;
    char            *id;
    char            *json;
    int              len;
    uint64_t         version;
    int              priority;
    int              tries;
    ArkimeSeqNum_cb  func;
    gpointer         uw;
} ArkimeMgOp_t;

typedef struct {
    mongoc_client_pool_t *pool;
    char                 *database;
    GAsyncQueue          *queue;
    GThread              *thread;
    int                   outstanding;
    volatile int          exiting;
} ArkimeMgDb_t;

LOCAL ArkimeMgDb_t           clusterMg;
LOCAL ArkimeMgDb_t           sessionsMg;
LOCAL uint64_t               mgDropped;
LOCAL uint64_t               mgLost;
LOCAL uint64_t               mgSessionsFailed;
LOCAL uint64_t               mgSessionsDropped;
LOCAL uint64_t               mgHealthMs;
LOCAL int                    mgInited;

// arkime_db_cluster_tables, each prefixed like the ES indices
LOCAL const ArkimeDbTable_t *mgLayout;
LOCAL char                 **mgTables;
// Live captures version the stats doc so an out of order one is skipped
LOCAL uint64_t               mgStatsVersion;
// The sessions collections that already have their indexes
LOCAL GHashTable            *mgSessionColls;

/******************************************************************************/
LOCAL void arkime_mongodb_init_library()
{
    if (mgInited++)
        return;
    mongoc_init();
}
/******************************************************************************/
// A fixed bson doc from json, made once and kept
LOCAL const bson_t *arkime_mongodb_const(const char *json)
{
    bson_error_t error;
    bson_t *doc = bson_new_from_json((const uint8_t *)json, -1, &error);
    if (!doc)
        LOGEXIT("ERROR - Bad MongoDB json %s: %s", json, error.message);
    return doc;
}
/******************************************************************************/
LOCAL gboolean arkime_mongodb_loopback(const char *host)
{
    return !host || !host[0] || host[0] == '/' || strcmp(host, "localhost") == 0 || strcmp(host, "::1") == 0 ||
           (strncmp(host, "127.", 4) == 0 && g_hostname_is_ip_address(host));
}
/******************************************************************************/
/* Like the OpenSearch/Elasticsearch connection, a remote server's certificate is
 * verified with caTrustFile or the system CAs, and --insecure still requires TLS.
 * tls settings in the url win, loopback and unix socket servers are left alone.
 */
LOCAL void arkime_mongodb_connect(ArkimeMgDb_t *mg, const char *url)
{
    bson_error_t error;
    mongoc_uri_t *uri = mongoc_uri_new_with_error(url, &error);
    if (!uri)
        CONFIGEXIT("Couldn't parse the MongoDB url: %s", error.message);

    const char *database = mongoc_uri_get_database(uri);
    if (!database || !database[0])
        CONFIGEXIT("A mongodb:// url needs a database, mongodb://user:pass@host/database");
    mg->database = g_strdup(database);

    const bson_t *options = mongoc_uri_get_options(uri);
    gboolean tlsSet = FALSE;
    bson_iter_t iter;
    if (bson_iter_init(&iter, options)) {
        while (bson_iter_next(&iter)) {
            if (g_ascii_strncasecmp(bson_iter_key(&iter), "tls", 3) == 0 || g_ascii_strncasecmp(bson_iter_key(&iter), "ssl", 3) == 0)
                tlsSet = TRUE;
        }
    }

    gboolean remote = mongoc_uri_get_srv_hostname(uri) != NULL;
    for (const mongoc_host_list_t *h = mongoc_uri_get_hosts(uri); h; h = h->next) {
        if (h->family != AF_UNIX && !arkime_mongodb_loopback(h->host))
            remote = TRUE;
    }
    if (remote && !tlsSet) {
        mongoc_uri_set_option_as_bool(uri, MONGOC_URI_TLS, true);
        if (config.insecure) {
            mongoc_uri_set_option_as_bool(uri, MONGOC_URI_TLSALLOWINVALIDCERTIFICATES, true);
            mongoc_uri_set_option_as_bool(uri, MONGOC_URI_TLSALLOWINVALIDHOSTNAMES, true);
        } else if (config.caTrustFile) {
            mongoc_uri_set_option_as_utf8(uri, MONGOC_URI_TLSCAFILE, config.caTrustFile);
        }
    }
    mongoc_uri_set_option_as_int32(uri, MONGOC_URI_CONNECTTIMEOUTMS, 10000);
    mongoc_uri_set_option_as_int32(uri, MONGOC_URI_SERVERSELECTIONTIMEOUTMS, 10000);
    mongoc_uri_set_appname(uri, "arkime-capture");

    mg->pool = mongoc_client_pool_new_with_error(uri, &error);
    mongoc_uri_destroy(uri);
    if (!mg->pool)
        CONFIGEXIT("Couldn't create the MongoDB client: %s", error.message);
    mongoc_client_pool_set_error_api(mg->pool, MONGOC_ERROR_API_VERSION_2);

    // Checks the server, and its version, before anything is queued for it
    mongoc_client_t *client = mongoc_client_pool_pop(mg->pool);
    bson_t reply;
    bson_t *buildInfo = BCON_NEW("buildInfo", BCON_INT32(1));
    const gboolean ok = mongoc_client_command_simple(client, mg->database, buildInfo, NULL, &reply, &error);
    bson_destroy(buildInfo);
    if (!ok)
        LOGEXIT("ERROR - Couldn't connect to MongoDB: %s", error.message);
    if (bson_iter_init_find(&iter, &reply, "version") && BSON_ITER_HOLDS_UTF8(&iter)) {
        int major = 0, minor = 0;
        const char *version = bson_iter_utf8(&iter, NULL);
        sscanf(version, "%d.%d", &major, &minor);
        if (major < MG_MIN_MAJOR || (major == MG_MIN_MAJOR && minor < MG_MIN_MINOR))
            LOGEXIT("ERROR - MongoDB %s is too old, mongodb:// needs %d.%d or newer", version, MG_MIN_MAJOR, MG_MIN_MINOR);
    }
    bson_destroy(&reply);
    mongoc_client_pool_push(mg->pool, client);
}
/******************************************************************************/
LOCAL void arkime_mongodb_log_error(const char *what, const bson_error_t *error)
{
    static time_t lastLog;
    const time_t now = time(NULL);
    if (now - lastLog < 10 && !config.debug)
        return;
    lastLog = now;
    LOG("ERROR - MongoDB %s: %s", what, error->message);
}
/******************************************************************************/
// A server that can't be reached, not one that refused the write
LOCAL gboolean arkime_mongodb_down(const bson_error_t *error)
{
    return error->domain == MONGOC_ERROR_SERVER_SELECTION || error->domain == MONGOC_ERROR_STREAM ||
           (error->domain == MONGOC_ERROR_CLIENT && error->code == MONGOC_ERROR_CLIENT_AUTHENTICATE);
}
/******************************************************************************/
LOCAL int arkime_mongodb_table_num(const char *name)
{
    for (int i = 0; mgLayout[i].name; i++) {
        if (strcmp(name, mgLayout[i].name) == 0)
            return i;
    }
    LOGEXIT("ERROR - Unknown MongoDB table %s", name);
}
/******************************************************************************/
LOCAL bson_t *arkime_mongodb_json(const char *json, int len, const char *what)
{
    bson_error_t error;
    bson_t *doc = bson_new_from_json((const uint8_t *)json, len, &error);
    if (!doc)
        LOG("ERROR - MongoDB %s isn't json: %s %.*s", what, error.message, MIN(len, 200), json);
    return doc;
}
/******************************************************************************/
// An update pipeline from json, capture's doc goes in as a $literal so its own $ strings aren't
// expressions. A doc keyed "0" is how bson stores the one stage array.
LOCAL gboolean arkime_mongodb_update_pipeline(mongoc_client_t *client, const ArkimeMgOp_t *op, const char *pipeline, gboolean upsert, bson_error_t *error)
{
    static const bson_t *upsertOpts;
    if (!upsertOpts)
        upsertOpts = arkime_mongodb_const("{\"upsert\": true}");

    bson_t *update = arkime_mongodb_json(pipeline, -1, op->table);
    if (!update)
        return TRUE;
    mongoc_collection_t *coll = mongoc_client_get_collection(client, clusterMg.database, mgTables[arkime_mongodb_table_num(op->table)]);
    bson_t *selector = BCON_NEW("_id", BCON_UTF8(op->id));
    const gboolean ok = mongoc_collection_update_one(coll, selector, update, upsert ? upsertOpts : NULL, NULL, error);
    bson_destroy(selector);
    bson_destroy(update);
    mongoc_collection_destroy(coll);
    return ok;
}
/******************************************************************************/
// The doc as stored, { _id, d, v }, the same as stored() in mongodb.js. A version only replaces an
// older one, which keeps out of order stats out like ES external versioning.
LOCAL gboolean arkime_mongodb_do_put(mongoc_client_t *client, const ArkimeMgOp_t *op, bson_error_t *error)
{
    char *pipeline;
    if (op->version) {
        pipeline = g_strdup_printf("{\"0\": {\"$set\": {"
                                   "\"d\": {\"$cond\": [{\"$lt\": [{\"$ifNull\": [\"$v\", 0]}, {\"$numberLong\": \"%" PRIu64 "\"}]}, {\"$literal\": %.*s}, \"$d\"]}, "
                                   "\"v\": {\"$max\": [{\"$ifNull\": [\"$v\", 0]}, {\"$numberLong\": \"%" PRIu64 "\"}]}}}}",
                                   op->version, op->len, op->json, op->version);
    } else {
        pipeline = g_strdup_printf("{\"0\": {\"$set\": {\"d\": {\"$literal\": %.*s}, \"v\": {\"$add\": [{\"$ifNull\": [\"$v\", 0]}, 1]}}}}", op->len, op->json);
    }
    const gboolean ok = arkime_mongodb_update_pipeline(client, op, pipeline, TRUE, error);
    g_free(pipeline);
    return ok;
}
/******************************************************************************/
// Top level fields replace like the viewer's update, d = $mergeObjects(d, fields)
LOCAL gboolean arkime_mongodb_do_update(mongoc_client_t *client, const ArkimeMgOp_t *op, bson_error_t *error)
{
    char *pipeline = g_strdup_printf("{\"0\": {\"$set\": {\"d\": {\"$mergeObjects\": [\"$d\", {\"$literal\": %.*s}]}, \"v\": {\"$add\": [\"$v\", 1]}}}}", op->len, op->json);
    const gboolean ok = arkime_mongodb_update_pipeline(client, op, pipeline, FALSE, error);
    g_free(pipeline);
    return ok;
}
/******************************************************************************/
LOCAL gboolean arkime_mongodb_do_delete(mongoc_client_t *client, const ArkimeMgOp_t *op, bson_error_t *error)
{
    mongoc_collection_t *coll = mongoc_client_get_collection(client, clusterMg.database, mgTables[arkime_mongodb_table_num(op->table)]);
    bson_t selector = BSON_INITIALIZER;
    BSON_APPEND_UTF8(&selector, "_id", op->id);
    const gboolean ok = mongoc_collection_delete_one(coll, &selector, NULL, NULL, error);
    bson_destroy(&selector);
    mongoc_collection_destroy(coll);
    return ok;
}
/******************************************************************************/
// Returns the new value or 0 on failure
LOCAL uint32_t arkime_mongodb_next_sequence(mongoc_client_t *client, const char *name, bson_error_t *error)
{
    mongoc_collection_t *coll = mongoc_client_get_collection(client, clusterMg.database, mgTables[arkime_mongodb_table_num("sequence")]);
    bson_t selector = BSON_INITIALIZER;
    BSON_APPEND_UTF8(&selector, "_id", name);

    mongoc_find_and_modify_opts_t *opts = mongoc_find_and_modify_opts_new();
    static const bson_t *inc;
    if (!inc)
        inc = arkime_mongodb_const("{\"$inc\": {\"d.seq\": 1, \"v\": 1}}");
    mongoc_find_and_modify_opts_set_update(opts, inc);
    mongoc_find_and_modify_opts_set_flags(opts, MONGOC_FIND_AND_MODIFY_UPSERT | MONGOC_FIND_AND_MODIFY_RETURN_NEW);

    bson_t reply;
    uint32_t seq = 0;
    if (mongoc_collection_find_and_modify_with_opts(coll, &selector, opts, &reply, error)) {
        bson_iter_t iter;
        if (bson_iter_init(&iter, &reply) && bson_iter_find_descendant(&iter, "value.d.seq", &iter))
            seq = (uint32_t)bson_iter_as_int64(&iter);
    }
    bson_destroy(&reply);
    mongoc_find_and_modify_opts_destroy(opts);
    bson_destroy(&selector);
    mongoc_collection_destroy(coll);
    return seq;
}
/******************************************************************************/
// The session collections are made the first time capture writes to one: a wildcard index searches
// any field like ES does, and firstPacket and lastPacket pick the time range and sort with _id breaking ties
LOCAL void arkime_mongodb_session_indexes(mongoc_client_t *client, const char *collName)
{
    if (g_hash_table_contains(mgSessionColls, collName))
        return;

    mongoc_collection_t *coll = mongoc_client_get_collection(client, sessionsMg.database, collName);
    bson_error_t error;
    static const bson_t *indexes;
    if (!indexes)
        indexes = arkime_mongodb_const("{\"0\": {\"key\": {\"$**\": 1}, \"name\": \"all\", \"wildcardProjection\": {\"_id\": 0}},"
                                     " \"1\": {\"key\": {\"lastPacket\": 1, \"_id\": 1}, \"name\": \"lastPacketId\"},"
                                     " \"2\": {\"key\": {\"firstPacket\": 1, \"_id\": 1}, \"name\": \"firstPacketId\"}}");
    bson_t *cmd = BCON_NEW("createIndexes", BCON_UTF8(collName));
    BSON_APPEND_ARRAY(cmd, "indexes", indexes);
    if (mongoc_collection_write_command_with_opts(coll, cmd, NULL, NULL, &error))
        g_hash_table_add(mgSessionColls, g_strdup(collName));
    else
        arkime_mongodb_log_error("session indexes", &error);
    bson_destroy(cmd);
    mongoc_collection_destroy(coll);
}
/******************************************************************************/
LOCAL void arkime_mongodb_ip(bson_array_builder_t *ips, const char *path, const uint8_t *buf, int len)
{
    bson_t child;
    bson_array_builder_append_document_begin(ips, &child);
    BSON_APPEND_UTF8(&child, "f", path);
    BSON_APPEND_BINARY(&child, "a", BSON_SUBTYPE_BINARY, buf, len);
    bson_array_builder_append_document_end(ips, &child);
}
/******************************************************************************/
// Adds { f: dotted path, a: the ip's bytes } for each ip string, the _ip list mongodb.sessions.js matches
// ip fields with. path is where iter is, an array's positions aren't part of it like ES flattens them.
LOCAL void arkime_mongodb_ips(bson_iter_t *iter, const char *path, gboolean inArray, bson_array_builder_t *ips)
{
    while (bson_iter_next(iter)) {
        char *child = NULL;
        const char *p = path;
        if (!inArray)
            p = path[0] ? (child = g_strdup_printf("%s.%s", path, bson_iter_key(iter))) : bson_iter_key(iter);

        if (BSON_ITER_HOLDS_UTF8(iter)) {
            uint32_t len;
            const char *str = bson_iter_utf8(iter, &len);
            // An ipv4 is stored as ::ffff:a.b.c.d like mongodb.js ipBytes
            uint8_t buf[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
            if (len >= 2 && len <= 45 && (strchr(str, '.') || strchr(str, ':'))) {
                if (inet_pton(AF_INET, str, buf + 12) == 1 || inet_pton(AF_INET6, str, buf) == 1)
                    arkime_mongodb_ip(ips, p, buf, 16);
            }
        } else if (BSON_ITER_HOLDS_DOCUMENT(iter) || BSON_ITER_HOLDS_ARRAY(iter)) {
            bson_iter_t sub;
            if (bson_iter_recurse(iter, &sub))
                arkime_mongodb_ips(&sub, p, BSON_ITER_HOLDS_ARRAY(iter), ips);
        }
        g_free(child);
    }
}
/******************************************************************************/
// One line per session, each with the index ES would use and its _id. Each goes in a collection named
// after its index with the rest of the doc as the stored doc, replacing one with the same _id like ES.
LOCAL void arkime_mongodb_do_bulk(mongoc_client_t *client, const ArkimeMgOp_t *op)
{
    GHashTable *bulks = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    int docs = 0;
    const char *line = op->json;
    const char *end = op->json + op->len;
    while (line < end) {
        const char *nl = memchr(line, '\n', end - line);
        const int len = (nl ? nl : end) - line;
        if (len > 1) {
            docs++;
            bson_t *doc = arkime_mongodb_json(line, len, "session");
            bson_iter_t iter;
            if (doc && bson_iter_init_find(&iter, doc, "index") && BSON_ITER_HOLDS_UTF8(&iter)) {
                char *collName = g_strdup(bson_iter_utf8(&iter, NULL));
                bson_t stored = BSON_INITIALIZER;
                bson_t selector = BSON_INITIALIZER;
                bson_iter_init(&iter, doc);
                while (bson_iter_next(&iter)) {
                    if (strcmp(bson_iter_key(&iter), "index") == 0)
                        continue;
                    if (strcmp(bson_iter_key(&iter), "_id") == 0)
                        bson_append_iter(&selector, "_id", 3, &iter);
                    bson_append_iter(&stored, NULL, 0, &iter);
                }
                bson_array_builder_t *ips;
                bson_append_array_builder_begin(&stored, "_ip", 3, &ips);
                bson_iter_init(&iter, doc);
                arkime_mongodb_ips(&iter, "", FALSE, ips);
                bson_append_array_builder_end(&stored, ips);

                mongoc_bulk_operation_t *bulk = g_hash_table_lookup(bulks, collName);
                if (!bulk) {
                    arkime_mongodb_session_indexes(client, collName);
                    mongoc_collection_t *coll = mongoc_client_get_collection(client, sessionsMg.database, collName);
                    static const bson_t *unordered;
                    if (!unordered)
                        unordered = arkime_mongodb_const("{\"ordered\": false}");
                    bulk = mongoc_collection_create_bulk_operation_with_opts(coll, unordered);
                    mongoc_collection_destroy(coll);
                    g_hash_table_insert(bulks, collName, bulk);
                } else {
                    g_free(collName);
                }
                static const bson_t *upsert;
                if (!upsert)
                    upsert = arkime_mongodb_const("{\"upsert\": true}");
                mongoc_bulk_operation_replace_one_with_opts(bulk, &selector, &stored, upsert, NULL);
                bson_destroy(&selector);
                bson_destroy(&stored);
            } else {
                ARKIME_THREAD_INCR(mgSessionsFailed);
            }
            if (doc)
                bson_destroy(doc);
        }
        line += len + 1;
    }

    GHashTableIter hiter;
    gpointer key, value;
    g_hash_table_iter_init(&hiter, bulks);
    while (g_hash_table_iter_next(&hiter, &key, &value)) {
        mongoc_bulk_operation_t *bulk = value;
        bson_t reply;
        bson_error_t error;
        if (!mongoc_bulk_operation_execute(bulk, &reply, &error)) {
            bson_iter_t iter;
            int64_t failed = 0;
            if (bson_iter_init_find(&iter, &reply, "writeErrors") && BSON_ITER_HOLDS_ARRAY(&iter)) {
                bson_iter_t errs;
                bson_iter_recurse(&iter, &errs);
                while (bson_iter_next(&errs))
                    failed++;
            }
            ARKIME_THREAD_INCR_NUM(mgSessionsFailed, failed ? failed : docs);
            arkime_mongodb_log_error((char *)key, &error);
        }
        bson_destroy(&reply);
        mongoc_bulk_operation_destroy(bulk);
    }
    g_hash_table_destroy(bulks);
}
/******************************************************************************/
LOCAL void arkime_mongodb_op_free(ArkimeMgOp_t *op)
{
    g_free(op->id);
    if (op->type == MG_OP_BULK)
        arkime_http_free_buffer(op->json);
    else
        g_free(op->json);
    ARKIME_TYPE_FREE(ArkimeMgOp_t, op);
}
/******************************************************************************/
LOCAL void *arkime_mongodb_thread(void *mgp)
{
    ArkimeMgDb_t *mg = mgp;
    while (1) {
        ArkimeMgOp_t *op = g_async_queue_pop(mg->queue);
        if (op->type == MG_OP_EXIT) {
            arkime_mongodb_op_free(op);
            break;
        }

        uint32_t seq = 0;
        // A files or fields write waits in place for MongoDB to come back, so a file's create still
        // lands before its update. What queues up behind it is droppable, or waits too.
        while (1) {
            mongoc_client_t *client = mongoc_client_pool_pop(mg->pool);
            bson_error_t error = {0};
            gboolean ok = TRUE;
            switch (op->type) {
            case MG_OP_PUT:
                ok = arkime_mongodb_do_put(client, op, &error);
                break;
            case MG_OP_UPDATE:
                ok = arkime_mongodb_do_update(client, op, &error);
                break;
            case MG_OP_DELETE:
                ok = arkime_mongodb_do_delete(client, op, &error);
                break;
            case MG_OP_SEQUENCE:
                seq = arkime_mongodb_next_sequence(client, op->id, &error);
                ok = seq != 0;
                break;
            case MG_OP_BULK: {
                const uint64_t start = g_get_monotonic_time();
                arkime_mongodb_do_bulk(client, op);
                mgHealthMs = (g_get_monotonic_time() - start) / 1000;
                break;
            }
            }
            mongoc_client_pool_push(mg->pool, client);
            if (ok)
                break;

            arkime_mongodb_log_error(op->table ? op->table : "write", &error);
            // A refused write isn't retried, only a server that is down
            if (!arkime_mongodb_down(&error) || op->type == MG_OP_SEQUENCE || op->priority == ARKIME_HTTP_PRIORITY_DROPPABLE || mg->exiting)
                break;
            if (++op->tries >= MG_MAX_RETRIES) {
                mgLost++;
                LOG("ERROR - MongoDB %s write of %s lost after %d tries", op->table, op->id, op->tries);
                break;
            }
            sleep(1);
        }

        // Callbacks run off this thread's client, they may take their own locks
        if (op->type == MG_OP_SEQUENCE && seq && op->func)
            op->func(seq, op->uw);

        arkime_mongodb_op_free(op);
        ARKIME_THREAD_DECR(mg->outstanding);
    }
    return NULL;
}
/******************************************************************************/
LOCAL void arkime_mongodb_push(ArkimeMgDb_t *mg, ArkimeMgOp_t *op)
{
    ARKIME_THREAD_INCR(mg->outstanding);
    g_async_queue_push(mg->queue, op);
}
/******************************************************************************/
LOCAL ArkimeMgOp_t *arkime_mongodb_op(int type, const char *table, const char *id, const char *json, int len, uint64_t version, int priority)
{
    ArkimeMgOp_t *op = ARKIME_TYPE_ALLOC0(ArkimeMgOp_t);
    op->type = type;
    op->table = table;
    op->id = g_strdup(id);
    if (json) {
        op->json = g_strndup(json, len);
        op->len = len;
    }
    op->version = version;
    op->priority = priority;
    return op;
}
/******************************************************************************/
LOCAL void arkime_mongodb_queue(int type, const char *table, const char *id, const char *json, int len, uint64_t version, int priority)
{
    if (priority == ARKIME_HTTP_PRIORITY_DROPPABLE && clusterMg.outstanding > MG_MAX_DROPPABLE) {
        mgDropped++;
        return;
    }
    arkime_mongodb_push(&clusterMg, arkime_mongodb_op(type, table, id, json, len, version, priority));
}
/******************************************************************************/
LOCAL void arkime_mongodb_put_version(const char *table, const char *id, const char *json, int len, uint64_t version, gboolean sync, int priority)
{
    if (!sync) {
        arkime_mongodb_queue(MG_OP_PUT, table, id, json, len, version, priority);
        return;
    }

    ArkimeMgOp_t *op = arkime_mongodb_op(MG_OP_PUT, table, id, json, len, version, priority);
    mongoc_client_t *client = mongoc_client_pool_pop(clusterMg.pool);
    bson_error_t error;
    if (!arkime_mongodb_do_put(client, op, &error))
        arkime_mongodb_log_error(table, &error);
    mongoc_client_pool_push(clusterMg.pool, client);
    arkime_mongodb_op_free(op);
}
/******************************************************************************/
LOCAL void arkime_mongodb_put(const char *table, const char *id, const char *json, int len, gboolean sync, int priority)
{
    arkime_mongodb_put_version(table, id, json, len, 0, sync, priority);
}
/******************************************************************************/
LOCAL void arkime_mongodb_put_stats(const char *json, int len, gboolean sync, int priority)
{
    arkime_mongodb_put_version("stats", config.nodeName, json, len, config.pcapReadOffline ? 0 : ++mgStatsVersion, sync, priority);
}
/******************************************************************************/
LOCAL void arkime_mongodb_update(const char *table, const char *id, const char *json, int len, int priority)
{
    arkime_mongodb_queue(MG_OP_UPDATE, table, id, json, len, 0, priority);
}
/******************************************************************************/
LOCAL void arkime_mongodb_delete(const char *table, const char *id)
{
    arkime_mongodb_queue(MG_OP_DELETE, table, id, NULL, 0, 0, ARKIME_HTTP_PRIORITY_BEST);
}
/******************************************************************************/
LOCAL void arkime_mongodb_sequence(const char *name, ArkimeSeqNum_cb func, gpointer uw)
{
    ArkimeMgOp_t *op = arkime_mongodb_op(MG_OP_SEQUENCE, "sequence", name, NULL, 0, 0, ARKIME_HTTP_PRIORITY_BEST);
    op->func = func;
    op->uw = uw;
    arkime_mongodb_push(&clusterMg, op);
}
/******************************************************************************/
LOCAL uint32_t arkime_mongodb_sequence_sync(const char *name)
{
    while (1) {
        mongoc_client_t *client = mongoc_client_pool_pop(clusterMg.pool);
        bson_error_t error;
        const uint32_t seq = arkime_mongodb_next_sequence(client, name, &error);
        mongoc_client_pool_push(clusterMg.pool, client);
        if (seq)
            return seq;
        LOG("ERROR - Couldn't fetch sequence %s from MongoDB, retrying: %s", name, error.message);
        sleep(1);
    }
}
/******************************************************************************/
LOCAL gboolean arkime_mongodb_each(const char *table, ArkimeDbRow_cb func)
{
    mongoc_client_t *client = mongoc_client_pool_pop(clusterMg.pool);
    mongoc_collection_t *coll = mongoc_client_get_collection(client, clusterMg.database, mgTables[arkime_mongodb_table_num(table)]);
    static const bson_t *all, *projection;
    if (!all) {
        all = arkime_mongodb_const("{}");
        projection = arkime_mongodb_const("{\"projection\": {\"d\": 1}}");
    }
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(coll, all, projection, NULL);
    const bson_t *row;
    while (mongoc_cursor_next(cursor, &row)) {
        bson_iter_t iter;
        if (!bson_iter_init_find(&iter, row, "_id") || !BSON_ITER_HOLDS_UTF8(&iter))
            continue;
        uint32_t idLen;
        const char *id = bson_iter_utf8(&iter, &idLen);
        if (!bson_iter_init_find(&iter, row, "d") || !BSON_ITER_HOLDS_DOCUMENT(&iter))
            continue;
        const uint8_t *data;
        uint32_t dataLen;
        bson_iter_document(&iter, &dataLen, &data);
        bson_t d;
        bson_init_static(&d, data, dataLen);
        size_t jsonLen;
        char *json = bson_as_relaxed_extended_json(&d, &jsonLen);
        func((const uint8_t *)id, idLen, (const uint8_t *)json, jsonLen);
        bson_free(json);
    }
    bson_error_t error;
    const gboolean ok = !mongoc_cursor_error(cursor, &error);
    if (!ok)
        LOG("ERROR - Couldn't load %s from MongoDB: %s", table, error.message);
    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(coll);
    mongoc_client_pool_push(clusterMg.pool, client);
    return ok;
}
/******************************************************************************/
LOCAL gboolean arkime_mongodb_file_exists(const char *node, const char *name, uint32_t *num)
{
    mongoc_client_t *client = mongoc_client_pool_pop(clusterMg.pool);
    mongoc_collection_t *coll = mongoc_client_get_collection(client, clusterMg.database, mgTables[arkime_mongodb_table_num("files")]);
    bson_t *filter = BCON_NEW("d.node", BCON_UTF8(node), "d.name", BCON_UTF8(name));
    static const bson_t *opts;
    if (!opts)
        opts = arkime_mongodb_const("{\"projection\": {\"d.num\": 1}, \"sort\": {\"d.num\": -1}, \"limit\": 1}");
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(coll, filter, opts, NULL);
    const bson_t *row;
    gboolean found = FALSE;
    if (mongoc_cursor_next(cursor, &row)) {
        bson_iter_t iter;
        if (bson_iter_init(&iter, row) && bson_iter_find_descendant(&iter, "d.num", &iter)) {
            found = TRUE;
            if (num)
                *num = (uint32_t)bson_iter_as_int64(&iter);
        }
    }
    mongoc_cursor_destroy(cursor);
    bson_destroy(filter);
    mongoc_collection_destroy(coll);
    mongoc_client_pool_push(clusterMg.pool, client);
    return found;
}
/******************************************************************************/
LOCAL int arkime_mongodb_queue_length()
{
    return clusterMg.queue ? clusterMg.outstanding : 0;
}
/******************************************************************************/
// Finishes everything queued before closing
LOCAL void arkime_mongodb_stop(ArkimeMgDb_t *mg)
{
    if (!mg->queue)
        return;
    mg->exiting = 1;
    ArkimeMgOp_t *op = ARKIME_TYPE_ALLOC0(ArkimeMgOp_t);
    op->type = MG_OP_EXIT;
    g_async_queue_push(mg->queue, op);
    g_thread_join(mg->thread);
    g_async_queue_unref(mg->queue);
    mg->queue = NULL;
    mongoc_client_pool_destroy(mg->pool);
    mg->pool = NULL;
}
/******************************************************************************/
LOCAL void arkime_mongodb_exit()
{
    arkime_mongodb_stop(&clusterMg);
    if (mgDropped)
        LOG("MongoDB dropped %" PRIu64 " droppable writes", mgDropped);
    if (mgLost)
        LOG("MongoDB lost %" PRIu64 " writes while it was down", mgLost);
    if (!sessionsMg.queue && --mgInited == 0)
        mongoc_cleanup();
}
/******************************************************************************/
LOCAL const ArkimeClusterDb_t mgClusterDb = {
    .put           = arkime_mongodb_put,
    .put_stats     = arkime_mongodb_put_stats,
    .update        = arkime_mongodb_update,
    .del           = arkime_mongodb_delete,
    .sequence      = arkime_mongodb_sequence,
    .sequence_sync = arkime_mongodb_sequence_sync,
    .each          = arkime_mongodb_each,
    .file_exists   = arkime_mongodb_file_exists,
    .queue_length  = arkime_mongodb_queue_length,
    .exit          = arkime_mongodb_exit
};
/******************************************************************************/
// The indexes init() in mongodb.js makes for a collection's indexed fields
LOCAL void arkime_mongodb_indexes(mongoc_client_t *client, int n)
{
    GString *json = g_string_new("{");
    int i = 0;
    for (const ArkimeDbField_t *field = mgLayout[n].fields; field->field; field++) {
        if (!field->indexed)
            continue;
        g_string_append_printf(json, "%s\"%d\": {\"key\": {\"d.%s\": 1}, \"name\": \"f_%s\"}", i ? ", " : "", i, field->field, field->field);
        i++;
        if (field->indexed == ARKIME_DB_INDEXED_SORT) {
            g_string_append_printf(json, ", \"%d\": {\"key\": {\"_n.%s\": 1, \"d.%s\": 1}, \"name\": \"f_%s_nl\"}", i, field->field, field->field, field->field);
            i++;
        }
    }
    g_string_append(json, "}");
    if (i) {
        bson_t *indexes = bson_new_from_json((uint8_t *)json->str, json->len, NULL);
        bson_t *cmd = BCON_NEW("createIndexes", BCON_UTF8(mgTables[n]));
        BSON_APPEND_ARRAY(cmd, "indexes", indexes);
        mongoc_collection_t *coll = mongoc_client_get_collection(client, clusterMg.database, mgTables[n]);
        bson_error_t error;
        if (!mongoc_collection_write_command_with_opts(coll, cmd, NULL, NULL, &error))
            LOGEXIT("ERROR - Couldn't create the MongoDB %s indexes: %s", mgTables[n], error.message);
        mongoc_collection_destroy(coll);
        bson_destroy(cmd);
        bson_destroy(indexes);
    }
    g_string_free(json, TRUE);
}
/******************************************************************************/
LOCAL void arkime_mongodb_start(ArkimeMgDb_t *mg, const char *name)
{
    mg->queue = g_async_queue_new();
    mg->thread = g_thread_new(name, &arkime_mongodb_thread, mg);
}
/******************************************************************************/
LOCAL const ArkimeClusterDb_t *arkime_mongodb_cluster_init(const char *url)
{
    arkime_mongodb_init_library();
    mgLayout = arkime_db_cluster_tables();
    int ntables = 0;
    while (mgLayout[ntables].name)
        ntables++;
    mgTables = g_new0(char *, ntables + 1);
    for (int i = 0; i < ntables; i++) {
        mgTables[i] = g_strdup_printf("%s%s", config.clusterPrefix, mgLayout[i].name);
    }

    arkime_mongodb_connect(&clusterMg, url);
    mongoc_client_t *client = mongoc_client_pool_pop(clusterMg.pool);
    for (int i = 0; i < ntables; i++) {
        arkime_mongodb_indexes(client, i);
    }

    // File numbers start at 101
    char fn[200];
    snprintf(fn, sizeof(fn), "fn-%s", config.nodeName);
    mongoc_collection_t *coll = mongoc_client_get_collection(client, clusterMg.database, mgTables[arkime_mongodb_table_num("sequence")]);
    bson_t *selector = BCON_NEW("_id", BCON_UTF8(fn));
    bson_error_t error;
    if (!mongoc_collection_update_one(coll, selector, arkime_mongodb_const("{\"$setOnInsert\": {\"d\": {\"seq\": 100}, \"v\": 1}}"),
                                      arkime_mongodb_const("{\"upsert\": true}"), NULL, &error))
        LOGEXIT("ERROR - Couldn't create the %s sequence in MongoDB: %s", fn, error.message);
    bson_destroy(selector);
    mongoc_collection_destroy(coll);

    if (!config.pcapReadOffline && !config.dryRun) {
        coll = mongoc_client_get_collection(client, clusterMg.database, mgTables[arkime_mongodb_table_num("stats")]);
        selector = BCON_NEW("_id", BCON_UTF8(config.nodeName));
        mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(coll, selector, arkime_mongodb_const("{\"projection\": {\"v\": 1}}"), NULL);
        const bson_t *row;
        bson_iter_t iter;
        if (mongoc_cursor_next(cursor, &row) && bson_iter_init_find(&iter, row, "v"))
            mgStatsVersion = (uint64_t)bson_iter_as_int64(&iter);
        if (mongoc_cursor_error(cursor, &error))
            LOGEXIT("ERROR - Couldn't fetch the %s stats from MongoDB: %s", config.nodeName, error.message);
        mongoc_cursor_destroy(cursor);
        bson_destroy(selector);
        mongoc_collection_destroy(coll);
    }
    mongoc_client_pool_push(clusterMg.pool, client);

    arkime_mongodb_start(&clusterMg, "arkime-mongo");
    if (config.debug)
        LOG("Using MongoDB dbUrl for stats, dstats, files, fields and sequence");
    return &mgClusterDb;
}
/******************************************************************************/
LOCAL void arkime_mongodb_send_bulk(char *json, int len)
{
    if (sessionsMg.outstanding > MG_MAX_BULKS) {
        uint64_t docs = 0;
        for (int i = 0; i < len; i++) {
            if (json[i] == '\n')
                docs++;
        }
        ARKIME_THREAD_INCR_NUM(mgSessionsDropped, docs);
        arkime_http_free_buffer(json);
        return;
    }
    ArkimeMgOp_t *op = ARKIME_TYPE_ALLOC0(ArkimeMgOp_t);
    op->type = MG_OP_BULK;
    op->table = "sessions";
    op->json = json;
    op->len = len;
    arkime_mongodb_push(&sessionsMg, op);
}
/******************************************************************************/
LOCAL int arkime_mongodb_sessions_queue_length()
{
    return sessionsMg.queue ? sessionsMg.outstanding : 0;
}
/******************************************************************************/
LOCAL uint64_t arkime_mongodb_sessions_dropped()
{
    return mgSessionsDropped + mgSessionsFailed;
}
/******************************************************************************/
LOCAL uint64_t arkime_mongodb_health_ms()
{
    return mgHealthMs;
}
/******************************************************************************/
LOCAL void arkime_mongodb_sessions_exit()
{
    arkime_mongodb_stop(&sessionsMg);
    if (mgSessionsDropped || mgSessionsFailed)
        LOG("MongoDB sessions dropped %" PRIu64 " failed %" PRIu64, mgSessionsDropped, mgSessionsFailed);
    if (!clusterMg.queue && --mgInited == 0)
        mongoc_cleanup();
}
/******************************************************************************/
LOCAL const ArkimeSessionsDb_t mgSessionsDb = {
    .health_ms    = arkime_mongodb_health_ms,
    .queue_length = arkime_mongodb_sessions_queue_length,
    .dropped      = arkime_mongodb_sessions_dropped,
    .exit         = arkime_mongodb_sessions_exit
};
/******************************************************************************/
LOCAL const ArkimeSessionsDb_t *arkime_mongodb_sessions_init(const char *url)
{
    arkime_mongodb_init_library();
    arkime_mongodb_connect(&sessionsMg, url);
    mgSessionColls = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    arkime_mongodb_start(&sessionsMg, "arkime-mongo-sessions");

    // One json doc per line with its index and _id, and the *Tokens fields text searches use since
    // MongoDB has no analyzer for them
    arkime_db_set_send_bulk2(arkime_mongodb_send_bulk, FALSE, TRUE, 0xffff);
    arkime_db_set_tokens_enabled(TRUE);

    LOG("MongoDB sessions enabled: %s", sessionsMg.database);
    return &mgSessionsDb;
}
/******************************************************************************/
void arkime_plugin_init()
{
    arkime_db_cluster_register("mongodb", arkime_mongodb_cluster_init);
    arkime_db_cluster_register("mongodb+srv", arkime_mongodb_cluster_init);
    arkime_db_sessions_register("mongodb", arkime_mongodb_sessions_init);
    arkime_db_sessions_register("mongodb+srv", arkime_mongodb_sessions_init);
}
