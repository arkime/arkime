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


LOCAL GHashTable            *bucket2Region;
LOCAL char                  *s3Host;
LOCAL char                  *s3Region;
LOCAL gboolean               inited;
LOCAL gboolean               s3PathAccessStyle;

LOCAL char                   extraInfo[600];

/* S3Item is used to get the list of files from the http thread into the scheme thread */
typedef struct s3_item {
    struct s3_item  *item_next, *item_prev;
    char            *url;
} S3Item;

typedef struct {
    struct s3_item  *item_next, *item_prev;
    int              item_count;

    ARKIME_COND_EXTERN(lock);
    ARKIME_LOCK_EXTERN(lock);
    int     done;
} S3ItemHead;

S3ItemHead *s3Items;

LOCAL ARKIME_LOCK_DEFINE(waiting);
LOCAL ARKIME_LOCK_DEFINE(waitingdir);

/* S3Request is used to pass information from the scheme thread to the http thread */
typedef struct s3_request {
    ArkimeSchemeAction_t  *actions;
    const char            *url;
    char                  *continuation; // Continuation token, http thread -> scheme thread
    uint8_t                isDir : 1;    // Doint a prefix match
    uint8_t                isS3 : 1;     // Use S3 URL
    uint8_t                tryAgain : 1; // Try again because wrong region
    uint8_t                first : 1;    // The first attemp at url
} S3Request;


/******************************************************************************/
LOCAL S3ItemHead *s3_alloc()
{
    S3ItemHead *head = ARKIME_TYPE_ALLOC0(S3ItemHead);
    DLL_INIT(item_, head);
    ARKIME_LOCK_INIT(head->lock);
    ARKIME_COND_INIT(head->lock);
    return head;
}
/******************************************************************************/
LOCAL void s3_enqueue(S3ItemHead *head, const char *url)
{

    ARKIME_LOCK(head->lock);
    S3Item *item = ARKIME_TYPE_ALLOC0(S3Item);
    item->url = g_strdup(url);
    DLL_PUSH_TAIL(item_, s3Items, item);

    ARKIME_COND_SIGNAL(head->lock);
    ARKIME_UNLOCK(head->lock);
}
/******************************************************************************/
LOCAL void scheme_s3_init()
{
    s3Host = arkime_config_str(NULL, "s3Host", NULL);
    s3Region = arkime_config_str(NULL, "s3Region", "us-east-1");
    config.gapPacketPos = arkime_config_boolean(NULL, "s3GapPacketPos", TRUE);
    s3PathAccessStyle = arkime_config_boolean(NULL, "s3PathAccessStyle", FALSE);
    inited = TRUE;
    s3Items = s3_alloc();
}
/******************************************************************************/
LOCAL void scheme_s3_parse_region(const uint8_t *data, int data_len, const char *bucket, S3Request *req)
{
    const char *wrong = arkime_memstr((const char *)data, data_len, "' is wrong; expecting '", 23);
    if (wrong) {
        wrong += 23;
        const char *end = arkime_memstr(wrong, data_len - (wrong - (char *)data), "'", 1);
        if (end) {
            char *region = g_strndup(wrong, end - wrong);
            g_hash_table_insert(bucket2Region, g_strdup(bucket), region);
            req->tryAgain = TRUE;
        }
    } else {
        LOG("ERROR - %.*s", data_len, data);
    }
}

