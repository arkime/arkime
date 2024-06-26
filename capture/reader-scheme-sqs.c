/******************************************************************************/
/* reader-scheme-sqs.c
 *
 * Copyright 2023 All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <curl/curl.h>
#include "arkime.h"

extern ArkimeConfig_t        config;


LOCAL char                  *sqsAccessKeyId;
LOCAL char                  *sqsSecretAccessKey;
LOCAL char                  *s3Host;
LOCAL gboolean               inited;

typedef struct sqs_item {
    struct sqs_item  *item_next, *item_prev;
    char             *receiptHandle;
    char             *bucket;
    char             *key;
} SQSItem;

typedef struct sqs_item_head {
    struct sqs_item  *item_next, *item_prev;
    int               item_count;

    ARKIME_COND_EXTERN(lock);
    ARKIME_LOCK_EXTERN(lock);
    int     done;
} SQSItemHead;

LOCAL ARKIME_LOCK_DEFINE(waitingsqs);

typedef struct sqs_request {
    SQSItemHead *items;
    uint8_t      done : 1;
} SQSRequest;


/******************************************************************************/
LOCAL SQSItemHead *sqs_alloc()
{
    SQSItemHead *head = ARKIME_TYPE_ALLOC0(SQSItemHead);
    DLL_INIT(item_, head);
    ARKIME_LOCK_INIT(head->lock);
    ARKIME_COND_INIT(head->lock);
    return head;
}
/******************************************************************************/
LOCAL void sqs_enqueue(SQSItemHead *head, char *receiptHandle, char *bucket, char *key)
{

    ARKIME_LOCK(head->lock);
    SQSItem *item = ARKIME_TYPE_ALLOC0(SQSItem);
    item->receiptHandle = receiptHandle;
    item->bucket = bucket;
    item->key = key;
    DLL_PUSH_TAIL(item_, head, item);

    ARKIME_COND_SIGNAL(head->lock);
    ARKIME_UNLOCK(head->lock);
}
/******************************************************************************/
LOCAL void sqs_init()
{
    inited = TRUE;
    sqsAccessKeyId = arkime_config_str(NULL, "sqsAccessKeyId", NULL);
    sqsSecretAccessKey = arkime_config_str(NULL, "sqsSecretAccessKey", NULL);
    s3Host = arkime_config_str(NULL, "s3Host", NULL);
}
/******************************************************************************/
LOCAL void sqs_delete_done(int UNUSED(code), uint8_t *data, int data_len, gpointer UNUSED(uw))
{
    if (code != 200) {
        LOG("Delete failed %d %.*s", code, data_len, data);
    }
}
/******************************************************************************/
LOCAL void sqs_done(int UNUSED(code), uint8_t *data, int data_len, gpointer uw)
{
    if (code != 200) {
        LOG("Receive failed %d %.*s", code, data_len, data);
        ARKIME_UNLOCK(waitingsqs);
        return;
    }

    SQSRequest *req = (SQSRequest *)uw;

    // AWS uses "messages"
    static const char *messagesPath1[] = {"ReceiveMessageResponse", "ReceiveMessageResult", "messages", NULL};
    uint32_t messagesLen = 0;
    uint8_t *messages = (uint8_t *)arkime_js0n_get_path(data, data_len, messagesPath1, &messagesLen);

    if (!messages) {
        // LocalStack uses "Message"
        static const char *messagesPath2[] = {"ReceiveMessageResponse", "ReceiveMessageResult", "Message", NULL};
        messages = (uint8_t *)arkime_js0n_get_path(data, data_len, messagesPath2, &messagesLen);
    }

    // AWS returns "null" instead of removing key
    if (!messages || (messagesLen == 4 && memcmp(messages, "null", 4) == 0)) {
        if (!config.pcapMonitor)
            req->done = 1;
        ARKIME_UNLOCK(waitingsqs);
        ARKIME_COND_SIGNAL(req->items->lock);
        return;
    }

    int      i;
    uint32_t out[4 * 30];

    // Sometimes feel like an array, sometimes you don't
    if (messages[0] == '[') {
        int rc;
        if ((rc = js0n(messages, messagesLen, out, sizeof(out))) != 0) {
            LOG("ERROR - Parse error %d in >%.*s<\n", rc, messagesLen, messages);
            return;
        }
    } else {
        // Fake array of 1
        out[0] = 0;
        out[1] = messagesLen;
        out[2] = 0;
        out[3] = 0;
    }

    for (i = 0; out[i + 1]; i += 2) {
        uint32_t receiptLen = 0;
        uint8_t *receipt = (uint8_t *)arkime_js0n_get(messages + out[i], out[i + 1], "ReceiptHandle", &receiptLen);

        uint32_t bodyLen = 0;
        uint8_t *body = (uint8_t *)arkime_js0n_get(messages + out[i], out[i + 1], "Body", &bodyLen);
        if (!body) {
            LOG("No Body %.*s", out[i + 1], messages + out[i]);
            continue;
        }

        // The Body is actually json encoded into a string
        body[bodyLen] = 0;
        body = (uint8_t *)g_strcompress((char *)body);

        uint32_t recordsLen = 0;
        const uint8_t *records = arkime_js0n_get(body, strlen((char *)body), "Records", &recordsLen);
        if (!records) {
            LOG("No records %s", body);
            g_free(body);
            sqs_enqueue(req->items, g_strndup((char *)receipt, receiptLen), NULL, NULL);
            continue;
        }

        uint32_t s3Len = 0;
        const uint8_t *s3 = arkime_js0n_get(records + 1, recordsLen - 2, "s3", &s3Len);
        if (!s3) {
            LOG("No s3 %.*s", recordsLen - 2, records + 1);
            g_free(body);
            sqs_enqueue(req->items, g_strndup((char *)receipt, receiptLen), NULL, NULL);
            continue;
        }

        static const char *bucketPath[] = {"bucket", "name", NULL};
        uint32_t bucketLen = 0;
        const uint8_t *bucket = arkime_js0n_get_path(s3, s3Len, bucketPath, &bucketLen);

        static const char *keyPath[] = {"object", "key", NULL};
        uint32_t keyLen = 0;
        uint8_t *key = (uint8_t *)arkime_js0n_get_path(s3, s3Len, keyPath, &keyLen);

        key[keyLen] = 0;
        if (bucket && key && g_regex_match(config.offlineRegex, (char *)key, 0, NULL)) {
            sqs_enqueue(req->items, g_strndup((char *)receipt, receiptLen), g_strndup((char *)bucket, bucketLen), g_strndup((char *)key, keyLen));
        } else {
            sqs_enqueue(req->items, g_strndup((char *)receipt, receiptLen), NULL, NULL);
        }
        g_free(body);
    }

    ARKIME_UNLOCK(waitingsqs);
}
/******************************************************************************/
// sqs://sqs.us-east-1.amazonaws.com/80398EXAMPLE/MyQueue
// sqshttps://sqs.us-east-1.amazonaws.com/80398EXAMPLE/MyQueue
// sqshttp://sqs.us-east-1.localhost.localstack.cloud:4566/000000000000/my-queue
int scheme_sqs_load(const char *uri, gboolean UNUSED(dirHint))
{
    if (!inited)
        sqs_init();

    SQSRequest *req = ARKIME_TYPE_ALLOC0(SQSRequest);
    req->items = sqs_alloc();

    char **uris = g_strsplit(uri, "/", 0);

    if (!uris[2] || !uris[3] || !uris[4]) {
        LOGEXIT("ERROR - Invalid SQS uri %s", uri);
        return 1;
    }

    char **dots = g_strsplit(uris[2], ".", 0);

    char *scheme;
    if (strcmp(uris[0], "sqshttp") == 0)
        scheme = "http";
    else
        scheme = "https";

    char schemehostport[300];
    snprintf(schemehostport, sizeof(schemehostport), "%s://%s", scheme, uris[2]);

    char serverName[300];
    snprintf(serverName, sizeof(serverName), "sqs:%s:%s", uris[3], uris[4]);

    int isNew;
    void *server = arkime_http_get_or_create_server(serverName, schemehostport, 2, 100, TRUE, &isNew);
    if (isNew) {
        arkime_http_set_timeout(server, 0);

        char userpwd[100];
        snprintf(userpwd, sizeof(userpwd), "%s:%s", sqsAccessKeyId, sqsSecretAccessKey);
        arkime_http_set_userpwd(server, userpwd);

        char aws_sigv4[100];
        snprintf(aws_sigv4, sizeof(aws_sigv4), "aws:amz:%s:sqs", dots[1]);
        arkime_http_set_aws_sigv4(server, aws_sigv4);
    }

    // Construct the request URL
    char receiveFullPath[1000];
    snprintf(receiveFullPath, sizeof(receiveFullPath), "/%s/%s?Action=ReceiveMessage&Version=2012-11-05&MaxNumberOfMessages=1&WaitTimeSeconds=10", uris[3], uris[4]);

    if (config.debug)
        LOG("receiveFullPath: %s", receiveFullPath);

    char deleteFullPath[1000];
    snprintf(deleteFullPath, sizeof(deleteFullPath), "/%s/%s", uris[3], uris[4]);

    static char *headers[4] = {"Content-Type: application/x-www-form-urlencoded", "Expect:", "Accept: application/json", NULL};
    arkime_http_schedule(server, "POST", receiveFullPath, -1, NULL, 0, headers, ARKIME_HTTP_PRIORITY_BEST, sqs_done, req);

    ARKIME_LOCK(waitingsqs);
    ARKIME_LOCK(waitingsqs);
    ARKIME_UNLOCK(waitingsqs);

    const gboolean isaws = g_str_has_suffix(uris[2], "amazonaws.com");

    g_strfreev(uris);
    g_strfreev(dots);

    while (!req->done || DLL_COUNT(item_, req->items) > 0) {
        if (!req->done && DLL_COUNT(item_, req->items) == 0) {
            // request more items
            arkime_http_schedule(server, "POST", receiveFullPath, -1, NULL, 0, headers, ARKIME_HTTP_PRIORITY_BEST, sqs_done, req);
            ARKIME_LOCK(waitingsqs);
            ARKIME_LOCK(waitingsqs);
            ARKIME_UNLOCK(waitingsqs);
            continue;
        }

        ARKIME_LOCK(req->items->lock);
        while (DLL_COUNT(item_, req->items) == 0) {
            ARKIME_COND_WAIT(req->items->lock);
        }
        SQSItem *item;
        DLL_POP_HEAD(item_, req->items, item);
        ARKIME_UNLOCK(req->items->lock);

        if (item->bucket && item->key) {
            char s3url[1000];
            if (isaws) {
                snprintf(s3url, sizeof(s3url), "s3://%s/%s", item->bucket, item->key);
            } else if (s3Host) {
                snprintf(s3url, sizeof(s3url), "s3https://%s/%s/%s", s3Host, item->bucket, item->key);
            } else {
                snprintf(s3url, sizeof(s3url), "s3%s/%s/%s", schemehostport, item->bucket, item->key);
            }
            if (config.debug)
                LOG("SQS S3 URL: %s", s3url);
            arkime_reader_scheme_load(s3url, FALSE);
        }

        // Delete the message
        char *deletePost = arkime_http_get_buffer(2000);
        char *receiptHandle = g_uri_escape_string(item->receiptHandle, NULL, FALSE);
        snprintf(deletePost, 2000, "Action=DeleteMessage&ReceiptHandle=%s", receiptHandle);
        g_free(receiptHandle);

        arkime_http_schedule(server, "POST", deleteFullPath, -1, deletePost, strlen(deletePost), headers, ARKIME_HTTP_PRIORITY_DROPABLE, sqs_delete_done, NULL);

        g_free(item->receiptHandle);
        g_free(item->bucket);
        g_free(item->key);
        ARKIME_TYPE_FREE(SQSItem, item);
    }

    ARKIME_TYPE_FREE(S3ItemHead, req->items);
    ARKIME_TYPE_FREE(SQSRequest, req);
    return 1;
}
/******************************************************************************/
LOCAL void scheme_sqs_exit()
{
}
/******************************************************************************/
void arkime_reader_scheme_sqs_init()
{
    arkime_reader_scheme_register("sqs", scheme_sqs_load, scheme_sqs_exit);
    arkime_reader_scheme_register("sqshttp", scheme_sqs_load, scheme_sqs_exit);
    arkime_reader_scheme_register("sqshttps", scheme_sqs_load, scheme_sqs_exit);
}
