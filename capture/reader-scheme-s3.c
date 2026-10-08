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
    char                  *extraInfo;    // JSON describing the object, saved in the files record
    uint8_t                isDir : 1;    // Doing a prefix match
    uint8_t                isS3 : 1;     // Use S3 URL
    uint8_t                tryAgain : 1; // Try again because wrong region
    uint8_t                first : 1;    // The first attempt at url
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
    DLL_PUSH_TAIL(item_, head, item);

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
            int rlen = end - wrong;
            gboolean ok = (rlen > 0 && rlen < 40);
            for (int i = 0; ok && i < rlen; i++) {
                char c = wrong[i];
                if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
                    ok = FALSE;
            }
            if (ok) {
                g_hash_table_insert(bucket2Region, g_strdup(bucket), g_strndup(wrong, rlen));
                req->tryAgain = TRUE;
            } else {
                LOG("ERROR - Ignoring invalid region in S3 response");
            }
        }
    } else {
        LOG("ERROR - %.*s", data_len, data);
    }
}

/******************************************************************************/
// Bucket names end up in hostnames and paths, so only allow host safe names.
// Also allows the legacy us-east-1 uppercase, underscore, and length rules.
gboolean arkime_reader_scheme_s3_valid_bucket(const char *bucket)
{
    int len = strlen(bucket);
    if (len < 3 || len > 255)
        return FALSE;

    if (!g_ascii_isalnum(bucket[0]) || !g_ascii_isalnum(bucket[len - 1]))
        return FALSE;

    for (int i = 0; i < len; i++) {
        if (!g_ascii_isalnum(bucket[i]) && bucket[i] != '.' && bucket[i] != '-' && bucket[i] != '_')
            return FALSE;
    }

    return strstr(bucket, "..") == NULL;
}
/******************************************************************************/
// ListObjects keys are XML escaped
LOCAL char *scheme_s3_xml_unescape(const char *str)
{
    GString *out = g_string_sized_new(strlen(str));

    while (*str) {
        if (*str != '&') {
            g_string_append_c(out, *str++);
            continue;
        }

        const char *semi = strchr(str, ';');
        if (!semi)
            goto bad;

        int len = semi - str + 1;
        if (len == 5 && memcmp(str, "&amp;", 5) == 0) {
            g_string_append_c(out, '&');
        } else if (len == 4 && memcmp(str, "&lt;", 4) == 0) {
            g_string_append_c(out, '<');
        } else if (len == 4 && memcmp(str, "&gt;", 4) == 0) {
            g_string_append_c(out, '>');
        } else if (len == 6 && memcmp(str, "&quot;", 6) == 0) {
            g_string_append_c(out, '"');
        } else if (len == 6 && memcmp(str, "&apos;", 6) == 0) {
            g_string_append_c(out, '\'');
        } else if (str[1] == '#') {
            char *end;
            gunichar c;
            if (str[2] == 'x' || str[2] == 'X')
                c = strtoul(str + 3, &end, 16);
            else
                c = strtoul(str + 2, &end, 10);
            if (end != semi || c == 0 || !g_unichar_validate(c))
                goto bad;
            g_string_append_unichar(out, c);
        } else {
            goto bad;
        }
        str = semi + 1;
    }
    return g_string_free(out, FALSE);

bad:
    g_string_free(out, TRUE);
    return NULL;
}
/******************************************************************************/
// Decode a uri encoded key, reject . and .. segments, and return the key
// encoded the way SigV4 expects. Sets *raw to the decoded key.
LOCAL char *scheme_s3_key_encode(const char *encoded, char **raw)
{
    char *key = g_uri_unescape_string(encoded, NULL);
    if (!key || !*key) {
        g_free(key);
        return NULL;
    }

    char **segs = g_strsplit(key, "/", 0);
    for (int i = 0; segs[i]; i++) {
        if (strcmp(segs[i], ".") == 0 || strcmp(segs[i], "..") == 0) {
            g_strfreev(segs);
            g_free(key);
            return NULL;
        }
    }
    g_strfreev(segs);

    *raw = key;
    return g_uri_escape_string(key, "/", FALSE);
}
/******************************************************************************/
LOCAL char *scheme_s3_escape(const char *str, int len)
{
    // Worst case: every char expands to 3 bytes (%XX)
    char *out = g_malloc(len * 3 + 1);
    int  s;
    int  o;

    for (s = o = 0; s < len; s++) {
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
    out[o] = '\0';
    return out;
}
/******************************************************************************/
LOCAL void scheme_s3_done(int code, uint8_t *data, int data_len, gpointer uw)
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

    if (code < 200 || code >= 300) {
        LOG("ERROR - S3 list request failed with HTTP %d for %s", code, req->url);
        ARKIME_LOCK(s3Items->lock);
        s3Items->done = 1;
        ARKIME_COND_BROADCAST(s3Items->lock);
        ARKIME_UNLOCK(s3Items->lock);
        return;
    }

    const char *next = arkime_memstr((const char *)data, data_len, "<NextContinuationToken>", 23);

    if (next) {
        const char *endNext = arkime_memstr((const char *)data, data_len, "</NextContinuationToken>", 24);
        if (endNext && next < endNext) {
            next += 23;
            req->continuation = scheme_s3_escape(next, endNext - next);
        }
    }

    const char *start = (char *)data;
    const char *dataEnd = (char *)data + data_len;
    while (start < dataEnd) {
        char *key = (char *)arkime_memstr(start, dataEnd - start, "<Key>", 5);
        if (!key)
            break;
        key += 5;
        char *end = (char *)arkime_memstr(key, dataEnd - key, "</Key>", 6);
        if (!end)
            break;
        *end = 0;
        start = end + 6;

        char *rawKey = scheme_s3_xml_unescape(key);
        if (!rawKey) {
            LOG("WARNING - Skipping S3 key with bad XML escaping: %s", key);
            continue;
        }

        if (!g_regex_match(config.offlineRegex, rawKey, 0, NULL)) {
            g_free(rawKey);
            continue;
        }

        char *encKey = g_uri_escape_string(rawKey, "/", FALSE);
        g_free(rawKey);

        char uri[2000];
        if (req->isS3) {
            snprintf(uri, sizeof(uri), "s3://%s/%s", req->url, encKey);
        } else {
            snprintf(uri, sizeof(uri), "s3%s/%s", req->url, encKey);
        }
        g_free(encKey);

        s3_enqueue(s3Items, uri);
    }

    ARKIME_LOCK(s3Items->lock);
    s3Items->done = 1;
    ARKIME_COND_BROADCAST(s3Items->lock);
    ARKIME_UNLOCK(s3Items->lock);
}
/******************************************************************************/
LOCAL char *scheme_s3_extra(const char *endpoint, const char *bucket, const char *region, const char *path, gboolean pathStyle)
{
    // Escaping can expand each byte to 6
    int size = 6 * (strlen(endpoint) + strlen(bucket) + strlen(region) + strlen(path)) + 100;
    char *extraInfo = g_malloc(size);

    BSB bsb;
    BSB_INIT(bsb, extraInfo, size);
    BSB_EXPORT_cstr(bsb, "{\"endpoint\":");
    arkime_db_js0n_str(&bsb, (uint8_t *)endpoint, TRUE);
    BSB_EXPORT_cstr(bsb, ",\"bucket\":");
    arkime_db_js0n_str(&bsb, (uint8_t *)bucket, TRUE);
    BSB_EXPORT_cstr(bsb, ",\"region\":");
    arkime_db_js0n_str(&bsb, (uint8_t *)region, TRUE);
    BSB_EXPORT_cstr(bsb, ",\"path\":");
    arkime_db_js0n_str(&bsb, (uint8_t *)path, TRUE);
    BSB_EXPORT_sprintf(bsb, ", \"pathStyle\": %s}", pathStyle ? "true" : "false");
    BSB_EXPORT_u08(bsb, 0);

    if (config.debug)
        LOG("extraInfo: %s", extraInfo);

    return extraInfo;
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
    return arkime_reader_scheme_process(req->url, data, data_len, req->extraInfo, req->actions);
}
/******************************************************************************/
LOCAL gboolean scheme_s3_request(void *server, const ArkimeCredentials_t *creds, const char *path, const char *bucket, S3Request *req, gboolean pathStyle, ArkimeHttpRead_cb cb)
{
    char           objectkey[1600];

    if (pathStyle)
        snprintf(objectkey, sizeof(objectkey), "/%s%s", bucket, path);
    else
        snprintf(objectkey, sizeof(objectkey), "%s", path);

    if (config.debug)
        LOG("objectkey: %s", objectkey);

    char *headers[8];
    int   hi = 0;
    headers[hi++] = "Expect:";
    if (!pathStyle) {
        headers[hi++] = "Content-Type:";
    }

    char tokenHeader[1000];
    if (creds->token) {
        snprintf(tokenHeader, sizeof(tokenHeader), "X-Amz-Security-Token: %s", creds->token);
        headers[hi++] = tokenHeader;
    }
    headers[hi] = NULL;

    req->first = TRUE;
    req->tryAgain = FALSE;
    if (cb)
        return arkime_http_schedule2(server, "GET", objectkey, -1, NULL, 0, headers, ARKIME_HTTP_PRIORITY_NORMAL, scheme_s3_done, cb, req);
    return arkime_http_schedule(server, "GET", objectkey, -1, NULL, 0, headers, ARKIME_HTTP_PRIORITY_NORMAL, scheme_s3_done, req);
}
/******************************************************************************/
LOCAL void *scheme_s3_make_server(const ArkimeCredentials_t *creds, const char *schemehostport, const char *region)
{
    int isNew;
    void *server = arkime_http_get_or_create_server(schemehostport, schemehostport, 2, 2, TRUE, &isNew);
    if (isNew) {
        char userpwd[256];
        snprintf(userpwd, sizeof(userpwd), "%s:%s", creds->id, creds->key);
        arkime_http_set_userpwd(server, userpwd);

        char aws_sigv4[256];
        snprintf(aws_sigv4, sizeof(aws_sigv4), "aws:amz:%s:s3", region);
        arkime_http_set_aws_sigv4(server, aws_sigv4);

        arkime_http_set_timeout(server, 0);
    }
    return server;
}
/******************************************************************************/
// The prefix comes from a uri so may be encoded, decode it and then encode it as a query value
LOCAL char *scheme_s3_prefix_encode(const char *prefix)
{
    if (!prefix || !*prefix)
        return NULL;

    char *raw = g_uri_unescape_string(prefix, NULL);
    char *encoded = g_uri_escape_string(raw ? raw : prefix, NULL, FALSE);
    g_free(raw);
    return encoded;
}
/******************************************************************************/
LOCAL int scheme_s3_load_dir(const char *dir, ArkimeSchemeFlags flags, ArkimeSchemeAction_t *actions)
{
    char **uris = g_strsplit(dir, "/", 4);

    if (!uris[0] || !uris[1] || !uris[2] || !uris[2][0]) {
        LOG("ERROR - Invalid S3 dir uri %s", dir);
        g_strfreev(uris);
        return 1;
    }

    if (!arkime_reader_scheme_s3_valid_bucket(uris[2])) {
        LOG("ERROR - Invalid S3 bucket %s", dir);
        g_strfreev(uris);
        return 1;
    }

    char *prefix = scheme_s3_prefix_encode(uris[3]);
    char uri[2000];
    if (prefix) {
        snprintf(uri, sizeof(uri), "s3://%s/?list-type=2&prefix=%s", uris[2], prefix);
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

    while (!s3Items->done || DLL_COUNT(item_, s3Items) > 0 || req.continuation) {
        if (req.continuation) {
            char *uri2;

            if (prefix) {
                uri2 = g_strdup_printf("s3://%s/?continuation-token=%s&list-type=2&prefix=%s", uris[2], req.continuation, prefix);
            } else {
                uri2 = g_strdup_printf("s3://%s/?continuation-token=%s&list-type=2", uris[2], req.continuation);
            }

            g_free(req.continuation);
            req.continuation = NULL;

            // Another page is coming, clear done before it can be set again
            ARKIME_LOCK(s3Items->lock);
            s3Items->done = 0;
            ARKIME_UNLOCK(s3Items->lock);

            scheme_s3_request(server, creds, uri2 + 5 + strlen(uris[2]), uris[2], &req, s3PathAccessStyle, NULL);
            g_free(uri2);
            ARKIME_LOCK(waitingdir);
        }
        ARKIME_LOCK(s3Items->lock);
        while (DLL_COUNT(item_, s3Items) == 0 && !s3Items->done) {
            ARKIME_COND_WAIT(s3Items->lock);
        }
        if (DLL_COUNT(item_, s3Items) == 0) {
            ARKIME_UNLOCK(s3Items->lock);
            if (req.continuation) // Empty page, but more pages to fetch
                continue;
            break;
        }
        S3Item *item;
        DLL_POP_HEAD(item_, s3Items, item);
        ARKIME_UNLOCK(s3Items->lock);
        arkime_reader_scheme_load(item->url, flags & (ArkimeSchemeFlags)(~ARKIME_SCHEME_FLAG_DIRHINT), actions);
        g_free(item->url);
        ARKIME_TYPE_FREE(S3Item, item);
    }
    g_free(prefix);
    g_strfreev(uris);
    return 1;
}
/******************************************************************************/
LOCAL int scheme_s3_load_full_dir(const char *dir, ArkimeSchemeFlags flags, ArkimeSchemeAction_t *actions)
{
    CURLU *h = curl_url();
    curl_url_set(h, CURLUPART_URL, dir, CURLU_NON_SUPPORT_SCHEME);

    int rc = 0;
    char *scheme = NULL;
    rc += curl_url_get(h, CURLUPART_SCHEME, &scheme, 0);

    char *host = NULL;
    rc += curl_url_get(h, CURLUPART_HOST, &host, 0);

    char *port = NULL;
    curl_url_get(h, CURLUPART_PORT, &port, 0);

    char *path = NULL;
    rc += curl_url_get(h, CURLUPART_PATH, &path, 0);

    if (rc) {
        LOG("Error parsing %s", dir);
        curl_free(scheme);
        curl_free(host);
        curl_free(port);
        curl_free(path);
        curl_url_cleanup(h);
        return 1;
    }

    char **paths = g_strsplit(path, "/", 3);  // Split into at most 3: empty, bucket, prefix

    if (!paths[0] || !paths[1] || paths[1][0] == 0 || !arkime_reader_scheme_s3_valid_bucket(paths[1])) {
        LOG("ERROR - S3 directory URL missing or invalid bucket: %s", dir);
        g_strfreev(paths);
        curl_free(scheme);
        curl_free(host);
        curl_free(port);
        curl_free(path);
        curl_url_cleanup(h);
        return 1;
    }

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

    char *prefix = scheme_s3_prefix_encode(paths[2]);
    char uri[2000];
    if (prefix) {
        snprintf(uri, sizeof(uri), "%s/?list-type=2&prefix=%s", shpb, prefix);
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

    s3Items->done = 0;

    scheme_s3_request(server, creds, uri + strlen(shpb), paths[1], &req, TRUE, NULL);

    curl_free(scheme);
    curl_free(host);
    curl_free(port);
    curl_free(path);
    curl_url_cleanup(h);

    while (!s3Items->done || DLL_COUNT(item_, s3Items) > 0 || req.continuation) {
        if (req.continuation) {
            char *uri2;

            if (prefix) {
                uri2 = g_strdup_printf("%s/?continuation-token=%s&list-type=2&prefix=%s", shpb, req.continuation, prefix);
            } else {
                uri2 = g_strdup_printf("%s/?continuation-token=%s&list-type=2", shpb, req.continuation);
            }

            g_free(req.continuation);
            req.continuation = NULL;

            // Another page is coming, clear done before it can be set again
            ARKIME_LOCK(s3Items->lock);
            s3Items->done = 0;
            ARKIME_UNLOCK(s3Items->lock);

            scheme_s3_request(server, creds, uri2 + strlen(shpb), paths[1], &req, TRUE, NULL);
            g_free(uri2);
            ARKIME_LOCK(waitingdir);
        }
        ARKIME_LOCK(s3Items->lock);
        while (DLL_COUNT(item_, s3Items) == 0 && !s3Items->done) {
            ARKIME_COND_WAIT(s3Items->lock);
        }
        if (DLL_COUNT(item_, s3Items) == 0) {
            ARKIME_UNLOCK(s3Items->lock);
            if (req.continuation) // Empty page, but more pages to fetch
                continue;
            break;
        }
        S3Item *item;
        DLL_POP_HEAD(item_, s3Items, item);
        ARKIME_UNLOCK(s3Items->lock);
        arkime_reader_scheme_load(item->url, flags & (ArkimeSchemeFlags)(~ARKIME_SCHEME_FLAG_DIRHINT), actions);
        g_free(item->url);
        ARKIME_TYPE_FREE(S3Item, item);
    }
    g_free(prefix);
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

    if (!uris[0] || !uris[1] || !uris[2] || !uris[3]) {
        LOG("ERROR - Invalid S3 uri %s", uri);
        g_strfreev(uris);
        return 1;
    }

    if (!arkime_reader_scheme_s3_valid_bucket(uris[2])) {
        LOG("ERROR - Invalid S3 bucket %s", uri);
        g_strfreev(uris);
        return 1;
    }

    char *rawKey = NULL;
    char *encKey = scheme_s3_key_encode(uri + 6 + strlen(uris[2]), &rawKey);
    if (!encKey) {
        LOG("ERROR - Invalid S3 key %s", uri);
        g_strfreev(uris);
        return 1;
    }
    char *reqPath = g_strconcat("/", encKey, NULL);
    char *extraPath = g_strconcat("/", rawKey, NULL);
    g_free(encKey);
    g_free(rawKey);

    S3Request req = {
        .actions = actions,
        .url = uri,
        .isDir = FALSE,
        .isS3 = TRUE,
        .tryAgain = FALSE,
        .first = TRUE
    };

    const ArkimeCredentials_t *creds = arkime_credentials_get("s3", "s3AccessKeyId", "s3SecretAccessKey");

    int rc = 0;
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

        g_free(req.extraInfo);
        req.extraInfo = scheme_s3_extra(schemehostport, uris[2], region, extraPath, s3PathAccessStyle);
        if (scheme_s3_request(server, creds, reqPath, uris[2], &req, s3PathAccessStyle, scheme_s3_read)) {
            LOG("ERROR - Failed to request %s", uri);
            rc = 1;
            break;
        }

        ARKIME_LOCK(waiting);
        ARKIME_LOCK(waiting);
        ARKIME_UNLOCK(waiting);
    } while (req.tryAgain);
    g_free(req.extraInfo);
    g_free(reqPath);
    g_free(extraPath);
    g_strfreev(uris);

    return rc;
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
    // PATH_AS_IS so .. segments reach the key check instead of being collapsed
    curl_url_set(h, CURLUPART_URL, uri, CURLU_NON_SUPPORT_SCHEME | CURLU_PATH_AS_IS);

    int rc = 0;
    char *scheme = NULL;
    rc += curl_url_get(h, CURLUPART_SCHEME, &scheme, 0);

    char *host = NULL;
    rc += curl_url_get(h, CURLUPART_HOST, &host, 0);

    char *port = NULL;
    curl_url_get(h, CURLUPART_PORT, &port, 0);

    char *path = NULL;
    rc += curl_url_get(h, CURLUPART_PATH, &path, 0);

    // Keys are uri encoded, so a query or fragment means a bad key
    char *query = NULL;
    char *fragment = NULL;
    curl_url_get(h, CURLUPART_QUERY, &query, 0);
    curl_url_get(h, CURLUPART_FRAGMENT, &fragment, 0);
    if (query || fragment)
        rc++;
    curl_free(query);
    curl_free(fragment);

    if (rc) {
        LOG("Error parsing %s", uri);
        curl_free(scheme);
        curl_free(host);
        curl_free(port);
        curl_free(path);
        curl_url_cleanup(h);
        return 1;
    }

    char **paths = g_strsplit(path, "/", 0);

    if (!paths[0] || !paths[1] || paths[1][0] == 0 || !paths[2] || !arkime_reader_scheme_s3_valid_bucket(paths[1])) {
        LOG("ERROR - S3 file URL missing or invalid bucket or key: %s", uri);
        g_strfreev(paths);
        curl_free(scheme);
        curl_free(host);
        curl_free(port);
        curl_free(path);
        curl_url_cleanup(h);
        return 1;
    }

    char *rawKey = NULL;
    char *encKey = scheme_s3_key_encode(path + 2 + strlen(paths[1]), &rawKey);
    if (!encKey) {
        LOG("ERROR - Invalid S3 key %s", uri);
        g_strfreev(paths);
        curl_free(scheme);
        curl_free(host);
        curl_free(port);
        curl_free(path);
        curl_url_cleanup(h);
        return 1;
    }
    char *reqPath = g_strconcat("/", encKey, NULL);
    char *extraPath = g_strconcat("/", rawKey, NULL);
    g_free(encKey);
    g_free(rawKey);

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

    S3Request req = {
        .actions = actions,
        .url = uri,
        .extraInfo = scheme_s3_extra(schemehostport, paths[1], region, extraPath, TRUE),
        .isDir = FALSE,
        .isS3 = FALSE,
        .tryAgain = FALSE,
        .first = TRUE
    };

    gboolean failed = scheme_s3_request(server, creds, reqPath, paths[1], &req, TRUE, scheme_s3_read);
    g_free(reqPath);
    g_free(extraPath);

    curl_free(scheme);
    curl_free(host);
    curl_free(port);
    curl_free(path);
    g_strfreev(paths);
    curl_url_cleanup(h);

    if (failed) {
        LOG("ERROR - Failed to request %s", uri);
    } else {
        ARKIME_LOCK(waiting);
        ARKIME_LOCK(waiting);
        ARKIME_UNLOCK(waiting);
    }
    g_free(req.extraInfo);

    return failed ? 1 : 0;
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
