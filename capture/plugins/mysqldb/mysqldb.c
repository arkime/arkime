/******************************************************************************/
/* mysqldb.c -- MySQL dbUrl writer for capture's stats, dstats, files, fields
 *              and sequence, a rootPlugins plugin. Sessions still go to ES or
 *              ClickHouse. The tables are the same as mysqldb.js creates.
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <inttypes.h>
#include <mysql.h>
#include <errmsg.h>
#include <mysqld_error.h>
#include <openssl/x509.h>

extern ArkimeConfig_t config;

#define MY_MAX_DROPPABLE   500
// A files or fields write tries this many times, a second plus a reconnect apart, before it is lost
#define MY_MAX_RETRIES     30
// Indexed keyword columns and ids fit an InnoDB index entry
#define MY_KEY_CHARS       768
#define MY_TEXT_TYPE       "VARCHAR(768) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_bin"
#define MY_NUM_JSON        "'INTEGER', 'UNSIGNED INTEGER', 'DOUBLE', 'DECIMAL'"
#define MY_TABLE_OPTIONS   "ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_bin"
// MIN_VERSION in mysqldb.js
#define MY_MIN_MAJOR       26
#define MY_MIN_MINOR       10

enum {
    MY_OP_PUT,
    MY_OP_UPDATE,
    MY_OP_DELETE,
    MY_OP_SEQUENCE,
    MY_OP_EXIT
};

typedef struct {
    int              type;
    const char      *table;
    char            *id;
    char            *json;
    uint64_t         version;
    int              priority;
    int              tries;
    ArkimeSeqNum_cb  func;
    gpointer         uw;
} ArkimeMyOp_t;

typedef struct {
    char        *host;
    char        *user;
    char        *password;
    char        *database;
    char        *socket;
    unsigned int port;
    char        *sslMode;
    char        *sslCa;
    char        *sslCert;
    char        *sslKey;
} ArkimeMyUrl_t;

LOCAL MYSQL                 *myConn;
LOCAL ARKIME_LOCK_DEFINE(myConn);
LOCAL ArkimeMyUrl_t          myUrl;
LOCAL GAsyncQueue           *myQueue;
LOCAL GThread               *myThread;
LOCAL int                    myOutstanding;
LOCAL uint64_t               myDropped;
LOCAL uint64_t               myLost;
LOCAL volatile int           myExiting;

// arkime_db_cluster_tables, each prefixed like the ES indices
LOCAL const ArkimeDbTable_t *myLayout;
LOCAL char                 **myTables;
// Per typed table: the upsert and the versioned upsert, the id, version and json go where the %s are
LOCAL char                *(*myTypedSql)[2];
// Live captures version the stats doc so an out of order one is skipped
LOCAL uint64_t               myStatsVersion;

/******************************************************************************/
LOCAL int arkime_mysqldb_table_num(const char *name)
{
    for (int i = 0; myLayout[i].name; i++) {
        if (strcmp(name, myLayout[i].name) == 0)
            return i;
    }
    LOGEXIT("ERROR - Unknown MySQL table %s", name);
}
/******************************************************************************/
// The column a typed table gives each ES type, MYSQL_TYPES in mysqldb.js
LOCAL const char *arkime_mysqldb_type(const char *type)
{
    if (strcmp(type, "short") == 0 || strcmp(type, "byte") == 0)
        return "SMALLINT";
    if (strcmp(type, "integer") == 0)
        return "INT";
    if (strcmp(type, "long") == 0 || strcmp(type, "date") == 0)
        return "BIGINT";
    if (strcmp(type, "float") == 0 || strcmp(type, "double") == 0)
        return "DOUBLE";
    if (strcmp(type, "boolean") == 0)
        return "BOOLEAN";
    return MY_TEXT_TYPE;
}
/******************************************************************************/
LOCAL gboolean arkime_mysqldb_numeric(const char *type)
{
    return strcmp(type, "long") == 0 || strcmp(type, "date") == 0 || strcmp(type, "integer") == 0 || strcmp(type, "short") == 0 ||
           strcmp(type, "byte") == 0 || strcmp(type, "double") == 0 || strcmp(type, "float") == 0;
}
/******************************************************************************/
// The generated column an indexed json field gets, indexedColumn in mysqldb.js
LOCAL char *arkime_mysqldb_indexed_column(const char *field, const char *type)
{
    char j[200];
    snprintf(j, sizeof(j), "JSON_EXTRACT(doc, '$.\"%s\"')", field);

    if (arkime_mysqldb_numeric(type))
        return g_strdup_printf("DOUBLE GENERATED ALWAYS AS (CASE WHEN JSON_TYPE(%s) IN (" MY_NUM_JSON ") THEN CAST(%s AS DOUBLE) END) VIRTUAL", j, j);
    if (strcmp(type, "boolean") == 0)
        return g_strdup_printf("TINYINT GENERATED ALWAYS AS (CASE WHEN JSON_TYPE(%s) = 'BOOLEAN' THEN %s = CAST('true' AS JSON) END) VIRTUAL", j, j);
    if (strcmp(type, "ip") == 0)
        return g_strdup_printf("VARBINARY(16) GENERATED ALWAYS AS (CASE WHEN JSON_TYPE(%s) = 'STRING' THEN IF(IS_IPV4(JSON_UNQUOTE(%s)) OR IS_IPV6(JSON_UNQUOTE(%s)), "
                               "INET6_ATON(JSON_UNQUOTE(%s)), NULL) END) VIRTUAL", j, j, j, j);
    return g_strdup_printf(MY_TEXT_TYPE " GENERATED ALWAYS AS (CASE WHEN JSON_TYPE(%s) = 'STRING' THEN LEFT(JSON_UNQUOTE(%s), %d) END) VIRTUAL", j, j, MY_KEY_CHARS);
}
/******************************************************************************/
LOCAL void arkime_mysqldb_log_error(const char *what)
{
    static time_t lastLog;
    const time_t now = time(NULL);
    if (now - lastLog < 10 && !config.debug)
        return;
    lastLog = now;
    LOG("ERROR - MySQL %s: %s", what, myConn ? mysql_error(myConn) : "not connected");
}
/******************************************************************************/
LOCAL gboolean arkime_mysqldb_loopback(const char *host)
{
    return !host || !host[0] || strcmp(host, "localhost") == 0 || strcmp(host, "::1") == 0 ||
           (strncmp(host, "127.", 4) == 0 && g_hostname_is_ip_address(host));
}
/******************************************************************************/
LOCAL char *arkime_mysqldb_unescape(const char *str, int len)
{
    char *part = g_strndup(str, len);
    char *out = g_uri_unescape_string(part, NULL);
    g_free(part);
    return out ? out : g_strdup("");
}
/******************************************************************************/
// mysql://user:password@host[:port]/database[?socket=/path&ssl-mode=...&ssl-ca=...&ssl-cert=...&ssl-key=...]
LOCAL void arkime_mysqldb_parse_url(const char *url)
{
    const char *p = url + strlen("mysql://");
    const char *query = strchr(p, '?');
    const char *end = query ? query : p + strlen(p);
    const char *slash = memchr(p, '/', end - p);
    if (!slash || slash + 1 >= end)
        CONFIGEXIT("A mysql:// dbUrl needs a database, mysql://user:pass@host/database");
    myUrl.database = arkime_mysqldb_unescape(slash + 1, end - slash - 1);

    const char *at = g_strrstr_len(p, slash - p, "@");
    const char *hp = p;
    if (at) {
        const char *colon = memchr(p, ':', at - p);
        myUrl.user = arkime_mysqldb_unescape(p, (colon ? colon : at) - p);
        if (colon)
            myUrl.password = arkime_mysqldb_unescape(colon + 1, at - colon - 1);
        hp = at + 1;
    }

    myUrl.port = 3306;
    if (*hp == '[') {
        const char *close = memchr(hp, ']', slash - hp);
        if (!close)
            CONFIGEXIT("Couldn't parse the MySQL dbUrl host");
        myUrl.host = g_strndup(hp + 1, close - hp - 1);
        if (close[1] == ':')
            myUrl.port = atoi(close + 2);
    } else {
        const char *colon = memchr(hp, ':', slash - hp);
        myUrl.host = g_strndup(hp, (colon ? colon : slash) - hp);
        if (colon)
            myUrl.port = atoi(colon + 1);
    }
    if (!myUrl.host[0]) {
        g_free(myUrl.host);
        myUrl.host = g_strdup("localhost");
    }

    if (!query)
        return;
    gchar **params = g_strsplit(query + 1, "&", 0);
    for (int i = 0; params[i]; i++) {
        char *eq = strchr(params[i], '=');
        if (!eq)
            continue;
        *eq = 0;
        char *value = g_uri_unescape_string(eq + 1, NULL);
        if (strcmp(params[i], "socket") == 0)
            myUrl.socket = value;
        else if (strcmp(params[i], "ssl-mode") == 0) {
            myUrl.sslMode = g_ascii_strup(value, -1);
            g_free(value);
        } else if (strcmp(params[i], "ssl-ca") == 0)
            myUrl.sslCa = value;
        else if (strcmp(params[i], "ssl-cert") == 0)
            myUrl.sslCert = value;
        else if (strcmp(params[i], "ssl-key") == 0)
            myUrl.sslKey = value;
        else
            g_free(value);
    }
    g_strfreev(params);
}
/******************************************************************************/
/* Like the OpenSearch/Elasticsearch connection, a remote server's certificate is
 * verified with caTrustFile or the system CAs, and --insecure still requires TLS.
 * ssl-mode and the ssl files in the url win, loopback and unix socket servers keep
 * the client default.
 */
