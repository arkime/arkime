/******************************************************************************/
/* db.pg.c -- Postgres clusterDbUrl writer for capture's stats, dstats, files,
 *            fields and sequence, sessions still go to ES or ClickHouse
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include "arkimeconfig.h"
#include <inttypes.h>

extern ArkimeConfig_t config;

#ifdef HAVE_LIBPQ
#include <libpq-fe.h>

#define PG_MAX_DROPPABLE   500

enum {
    PG_OP_PUT,
    PG_OP_UPDATE,
    PG_OP_DELETE,
    PG_OP_SEQUENCE,
    PG_OP_EXIT
};

typedef struct {
    int              type;
    const char      *table;
    char            *id;
    char            *json;
    uint64_t         version;
    ArkimeSeqNum_cb  func;
    gpointer         uw;
} ArkimeDbPgOp_t;

LOCAL PGconn       *pgConn;
LOCAL ARKIME_LOCK_DEFINE(pgConn);
LOCAL GAsyncQueue  *pgQueue;
LOCAL GThread      *pgThread;
LOCAL int           pgOutstanding;
LOCAL uint64_t      pgDropped;

// Same layout as the viewer's ArkimeDb Postgres collections, prefixed like the ES indices
LOCAL const char   *pgTableNames[] = {"stats", "dstats", "files", "fields", "sequence", NULL};
// Same generated columns and indexes as COLLECTION_OPTIONS in viewer/db.cluster.js, desc ones are also sorted descending
LOCAL const struct {
    const char *table;
    const char *field;
    gboolean    desc;
} pgIndexed[] = {
    {"files", "node", FALSE},
    {"files", "name", FALSE},
    {"files", "first", TRUE},
    {"files", "num", TRUE},
    {"files", "locked", FALSE},
    {NULL, NULL, FALSE}
};
LOCAL char         *pgTables[6];

// stats and dstats keep each field of the stats doc in its own column, the same list and types as
// STATS_FIELDS in viewer/db.cluster.js. A new field goes in both, its column is added at startup.
LOCAL const char   *pgTypedNames[] = {"stats", "dstats", NULL};
LOCAL const struct {
    const char *field;
    const char *type;
} pgStatsColumns[] = {
    {"ver", "text"}, {"nodeName", "text"}, {"hostname", "text"}, {"interval", "smallint"}, {"currentTime", "bigint"},
    {"freeSpaceP", "real"}, {"memoryP", "real"}, {"usedSpaceM", "bigint"}, {"freeSpaceM", "bigint"}, {"monitoring", "bigint"},
    {"memory", "bigint"}, {"cpu", "bigint"}, {"diskQueue", "bigint"}, {"esQueue", "bigint"}, {"packetQueue", "bigint"},
    {"fragsQueue", "bigint"}, {"frags", "bigint"}, {"needSave", "bigint"}, {"closeQueue", "bigint"}, {"tcpSessions", "bigint"},
    {"udpSessions", "bigint"}, {"icmpSessions", "bigint"}, {"sctpSessions", "bigint"}, {"espSessions", "bigint"},
    {"otherSessions", "bigint"}, {"deltaPackets", "bigint"}, {"deltaBytes", "bigint"}, {"deltaWrittenBytes", "bigint"},
    {"deltaUnwrittenBytes", "bigint"}, {"deltaSessions", "bigint"}, {"deltaSessionBytes", "bigint"}, {"deltaDropped", "bigint"},
    {"deltaFragsDropped", "bigint"}, {"deltaOverloadDropped", "bigint"}, {"deltaESDropped", "bigint"},
    {"deltaDupDropped", "bigint"}, {"esHealthMS", "bigint"}, {"deltaMS", "bigint"}, {"startTime", "bigint"},
    {NULL, NULL}
};
// dstats is searched by node, interval and time
LOCAL const char   *pgDstatsIndexed[] = {"nodeName", "interval", "currentTime", NULL};
// Per typed table: the upsert and the versioned upsert
LOCAL char         *pgTypedSql[2][2];
// Live captures version the stats doc so an out of order one is skipped
LOCAL uint64_t      pgStatsVersion;

/******************************************************************************/
LOCAL const char *arkime_db_pg_table(const char *name)
{
    for (int i = 0; pgTableNames[i]; i++) {
        if (strcmp(name, pgTableNames[i]) == 0)
            return pgTables[i];
    }
    LOGEXIT("ERROR - Unknown Postgres table %s", name);
}

