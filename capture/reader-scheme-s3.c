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
LOCAL char                  *s3AccessKeyId;
LOCAL char                  *s3SecretAccessKey;
LOCAL char                  *s3Token;
LOCAL char                  *s3Host;
LOCAL char                  *s3Region;
LOCAL gboolean               inited;
LOCAL gboolean               s3PathAccessStyle;

LOCAL char                   extraInfo[600];

typedef struct s3_item {
    struct s3_item  *item_next, *item_prev;
    char            *url;
} S3Item;

typedef struct s3_item_head {
    struct s3_item  *item_next, *item_prev;
    int              item_count;

    ARKIME_COND_EXTERN(lock);
    ARKIME_LOCK_EXTERN(lock);
    int     done;
} S3ItemHead;

S3ItemHead *s3Items;

LOCAL ARKIME_LOCK_DEFINE(waiting);
LOCAL ARKIME_LOCK_DEFINE(waitingdir);

typedef struct s3_request {
    const char  *url;
    char        *continuation;
    uint8_t      isDir : 1;
    uint8_t      isS3 : 1;
    uint8_t      tryAgain : 1;
    uint8_t      first : 1;
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
    s3AccessKeyId = arkime_config_str(NULL, "s3AccessKeyId", NULL);
    s3SecretAccessKey = arkime_config_str(NULL, "s3SecretAccessKey", NULL);
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
    return arkime_reader_scheme_process(req->url, data, data_len, extraInfo);
}
/******************************************************************************/
LOCAL void scheme_s3_request(void *server, const char *host, char *region, const char *path, const char *bucket, S3Request *req, gboolean pathStyle, ArkimeHttpRead_cb cb)
{
    char           canonicalRequest[20000];
    char           datetime[17];
    char           objectkey[1000];
    char           fullpath[2000];
    char           tokenHeader[4200];
    struct timeval outputFileTime;

    gettimeofday(&outputFileTime, 0);
    struct tm      gm;
    gmtime_r(&outputFileTime.tv_sec, &gm);
    snprintf(datetime, sizeof(datetime),
             "%04u%02u%02uT%02u%02u%02uZ",
             gm.tm_year + 1900,
             gm.tm_mon + 1,
             gm.tm_mday,
             gm.tm_hour,
             gm.tm_min,
             gm.tm_sec);

    if (s3Token) {
        snprintf(tokenHeader, sizeof(tokenHeader), "x-amz-security-token:%s\n", s3Token);
    }

    if (pathStyle)
        snprintf(objectkey, sizeof(objectkey), "/%s%s", bucket, path);
    else
        snprintf(objectkey, sizeof(objectkey), "%s", path);

    int pathlen = strlen(objectkey);
    char cqs[2000];
    cqs[0] = 0;
    unsigned int cqslen = 0;

    char *qs;
    if ((qs = strchr(objectkey, '?'))) {
        pathlen = qs - objectkey;
        qs++;
        char first = 1;
        while (*qs && cqslen < sizeof(cqs) - 5) {
            if (*qs == '/') {
                cqs[cqslen++] = '%';
                cqs[cqslen++] = '2';
                cqs[cqslen++] = 'F';
            } else if (*qs == '+') {
                cqs[cqslen++] = '%';
                cqs[cqslen++] = '2';
                cqs[cqslen++] = 'B';
            } else if (*qs == '&') {
                first = 1;
                cqs[cqslen++] = '&';
            } else if (*qs == '=') {
                if (first) {
                    first = 0;
                    cqs[cqslen++] = '=';
                } else {
                    cqs[cqslen++] = '%';
                    cqs[cqslen++] = '3';
                    cqs[cqslen++] = 'D';
                }
            } else {
                cqs[cqslen++] = *qs;
            }
            qs++;
        }
    }

    snprintf(canonicalRequest, sizeof(canonicalRequest),
             "%s\n"       // HTTPRequestMethod
             "%.*s\n"     // CanonicalURI
             "%.*s\n"     // CanonicalQueryString
             //CanonicalHeaders
             "host:%s\n"
             "x-amz-content-sha256:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\n"
             "x-amz-date:%s\n"
             "%s"
             "\n"
             // SignedHeaders
             "host;x-amz-content-sha256;x-amz-date%s\n"
             "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" // Hex(SHA256Hash(""))
             ,
             "GET",
             pathlen,
             objectkey,
             cqslen,
             cqs,
             host,
             datetime,
             (s3Token ? tokenHeader : ""),
             (s3Token ? ";x-amz-security-token" : "")
            );
    if (config.debug > 4)
        LOG("canonicalRequest: %s", canonicalRequest);

    GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(checksum, (guchar *)canonicalRequest, -1);

    char stringToSign[1000];
    snprintf(stringToSign, sizeof(stringToSign),
             "AWS4-HMAC-SHA256\n"
             "%s\n"
             "%8.8s/%s/s3/aws4_request\n"
             "%s"
             ,
             datetime,
             datetime,
             region,
             g_checksum_get_string(checksum));
    if (config.debug > 4)
        LOG("stringToSign: %s", stringToSign);

    char kSecret[1000];
    snprintf(kSecret, sizeof(kSecret), "AWS4%s", s3SecretAccessKey);

    char  kDate[65];
    gsize kDateLen = sizeof(kDate);
    GHmac *hmac = g_hmac_new(G_CHECKSUM_SHA256, (guchar *)kSecret, strlen(kSecret));
    g_hmac_update(hmac, (guchar *)datetime, 8);
    g_hmac_get_digest(hmac, (guchar *)kDate, &kDateLen);
    g_hmac_unref(hmac);

    char  kRegion[65];
    gsize kRegionLen = sizeof(kRegion);
    hmac = g_hmac_new(G_CHECKSUM_SHA256, (guchar *)kDate, kDateLen);
    g_hmac_update(hmac, (guchar *)region, -1);
    g_hmac_get_digest(hmac, (guchar *)kRegion, &kRegionLen);
    g_hmac_unref(hmac);

    char  kService[65];
    gsize kServiceLen = sizeof(kService);
    hmac = g_hmac_new(G_CHECKSUM_SHA256, (guchar *)kRegion, kRegionLen);
    g_hmac_update(hmac, (guchar *)"s3", 2);
    g_hmac_get_digest(hmac, (guchar *)kService, &kServiceLen);
    g_hmac_unref(hmac);

    char kSigning[65];
    gsize kSigningLen = sizeof(kSigning);
    hmac = g_hmac_new(G_CHECKSUM_SHA256, (guchar *)kService, kServiceLen);
    g_hmac_update(hmac, (guchar *)"aws4_request", 12);
    g_hmac_get_digest(hmac, (guchar *)kSigning, &kSigningLen);
    g_hmac_unref(hmac);

    char signature[65];
    hmac = g_hmac_new(G_CHECKSUM_SHA256, (guchar *)kSigning, kSigningLen);
    g_hmac_update(hmac, (guchar *)stringToSign, -1);
    g_strlcpy(signature, g_hmac_get_string(hmac), sizeof(signature));
    g_hmac_unref(hmac);

    if (config.debug > 3)
        LOG("signature: %s", signature);

    snprintf(fullpath, sizeof(fullpath), "%s", objectkey);
    if (config.debug > 3)
        LOG("fullpath: %s", fullpath);

    char strs[3][1000];
    char *headers[8];
    headers[0] = "Expect:";
    headers[1] = "Content-Type:";
    headers[2] = strs[0];
    headers[3] = strs[1];
    headers[4] = strs[2];

    int nextHeader = 5;

    snprintf(strs[0], sizeof(strs[0]),
             "Authorization: AWS4-HMAC-SHA256 Credential=%s/%8.8s/%s/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date%s,Signature=%s",
             s3AccessKeyId, datetime, region,
             (s3Token ? ";x-amz-security-token" : ""),
             signature
            );

    snprintf(strs[1], sizeof(strs[1]), "x-amz-content-sha256: %s", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    snprintf(strs[2], sizeof(strs[2]), "x-amz-date: %s", datetime);

    if (s3Token) {
        snprintf(tokenHeader, sizeof(tokenHeader), "x-amz-security-token: %s", s3Token);
        headers[nextHeader++] = tokenHeader;
    }

    headers[nextHeader] = NULL;

    req->first = TRUE;
    req->tryAgain = FALSE;
    if (cb)
        arkime_http_schedule2(server, "GET", fullpath, -1, NULL, 0, headers, ARKIME_HTTP_PRIORITY_NORMAL, scheme_s3_done, cb, req);
    else
        arkime_http_schedule(server, "GET", fullpath, -1, NULL, 0, headers, ARKIME_HTTP_PRIORITY_NORMAL, scheme_s3_done, req);
    g_checksum_free(checksum);
}
/******************************************************************************/
LOCAL int scheme_s3_load_dir(const char *dir)
{
    char **uris = g_strsplit(dir, "/", 4);

    char uri[2000];
    if (uris[3]) {
        snprintf(uri, sizeof(uri), "s3://%s/?list-type=2&prefix=%s", uris[2], uris[3]);
    } else {
        snprintf(uri, sizeof(uri), "s3://%s/?list-type=2", uris[2]);
    }

    S3Request req = {
        .url = uris[2],
        .isDir = TRUE,
        .isS3 = TRUE,
        .tryAgain = FALSE,
        .first = TRUE
    };

    s3Items->done = 0;

    void *server;
    char  hostport[256];
    char *region;
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

        int isNew;
        server = arkime_http_get_or_create_server(schemehostport, schemehostport, 2, 2, TRUE, &isNew);
        if (isNew)
            arkime_http_set_timeout(server, 0);

        scheme_s3_request(server, hostport, region, uri + 5 + strlen(uris[2]), uris[2], &req, s3PathAccessStyle, NULL);

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

            scheme_s3_request(server, hostport, region, uri2 + 5 + strlen(uris[2]), uris[2], &req, s3PathAccessStyle, NULL);
            ARKIME_LOCK(waitingdir);
        }
        ARKIME_LOCK(s3Items->lock);
        while (DLL_COUNT(item_, s3Items) == 0) {
            ARKIME_COND_WAIT(s3Items->lock);
        }
        S3Item *item;
        DLL_POP_HEAD(item_, s3Items, item);
        ARKIME_UNLOCK(s3Items->lock);
        arkime_reader_scheme_load(item->url, FALSE);
        g_free(item->url);
        ARKIME_TYPE_FREE(S3Item, item);
    }
    g_strfreev(uris);
    return 1;
}
/******************************************************************************/
LOCAL int scheme_s3_load_full_dir(const char *dir)
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

    int isNew;
    void *server = arkime_http_get_or_create_server(schemehostport, schemehostport, 2, 2, TRUE, &isNew);
    if (isNew)
        arkime_http_set_timeout(server, 0);

    char region[100];
    g_strlcpy(region, s3Region, sizeof(region)); // default

    char *s3 = strstr(host, ".s3-");
    if (s3) {
        s3 += 4;
        const char *dot = strchr(s3, '.');
        if (dot) {
            memcpy(region, s3, dot - s3);
            region[dot - s3] = 0;
        }
    }

    char shpb[1000];
    snprintf(shpb, sizeof(shpb), "%s/%s", schemehostport, paths[1]);

    char uri[2000];
    if (paths[2] && paths[2][0] != 0) {
        snprintf(uri, sizeof(uri), "%s/?list-type=2&prefix=%s", shpb, paths[2]);
    } else {
        snprintf(uri, sizeof(uri), "%s/?list-type=2", shpb);
    }

    S3Request req = {
        .url = shpb,
        .isDir = TRUE,
        .isS3 = FALSE,
        .tryAgain = FALSE,
        .first = TRUE
    };

    scheme_s3_request(server, hostport, region, uri + strlen(shpb), paths[1], &req, TRUE, NULL);

    curl_free(scheme);
    curl_free(host);
    curl_free(port);
    curl_free(path);
    curl_url_cleanup(h);

    while (!s3Items->done || DLL_COUNT(item_, s3Items) > 0) {
        if (req.continuation) {
            char uri2[3000];

            if (paths[3]) {
                snprintf(uri2, sizeof(uri2), "%s://%s/?continuation-token=%s&list-type=2&prefix=%s", scheme, paths[2], req.continuation, paths[3]);
            } else {
                snprintf(uri2, sizeof(uri2), "%s://%s/?continuation-token=%s&list-type=2", scheme, paths[2], req.continuation);
            }

            g_free(req.continuation);
            req.continuation = NULL;

            scheme_s3_request(server, hostport, region, uri2 + 5 + strlen(paths[2]), paths[2], &req, TRUE, NULL);
            ARKIME_LOCK(waitingdir);
        }
        ARKIME_LOCK(s3Items->lock);
        while (DLL_COUNT(item_, s3Items) == 0) {
            ARKIME_COND_WAIT(s3Items->lock);
        }
        S3Item *item;
        DLL_POP_HEAD(item_, s3Items, item);
        ARKIME_UNLOCK(s3Items->lock);
        arkime_reader_scheme_load(item->url, FALSE);
        g_free(item->url);
        ARKIME_TYPE_FREE(S3Item, item);
    }
    g_strfreev(paths);
    return 1;
}
/******************************************************************************/
// s3://bucketname/path
int scheme_s3_load(const char *uri, gboolean dirHint)
{
    if (!inited)
        scheme_s3_init();

    if (dirHint || g_str_has_suffix(uri, "/")) {
        return scheme_s3_load_dir(uri);
    }

    if (config.pcapSkip && arkime_db_file_exists(uri, NULL)) {
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
        .url = uri,
        .isDir = FALSE,
        .isS3 = TRUE,
        .tryAgain = FALSE,
        .first = TRUE
    };

    do {
        char *region = g_hash_table_lookup(bucket2Region, uris[2]);
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

        int isNew;
        void *server = arkime_http_get_or_create_server(schemehostport, schemehostport, 2, 2, TRUE, &isNew);
        if (isNew)
            arkime_http_set_timeout(server, 0);

        snprintf(extraInfo, sizeof(extraInfo), "{\"endpoint\":\"%s\",\"bucket\":\"%s\",\"region\":\"%s\",\"path\":\"%s\", \"pathStyle\": %s}",
                 schemehostport,
                 uris[2],
                 region,
                 uri + 5 + strlen(uris[2]),
                 s3PathAccessStyle ? "true" : "false");
        scheme_s3_request(server, hostport, region, uri + 5 + strlen(uris[2]), uris[2], &req, s3PathAccessStyle, scheme_s3_read);

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
int scheme_s3_load_full(const char *uri, gboolean dirHint)
{
    if (!inited)
        scheme_s3_init();

    if (dirHint || g_str_has_suffix(uri, "/")) {
        return scheme_s3_load_full_dir(uri);
    }

    if (config.pcapSkip && arkime_db_file_exists(uri, NULL)) {
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

    int isNew;
    void *server = arkime_http_get_or_create_server(schemehostport, schemehostport, 2, 2, TRUE, &isNew);
    if (isNew)
        arkime_http_set_timeout(server, 0);

    char region[100];
    g_strlcpy(region, s3Region, sizeof(region)); // default

    char *s3 = strstr(host, ".s3-");
    if (s3) {
        s3 += 4;
        const char *dot = strchr(s3, '.');
        if (dot) {
            memcpy(region, s3, dot - s3);
            region[dot - s3] = 0;
        }
    }

    snprintf(extraInfo, sizeof(extraInfo), "{\"endpoint\":\"%s\",\"bucket\":\"%s\",\"region\":\"%s\",\"path\":\"%s\", \"pathStyle\": true}",
             schemehostport,
             paths[1],
             region,
             path + 1 + strlen(paths[1]));
    if (config.debug)
        LOG("extraInfo: %s", extraInfo);

    S3Request req = {
        .url = uri,
        .isDir = FALSE,
        .isS3 = FALSE,
        .tryAgain = FALSE,
        .first = TRUE
    };

    scheme_s3_request(server, hostport, region, path + 1 + strlen(paths[1]), paths[1], &req, TRUE, scheme_s3_read);

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