LOCAL MYSQL *arkime_mysqldb_connect()
{
    MYSQL *conn = mysql_init(NULL);
    if (!conn)
        LOGEXIT("ERROR - Out of memory for a MySQL connection");

    // Every op reconnects under the myConn lock, so a server that drops packets can't hold capture for the OS's timeouts
    const unsigned int timeout = 10;
    mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    const unsigned int rwTimeout = 60;
    mysql_options(conn, MYSQL_OPT_READ_TIMEOUT, &rwTimeout);
    mysql_options(conn, MYSQL_OPT_WRITE_TIMEOUT, &rwTimeout);
    mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    // The same session as mysqldb.js, whatever the server's defaults are
    mysql_options(conn, MYSQL_INIT_COMMAND, "SET NAMES utf8mb4 COLLATE utf8mb4_bin, SESSION sql_mode = 'STRICT_ALL_TABLES,NO_ENGINE_SUBSTITUTION', "
                  "SESSION lock_wait_timeout = 30, SESSION time_zone = '+00:00'");
    const bool getKey = true;
    mysql_options(conn, MYSQL_OPT_GET_SERVER_PUBLIC_KEY, &getKey);

    unsigned int mode = 0;
    if (myUrl.sslMode) {
        if (strcmp(myUrl.sslMode, "DISABLED") == 0)
            mode = SSL_MODE_DISABLED;
        else if (strcmp(myUrl.sslMode, "REQUIRED") == 0)
            mode = SSL_MODE_REQUIRED;
        else if (strcmp(myUrl.sslMode, "VERIFY_CA") == 0)
            mode = SSL_MODE_VERIFY_CA;
        else if (strcmp(myUrl.sslMode, "VERIFY_IDENTITY") == 0)
            mode = SSL_MODE_VERIFY_IDENTITY;
        else
            CONFIGEXIT("MySQL ssl-mode '%s' isn't one of DISABLED, REQUIRED, VERIFY_CA or VERIFY_IDENTITY", myUrl.sslMode);
    } else if (myUrl.sslCa || myUrl.sslCert) {
        mode = SSL_MODE_VERIFY_IDENTITY;
    } else if (!myUrl.socket && !arkime_mysqldb_loopback(myUrl.host)) {
        mode = config.insecure ? SSL_MODE_REQUIRED : SSL_MODE_VERIFY_IDENTITY;
        if (!config.insecure) {
            if (config.caTrustFile) {
                mysql_options(conn, MYSQL_OPT_SSL_CA, config.caTrustFile);
            } else if (g_file_test(X509_get_default_cert_file(), G_FILE_TEST_EXISTS)) {
                mysql_options(conn, MYSQL_OPT_SSL_CA, X509_get_default_cert_file());
            } else {
                mysql_options(conn, MYSQL_OPT_SSL_CAPATH, X509_get_default_cert_dir());
            }
        }
    }
    if (mode)
        mysql_options(conn, MYSQL_OPT_SSL_MODE, &mode);
    if (myUrl.sslCa)
        mysql_options(conn, MYSQL_OPT_SSL_CA, myUrl.sslCa);
    if (myUrl.sslCert)
        mysql_options(conn, MYSQL_OPT_SSL_CERT, myUrl.sslCert);
    if (myUrl.sslKey)
        mysql_options(conn, MYSQL_OPT_SSL_KEY, myUrl.sslKey);

    if (!mysql_real_connect(conn, myUrl.socket ? NULL : myUrl.host, myUrl.user, myUrl.password, myUrl.database, myUrl.port, myUrl.socket, 0)) {
        LOG("ERROR - Couldn't connect to the MySQL dbUrl: %s", mysql_error(conn));
        mysql_close(conn);
        return NULL;
    }
    return conn;
}
/******************************************************************************/
LOCAL gboolean arkime_mysqldb_gone(unsigned int err)
{
    return err == CR_SERVER_GONE_ERROR || err == CR_SERVER_LOST || err == CR_CONNECTION_ERROR || err == CR_CONN_HOST_ERROR ||
           err == CR_SERVER_LOST_EXTENDED || err == ER_CLIENT_INTERACTION_TIMEOUT;
}
/******************************************************************************/
// The calling thread holds the myConn lock, which every thread that uses the client library inits once
LOCAL void arkime_mysqldb_lock()
{
    ARKIME_LOCK(myConn);
    mysql_thread_init();
}
/******************************************************************************/
// Caller holds the myConn lock. Reconnects once if the connection dropped, a
// statement error isn't retried. Sets *down when the server can't be reached.
LOCAL gboolean arkime_mysqldb_exec(const char *what, const char *sql, gboolean *down)
{
    if (down)
        *down = FALSE;
    for (int tries = 0; tries < 2; tries++) {
        if (!myConn)
            myConn = arkime_mysqldb_connect();
        if (!myConn)
            break;

        if (mysql_real_query(myConn, sql, strlen(sql)) == 0)
            return TRUE;

        if (!arkime_mysqldb_gone(mysql_errno(myConn))) {
            arkime_mysqldb_log_error(what);
            return FALSE;
        }
        arkime_mysqldb_log_error(what);
        mysql_close(myConn);
        myConn = NULL;
    }
    if (down)
        *down = TRUE;
    return FALSE;
}
/******************************************************************************/
// Caller holds the myConn lock, a quoted and escaped sql string
LOCAL void arkime_mysqldb_quote(GString *sql, const char *str, int len)
{
    if (len < 0)
        len = strlen(str);
    char *buf = g_malloc(len * 2 + 1);
    const unsigned long n = mysql_real_escape_string_quote(myConn, buf, str, len, '\'');
    g_string_append_c(sql, '\'');
    g_string_append_len(sql, buf, n);
    g_string_append_c(sql, '\'');
    g_free(buf);
}
/******************************************************************************/
// Caller holds the myConn lock. Returns the new value or 0 on failure.
LOCAL uint32_t arkime_mysqldb_next_sequence(const char *name, gboolean *down)
{
    if (!myConn && !(myConn = arkime_mysqldb_connect())) {
        if (down)
            *down = TRUE;
        return 0;
    }
    GString *sql = g_string_new(NULL);
    g_string_printf(sql, "INSERT INTO `%s` (id, doc) VALUES (", myTables[arkime_mysqldb_table_num("sequence")]);
    arkime_mysqldb_quote(sql, name, -1);
    g_string_append(sql, ", '{\"seq\": 1}') ON DUPLICATE KEY UPDATE "
                    "doc = JSON_SET(doc, '$.seq', LAST_INSERT_ID(COALESCE(CAST(JSON_EXTRACT(doc, '$.seq') AS UNSIGNED), 0) + 1)), version = version + 1");
    uint32_t seq = 0;
    if (arkime_mysqldb_exec("sequence", sql->str, down))
        seq = mysql_affected_rows(myConn) == 1 ? 1 : (uint32_t)mysql_insert_id(myConn);
    g_string_free(sql, TRUE);
    return seq;
}
/******************************************************************************/
// Caller holds the myConn lock. A version only replaces an older one, which keeps
// out of order stats out like ES external versioning.
LOCAL gboolean arkime_mysqldb_do_put(const char *name, const char *id, const char *json, uint64_t version, gboolean *down)
{
    if (!myConn && !(myConn = arkime_mysqldb_connect())) {
        *down = TRUE;
        return FALSE;
    }
    const int n = arkime_mysqldb_table_num(name);
    GString *sql = g_string_new(NULL);
    GString *qid = g_string_new(NULL);
    GString *qjson = g_string_new(NULL);
    arkime_mysqldb_quote(qid, id, -1);
    arkime_mysqldb_quote(qjson, json, -1);

    if (myLayout[n].typed) {
        char vstr[30];
        snprintf(vstr, sizeof(vstr), "%" PRIu64, version);
        g_string_printf(sql, myTypedSql[n][version ? 1 : 0], qid->str, vstr, qjson->str);
    } else if (version) {
        g_string_printf(sql, "INSERT INTO `%s` (id, doc, version) VALUES (%s, CAST(%s AS JSON), %" PRIu64 ") AS new ON DUPLICATE KEY UPDATE "
                        "doc = IF(version < new.version, new.doc, doc), version = GREATEST(version, new.version)",
                        myTables[n], qid->str, qjson->str, version);
    } else {
        g_string_printf(sql, "INSERT INTO `%s` (id, doc) VALUES (%s, CAST(%s AS JSON)) AS new ON DUPLICATE KEY UPDATE doc = new.doc, version = version + 1",
                        myTables[n], qid->str, qjson->str);
    }

    const gboolean ok = arkime_mysqldb_exec(name, sql->str, down);
    g_string_free(sql, TRUE);
    g_string_free(qid, TRUE);
    g_string_free(qjson, TRUE);
    return ok;
}
/******************************************************************************/
// Caller holds the myConn lock. Capture's updates only set scalars and arrays, which
// JSON_MERGE_PATCH replaces at the top level the same as the viewer's update.
LOCAL gboolean arkime_mysqldb_do_update(const ArkimeMyOp_t *op, gboolean *down)
{
    if (!myConn && !(myConn = arkime_mysqldb_connect())) {
        *down = TRUE;
        return FALSE;
    }
    GString *sql = g_string_new(NULL);
    g_string_printf(sql, "UPDATE `%s` SET doc = JSON_MERGE_PATCH(doc, CAST(", myTables[arkime_mysqldb_table_num(op->table)]);
    arkime_mysqldb_quote(sql, op->json, -1);
    g_string_append(sql, " AS JSON)), version = version + 1 WHERE id = ");
    arkime_mysqldb_quote(sql, op->id, -1);
    const gboolean ok = arkime_mysqldb_exec(op->table, sql->str, down);
    g_string_free(sql, TRUE);
    return ok;
}
/******************************************************************************/
LOCAL gboolean arkime_mysqldb_do_delete(const ArkimeMyOp_t *op, gboolean *down)
{
    if (!myConn && !(myConn = arkime_mysqldb_connect())) {
        *down = TRUE;
        return FALSE;
    }
    GString *sql = g_string_new(NULL);
    g_string_printf(sql, "DELETE FROM `%s` WHERE id = ", myTables[arkime_mysqldb_table_num(op->table)]);
    arkime_mysqldb_quote(sql, op->id, -1);
    const gboolean ok = arkime_mysqldb_exec(op->table, sql->str, down);
    g_string_free(sql, TRUE);
    return ok;
}
/******************************************************************************/
LOCAL void arkime_mysqldb_op_free(ArkimeMyOp_t *op)
{
    g_free(op->id);
    g_free(op->json);
    ARKIME_TYPE_FREE(ArkimeMyOp_t, op);
}
/******************************************************************************/
LOCAL void *arkime_mysqldb_thread(void *UNUSED(threadp))
{
    mysql_thread_init();
    while (1) {
        ArkimeMyOp_t *op = g_async_queue_pop(myQueue);
        if (op->type == MY_OP_EXIT) {
            arkime_mysqldb_op_free(op);
            break;
        }

        uint32_t seq = 0;
        gboolean down = FALSE;

        // A files or fields write waits in place for MySQL to come back, so a file's create still
        // lands before its update. What queues up behind it is droppable, or waits too.
        while (1) {
            arkime_mysqldb_lock();
            switch (op->type) {
            case MY_OP_PUT:
                arkime_mysqldb_do_put(op->table, op->id, op->json, op->version, &down);
                break;
            case MY_OP_UPDATE:
                arkime_mysqldb_do_update(op, &down);
                break;
            case MY_OP_DELETE:
                arkime_mysqldb_do_delete(op, &down);
                break;
            default:
                seq = arkime_mysqldb_next_sequence(op->id, &down);
                break;
            }
            ARKIME_UNLOCK(myConn);

            // A statement error isn't retried, only a server that is down
            if (!down || op->type == MY_OP_SEQUENCE || op->priority == ARKIME_HTTP_PRIORITY_DROPPABLE || myExiting)
                break;
            if (++op->tries >= MY_MAX_RETRIES) {
                myLost++;
                LOG("ERROR - MySQL %s write of %s lost after %d tries", op->table, op->id, op->tries);
                break;
            }
            sleep(1);
        }

        // Callbacks run unlocked, they may take their own locks
        if (op->type == MY_OP_SEQUENCE && seq && op->func)
            op->func(seq, op->uw);

        arkime_mysqldb_op_free(op);
        ARKIME_THREAD_DECR(myOutstanding);
    }
    mysql_thread_end();
    return NULL;
}
/******************************************************************************/
LOCAL void arkime_mysqldb_push(int type, const char *table, const char *id, const char *json, int len, uint64_t version, int priority)
{
    if (priority == ARKIME_HTTP_PRIORITY_DROPPABLE && myOutstanding > MY_MAX_DROPPABLE) {
        myDropped++;
        return;
    }

    ArkimeMyOp_t *op = ARKIME_TYPE_ALLOC0(ArkimeMyOp_t);
    op->type = type;
    op->table = table;
    op->id = g_strdup(id);
    if (json)
        op->json = g_strndup(json, len);
    op->version = version;
    op->priority = priority;

    ARKIME_THREAD_INCR(myOutstanding);
    g_async_queue_push(myQueue, op);
}
/******************************************************************************/
LOCAL void arkime_mysqldb_put_version(const char *table, const char *id, const char *json, int len, uint64_t version, gboolean sync, int priority)
{
    if (!sync) {
        arkime_mysqldb_push(MY_OP_PUT, table, id, json, len, version, priority);
        return;
    }

    char *doc = g_strndup(json, len);
    gboolean down;
    arkime_mysqldb_lock();
    arkime_mysqldb_do_put(table, id, doc, version, &down);
    ARKIME_UNLOCK(myConn);
    g_free(doc);
}
/******************************************************************************/
LOCAL void arkime_mysqldb_put(const char *table, const char *id, const char *json, int len, gboolean sync, int priority)
{
    arkime_mysqldb_put_version(table, id, json, len, 0, sync, priority);
}
/******************************************************************************/
LOCAL void arkime_mysqldb_put_stats(const char *json, int len, gboolean sync, int priority)
{
    arkime_mysqldb_put_version("stats", config.nodeName, json, len, config.pcapReadOffline ? 0 : ++myStatsVersion, sync, priority);
}
/******************************************************************************/
LOCAL void arkime_mysqldb_update(const char *table, const char *id, const char *json, int len, int priority)
{
    arkime_mysqldb_push(MY_OP_UPDATE, table, id, json, len, 0, priority);
}
/******************************************************************************/
LOCAL void arkime_mysqldb_delete(const char *table, const char *id)
{
    arkime_mysqldb_push(MY_OP_DELETE, table, id, NULL, 0, 0, ARKIME_HTTP_PRIORITY_BEST);
}
/******************************************************************************/
LOCAL void arkime_mysqldb_sequence(const char *name, ArkimeSeqNum_cb func, gpointer uw)
{
    ArkimeMyOp_t *op = ARKIME_TYPE_ALLOC0(ArkimeMyOp_t);
    op->type = MY_OP_SEQUENCE;
    op->id = g_strdup(name);
    op->func = func;
    op->uw = uw;

    ARKIME_THREAD_INCR(myOutstanding);
    g_async_queue_push(myQueue, op);
}
/******************************************************************************/
LOCAL uint32_t arkime_mysqldb_sequence_sync(const char *name)
{
    while (1) {
        arkime_mysqldb_lock();
        const uint32_t seq = arkime_mysqldb_next_sequence(name, NULL);
        ARKIME_UNLOCK(myConn);
        if (seq)
            return seq;
        LOG("ERROR - Couldn't fetch sequence %s from MySQL, retrying", name);
        sleep(1);
    }
}
/******************************************************************************/
// Caller holds the myConn lock, the rows of a query or NULL
LOCAL MYSQL_RES *arkime_mysqldb_select(const char *what, const char *sql)
{
    if (!arkime_mysqldb_exec(what, sql, NULL))
        return NULL;
    MYSQL_RES *res = mysql_store_result(myConn);
    if (!res)
        arkime_mysqldb_log_error(what);
    return res;
}
/******************************************************************************/
LOCAL gboolean arkime_mysqldb_each(const char *table, ArkimeDbRow_cb func)
{
    char sql[200];
    snprintf(sql, sizeof(sql), "SELECT id, doc FROM `%s`", myTables[arkime_mysqldb_table_num(table)]);

    arkime_mysqldb_lock();
    MYSQL_RES *res = arkime_mysqldb_select(table, sql);
    ARKIME_UNLOCK(myConn);
    if (!res) {
        LOG("ERROR - Couldn't load %s from MySQL", table);
        return FALSE;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        const unsigned long *lengths = mysql_fetch_lengths(res);
        func((uint8_t *)row[0], lengths[0], (uint8_t *)row[1], lengths[1]);
    }
    mysql_free_result(res);
    return TRUE;
}
/******************************************************************************/
// f_node and f_name find it with their indexes, the json name checks a name longer than f_name holds
LOCAL gboolean arkime_mysqldb_file_exists(const char *node, const char *name, uint32_t *num)
{
    arkime_mysqldb_lock();
    if (!myConn && !(myConn = arkime_mysqldb_connect())) {
        ARKIME_UNLOCK(myConn);
        return FALSE;
    }
    GString *sql = g_string_new(NULL);
    g_string_printf(sql, "SELECT JSON_EXTRACT(doc, '$.num') FROM `%s` WHERE f_node = ", myTables[arkime_mysqldb_table_num("files")]);
    arkime_mysqldb_quote(sql, node, -1);
    g_string_append_printf(sql, " AND f_name = LEFT(");
    arkime_mysqldb_quote(sql, name, -1);
    g_string_append_printf(sql, ", %d) AND JSON_UNQUOTE(JSON_EXTRACT(doc, '$.name')) COLLATE utf8mb4_0900_bin = ", MY_KEY_CHARS);
    arkime_mysqldb_quote(sql, name, -1);
    g_string_append(sql, " ORDER BY f_num DESC LIMIT 1");

    MYSQL_RES *res = arkime_mysqldb_select("files", sql->str);
    ARKIME_UNLOCK(myConn);
    g_string_free(sql, TRUE);
    if (!res)
        return FALSE;

    MYSQL_ROW row = mysql_fetch_row(res);
    const gboolean found = row && row[0];
    if (found && num)
        *num = atol(row[0]);
    mysql_free_result(res);
    return found;
}
/******************************************************************************/
LOCAL int arkime_mysqldb_queue_length()
{
    return myQueue ? myOutstanding : 0;
}
/******************************************************************************/
// Finishes everything queued before closing
LOCAL void arkime_mysqldb_exit()
{
    if (!myQueue)
        return;

    // A write waiting for MySQL to come back stops waiting
    myExiting = 1;
    ArkimeMyOp_t *op = ARKIME_TYPE_ALLOC0(ArkimeMyOp_t);
    op->type = MY_OP_EXIT;
    g_async_queue_push(myQueue, op);
    g_thread_join(myThread);
    g_async_queue_unref(myQueue);
    myQueue = NULL;

    if (myDropped)
        LOG("MySQL dropped %" PRIu64 " droppable writes", myDropped);
    if (myLost)
        LOG("MySQL lost %" PRIu64 " writes while it was down", myLost);

    if (myConn)
        mysql_close(myConn);
    myConn = NULL;
    mysql_library_end();
}
/******************************************************************************/
LOCAL const ArkimeClusterDb_t myClusterDb = {
    .put           = arkime_mysqldb_put,
    .put_stats     = arkime_mysqldb_put_stats,
    .update        = arkime_mysqldb_update,
    .del           = arkime_mysqldb_delete,
    .sequence      = arkime_mysqldb_sequence,
    .sequence_sync = arkime_mysqldb_sequence_sync,
    .each          = arkime_mysqldb_each,
    .file_exists   = arkime_mysqldb_file_exists,
    .queue_length  = arkime_mysqldb_queue_length,
    .exit          = arkime_mysqldb_exit
};
/******************************************************************************/
// Several captures and viewers can race creating the tables, a duplicate column or index means it exists now
LOCAL void arkime_mysqldb_create(const char *sql)
{
    if (mysql_real_query(myConn, sql, strlen(sql)) != 0) {
        const unsigned int err = mysql_errno(myConn);
        if (err != ER_DUP_FIELDNAME && err != ER_DUP_KEYNAME)
            LOGEXIT("ERROR - Couldn't create the MySQL tables: %s\n%s", mysql_error(myConn), sql);
    }
}
/******************************************************************************/
// The names a query's first column has, lowercased since MySQL names don't care about case
LOCAL GHashTable *arkime_mysqldb_names(const char *sql)
{
    if (mysql_real_query(myConn, sql, strlen(sql)) != 0)
        LOGEXIT("ERROR - Couldn't read the MySQL tables: %s", mysql_error(myConn));
    MYSQL_RES *res = mysql_store_result(myConn);
    GHashTable *names = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    MYSQL_ROW row;
    while (res && (row = mysql_fetch_row(res))) {
        g_hash_table_add(names, g_ascii_strdown(row[0], -1));
    }
    if (res)
        mysql_free_result(res);
    return names;
}
/******************************************************************************/
LOCAL GHashTable *arkime_mysqldb_table_names(const char *what, const char *t)
{
    GString *sql = g_string_new(NULL);
    if (strcmp(what, "columns") == 0)
        g_string_printf(sql, "SELECT COLUMN_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = ");
    else
        g_string_printf(sql, "SELECT DISTINCT INDEX_NAME FROM information_schema.STATISTICS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = ");
    arkime_mysqldb_quote(sql, t, -1);
    GHashTable *names = arkime_mysqldb_names(sql->str);
    g_string_free(sql, TRUE);
    return names;
}
/******************************************************************************/
LOCAL void arkime_mysqldb_index(const char *t, GHashTable *indexes, const char *field, const char *column, int indexed)
{
    char sql[500];
    char name[100];
    snprintf(name, sizeof(name), "f_%s", field);
    char *lower = g_ascii_strdown(name, -1);
    if (!g_hash_table_contains(indexes, lower)) {
        snprintf(sql, sizeof(sql), "CREATE INDEX %s ON `%s` (%s)", name, t, column);
        arkime_mysqldb_create(sql);
    }
    g_free(lower);
    // Ascending with the missing values last, a descending sort reads the plain index backwards
    if (indexed == ARKIME_DB_INDEXED_SORT) {
        snprintf(name, sizeof(name), "f_%s_nl", field);
        lower = g_ascii_strdown(name, -1);
        if (!g_hash_table_contains(indexes, lower)) {
            snprintf(sql, sizeof(sql), "CREATE INDEX %s ON `%s` ((%s IS NULL), %s)", name, t, column, column);
            arkime_mysqldb_create(sql);
        }
        g_free(lower);
    }
}
/******************************************************************************/
// A json table and the generated columns of its indexed fields, createTables in mysqldb.js
LOCAL void arkime_mysqldb_json_init(int n)
{
    const char *t = myTables[n];
    char sql[500];
    snprintf(sql, sizeof(sql), "CREATE TABLE IF NOT EXISTS `%s` (id VARBINARY(3072) NOT NULL PRIMARY KEY, doc JSON NOT NULL CHECK (JSON_TYPE(doc) = 'OBJECT'), "
             "version INT NOT NULL DEFAULT 1) " MY_TABLE_OPTIONS, t);
    arkime_mysqldb_create(sql);

    GHashTable *columns = arkime_mysqldb_table_names("columns", t);
    GHashTable *indexes = arkime_mysqldb_table_names("indexes", t);
    for (const ArkimeDbField_t *field = myLayout[n].fields; field->field; field++) {
        if (!field->indexed)
            continue;
        char col[100];
        snprintf(col, sizeof(col), "f_%s", field->field);
        char *lower = g_ascii_strdown(col, -1);
        if (!g_hash_table_contains(columns, lower)) {
            char *def = arkime_mysqldb_indexed_column(field->field, field->type);
            char *alter = g_strdup_printf("ALTER TABLE `%s` ADD COLUMN %s %s", t, col, def);
            arkime_mysqldb_create(alter);
            g_free(alter);
            g_free(def);
        }
        g_free(lower);
        arkime_mysqldb_index(t, indexes, field->field, col, field->indexed);
    }
    g_hash_table_destroy(columns);
    g_hash_table_destroy(indexes);
}
/******************************************************************************/
// Creates the typed table, adds the columns it is missing and builds its statements. An
// ALTER waits for the table's metadata lock even when the column exists, so only the missing ones are added.
LOCAL void arkime_mysqldb_typed_init(int n)
{
    const char *t = myTables[n];
    const ArkimeDbField_t *fields = myLayout[n].fields;
    char sql[300];
    snprintf(sql, sizeof(sql), "CREATE TABLE IF NOT EXISTS `%s` (id VARBINARY(3072) NOT NULL PRIMARY KEY, version INT NOT NULL DEFAULT 1) " MY_TABLE_OPTIONS, t);
    arkime_mysqldb_create(sql);

    GHashTable *columns = arkime_mysqldb_table_names("columns", t);
    GString *alter = g_string_new(NULL);
    for (int c = 0; fields[c].field; c++) {
        char *lower = g_ascii_strdown(fields[c].field, -1);
        if (!g_hash_table_contains(columns, lower))
            g_string_append_printf(alter, "%s ADD COLUMN `%s` %s", alter->len ? "," : "", fields[c].field, arkime_mysqldb_type(fields[c].type));
        g_free(lower);
    }
    g_hash_table_destroy(columns);
    if (alter->len) {
        g_string_prepend(alter, "` ");
        g_string_prepend(alter, t);
        g_string_prepend(alter, "ALTER TABLE `");
        arkime_mysqldb_create(alter->str);
    }
    g_string_free(alter, TRUE);

    GHashTable *indexes = arkime_mysqldb_table_names("indexes", t);
    for (int c = 0; fields[c].field; c++) {
        if (!fields[c].indexed)
            continue;
        char col[100];
        snprintf(col, sizeof(col), "`%s`", fields[c].field);
        arkime_mysqldb_index(t, indexes, fields[c].field, col, fields[c].indexed);
    }
    g_hash_table_destroy(indexes);

    // Capture writes its own columns from the json, ones a newer version added keep their value
    GString *cols = g_string_new(NULL);
    GString *jcols = g_string_new(NULL);
    GString *sets = g_string_new(NULL);
    GString *vsets = g_string_new(NULL);
    for (int c = 0; fields[c].field; c++) {
        const char *f = fields[c].field;
        g_string_append_printf(cols, ", `%s`", f);
        g_string_append_printf(jcols, "%s`%s` %s PATH '$.\"%s\"' NULL ON EMPTY NULL ON ERROR", c ? ", " : "", f, arkime_mysqldb_type(fields[c].type), f);
        g_string_append_printf(sets, "`%s` = j.`%s`, ", f, f);
        g_string_append_printf(vsets, "`%s` = IF(version < ver.v, j.`%s`, `%s`), ", f, f, f);
    }
    // Escaped %s for the id, the version and the json
    myTypedSql[n][0] = g_strdup_printf("INSERT INTO `%s` (id%s) SELECT %%s, j.* FROM (SELECT %%s AS v) AS ver, "
                                       "JSON_TABLE(CAST(%%s AS JSON), '$' COLUMNS (%s)) AS j "
                                       "ON DUPLICATE KEY UPDATE %sversion = version + 1",
                                       t, cols->str, jcols->str, sets->str);
    myTypedSql[n][1] = g_strdup_printf("INSERT INTO `%s` (id, version%s) SELECT %%s, ver.v, j.* FROM (SELECT %%s AS v) AS ver, "
                                       "JSON_TABLE(CAST(%%s AS JSON), '$' COLUMNS (%s)) AS j "
                                       "ON DUPLICATE KEY UPDATE %sversion = GREATEST(version, ver.v)",
                                       t, cols->str, jcols->str, vsets->str);
    g_string_free(cols, TRUE);
    g_string_free(jcols, TRUE);
    g_string_free(sets, TRUE);
    g_string_free(vsets, TRUE);
}
/******************************************************************************/
LOCAL void arkime_mysqldb_check_version()
{
    const char *version = mysql_get_server_info(myConn);
    int major = 0, minor = 0;
    sscanf(version, "%d.%d", &major, &minor);
    if (major < MY_MIN_MAJOR || (major == MY_MIN_MAJOR && minor < MY_MIN_MINOR))
        LOGEXIT("ERROR - MySQL %s is too old, a mysql:// dbUrl needs %d.%d or newer", version, MY_MIN_MAJOR, MY_MIN_MINOR);
}
/******************************************************************************/
LOCAL const ArkimeClusterDb_t *arkime_mysqldb_cluster_init(const char *url)
{
    for (const char *c = config.clusterPrefix; *c; c++) {
        if (!g_ascii_isalnum(*c) && *c != '_')
            CONFIGEXIT("dbPrefix '%s' can only have letters, digits and _ with a MySQL dbUrl", config.clusterPrefix);
    }

    myLayout = arkime_db_cluster_tables();
    int ntables = 0;
    while (myLayout[ntables].name)
        ntables++;
    myTables = g_new0(char *, ntables + 1);
    myTypedSql = g_malloc0(sizeof(*myTypedSql) * ntables);
    for (int i = 0; i < ntables; i++) {
        myTables[i] = g_strdup_printf("%s%s", config.clusterPrefix, myLayout[i].name);
        if (strlen(myTables[i]) > 64)
            CONFIGEXIT("dbPrefix '%s' makes the MySQL table %s longer than 64", config.clusterPrefix, myTables[i]);
    }

    arkime_mysqldb_parse_url(url);
    myConn = arkime_mysqldb_connect();
    if (!myConn)
        LOGEXIT("ERROR - Couldn't connect to the MySQL dbUrl");
    arkime_mysqldb_check_version();

    for (int i = 0; i < ntables; i++) {
        if (myLayout[i].typed)
            arkime_mysqldb_typed_init(i);
        else
            arkime_mysqldb_json_init(i);
    }

    // File numbers start at 101
    char fn[200];
    snprintf(fn, sizeof(fn), "fn-%s", config.nodeName);
    GString *sql = g_string_new(NULL);
    g_string_printf(sql, "INSERT INTO `%s` (id, doc) VALUES (", myTables[arkime_mysqldb_table_num("sequence")]);
    arkime_mysqldb_quote(sql, fn, -1);
    g_string_append(sql, ", '{\"seq\": 100}') ON DUPLICATE KEY UPDATE id = id");
    if (!arkime_mysqldb_exec("sequence", sql->str, NULL))
        LOGEXIT("ERROR - Couldn't create the %s sequence in MySQL", fn);

    if (!config.pcapReadOffline && !config.dryRun) {
        g_string_printf(sql, "SELECT version FROM `%s` WHERE id = ", myTables[arkime_mysqldb_table_num("stats")]);
        arkime_mysqldb_quote(sql, config.nodeName, -1);
        MYSQL_RES *res = arkime_mysqldb_select("stats", sql->str);
        if (!res)
            LOGEXIT("ERROR - Couldn't fetch the %s stats from MySQL", config.nodeName);
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row && row[0])
            myStatsVersion = strtoull(row[0], NULL, 10);
        mysql_free_result(res);
    }
    g_string_free(sql, TRUE);

    myQueue = g_async_queue_new();
    myThread = g_thread_new("arkime-mysql", &arkime_mysqldb_thread, NULL);

    if (config.debug)
        LOG("Using MySQL dbUrl for stats, dstats, files, fields and sequence");
    return &myClusterDb;
}
/******************************************************************************/
void arkime_plugin_init()
{
    if (mysql_library_init(0, NULL, NULL))
        LOGEXIT("ERROR - Couldn't initialize the MySQL client library");
    arkime_db_cluster_register("mysql", arkime_mysqldb_cluster_init);
}