/******************************************************************************/
LOCAL int arkime_db_pg_typed(const char *name)
{
    for (int i = 0; pgTypedNames[i]; i++) {
        if (strcmp(name, pgTypedNames[i]) == 0)
            return i;
    }
    return -1;
}
/******************************************************************************/
LOCAL void arkime_db_pg_log_error(const char *what, const PGresult *res)
{
    static time_t lastLog;
    const time_t now = time(NULL);
    if (now - lastLog < 10 && !config.debug)
        return;
    lastLog = now;
    LOG("ERROR - Postgres %s: %s", what, res ? PQresultErrorMessage(res) : PQerrorMessage(pgConn));
}
/******************************************************************************/
// Caller holds the pgConn lock. Reconnects once if the connection dropped, a
// statement error isn't retried. Returns NULL on failure.
LOCAL PGresult *arkime_db_pg_exec(const char *what, const char *sql, int nParams, const char *const *params, ExecStatusType want)
{
    for (int tries = 0; tries < 2; tries++) {
        if (PQstatus(pgConn) != CONNECTION_OK)
            PQreset(pgConn);

        PGresult *res = PQexecParams(pgConn, sql, nParams, NULL, params, NULL, NULL, 0);
        if (PQresultStatus(res) == want)
            return res;

        if (PQstatus(pgConn) == CONNECTION_OK) {
            arkime_db_pg_log_error(what, res);
            PQclear(res);
            return NULL;
        }
        PQclear(res);
    }
    arkime_db_pg_log_error(what, NULL);
    return NULL;
}
/******************************************************************************/
LOCAL gboolean arkime_db_pg_command(const char *what, const char *sql, int nParams, const char *const *params)
{
    PGresult *res = arkime_db_pg_exec(what, sql, nParams, params, PGRES_COMMAND_OK);
    if (!res)
        return FALSE;
    PQclear(res);
    return TRUE;
}
/******************************************************************************/
// Caller holds the pgConn lock. Returns the new value or 0 on failure.
LOCAL uint32_t arkime_db_pg_next_sequence(const char *name)
{
    const char *params[1] = {name};
    const char *t = arkime_db_pg_table("sequence");
    char sql[400];
    snprintf(sql, sizeof(sql), "INSERT INTO %s (id, doc) VALUES ($1, '{\"seq\":1}') "
             "ON CONFLICT (id) DO UPDATE SET doc = jsonb_set(%s.doc, '{seq}', to_jsonb(COALESCE((%s.doc->>'seq')::bigint, 0) + 1)) "
             "RETURNING (doc->>'seq')::bigint", t, t, t);
    PGresult *res = arkime_db_pg_exec("sequence", sql, 1, params, PGRES_TUPLES_OK);
    if (!res)
        return 0;
    const uint32_t seq = atol(PQgetvalue(res, 0, 0));
    PQclear(res);
    return seq;
}
/******************************************************************************/
// Caller holds the pgConn lock. A version only replaces an older one, which keeps
// out of order stats out like ES external versioning.
LOCAL void arkime_db_pg_do_put(const char *name, const char *id, const char *json, uint64_t version)
{
    const char *table = arkime_db_pg_table(name);
    const int typed = arkime_db_pg_typed(name);
    if (typed >= 0) {
        char vstr[30];
        snprintf(vstr, sizeof(vstr), "%" PRIu64, version);
        const char *params[3] = {id, json, vstr};
        arkime_db_pg_command(table, pgTypedSql[typed][version ? 1 : 0], version ? 3 : 2, params);
        return;
    }

    char sql[300];
    if (version) {
        char vstr[30];
        snprintf(vstr, sizeof(vstr), "%" PRIu64, version);
        const char *params[3] = {id, json, vstr};
        snprintf(sql, sizeof(sql), "INSERT INTO %s (id, doc, version) VALUES ($1, $2::jsonb, $3) "
                 "ON CONFLICT (id) DO UPDATE SET doc = excluded.doc, version = excluded.version WHERE %s.version < excluded.version", table, table);
        arkime_db_pg_command(table, sql, 3, params);
    } else {
        const char *params[2] = {id, json};
        snprintf(sql, sizeof(sql), "INSERT INTO %s (id, doc) VALUES ($1, $2::jsonb) "
                 "ON CONFLICT (id) DO UPDATE SET doc = excluded.doc, version = %s.version + 1", table, table);
        arkime_db_pg_command(table, sql, 2, params);
    }
}
/******************************************************************************/
LOCAL void arkime_db_pg_op_free(ArkimeDbPgOp_t *op)
{
    g_free(op->id);
    g_free(op->json);
    ARKIME_TYPE_FREE(ArkimeDbPgOp_t, op);
}
/******************************************************************************/
LOCAL void *arkime_db_pg_thread(void *UNUSED(threadp))
{
    while (1) {
        ArkimeDbPgOp_t *op = g_async_queue_pop(pgQueue);
        if (op->type == PG_OP_EXIT) {
            arkime_db_pg_op_free(op);
            break;
        }

        char     sql[300];
        uint32_t seq = 0;

        ARKIME_LOCK(pgConn);
        switch (op->type) {
        case PG_OP_PUT:
            arkime_db_pg_do_put(op->table, op->id, op->json, op->version);
            break;
        case PG_OP_UPDATE: {
            const char *params[2] = {op->id, op->json};
            snprintf(sql, sizeof(sql), "UPDATE %s SET doc = doc || $2::jsonb, version = version + 1 WHERE id = $1", arkime_db_pg_table(op->table));
            arkime_db_pg_command(op->table, sql, 2, params);
            break;
        }
        case PG_OP_DELETE: {
            const char *params[1] = {op->id};
            snprintf(sql, sizeof(sql), "DELETE FROM %s WHERE id = $1", arkime_db_pg_table(op->table));
            arkime_db_pg_command(op->table, sql, 1, params);
            break;
        }
        case PG_OP_SEQUENCE:
            seq = arkime_db_pg_next_sequence(op->id);
            break;
        }
        ARKIME_UNLOCK(pgConn);

        // Callbacks run unlocked, they may take their own locks
        if (op->type == PG_OP_SEQUENCE && seq && op->func)
            op->func(seq, op->uw);

        arkime_db_pg_op_free(op);
        ARKIME_THREAD_DECR(pgOutstanding);
    }
    return NULL;
}
/******************************************************************************/
LOCAL void arkime_db_pg_push(int type, const char *table, const char *id, const char *json, int len, uint64_t version, int priority)
{
    if (priority == ARKIME_HTTP_PRIORITY_DROPPABLE && pgOutstanding > PG_MAX_DROPPABLE) {
        pgDropped++;
        return;
    }

    ArkimeDbPgOp_t *op = ARKIME_TYPE_ALLOC0(ArkimeDbPgOp_t);
    op->type = type;
    op->table = table;
    op->id = g_strdup(id);
    if (json)
        op->json = g_strndup(json, len);
    op->version = version;

    ARKIME_THREAD_INCR(pgOutstanding);
    g_async_queue_push(pgQueue, op);
}
/******************************************************************************/
LOCAL void arkime_db_pg_put_version(const char *table, const char *id, const char *json, int len, uint64_t version, gboolean sync, int priority)
{
    if (!sync) {
        arkime_db_pg_push(PG_OP_PUT, table, id, json, len, version, priority);
        return;
    }

    char *doc = g_strndup(json, len);
    ARKIME_LOCK(pgConn);
    arkime_db_pg_do_put(table, id, doc, version);
    ARKIME_UNLOCK(pgConn);
    g_free(doc);
}
/******************************************************************************/
LOCAL void arkime_db_pg_put(const char *table, const char *id, const char *json, int len, gboolean sync, int priority)
{
    arkime_db_pg_put_version(table, id, json, len, 0, sync, priority);
}
/******************************************************************************/
LOCAL void arkime_db_pg_put_stats(const char *json, int len, gboolean sync, int priority)
{
    arkime_db_pg_put_version("stats", config.nodeName, json, len, config.pcapReadOffline ? 0 : ++pgStatsVersion, sync, priority);
}
/******************************************************************************/
LOCAL void arkime_db_pg_update(const char *table, const char *id, const char *json, int len, int priority)
{
    arkime_db_pg_push(PG_OP_UPDATE, table, id, json, len, 0, priority);
}
/******************************************************************************/
LOCAL void arkime_db_pg_delete(const char *table, const char *id)
{
    arkime_db_pg_push(PG_OP_DELETE, table, id, NULL, 0, 0, ARKIME_HTTP_PRIORITY_BEST);
}
/******************************************************************************/
LOCAL void arkime_db_pg_sequence(const char *name, ArkimeSeqNum_cb func, gpointer uw)
{
    ArkimeDbPgOp_t *op = ARKIME_TYPE_ALLOC0(ArkimeDbPgOp_t);
    op->type = PG_OP_SEQUENCE;
    op->id = g_strdup(name);
    op->func = func;
    op->uw = uw;

    ARKIME_THREAD_INCR(pgOutstanding);
    g_async_queue_push(pgQueue, op);
}
/******************************************************************************/
LOCAL uint32_t arkime_db_pg_sequence_sync(const char *name)
{
    while (1) {
        ARKIME_LOCK(pgConn);
        const uint32_t seq = arkime_db_pg_next_sequence(name);
        ARKIME_UNLOCK(pgConn);
        if (seq)
            return seq;
        LOG("ERROR - Couldn't fetch sequence %s from Postgres, retrying", name);
        sleep(1);
    }
}
/******************************************************************************/
LOCAL void arkime_db_pg_each(const char *table, ArkimeDbRow_cb func)
{
    char sql[100];
    snprintf(sql, sizeof(sql), "SELECT id, doc::text FROM %s", arkime_db_pg_table(table));

    ARKIME_LOCK(pgConn);
    PGresult *res = arkime_db_pg_exec(table, sql, 0, NULL, PGRES_TUPLES_OK);
    ARKIME_UNLOCK(pgConn);
    if (!res)
        LOGEXIT("ERROR - Couldn't load %s from Postgres", table);

    for (int i = 0; i < PQntuples(res); i++) {
        func((uint8_t *)PQgetvalue(res, i, 0), PQgetlength(res, i, 0), (uint8_t *)PQgetvalue(res, i, 1), PQgetlength(res, i, 1));
    }
    PQclear(res);
}
/******************************************************************************/
LOCAL gboolean arkime_db_pg_file_exists(const char *node, const char *name, uint32_t *num)
{
    const char *params[2] = {node, name};
    char sql[200];
    snprintf(sql, sizeof(sql), "SELECT f_num FROM %s WHERE f_node = to_jsonb($1::text) AND f_name = to_jsonb($2::text) "
             "ORDER BY f_num DESC LIMIT 1", arkime_db_pg_table("files"));

    ARKIME_LOCK(pgConn);
    PGresult *res = arkime_db_pg_exec("files", sql, 2, params, PGRES_TUPLES_OK);
    ARKIME_UNLOCK(pgConn);
    if (!res)
        return FALSE;

    const gboolean found = PQntuples(res) > 0 && !PQgetisnull(res, 0, 0);
    if (found && num)
        *num = atol(PQgetvalue(res, 0, 0));
    PQclear(res);
    return found;
}
/******************************************************************************/
LOCAL int arkime_db_pg_queue_length()
{
    return pgConn ? pgOutstanding : 0;
}
/******************************************************************************/
// Finishes everything queued before closing
LOCAL void arkime_db_pg_exit()
{
    if (!pgConn)
        return;

    ArkimeDbPgOp_t *op = ARKIME_TYPE_ALLOC0(ArkimeDbPgOp_t);
    op->type = PG_OP_EXIT;
    g_async_queue_push(pgQueue, op);
    g_thread_join(pgThread);
    g_async_queue_unref(pgQueue);

    if (pgDropped)
        LOG("Postgres dropped %" PRIu64 " droppable writes", pgDropped);

    PQfinish(pgConn);
    pgConn = NULL;
}
/******************************************************************************/
LOCAL const ArkimeClusterDb_t pgClusterDb = {
    .put           = arkime_db_pg_put,
    .put_stats     = arkime_db_pg_put_stats,
    .update        = arkime_db_pg_update,
    .del           = arkime_db_pg_delete,
    .sequence      = arkime_db_pg_sequence,
    .sequence_sync = arkime_db_pg_sequence_sync,
    .each          = arkime_db_pg_each,
    .file_exists   = arkime_db_pg_file_exists,
    .queue_length  = arkime_db_pg_queue_length,
    .exit          = arkime_db_pg_exit
};
/******************************************************************************/
// Several captures and viewers can race creating the tables, a duplicate means it exists now
LOCAL void arkime_db_pg_create(const char *sql)
{
    PGresult *res = PQexec(pgConn, sql);
    const char *state = PQresultErrorField(res, PG_DIAG_SQLSTATE);
    if (PQresultStatus(res) != PGRES_COMMAND_OK &&
        !(state && (strcmp(state, "23505") == 0 || strcmp(state, "42P07") == 0 || strcmp(state, "42710") == 0 || strcmp(state, "42701") == 0))) {
        LOGEXIT("ERROR - Couldn't create Postgres tables: %s", PQresultErrorMessage(res));
    }
    PQclear(res);
}
/******************************************************************************/
// Creates the typed table, adds the columns it is missing and builds its statements. An
// ALTER takes a lock even when the column exists, so only the missing ones are added.
LOCAL void arkime_db_pg_typed_init(int n)
{
    const char *t = arkime_db_pg_table(pgTypedNames[n]);
    char sql[300];
    snprintf(sql, sizeof(sql), "CREATE TABLE IF NOT EXISTS %s (id TEXT PRIMARY KEY, version INTEGER NOT NULL DEFAULT 1)", t);
    arkime_db_pg_create(sql);

    const char *params[1] = {t};
    PGresult *res = PQexecParams(pgConn, "SELECT column_name FROM information_schema.columns WHERE table_schema = current_schema() AND table_name = $1",
                                 1, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
        LOGEXIT("ERROR - Couldn't read the %s columns: %s", t, PQresultErrorMessage(res));

    GString *alter = g_string_new(NULL);
    for (int c = 0; pgStatsColumns[c].field; c++) {
        gboolean have = FALSE;
        for (int r = 0; r < PQntuples(res) && !have; r++) {
            have = strcmp(PQgetvalue(res, r, 0), pgStatsColumns[c].field) == 0;
        }
        if (!have)
            g_string_append_printf(alter, "%s ADD COLUMN IF NOT EXISTS \"%s\" %s", alter->len ? "," : "", pgStatsColumns[c].field, pgStatsColumns[c].type);
    }
    PQclear(res);

    if (alter->len) {
        g_string_prepend(alter, "BEGIN; SET LOCAL lock_timeout = '10s'; ALTER TABLE ");
        g_string_insert(alter, strlen("BEGIN; SET LOCAL lock_timeout = '10s'; ALTER TABLE "), t);
        g_string_append(alter, "; COMMIT");
        arkime_db_pg_create(alter->str);
        if (PQtransactionStatus(pgConn) != PQTRANS_IDLE)
            PQclear(PQexec(pgConn, "ROLLBACK"));
    }
    g_string_free(alter, TRUE);

    if (strcmp(pgTypedNames[n], "dstats") == 0) {
        for (int i = 0; pgDstatsIndexed[i]; i++) {
            snprintf(sql, sizeof(sql), "CREATE INDEX IF NOT EXISTS %s_f_%s ON %s (\"%s\")", t, pgDstatsIndexed[i], t, pgDstatsIndexed[i]);
            arkime_db_pg_create(sql);
        }
    }

    // Capture writes its own columns, ones a newer version added keep their value
    GString *cols = g_string_new(NULL);
    GString *vals = g_string_new(NULL);
    GString *sets = g_string_new(NULL);
    for (int c = 0; pgStatsColumns[c].field; c++) {
        const char *f = pgStatsColumns[c].field;
        g_string_append_printf(cols, ", \"%s\"", f);
        g_string_append_printf(vals, ", p.\"%s\"", f);
        g_string_append_printf(sets, "%s\"%s\" = excluded.\"%s\"", c ? ", " : "", f, f);
    }
    pgTypedSql[n][0] = g_strdup_printf("INSERT INTO %s (id%s) SELECT $1%s FROM jsonb_populate_record(NULL::%s, $2::jsonb) p "
                                       "ON CONFLICT (id) DO UPDATE SET %s, version = %s.version + 1",
                                       t, cols->str, vals->str, t, sets->str, t);
    pgTypedSql[n][1] = g_strdup_printf("INSERT INTO %s (id, version%s) SELECT $1, $3::integer%s FROM jsonb_populate_record(NULL::%s, $2::jsonb) p "
                                       "ON CONFLICT (id) DO UPDATE SET %s, version = excluded.version WHERE %s.version < excluded.version",
                                       t, cols->str, vals->str, t, sets->str, t);
    g_string_free(cols, TRUE);
    g_string_free(vals, TRUE);
    g_string_free(sets, TRUE);
}
/******************************************************************************/
LOCAL gboolean arkime_db_pg_loopback(const char *host)
{
    return host[0] == '/' || host[0] == '@' || strcmp(host, "localhost") == 0 || strcmp(host, "::1") == 0 ||
           (strncmp(host, "127.", 4) == 0 && g_hostname_is_ip_address(host));
}
/******************************************************************************/
LOCAL void arkime_db_pg_notice(void *UNUSED(arg), const char *message)
{
    if (config.debug)
        LOG("Postgres %.*s", (int)strcspn(message, "\n"), message);
}
/******************************************************************************/
/* Like the OpenSearch/Elasticsearch connection, a remote server's certificate is
 * verified with caTrustFile or the system CAs, and --insecure still requires TLS.
 * Settings in the url win, loopback and unix socket servers keep the libpq default.
 */
LOCAL PGconn *arkime_db_pg_connect(const char *url)
{
    char *err = NULL;
    PQconninfoOption *opts = PQconninfoParse(url, &err);
    if (!opts) {
        LOGEXIT("ERROR - Couldn't parse the Postgres clusterDbUrl: %s", err ? err : "out of memory");
    }

    gboolean sslSet = FALSE;
    gboolean verify = FALSE;
    gboolean remote = FALSE;
    for (PQconninfoOption *o = opts; o->keyword; o++) {
        if (!o->val || !o->val[0])
            continue;
        if (strcmp(o->keyword, "sslmode") == 0 || strcmp(o->keyword, "requiressl") == 0) {
            sslSet = TRUE;
            verify = strncmp(o->val, "verify-", 7) == 0;
        } else if (strcmp(o->keyword, "host") == 0 || strcmp(o->keyword, "hostaddr") == 0) {
            gchar **hosts = g_strsplit(o->val, ",", 0);
            for (int i = 0; hosts[i]; i++) {
                if (hosts[i][0] && !arkime_db_pg_loopback(hosts[i]))
                    remote = TRUE;
            }
            g_strfreev(hosts);
        }
    }
    PQconninfoFree(opts);

    // Entries before dbname are overridden by the same setting in the url
    const char *keywords[4];
    const char *values[4];
    int n = 0;
    if (remote && !sslSet) {
        keywords[n] = "sslmode";
        values[n++] = config.insecure ? "require" : "verify-full";
        verify = !config.insecure;
    }
    // libpq verifies require too when it has a root cert, so only give one when verifying
    if (remote && verify && config.caTrustFile) {
        keywords[n] = "sslrootcert";
        values[n++] = config.caTrustFile;
    } else if (remote && verify && !sslSet && PQlibVersion() >= 160000) {
        keywords[n] = "sslrootcert";
        values[n++] = "system";
    }
    keywords[n] = "dbname";
    values[n++] = url;
    keywords[n] = NULL;
    values[n] = NULL;

    return PQconnectdbParams(keywords, values, 1);
}
/******************************************************************************/
LOCAL const ArkimeClusterDb_t *arkime_db_pg_cluster_init(const char *url)
{
    for (const char *c = config.clusterPrefix; *c; c++) {
        if (!g_ascii_isalnum(*c) && *c != '_')
            CONFIGEXIT("clusterDbPrefix '%s' can only have letters, digits and _ with a Postgres clusterDbUrl", config.clusterPrefix);
    }
    for (int i = 0; pgTableNames[i]; i++) {
        pgTables[i] = g_strdup_printf("%s%s", config.clusterPrefix, pgTableNames[i]);
    }

    pgConn = arkime_db_pg_connect(url);
    if (PQstatus(pgConn) != CONNECTION_OK) {
        LOGEXIT("ERROR - Couldn't connect to the Postgres clusterDbUrl: %s", PQerrorMessage(pgConn));
    }
    // Every start creates the tables IF NOT EXISTS, which libpq would print a NOTICE for
    PQsetNoticeProcessor(pgConn, arkime_db_pg_notice, NULL);

    char sql[300];
    for (int i = 0; pgTables[i]; i++) {
        if (arkime_db_pg_typed(pgTableNames[i]) >= 0)
            continue;
        snprintf(sql, sizeof(sql), "CREATE TABLE IF NOT EXISTS %s (id TEXT PRIMARY KEY, doc JSONB NOT NULL, version INTEGER NOT NULL DEFAULT 1)", pgTables[i]);
        arkime_db_pg_create(sql);
    }
    for (int i = 0; pgTypedNames[i]; i++) {
        arkime_db_pg_typed_init(i);
    }

    for (int i = 0; pgIndexed[i].table; i++) {
        const char *t = arkime_db_pg_table(pgIndexed[i].table);
        const char *f = pgIndexed[i].field;
        snprintf(sql, sizeof(sql), "ALTER TABLE %s ADD COLUMN IF NOT EXISTS f_%s JSONB GENERATED ALWAYS AS (NULLIF(doc #> '{%s}', 'null'::jsonb)) STORED", t, f, f);
        arkime_db_pg_create(sql);
        snprintf(sql, sizeof(sql), "CREATE INDEX IF NOT EXISTS %s_f_%s ON %s (f_%s)", t, f, t, f);
        arkime_db_pg_create(sql);
        if (pgIndexed[i].desc) {
            snprintf(sql, sizeof(sql), "CREATE INDEX IF NOT EXISTS %s_f_%s_desc ON %s (f_%s DESC NULLS LAST)", t, f, t, f);
            arkime_db_pg_create(sql);
        }
    }

    // File numbers start at 101
    char fn[200];
    snprintf(fn, sizeof(fn), "fn-%s", config.nodeName);
    const char *params[1] = {fn};
    snprintf(sql, sizeof(sql), "INSERT INTO %s (id, doc) VALUES ($1, '{\"seq\":100}') ON CONFLICT (id) DO NOTHING", arkime_db_pg_table("sequence"));
    if (!arkime_db_pg_command("sequence", sql, 1, params))
        LOGEXIT("ERROR - Couldn't create the %s sequence in Postgres", fn);

    if (!config.pcapReadOffline && !config.dryRun) {
        const char *sparams[1] = {config.nodeName};
        snprintf(sql, sizeof(sql), "SELECT version FROM %s WHERE id = $1", arkime_db_pg_table("stats"));
        PGresult *res = arkime_db_pg_exec("stats", sql, 1, sparams, PGRES_TUPLES_OK);
        if (!res)
            LOGEXIT("ERROR - Couldn't fetch the %s stats from Postgres", config.nodeName);
        if (PQntuples(res) > 0)
            pgStatsVersion = strtoull(PQgetvalue(res, 0, 0), NULL, 10);
        PQclear(res);
    }

    pgQueue = g_async_queue_new();
    pgThread = g_thread_new("arkime-pg", &arkime_db_pg_thread, NULL);

    if (config.debug)
        LOG("Using Postgres clusterDbUrl for stats, dstats, files, fields and sequence");
    return &pgClusterDb;
}

#else

/******************************************************************************/
LOCAL const ArkimeClusterDb_t *arkime_db_pg_cluster_init(const char *UNUSED(url))
{
    LOGEXIT("ERROR - clusterDbUrl is Postgres, but capture was built without libpq 13 or newer");
}
#endif
/******************************************************************************/
void arkime_db_pg_init()
{
    arkime_db_cluster_register("postgres", arkime_db_pg_cluster_init);
    arkime_db_cluster_register("postgresql", arkime_db_pg_cluster_init);
}