/******************************************************************************/
LOCAL char *scheme_s3_escape(const char *str, int len)
{
    char out[3000];
    int  s;
    int  o;

    out[0] = 0;

    for (s = o = 0; s < len && o + 3 < (int)sizeof(out); s++) {
        if (str[s] == '+') {
            out[o++] = '%';
            out[o++] = '2';
            out[o++] = 'B';
        } else if (str[s] == '=') {
            out[o++] = '%';
            out[o++] = '3';
            out[o++] = 'D';
        } else {
            out[o++] = str[s];
        }
    }
    return g_strndup(out, o);
}
/******************************************************************************/
LOCAL void scheme_s3_done(int UNUSED(code), uint8_t *data, int data_len, gpointer uw)
{
    S3Request *req = uw;
    if (!req->isDir) {
        ARKIME_UNLOCK(waiting);
        return;
    }

    if (req->first) {
        req->first = FALSE;
        if (data_len > 200 && arkime_memstr((char *)data, MIN(400, data_len), "' is wrong; expecting '", 23)) {
            scheme_s3_parse_region(data, data_len, req->url, req);
            ARKIME_UNLOCK(waitingdir);
            return;
        }
    }

    ARKIME_UNLOCK(waitingdir);

    const char *next = arkime_memstr((const char *)data, data_len, "<NextContinuationToken>", 23);

    if (next) {
        const char *endNext = arkime_memstr((const char *)data, data_len, "</NextContinuationToken>", 24);
        if (next < endNext) {
            next += 23;
            req->continuation = scheme_s3_escape(next, endNext - next);
        }
    }

    char *start = (char *)data;
    while (start < (char *)data + data_len) {
        char *key = strstr(start, "<Key>");
        if (!key)
            break;
        key += 5;
        char *end = strstr(key, "</Key>");
        if (!end)
            break;
        *end = 0;
        start = end + 6;

        if (!g_regex_match(config.offlineRegex, key, 0, NULL)) {
            continue;
        }

        char uri[2000];
        if (req->isS3) {
            snprintf(uri, sizeof(uri), "s3://%s/%s", req->url, key);
        } else {
            snprintf(uri, sizeof(uri), "s3%s/%s", req->url, key);
        }

        s3_enqueue(s3Items, uri);
    }

    s3Items->done = 1;
}
/******************************************************************************/
LOCAL int scheme_s3_read(uint8_t *data, int data_len, gpointer uw)
{
    S3Request *req = uw;
    if (req->first) {
        req->first = FALSE;
        if (data_len > 10 && data[0] == '<') {
            char **uris = g_strsplit(req->url, "/", 4);
            scheme_s3_parse_region(data, data_len, uris[2], req);
            g_strfreev(uris);
            return 1;
        }
    }
    return arkime_reader_scheme_process(req->url, data, data_len, extraInfo, req->actions);
}
/******************************************************************************/
LOCAL void scheme_s3_request(void *server, const ArkimeCredentials_t *creds, const char *path, const char *bucket, S3Request *req, gboolean pathStyle, ArkimeHttpRead_cb cb)
{
    char           objectkey[1000];

    if (pathStyle)
        snprintf(objectkey, sizeof(objectkey), "/%s%s", bucket, path);
    else
        snprintf(objectkey, sizeof(objectkey), "%s", path);

    if (config.debug)
        LOG("objectkey: %s", objectkey);

    char *headers[8];
    headers[0] = "Expect:";
    if (pathStyle) {
        headers[1] = NULL;
    } else {
        headers[1] = "Content-Type:";
    }
    headers[2] = NULL;
    headers[3] = NULL;

    char tokenHeader[1000];
    if (creds->token) {
        snprintf(tokenHeader, sizeof(tokenHeader), "X-Amz-Security-Token: %s", creds->token);
        headers[2] = tokenHeader;
    }

    req->first = TRUE;
    req->tryAgain = FALSE;
    if (cb)
        arkime_http_schedule2(server, "GET", objectkey, -1, NULL, 0, headers, ARKIME_HTTP_PRIORITY_NORMAL, scheme_s3_done, cb, req);
    else
        arkime_http_schedule(server, "GET", objectkey, -1, NULL, 0, headers, ARKIME_HTTP_PRIORITY_NORMAL, scheme_s3_done, req);
}
/******************************************************************************/
LOCAL void *scheme_s3_make_server(const ArkimeCredentials_t *creds, const char *schemehostport, const char *region)
{
    int isNew;
    void *server = arkime_http_get_or_create_server(schemehostport, schemehostport, 2, 2, TRUE, &isNew);
    if (isNew) {
        char userpwd[100];
        snprintf(userpwd, sizeof(userpwd), "%s:%s", creds->id, creds->key);
        arkime_http_set_userpwd(server, userpwd);

        char aws_sigv4[100];
        snprintf(aws_sigv4, sizeof(aws_sigv4), "aws:amz:%s:s3", region);
        arkime_http_set_aws_sigv4(server, aws_sigv4);

        arkime_http_set_timeout(server, 0);
    }
    return server;
}
/******************************************************************************/
LOCAL int scheme_s3_load_dir(const char *dir, ArkimeSchemeFlags flags, ArkimeSchemeAction_t *actions)
{
    char **uris = g_strsplit(dir, "/", 4);

    char uri[2000];
    if (uris[3]) {
        snprintf(uri, sizeof(uri), "s3://%s/?list-type=2&prefix=%s", uris[2], uris[3]);
    } else {
        snprintf(uri, sizeof(uri), "s3://%s/?list-type=2", uris[2]);
    }

    S3Request req = {
        .actions = actions,
        .url = uris[2],
        .isDir = TRUE,
        .isS3 = TRUE,
        .tryAgain = FALSE,
        .first = TRUE
    };

    s3Items->done = 0;

    void *server;
    char  hostport[256];
    const char *region;

    const ArkimeCredentials_t *creds = arkime_credentials_get("s3", "s3AccessKeyId", "s3SecretAccessKey");

    do {
        region = g_hash_table_lookup(bucket2Region, uris[2]);
        if (!region) {
            region = s3Region;
        }

        if (s3Host) {
            snprintf(hostport, sizeof(hostport), "%s.%s", uris[2], s3Host);
        } else if (strcmp(region, "us-east-1") == 0) {
            snprintf(hostport, sizeof(hostport), "%s.s3.amazonaws.com", uris[2]);
        } else {
            snprintf(hostport, sizeof(hostport), "%s.s3-%s.amazonaws.com", uris[2], region);
        }

        char schemehostport[300];
        snprintf(schemehostport, sizeof(schemehostport), "https://%s", hostport);

        server = scheme_s3_make_server(creds, schemehostport, region);

        scheme_s3_request(server, creds, uri + 5 + strlen(uris[2]), uris[2], &req, s3PathAccessStyle, NULL);

        ARKIME_LOCK(waitingdir);
        ARKIME_LOCK(waitingdir);
        ARKIME_UNLOCK(waitingdir);
    } while (req.tryAgain);

    while (!s3Items->done || DLL_COUNT(item_, s3Items) > 0) {
        if (req.continuation) {
            char uri2[3000];

            if (uris[3]) {
                snprintf(uri2, sizeof(uri2), "s3://%s/?continuation-token=%s&list-type=2&prefix=%s", uris[2], req.continuation, uris[3]);
            } else {
                snprintf(uri2, sizeof(uri2), "s3://%s/?continuation-token=%s&list-type=2", uris[2], req.continuation);
            }

            g_free(req.continuation);
            req.continuation = NULL;

            scheme_s3_request(server, creds, uri2 + 5 + strlen(uris[2]), uris[2], &req, s3PathAccessStyle, NULL);
            ARKIME_LOCK(waitingdir);
        }
        ARKIME_LOCK(s3Items->lock);
        while (DLL_COUNT(item_, s3Items) == 0) {
            ARKIME_COND_WAIT(s3Items->lock);
        }
        S3Item *item;
        DLL_POP_HEAD(item_, s3Items, item);
        ARKIME_UNLOCK(s3Items->lock);
        arkime_reader_scheme_load(item->url, flags & (ArkimeSchemeFlags)(~ARKIME_SCHEME_FLAG_DIRHINT), actions);
        g_free(item->url);
        ARKIME_TYPE_FREE(S3Item, item);
    }
    g_strfreev(uris);
    return 1;
}
/******************************************************************************/
LOCAL int scheme_s3_load_full_dir(const char *dir, ArkimeSchemeFlags flags, ArkimeSchemeAction_t *actions)
{
    CURLU *h = curl_url();
    curl_url_set(h, CURLUPART_URL, dir, CURLU_NON_SUPPORT_SCHEME);

    char *scheme;
    curl_url_get(h, CURLUPART_SCHEME, &scheme, 0);

    char *host;
    curl_url_get(h, CURLUPART_HOST, &host, 0);

    char *port;
    curl_url_get(h, CURLUPART_PORT, &port, 0);

    char *path;
    curl_url_get(h, CURLUPART_PATH, &path, 0);

    char **paths = g_strsplit(path, "/", 3);  // Split into at most 3: empty, bucket, prefix

    char schemehostport[300];
    if (port)
        snprintf(schemehostport, sizeof(schemehostport), "%s://%s:%s", scheme + 2, host, port);
    else
        snprintf(schemehostport, sizeof(schemehostport), "%s://%s", scheme + 2, host);

    char hostport[256];
    if (port)
        snprintf(hostport, sizeof(hostport), "%s:%s", host, port);
    else
        snprintf(hostport, sizeof(hostport), "%s", host);

    char region[100];
    g_strlcpy(region, s3Region, sizeof(region)); // default

    char *s3 = strstr(host, ".s3-");
    if (s3) {
        s3 += 4;
        const char *dot = strchr(s3, '.');
        if (dot && dot - s3 < (int)sizeof(region)) {
            memcpy(region, s3, dot - s3);
            region[dot - s3] = 0;
        }
    }

    const ArkimeCredentials_t *creds = arkime_credentials_get("s3", "s3AccessKeyId", "s3SecretAccessKey");
    void *server = scheme_s3_make_server(creds, schemehostport, region);

    char shpb[1000];
    snprintf(shpb, sizeof(shpb), "%s/%s", schemehostport, paths[1]);

    char uri[2000];
    if (paths[2] && paths[2][0] != 0) {
        snprintf(uri, sizeof(uri), "%s/?list-type=2&prefix=%s", shpb, paths[2]);
    } else {
        snprintf(uri, sizeof(uri), "%s/?list-type=2", shpb);
    }

    S3Request req = {
        .actions = actions,
        .url = shpb,
        .isDir = TRUE,
        .isS3 = FALSE,
        .tryAgain = FALSE,
        .first = TRUE
    };

    scheme_s3_request(server, creds, uri + strlen(shpb), paths[1], &req, TRUE, NULL);

    curl_free(scheme);
    curl_free(host);
    curl_free(port);
    curl_free(path);
    curl_url_cleanup(h);

    while (!s3Items->done || DLL_COUNT(item_, s3Items) > 0) {
        if (req.continuation) {
            char uri2[3000];

            if (paths[2] && paths[2][0] != 0) {
                snprintf(uri2, sizeof(uri2), "%s/?continuation-token=%s&list-type=2&prefix=%s", shpb, req.continuation, paths[2]);
            } else {
                snprintf(uri2, sizeof(uri2), "%s/?continuation-token=%s&list-type=2", shpb, req.continuation);
            }

            g_free(req.continuation);
            req.continuation = NULL;

            scheme_s3_request(server, creds, uri2 + strlen(shpb), paths[1], &req, TRUE, NULL);
            ARKIME_LOCK(waitingdir);
        }
        ARKIME_LOCK(s3Items->lock);
        while (DLL_COUNT(item_, s3Items) == 0) {
            ARKIME_COND_WAIT(s3Items->lock);
        }
        S3Item *item;
        DLL_POP_HEAD(item_, s3Items, item);
        ARKIME_UNLOCK(s3Items->lock);
        arkime_reader_scheme_load(item->url, flags & (ArkimeSchemeFlags)(~ARKIME_SCHEME_FLAG_DIRHINT), actions);
        g_free(item->url);
        ARKIME_TYPE_FREE(S3Item, item);
    }
    g_strfreev(paths);
    return 1;
}
/******************************************************************************/
// s3://bucketname/path
LOCAL int scheme_s3_load(const char *uri, ArkimeSchemeFlags flags, ArkimeSchemeAction_t *actions)
{
    if (!inited)
        scheme_s3_init();

    if ((flags & ARKIME_SCHEME_FLAG_DIRHINT) || g_str_has_suffix(uri, "/")) {
        return scheme_s3_load_dir(uri, flags, actions);
    }

    if ((flags & ARKIME_SCHEME_FLAG_SKIP) && arkime_db_file_exists(uri, NULL)) {
        if (config.debug)
            LOG("Skipping %s", uri);
        return 1;
    }

    if (config.pcapReprocess && !arkime_db_file_exists(uri, NULL)) {
        LOG("Can't reprocess %s", uri);
        return 1;
    }

    char **uris = g_strsplit(uri, "/", 0);

    S3Request req = {
        .actions = actions,
        .url = uri,
        .isDir = FALSE,
        .isS3 = TRUE,
        .tryAgain = FALSE,
        .first = TRUE
    };

    const ArkimeCredentials_t *creds = arkime_credentials_get("s3", "s3AccessKeyId", "s3SecretAccessKey");

    do {
        const char *region = g_hash_table_lookup(bucket2Region, uris[2]);
        if (!region) {
            region = s3Region;
        }

        char hostport[256];
        if (s3Host) {
            snprintf(hostport, sizeof(hostport), "%s.%s", uris[2], s3Host);
        } else if (strcmp(region, "us-east-1") == 0) {
            snprintf(hostport, sizeof(hostport), "%s.s3.amazonaws.com", uris[2]);
        } else {
            snprintf(hostport, sizeof(hostport), "%s.s3-%s.amazonaws.com", uris[2], region);
        }

        char schemehostport[300];
        snprintf(schemehostport, sizeof(schemehostport), "https://%s", hostport);

        void *server = scheme_s3_make_server(creds, schemehostport, region);

        snprintf(extraInfo, sizeof(extraInfo), "{\"endpoint\":\"%s\",\"bucket\":\"%s\",\"region\":\"%s\",\"path\":\"%s\", \"pathStyle\": %s}",
                 schemehostport,
                 uris[2],
                 region,
                 uri + 5 + strlen(uris[2]),
                 s3PathAccessStyle ? "true" : "false");
        scheme_s3_request(server, creds, uri + 5 + strlen(uris[2]), uris[2], &req, s3PathAccessStyle, scheme_s3_read);

        ARKIME_LOCK(waiting);
        ARKIME_LOCK(waiting);
        ARKIME_UNLOCK(waiting);
    } while (req.tryAgain);
    g_strfreev(uris);

    return 0;
}
/******************************************************************************/
// s3http://hostport/bucketname/key
// s3https://hostport/bucketname/key
LOCAL int scheme_s3_load_full(const char *uri, ArkimeSchemeFlags flags, ArkimeSchemeAction_t *actions)
{
    if (!inited)
        scheme_s3_init();

    if ((flags & ARKIME_SCHEME_FLAG_DIRHINT) || g_str_has_suffix(uri, "/")) {
        return scheme_s3_load_full_dir(uri, flags, actions);
    }

    if ((flags & ARKIME_SCHEME_FLAG_SKIP) && arkime_db_file_exists(uri, NULL)) {
        if (config.debug)
            LOG("Skipping %s", uri);
        return 1;
    }

    if (config.pcapReprocess && !arkime_db_file_exists(uri, NULL)) {
        LOG("Can't reprocess %s", uri);
        return 1;
    }

    CURLU *h = curl_url();
    curl_url_set(h, CURLUPART_URL, uri, CURLU_NON_SUPPORT_SCHEME);

    char *scheme;
    curl_url_get(h, CURLUPART_SCHEME, &scheme, 0);

    char *host;
    curl_url_get(h, CURLUPART_HOST, &host, 0);

    char *port;
    curl_url_get(h, CURLUPART_PORT, &port, 0);

    char *path;
    curl_url_get(h, CURLUPART_PATH, &path, 0);

    char **paths = g_strsplit(path, "/", 0);

    char schemehostport[300];
    if (port)
        snprintf(schemehostport, sizeof(schemehostport), "%s://%s:%s", scheme + 2, host, port);
    else
        snprintf(schemehostport, sizeof(schemehostport), "%s://%s", scheme + 2, host);

    char hostport[256];
    if (port)
        snprintf(hostport, sizeof(hostport), "%s:%s", host, port);
    else
        snprintf(hostport, sizeof(hostport), "%s", host);

    const ArkimeCredentials_t *creds = arkime_credentials_get("s3", "s3AccessKeyId", "s3SecretAccessKey");

    char region[100];
    g_strlcpy(region, s3Region, sizeof(region)); // default

    char *s3 = strstr(host, ".s3-");
    if (s3) {
        s3 += 4;
        const char *dot = strchr(s3, '.');
        if (dot && dot - s3 < (int)sizeof(region)) {
            memcpy(region, s3, dot - s3);
            region[dot - s3] = 0;
        }
    }

    void *server = scheme_s3_make_server(creds, schemehostport, region);

    snprintf(extraInfo, sizeof(extraInfo), "{\"endpoint\":\"%s\",\"bucket\":\"%s\",\"region\":\"%s\",\"path\":\"%s\", \"pathStyle\": true}",
             schemehostport,
             paths[1],
             region,
             path + 1 + strlen(paths[1]));
    if (config.debug)
        LOG("extraInfo: %s", extraInfo);

    S3Request req = {
        .actions = actions,
        .url = uri,
        .isDir = FALSE,
        .isS3 = FALSE,
        .tryAgain = FALSE,
        .first = TRUE
    };

    scheme_s3_request(server, creds, path + 1 + strlen(paths[1]), paths[1], &req, TRUE, scheme_s3_read);

    curl_free(scheme);
    curl_free(host);
    curl_free(port);
    curl_free(path);
    g_strfreev(paths);
    curl_url_cleanup(h);

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
    arkime_reader_scheme_register("s3http", scheme_s3_load_full, scheme_s3_exit);
    arkime_reader_scheme_register("s3https", scheme_s3_load_full, scheme_s3_exit);
    bucket2Region = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
}
