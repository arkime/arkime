/******************************************************************************/
/* reader-scheme-s3.c
 *
 * Copyright 2023 All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <curl/curl.h>
#include "arkime.h"

extern ArkimeConfig_t        config;


LOCAL GHashTable            *servers;

LOCAL ARKIME_LOCK_DEFINE(waiting);

/******************************************************************************/
LOCAL void scheme_s3_done(int code, uint8_t *data, int data_len, gpointer UNUSED(uw))
{
    ARKIME_UNLOCK(waiting);
}
/******************************************************************************/
LOCAL int scheme_s3_read(uint8_t *data, int data_len, gpointer uw)
{
    return arkime_reader_scheme_process((char *)uw, data, data_len);
}
/******************************************************************************/
int scheme_s3_load(const char *uri)
{
    if (config.pcapSkip && arkime_db_file_exists(uri, NULL)) {
        if (config.debug)
            LOG("Skipping %s", uri);
        return 1;
    }

    if (config.pcapReprocess && !arkime_db_file_exists(uri, NULL)) {
        LOG("Can't reprocess %s", uri);
        return 1;
    }

    int rc;
    CURLU *h = curl_url();
    curl_url_set(h, CURLUPART_URL, uri, CURLU_NON_SUPPORT_SCHEME);

    char *scheme;
    rc = curl_url_get(h, CURLUPART_SCHEME, &scheme, 0);

    char *host;
    rc = curl_url_get(h, CURLUPART_HOST, &host, 0);

    char *port;
    rc = curl_url_get(h, CURLUPART_PORT, &port, 0);

    char *path;
    rc = curl_url_get(h, CURLUPART_PATH, &path, 0);


    char hostport[1000];
    snprintf(hostport, sizeof(hostport), "%s://%s:%s", scheme, host, port);

    void *server = g_hash_table_lookup(servers, hostport);
    if (!server) {
        server = arkime_http_create_server(hostport, 2, 2, TRUE);
        g_hash_table_insert(servers, g_strdup(hostport), server);
    }

    arkime_http_schedule2(server, "GET", path, -1, NULL, 0, NULL, ARKIME_HTTP_PRIORITY_NORMAL, scheme_s3_done, scheme_s3_read, (gpointer)uri);

    curl_free(scheme);
    curl_free(host);
    curl_free(port);
    curl_free(path);

    ARKIME_LOCK(waiting);
    ARKIME_LOCK(waiting);
    ARKIME_UNLOCK(waiting);

    return 0;
}
/******************************************************************************/
LOCAL void scheme_s3_exit()
{
}
/******************************************************************************/
void arkime_reader_scheme_s3_init()
{
    arkime_reader_scheme_register("s3", scheme_s3_load, scheme_s3_exit);
    arkime_reader_scheme_register("s3http", scheme_s3_load, scheme_s3_exit);
    arkime_reader_scheme_register("s3https", scheme_s3_load, scheme_s3_exit);
    servers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, arkime_http_free_server);
}
