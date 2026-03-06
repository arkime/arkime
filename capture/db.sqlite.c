/******************************************************************************/
/* db.sqlite.c  -- SQLite database implementation
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include "arkimeconfig.h"
#include <inttypes.h>
#include <errno.h>
#include "thirdparty/sqlite3.h"

extern ArkimeConfig_t        config;

LOCAL sqlite3                *sqliteDB = 0;
LOCAL uint64_t                sqliteStartTime = 0;
LOCAL uint64_t                sqliteSessionCounter = 0;

LOCAL sqlite3_stmt           *insertSessionStmt = 0;
LOCAL sqlite3_stmt           *insertFileStmt = 0;
LOCAL sqlite3_stmt           *updateFileStmt = 0;
LOCAL sqlite3_stmt           *fileExistsStmt = 0;
LOCAL sqlite3_stmt           *insertStatsStmt = 0;
LOCAL sqlite3_stmt           *loadStatsStmt = 0;
LOCAL sqlite3_stmt           *insertDStatsStmt = 0;
LOCAL sqlite3_stmt           *getSeqStmt = 0;
LOCAL sqlite3_stmt           *insertFieldStmt = 0;
LOCAL sqlite3_stmt           *deleteFieldStmt = 0;
LOCAL sqlite3_stmt           *loadFieldsStmt = 0;

LOCAL ARKIME_LOCK_DEFINE(sqliteLock);

/******************************************************************************/
LOCAL void sqlite_check_rc(int rc, const char *msg)
{
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
        LOG("ERROR - SQLite %s: %s", msg, sqlite3_errmsg(sqliteDB));
    }
}
/******************************************************************************/
LOCAL void sqlite_create_tables()
{
    char *err = NULL;

    /* Sessions table - stores the full JSON blob */
    int rc = sqlite3_exec(sqliteDB,
                          "CREATE TABLE IF NOT EXISTS sessions ("
                          "  id TEXT PRIMARY KEY,"
                          "  json TEXT NOT NULL"
                          ");"
                          "CREATE INDEX IF NOT EXISTS sessions_first_packet ON sessions(json_extract(json, '$.firstPacket'));"
                          "CREATE INDEX IF NOT EXISTS sessions_last_packet ON sessions(json_extract(json, '$.lastPacket'));",
                          NULL, NULL, &err);
    if (err) {
        LOG("ERROR - SQLite create sessions: %s", err);
        sqlite3_free(err);
    }

    /* Files table */
    rc = sqlite3_exec(sqliteDB,
                      "CREATE TABLE IF NOT EXISTS files ("
                      "  node TEXT NOT NULL,"
                      "  num INTEGER NOT NULL,"
                      "  json TEXT NOT NULL,"
                      "  PRIMARY KEY (node, num)"
                      ");",
                      NULL, NULL, &err);
    if (err) {
        LOG("ERROR - SQLite create files: %s", err);
        sqlite3_free(err);
    }

    /* Stats table */
    rc = sqlite3_exec(sqliteDB,
                      "CREATE TABLE IF NOT EXISTS stats ("
                      "  node TEXT PRIMARY KEY,"
                      "  json TEXT NOT NULL,"
                      "  version INTEGER DEFAULT 1"
                      ");",
                      NULL, NULL, &err);
    if (err) {
        LOG("ERROR - SQLite create stats: %s", err);
        sqlite3_free(err);
    }

    /* DStats (interval stats) table */
    rc = sqlite3_exec(sqliteDB,
                      "CREATE TABLE IF NOT EXISTS dstats ("
                      "  id TEXT PRIMARY KEY,"
                      "  json TEXT NOT NULL"
                      ");",
                      NULL, NULL, &err);
    if (err) {
        LOG("ERROR - SQLite create dstats: %s", err);
        sqlite3_free(err);
    }

    /* Sequence table for file numbering */
    rc = sqlite3_exec(sqliteDB,
                      "CREATE TABLE IF NOT EXISTS sequence ("
                      "  name TEXT PRIMARY KEY,"
                      "  value INTEGER DEFAULT 0"
                      ");",
                      NULL, NULL, &err);
    if (err) {
        LOG("ERROR - SQLite create sequence: %s", err);
        sqlite3_free(err);
    }

    /* Fields table */
    rc = sqlite3_exec(sqliteDB,
                      "CREATE TABLE IF NOT EXISTS fields ("
                      "  id TEXT PRIMARY KEY,"
                      "  json TEXT NOT NULL"
                      ");",
                      NULL, NULL, &err);
    if (err) {
        LOG("ERROR - SQLite create fields: %s", err);
        sqlite3_free(err);
    }

    /* DB version metadata */
    rc = sqlite3_exec(sqliteDB,
                      "CREATE TABLE IF NOT EXISTS metadata ("
                      "  key TEXT PRIMARY KEY,"
                      "  value TEXT"
                      ");"
                      "INSERT OR IGNORE INTO metadata (key, value) VALUES ('molochDbVersion', '85');",
                      NULL, NULL, &err);
    if (err) {
        LOG("ERROR - SQLite create metadata: %s", err);
        sqlite3_free(err);
    }

    (void)rc;
}
/******************************************************************************/
LOCAL void sqlite_prepare_statements()
{
    int rc;

    rc = sqlite3_prepare_v2(sqliteDB,
                            "INSERT OR REPLACE INTO sessions (id, json) VALUES (?, ?)",
                            -1, &insertSessionStmt, NULL);
    sqlite_check_rc(rc, "prepare insertSession");

    rc = sqlite3_prepare_v2(sqliteDB,
                            "INSERT OR REPLACE INTO files (node, num, json) VALUES (?, ?, ?)",
                            -1, &insertFileStmt, NULL);
    sqlite_check_rc(rc, "prepare insertFile");

    rc = sqlite3_prepare_v2(sqliteDB,
                            "UPDATE files SET json = json_patch(json, ?) WHERE node = ? AND num = ?",
                            -1, &updateFileStmt, NULL);
    sqlite_check_rc(rc, "prepare updateFile");

    rc = sqlite3_prepare_v2(sqliteDB,
                            "SELECT num FROM files WHERE node = ? AND name = ? ORDER BY num DESC LIMIT 1",
                            -1, &fileExistsStmt, NULL);
    sqlite_check_rc(rc, "prepare fileExists");

    rc = sqlite3_prepare_v2(sqliteDB,
                            "INSERT OR REPLACE INTO stats (node, json, version) "
                            "VALUES (?, ?, COALESCE((SELECT version + 1 FROM stats WHERE node = ?), 1))",
                            -1, &insertStatsStmt, NULL);
    sqlite_check_rc(rc, "prepare insertStats");

    rc = sqlite3_prepare_v2(sqliteDB,
                            "SELECT json, version FROM stats WHERE node = ?",
                            -1, &loadStatsStmt, NULL);
    sqlite_check_rc(rc, "prepare loadStats");

    rc = sqlite3_prepare_v2(sqliteDB,
                            "INSERT OR REPLACE INTO dstats (id, json) VALUES (?, ?)",
                            -1, &insertDStatsStmt, NULL);
    sqlite_check_rc(rc, "prepare insertDStats");

    rc = sqlite3_prepare_v2(sqliteDB,
                            "INSERT INTO sequence (name, value) VALUES (?, 1) "
                            "ON CONFLICT(name) DO UPDATE SET value = value + 1 RETURNING value",
                            -1, &getSeqStmt, NULL);
    sqlite_check_rc(rc, "prepare getSeq");

    rc = sqlite3_prepare_v2(sqliteDB,
                            "INSERT OR REPLACE INTO fields (id, json) VALUES (?, ?)",
                            -1, &insertFieldStmt, NULL);
    sqlite_check_rc(rc, "prepare insertField");

    rc = sqlite3_prepare_v2(sqliteDB,
                            "DELETE FROM fields WHERE id = ?",
                            -1, &deleteFieldStmt, NULL);
    sqlite_check_rc(rc, "prepare deleteField");

    rc = sqlite3_prepare_v2(sqliteDB,
                            "SELECT id, json FROM fields",
                            -1, &loadFieldsStmt, NULL);
    sqlite_check_rc(rc, "prepare loadFields");
}
/******************************************************************************/
LOCAL void arkime_db_sqlite_send_bulk(char *json, int len)
{
    if (len <= 0 || !json)
        return;

    char idBuf[256];
    snprintf(idBuf, sizeof(idBuf), "%s-%" PRIu64 "-%" PRIu64, config.nodeName, sqliteStartTime, sqliteSessionCounter++);

    ARKIME_LOCK(sqliteLock);

    sqlite3_reset(insertSessionStmt);
    sqlite3_bind_text(insertSessionStmt, 1, idBuf, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insertSessionStmt, 2, json, len, SQLITE_TRANSIENT);

    int rc = sqlite3_step(insertSessionStmt);
    if (rc != SQLITE_DONE) {
        LOG("ERROR - SQLite insert session: %s", sqlite3_errmsg(sqliteDB));
    }

    ARKIME_UNLOCK(sqliteLock);

    free(json);
}
/******************************************************************************/
LOCAL void arkime_db_sqlite_load_stats(uint64_t *totalPackets, uint64_t *totalK, uint64_t *totalSessions, uint64_t *totalDropped, uint64_t *dbVersion)
{
    *totalPackets = 0;
    *totalK = 0;
    *totalSessions = 0;
    *totalDropped = 0;
    *dbVersion = 0;

    ARKIME_LOCK(sqliteLock);

    sqlite3_reset(loadStatsStmt);
    sqlite3_bind_text(loadStatsStmt, 1, config.nodeName, -1, SQLITE_STATIC);

    if (sqlite3_step(loadStatsStmt) == SQLITE_ROW) {
        const uint8_t *json = sqlite3_column_text(loadStatsStmt, 0);
        int json_len = sqlite3_column_bytes(loadStatsStmt, 0);
        *dbVersion = sqlite3_column_int64(loadStatsStmt, 1);

        if (json && json_len > 0) {
            uint32_t len;
            const uint8_t *val;

            if ((val = arkime_js0n_get(json, json_len, "totalPackets", &len)) && len)
                * totalPackets = atoll((char *)val);
            if ((val = arkime_js0n_get(json, json_len, "totalK", &len)) && len)
                * totalK = atoll((char *)val);
            if ((val = arkime_js0n_get(json, json_len, "totalSessions", &len)) && len)
                * totalSessions = atoll((char *)val);
            if ((val = arkime_js0n_get(json, json_len, "totalDropped", &len)) && len)
                * totalDropped = atoll((char *)val);
        }
    }

    ARKIME_UNLOCK(sqliteLock);
}
/******************************************************************************/
LOCAL void arkime_db_sqlite_send_stats(char *json, int json_len, int n, uint64_t currentTimeSec, uint64_t UNUSED(dbVersion), gboolean UNUSED(sync))
{
    ARKIME_LOCK(sqliteLock);

    if (n == 0) {
        sqlite3_reset(insertStatsStmt);
        sqlite3_bind_text(insertStatsStmt, 1, config.nodeName, -1, SQLITE_STATIC);
        sqlite3_bind_text(insertStatsStmt, 2, json, json_len, SQLITE_TRANSIENT);
        sqlite3_bind_text(insertStatsStmt, 3, config.nodeName, -1, SQLITE_STATIC);

        int rc = sqlite3_step(insertStatsStmt);
        sqlite_check_rc(rc, "insert stats");
    } else {
        static const int intervals[4] = {1, 5, 60, 600};
        char id[200];
        snprintf(id, sizeof(id), "%s-%d-%d", config.nodeName, (int)(currentTimeSec / intervals[n]) % 1440, intervals[n]);

        sqlite3_reset(insertDStatsStmt);
        sqlite3_bind_text(insertDStatsStmt, 1, id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insertDStatsStmt, 2, json, json_len, SQLITE_TRANSIENT);

        int rc = sqlite3_step(insertDStatsStmt);
        sqlite_check_rc(rc, "insert dstats");
    }

    ARKIME_UNLOCK(sqliteLock);
    free(json);
}
/******************************************************************************/
LOCAL void arkime_db_sqlite_check(ArkimeDbMode_t UNUSED(mode))
{
    /* Verify db version from metadata table */
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(sqliteDB,
                                "SELECT value FROM metadata WHERE key = 'molochDbVersion'",
                                -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        LOGEXIT("ERROR - SQLite database might not be initialized");
    }

    extern int arkimeDbVersion;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        arkimeDbVersion = sqlite3_column_int(stmt, 0);
    } else {
        LOGEXIT("ERROR - Database version couldn't be found in SQLite database");
    }
    sqlite3_finalize(stmt);

    if (arkimeDbVersion < 83) {
        LOGEXIT("ERROR - Database version '%d' is too old, needs to be at least 83", arkimeDbVersion);
    }
}
/******************************************************************************/
LOCAL void arkime_db_sqlite_get_sequence_number(const char *name, ArkimeSeqNum_cb func, gpointer uw)
{
    uint32_t seq = 0;

    ARKIME_LOCK(sqliteLock);

    sqlite3_reset(getSeqStmt);
    sqlite3_bind_text(getSeqStmt, 1, name, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(getSeqStmt) == SQLITE_ROW) {
        seq = sqlite3_column_int(getSeqStmt, 0);
    }

    ARKIME_UNLOCK(sqliteLock);

    if (func)
        func(seq, uw);
}
/******************************************************************************/
LOCAL uint32_t arkime_db_sqlite_get_sequence_number_sync(const char *name)
{
    uint32_t seq = 0;

    ARKIME_LOCK(sqliteLock);

    sqlite3_reset(getSeqStmt);
    sqlite3_bind_text(getSeqStmt, 1, name, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(getSeqStmt) == SQLITE_ROW) {
        seq = sqlite3_column_int(getSeqStmt, 0);
    }

    ARKIME_UNLOCK(sqliteLock);
    return seq;
}
/******************************************************************************/
LOCAL void arkime_db_sqlite_create_file(char *json, int json_len, uint32_t num)
{
    ARKIME_LOCK(sqliteLock);

    sqlite3_reset(insertFileStmt);
    sqlite3_bind_text(insertFileStmt, 1, config.nodeName, -1, SQLITE_STATIC);
    sqlite3_bind_int(insertFileStmt, 2, num);
    sqlite3_bind_text(insertFileStmt, 3, json, json_len, SQLITE_TRANSIENT);

    int rc = sqlite3_step(insertFileStmt);
    sqlite_check_rc(rc, "create file");

    ARKIME_UNLOCK(sqliteLock);
    free(json);
}
/******************************************************************************/
LOCAL void arkime_db_sqlite_update_file(char *json, int json_len, uint32_t fileid)
{
    ARKIME_LOCK(sqliteLock);

    sqlite3_reset(updateFileStmt);
    sqlite3_bind_text(updateFileStmt, 1, json, json_len, SQLITE_TRANSIENT);
    sqlite3_bind_text(updateFileStmt, 2, config.nodeName, -1, SQLITE_STATIC);
    sqlite3_bind_int(updateFileStmt, 3, fileid);

    int rc = sqlite3_step(updateFileStmt);
    sqlite_check_rc(rc, "update file");

    ARKIME_UNLOCK(sqliteLock);
    free(json);
}
/******************************************************************************/
LOCAL gboolean arkime_db_sqlite_file_exists(const char *filename, uint32_t *outputId)
{
    /* fileExistsStmt was prepared against a different schema (node + name columns).
     * Since files table stores JSON with name inside it, do a JSON query instead. */
    gboolean found = FALSE;
    sqlite3_stmt *stmt = NULL;

    ARKIME_LOCK(sqliteLock);

    int rc = sqlite3_prepare_v2(sqliteDB,
                                "SELECT num FROM files WHERE node = ? AND json_extract(json, '$.name') = ? ORDER BY num DESC LIMIT 1",
                                -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, config.nodeName, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, filename, -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            found = TRUE;
            if (outputId) {
                *outputId = sqlite3_column_int(stmt, 0);
            }
        }
        sqlite3_finalize(stmt);
    }

    ARKIME_UNLOCK(sqliteLock);
    return found;
}
/******************************************************************************/
LOCAL void arkime_db_sqlite_load_fields()
{
    ARKIME_LOCK(sqliteLock);

    sqlite3_reset(loadFieldsStmt);

    while (sqlite3_step(loadFieldsStmt) == SQLITE_ROW) {
        const uint8_t *id = sqlite3_column_text(loadFieldsStmt, 0);
        int id_len = sqlite3_column_bytes(loadFieldsStmt, 0);
        const uint8_t *json = sqlite3_column_text(loadFieldsStmt, 1);
        int json_len = sqlite3_column_bytes(loadFieldsStmt, 1);

        if (id && json && id_len > 0 && json_len > 0) {
            arkime_field_define_json(id, id_len, json, json_len);
        }
    }

    ARKIME_UNLOCK(sqliteLock);
}
/******************************************************************************/
LOCAL void arkime_db_sqlite_send_fields(char *json, int json_len, gboolean UNUSED(sync))
{
    /* json is ES _bulk format: pairs of action-line + data-line separated by \n.
     * Parse each pair and handle index/update/delete. */

    ARKIME_LOCK(sqliteLock);

    const char *ptr = json;
    const char *end = json + json_len;

    while (ptr < end) {
        /* Find end of action line */
        const char *nl = memchr(ptr, '\n', end - ptr);
        if (!nl) break;
        int action_len = nl - ptr;

        /* Parse the action line to determine operation and _id */
        uint32_t index_len = 0, delete_len = 0, update_len = 0;
        const uint8_t *index_val = arkime_js0n_get((uint8_t *)ptr, action_len, "index", &index_len);
        const uint8_t *delete_val = arkime_js0n_get((uint8_t *)ptr, action_len, "delete", &delete_len);
        const uint8_t *update_val = arkime_js0n_get((uint8_t *)ptr, action_len, "update", &update_len);

        if (delete_val && delete_len > 0) {
            /* Delete: {"delete": {"_index": "...", "_id": "..."}} */
            uint32_t id_len = 0;
            const uint8_t *id_val = arkime_js0n_get(delete_val, delete_len, "_id", &id_len);
            if (id_val && id_len > 0) {
                sqlite3_reset(deleteFieldStmt);
                sqlite3_bind_text(deleteFieldStmt, 1, (char *)id_val, id_len, SQLITE_TRANSIENT);
                sqlite3_step(deleteFieldStmt);
            }
            ptr = nl + 1;
            continue;
        }

        /* For index and update, consume the data line */
        ptr = nl + 1;
        nl = memchr(ptr, '\n', end - ptr);
        if (!nl) break;
        int data_len = nl - ptr;

        if (index_val && index_len > 0) {
            /* Index: insert/replace the field */
            uint32_t id_len = 0;
            const uint8_t *id_val = arkime_js0n_get(index_val, index_len, "_id", &id_len);
            if (id_val && id_len > 0) {
                sqlite3_reset(insertFieldStmt);
                sqlite3_bind_text(insertFieldStmt, 1, (char *)id_val, id_len, SQLITE_TRANSIENT);
                sqlite3_bind_text(insertFieldStmt, 2, ptr, data_len, SQLITE_TRANSIENT);
                sqlite3_step(insertFieldStmt);
            }
        } else if (update_val && update_len > 0) {
            /* Update: merge the doc fields - extract the "doc" portion and merge */
            uint32_t id_len = 0;
            const uint8_t *id_val = arkime_js0n_get(update_val, update_len, "_id", &id_len);
            if (id_val && id_len > 0) {
                /* The data line is {"doc": {...}}. Use json_patch to merge. */
                uint32_t doc_len = 0;
                const uint8_t *doc = arkime_js0n_get((uint8_t *)ptr, data_len, "doc", &doc_len);
                if (doc && doc_len > 0) {
                    char sql[4096];
                    snprintf(sql, sizeof(sql),
                             "UPDATE fields SET json = json_patch(json, '{%.*s}') WHERE id = '%.*s'",
                             doc_len, doc, id_len, id_val);
                    sqlite3_exec(sqliteDB, sql, NULL, NULL, NULL);
                } else {
                    /* If no doc wrapper, just store the whole thing */
                    sqlite3_reset(insertFieldStmt);
                    sqlite3_bind_text(insertFieldStmt, 1, (char *)id_val, id_len, SQLITE_TRANSIENT);
                    sqlite3_bind_text(insertFieldStmt, 2, ptr, data_len, SQLITE_TRANSIENT);
                    sqlite3_step(insertFieldStmt);
                }
            }
        }

        ptr = nl + 1;
    }

    ARKIME_UNLOCK(sqliteLock);
    free(json);
}
/******************************************************************************/
LOCAL int arkime_db_sqlite_queue_length()
{
    return 0;
}
/******************************************************************************/
LOCAL int arkime_db_sqlite_queue_length_best()
{
    return 0;
}
/******************************************************************************/
LOCAL uint64_t arkime_db_sqlite_dropped_count()
{
    return 0;
}
/******************************************************************************/
LOCAL void arkime_db_sqlite_refresh()
{
    /* No-op for SQLite - WAL mode auto-commits */
}
/******************************************************************************/
LOCAL void arkime_db_sqlite_exit()
{
    if (insertSessionStmt)  sqlite3_finalize(insertSessionStmt);
    if (insertFileStmt)     sqlite3_finalize(insertFileStmt);
    if (updateFileStmt)     sqlite3_finalize(updateFileStmt);
    if (fileExistsStmt)     sqlite3_finalize(fileExistsStmt);
    if (insertStatsStmt)    sqlite3_finalize(insertStatsStmt);
    if (loadStatsStmt)      sqlite3_finalize(loadStatsStmt);
    if (insertDStatsStmt)   sqlite3_finalize(insertDStatsStmt);
    if (getSeqStmt)         sqlite3_finalize(getSeqStmt);
    if (insertFieldStmt)    sqlite3_finalize(insertFieldStmt);
    if (deleteFieldStmt)    sqlite3_finalize(deleteFieldStmt);
    if (loadFieldsStmt)     sqlite3_finalize(loadFieldsStmt);

    if (sqliteDB) {
        sqlite3_wal_checkpoint_v2(sqliteDB, NULL, SQLITE_CHECKPOINT_TRUNCATE, NULL, NULL);
        sqlite3_close(sqliteDB);
        sqliteDB = NULL;
    }
}
/******************************************************************************/
LOCAL ArkimeDbImpl_t sqliteImpl = {
    .init                    = NULL,
    .exit                    = arkime_db_sqlite_exit,
    .load_stats              = arkime_db_sqlite_load_stats,
    .send_stats              = arkime_db_sqlite_send_stats,
    .check                   = arkime_db_sqlite_check,
    .get_sequence_number     = arkime_db_sqlite_get_sequence_number,
    .get_sequence_number_sync = arkime_db_sqlite_get_sequence_number_sync,
    .create_file             = arkime_db_sqlite_create_file,
    .update_file             = arkime_db_sqlite_update_file,
    .file_exists             = arkime_db_sqlite_file_exists,
    .load_fields             = arkime_db_sqlite_load_fields,
    .send_fields             = arkime_db_sqlite_send_fields,
    .queue_length            = arkime_db_sqlite_queue_length,
    .queue_length_best       = arkime_db_sqlite_queue_length_best,
    .dropped_count           = arkime_db_sqlite_dropped_count,
    .refresh                 = arkime_db_sqlite_refresh,
};
/******************************************************************************/
LOCAL void arkime_db_sqlite_init_impl(const char *url, ArkimeDbMode_t UNUSED(mode))
{
    const char *dbPath;
    char pathBuf[1024];

    if (url) {
        /* url is "sqlite:///path/to/db" - skip past "sqlite://" */
        if (strncmp(url, "sqlite://", 9) == 0) {
            dbPath = url + 9;
        } else {
            dbPath = url;
        }
    } else {
        snprintf(pathBuf, sizeof(pathBuf), "%s/arkime.db", config.configFile ? g_path_get_dirname(config.configFile) : ".");
        dbPath = pathBuf;
    }

    LOG("Opening SQLite database: %s", dbPath);

    int rc = sqlite3_open(dbPath, &sqliteDB);
    if (rc != SQLITE_OK) {
        LOGEXIT("ERROR - Can't open SQLite database %s: %s", dbPath, sqlite3_errmsg(sqliteDB));
    }

    /* Enable WAL mode for better concurrent read/write performance */
    sqlite3_exec(sqliteDB, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
    sqlite3_exec(sqliteDB, "PRAGMA synchronous=NORMAL", NULL, NULL, NULL);
    sqlite3_exec(sqliteDB, "PRAGMA busy_timeout=5000", NULL, NULL, NULL);

    ARKIME_LOCK_INIT(sqliteLock);

    sqliteStartTime = time(NULL);

    sqlite_create_tables();
    sqlite_prepare_statements();

    /* Register our send_bulk handler: no bulk header, include index in doc, 1 doc at a time */
    arkime_db_set_send_bulk2(arkime_db_sqlite_send_bulk, FALSE, FALSE, 1);
}
/******************************************************************************/
void arkime_db_sqlite_init()
{
    sqliteImpl.init = arkime_db_sqlite_init_impl;
    arkime_db_register("sqlite", &sqliteImpl);
}
