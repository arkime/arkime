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


LOCAL GHashTable            *regions;
LOCAL GHashTable            *servers;
LOCAL char                  *s3AccessKeyId;
LOCAL char                  *s3SecretAccessKey;
LOCAL char                  *s3Token;
LOCAL char                  *s3Host;
LOCAL char                  *s3Region;
LOCAL gboolean               inited;
LOCAL gboolean               s3PathAccessStyle;

LOCAL uint32_t               first;
LOCAL uint32_t               tryAgain;
LOCAL char                   extraInfo[600];

LOCAL ARKIME_LOCK_DEFINE(waiting);

/******************************************************************************/
LOCAL void scheme_s3_init()
{
    s3AccessKeyId = arkime_config_str(NULL, "s3AccessKeyId", NULL);
    s3SecretAccessKey = arkime_config_str(NULL, "s3SecretAccessKey", NULL);
    s3Host = arkime_config_str(NULL, "s3Host", NULL);
    s3Region = arkime_config_str(NULL, "s3Region", "us-east-1");
    config.gapPacketPos = arkime_config_boolean(NULL, "s3GapPacketPos", TRUE);
    s3PathAccessStyle     = arkime_config_boolean(NULL, "s3PathAccessStyle", TRUE);
    inited = TRUE;
}

/******************************************************************************/
LOCAL void scheme_s3_done(int UNUSED(code), uint8_t UNUSED(*data), int UNUSED(data_len), gpointer UNUSED(uw))
{
    ARKIME_UNLOCK(waiting);
}
/******************************************************************************/
LOCAL int scheme_s3_read(uint8_t *data, int data_len, gpointer uw)
{
    if (first) {
        first = FALSE;
        if (data_len > 10 && data[0] == '<') {
            const char *wrong = arkime_memstr((char *)data, data_len, "' is wrong; expecting '", 23);
            if (wrong) {
                wrong += 23;
                const char *end = arkime_memstr(wrong, data_len - (wrong - (char *)data), "'", 1);
                if (end) {
                    char *region = g_strndup(wrong, end - wrong);
                    char **uris = g_strsplit((char *)uw, "/", 0);
                    g_hash_table_insert(regions, g_strdup(uris[2]), region);
                    g_strfreev(uris);
                    tryAgain = TRUE;
                }
            } else {
                LOG("ERROR - %.*s", data_len, data);
            }
            return 1;
        }
    }
    return arkime_reader_scheme_process((char *)uw, data, data_len, extraInfo);
}
/******************************************************************************/
LOCAL void scheme_s3_request(void *server, const char *host, char *region, const char *path, const char *bucket, const char *uri, gboolean pathStyle)
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

    snprintf(canonicalRequest, sizeof(canonicalRequest),
             "%s\n"       // HTTPRequestMethod
             "%s\n"       // CanonicalURI
             "\n"         // CanonicalQueryString
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
             objectkey,
             host,
             datetime,
             (s3Token ? tokenHeader : ""),
             (s3Token ? ";x-amz-security-token" : "")
            );
    //LOG("canonicalRequest: %s", canonicalRequest);

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
    //LOG("stringToSign: %s", stringToSign);

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

    //LOG("signature: %s", signature);

    snprintf(fullpath, sizeof(fullpath), "%s", objectkey);
    //LOG("fullpath: %s", fullpath);

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

    first = TRUE;
    tryAgain = FALSE;
    arkime_http_schedule2(server, "GET", fullpath, -1, NULL, 0, headers, ARKIME_HTTP_PRIORITY_NORMAL, scheme_s3_done, scheme_s3_read, (gpointer)uri);
    g_checksum_free(checksum);
}
/******************************************************************************/
// s3://bucketname/path
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

    if (!inited)
        scheme_s3_init();

    char **uris = g_strsplit(uri, "/", 0);

    do {
        char *region = g_hash_table_lookup(regions, uris[2]);
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

        void *server = g_hash_table_lookup(servers, schemehostport);
        if (!server) {
            server = arkime_http_create_server(schemehostport, 2, 2, TRUE);
            arkime_http_set_timeout(server, 0);
            g_hash_table_insert(servers, g_strdup(schemehostport), server);
        }

        snprintf(extraInfo, sizeof(extraInfo), "{\"endpoint\":\"%s\",\"bucket\":\"%s\",\"region\":\"%s\",\"path\":\"%s\", \"pathStyle\": %s}",
                 schemehostport,
                 uris[2],
                 region,
                 uri + 5 + strlen(uris[2]),
                 s3PathAccessStyle ? "true" : "false");
        scheme_s3_request(server, hostport, region, uri + 5 + strlen(uris[2]), uris[2], uri, s3PathAccessStyle);

        ARKIME_LOCK(waiting);
        ARKIME_LOCK(waiting);
        ARKIME_UNLOCK(waiting);
    } while (tryAgain);
    g_strfreev(uris);

    return 0;
}
/******************************************************************************/
// s3http://hostport/bucketname/key
// s3https://hostport/bucketname/key
int scheme_s3_load_full(const char *uri)
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

    if (!inited)
        scheme_s3_init();

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

    void *server = g_hash_table_lookup(servers, schemehostport);
    if (!server) {
        server = arkime_http_create_server(schemehostport, 2, 2, TRUE);
        arkime_http_set_timeout(server, 0);
        g_hash_table_insert(servers, g_strdup(schemehostport), server);
    }

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

    scheme_s3_request(server, hostport, region, path + 1 + strlen(paths[1]), paths[1], uri, TRUE);

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
    servers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, arkime_http_free_server);
    regions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
}
