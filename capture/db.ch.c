/******************************************************************************/
/* db.ch.c -- ClickHouse session writer for Arkime capture
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <inttypes.h>

LOCAL void *chServer;
LOCAL char *chQueryPath;
LOCAL int   chQueryPathLen;

extern ArkimeConfig_t config;

LOCAL uint64_t chDocsSent;
LOCAL uint64_t chDocsAcked;
LOCAL uint64_t chDocsFailed;

LOCAL int arkime_db_ch_can_quit(void);
/******************************************************************************/
LOCAL void arkime_db_ch_send_bulk_cb(int code, uint8_t *data, int data_len, gpointer uw)
{
    const uint64_t docs = GPOINTER_TO_UINT(uw);
    if (code != 200) {
        ARKIME_THREAD_INCR_NUM(chDocsFailed, docs);
        LOG("ClickHouse bulk issue.  Code: %d docs: %" PRIu64 "\n%.*s", code, docs, data_len, data);
    } else {
        ARKIME_THREAD_INCR_NUM(chDocsAcked, docs);
        if (config.debug)
            LOG("CH bulk ack docs=%" PRIu64 " total sent=%" PRIu64 " acked=%" PRIu64 " failed=%" PRIu64, docs, chDocsSent, chDocsAcked, chDocsFailed);
        if (config.debug > 4)
            LOG("ClickHouse Bulk Reply code:%d :>%.*s<", code, data_len, data);
    }
}
/******************************************************************************/
LOCAL void arkime_db_ch_send_bulk(char *json, int len)
{
    // Count documents (newline separated) for send/ack accounting
    uint64_t docs = 0;
    for (int i = 0; i < len; i++) {
        if (json[i] == '\n')
            docs++;
    }
    if (len > 0 && json[len - 1] != '\n')
        docs++;

    ARKIME_THREAD_INCR_NUM(chDocsSent, docs);

    if (config.debug)
        LOG("CH bulk send docs=%" PRIu64 " len=%d total sent=%" PRIu64, docs, len, chDocsSent);
    if (config.debug > 4)
        LOG("CH Bulk len=%d: %.*s", len, MIN(len, 800), json);

    arkime_http_schedule(chServer, "POST", chQueryPath, chQueryPathLen,
                         json, len, NULL,
                         ARKIME_HTTP_PRIORITY_NORMAL,
                         arkime_db_ch_send_bulk_cb, GUINT_TO_POINTER((guint)docs));
}
/******************************************************************************/
LOCAL gboolean arkime_db_ch_parse_url(const char *url, char **hostUrl, char **userpwd)
{
    const char *httpScheme;

    if (g_str_has_prefix(url, "clickhouse://")) {
        httpScheme = "http";
        url += 13;
    } else if (g_str_has_prefix(url, "chttp://")) {
        httpScheme = "http";
        url += 8;
    } else if (g_str_has_prefix(url, "chttps://")) {
        httpScheme = "https";
        url += 9;
    } else {
        return FALSE;
    }

    const char *authorityEnd = url + strlen(url);
    for (const char *ptr = url; *ptr; ptr++) {
        if (*ptr == '/' || *ptr == '?' || *ptr == '#') {
            authorityEnd = ptr;
            break;
        }
    }

    const char *hostStart = url;
    const char *at = memchr(url, '@', authorityEnd - url);
    if (at) {
        char *userinfo = g_strndup(url, at - url);
        char *unescaped = g_uri_unescape_string(userinfo, NULL);
        *userpwd = unescaped ? unescaped : g_strdup(userinfo);
        g_free(userinfo);
        hostStart = at + 1;
    }

    if (hostStart >= authorityEnd) {
        g_free(*userpwd);
        *userpwd = NULL;
        return FALSE;
    }

    char *host = g_strndup(hostStart, authorityEnd - hostStart);
    *hostUrl = g_strdup_printf("%s://%s", httpScheme, host);
    g_free(host);

    return TRUE;
}
/******************************************************************************/
void arkime_db_ch_init(void)
{
    const char *sessionsDbUrl = arkime_config_str(NULL, "sessionsDbUrl", NULL);

    if (!sessionsDbUrl ||
        (!g_str_has_prefix(sessionsDbUrl, "clickhouse://") &&
         !g_str_has_prefix(sessionsDbUrl, "chttp://") &&
         !g_str_has_prefix(sessionsDbUrl, "chttps://"))) {
        return;
    }

    char *hostUrl = NULL;
    char *userpwd = NULL;
    if (!arkime_db_ch_parse_url(sessionsDbUrl, &hostUrl, &userpwd)) {
        LOG("ERROR - Couldn't parse sessionsDbUrl '%s'", sessionsDbUrl);
        return;
    }

    const char *clickhouseUser = arkime_config_str(NULL, "clickhouseUser", "");
    const char *clickhousePassword = arkime_config_str(NULL, "clickhousePassword", "");
    const char *clickhouseDatabase = arkime_config_str(NULL, "clickhouseDatabase", "arkime");
    const char *clickhouseSessionsTable = arkime_config_str(NULL, "clickhouseSessionsTable", "sessions3");
    const char *clickhouseCABundle = arkime_config_str(NULL, "clickhouseCABundle", NULL);
    gboolean clickhouseInsecure = arkime_config_boolean(NULL, "clickhouseInsecure", FALSE);
    uint32_t maxBatchDocs = arkime_config_int(NULL, "clickhouseMaxBatchDocs", 200, 1, 0xffff);
    uint32_t maxBatchBytes = arkime_config_int(NULL, "clickhouseMaxBatchBytes", 4 * 1024 * 1024, 1, 128 * 1024 * 1024);

    if (!userpwd && (clickhouseUser[0] || clickhousePassword[0]))
        userpwd = g_strdup_printf("%s:%s", clickhouseUser, clickhousePassword);

    chServer = arkime_http_create_server(hostUrl, config.maxESConns, config.maxESRequests, config.compressES);

    static char *headers[3] = {"Content-Type: application/json", "Expect:", NULL};
    arkime_http_set_headers(chServer, headers);
    arkime_http_set_print_errors(chServer);

    if (userpwd)
        arkime_http_set_userpwd(chServer, userpwd);

    if (clickhouseInsecure)
        arkime_http_set_insecure(chServer, TRUE);

    if (clickhouseCABundle)
        arkime_http_set_ca_trust_file(chServer, clickhouseCABundle);

    char *insert = g_strdup_printf("INSERT INTO %s.%s%s (fields, `_id`, lastPacket) SELECT data, CAST(data.`_id` AS String), fromUnixTimestamp64Milli(toInt64OrZero(toString(data.lastPacket))) FROM input('data JSON') FORMAT JSONAsObject", clickhouseDatabase, config.prefix, clickhouseSessionsTable);
    char *query = g_uri_escape_string(insert, NULL, FALSE);
    char *database = g_uri_escape_string(clickhouseDatabase, NULL, FALSE);
    chQueryPath = g_strdup_printf("/?database=%s&query=%s", database, query);
    chQueryPathLen = strlen(chQueryPath);

    config.dbBulkSize = maxBatchBytes;
    arkime_db_set_send_bulk2(arkime_db_ch_send_bulk, FALSE, TRUE, (uint16_t)maxBatchDocs);
    arkime_add_can_quit(arkime_db_ch_can_quit, "ClickHouse");

    // ClickHouse has no equivalent of ES's copy_to+analyzer, so capture emits
    // the *Tokens fields itself (see db.c)
    arkime_db_set_tokens_enabled(TRUE);

    LOG("ClickHouse sessions enabled: %s %s.%s%s", hostUrl, clickhouseDatabase, config.prefix, clickhouseSessionsTable);

    g_free(database);
    g_free(query);
    g_free(insert);
    g_free(hostUrl);
    g_free(userpwd);
}
/******************************************************************************/
/* Don't let capture exit while the final session batch is still in flight */
LOCAL int arkime_db_ch_can_quit(void)
{
    const int queueLen = chServer ? arkime_http_queue_length(chServer) : 0;
    if (queueLen > 0) {
        if (config.debug)
            LOG("Can't quit, ClickHouse queue length %d", queueLen);
        return 1;
    }
    return 0;
}
