/* http.c  -- Functions dealing with http connections.
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include "arkime.h"
#include "zlib.h"
#include <errno.h>

//#define ARKIME_HTTP_DEBUG

extern ArkimeConfig_t        config;

struct arkimehttpserver_t;
typedef struct arkimehttpserver_t ArkimeHttpServer_t;

typedef struct arkimehttprequest_t {
    struct arkimehttprequest_t *rqt_next, *rqt_prev;

    ArkimeHttpResponse_cb func;
    gpointer              uw;

    ArkimeHttpServer_t   *server;
    CURL                 *easy;
    char                  url[1024];
    char                  key[1024];

    uint8_t              *dataIn;
    uint32_t              used;
    uint32_t              size;

    struct curl_slist    *headerList;
    char                 *dataOut;
    uint32_t              dataOutLen;
    uint16_t              snamePos;
    int16_t               retries;
    uint8_t               priority;
} ArkimeHttpRequest_t;

typedef struct {
    struct arkimehttprequest_t *rqt_next, *rqt_prev;
    int                         rqt_count;
} ArkimeHttpRequestHead_t;

typedef struct arkimehttpconn_t {
    struct arkimehttpconn_t *h_next, *h_prev;
    uint32_t                 h_hash;
    short                    h_bucket;

    uint8_t                  sessionId[ARKIME_SESSIONID_LEN];
} ArkimeHttpConn_t;

typedef struct arkimehttpconnhead_t {
    struct arkimehttpconn_t *h_next, *h_prev;
    short                    h_count;

} ArkimeHttpConnHead_t;

typedef struct {
   char    *clientCert;
   char    *clientKey;
   char    *clientKeyPass;
} ArkimeClientAuth_t;

LOCAL HASH_VAR(s_, connections, ArkimeHttpConnHead_t, 119);
LOCAL ARKIME_LOCK_DEFINE(connections);

#define PRIORITY_MAX ARKIME_HTTP_PRIORITY_NORMAL
LOCAL ArkimeHttpRequestHead_t requests[PRIORITY_MAX + 1];
LOCAL int                     requestsTimer;
LOCAL ARKIME_LOCK_DEFINE(requests);

LOCAL uint64_t connectionsSet[2048];

typedef struct {
    ArkimeHttpServer_t  *server;
    char                *name;
    time_t               allowedAtSeconds;
} ArkimeHttpServerName_t;

struct arkimehttpserver_t {
    uint64_t                 dropped;
    GHashTable              *fd2ev;
    ArkimeHttpServerName_t  *snames;
    ArkimeClientAuth_t      *clientAuth;
    char                   **defaultHeaders;
    int                      snamesCnt;
    int                      snamesPos;
    char                     compress;
    char                     printErrors;
    uint16_t                 maxConns;
    uint16_t                 maxOutstandingRequests;
    uint16_t                 outstanding;
    uint16_t                 connections;
    uint16_t                 maxRetries;

    ARKIME_LOCK_EXTERN(syncRequest);
    ArkimeHttpRequest_t      syncRequest;
    CURL                    *multi;
    guint                    multiTimer;
    int                      multiRunning;

    ArkimeHttpHeader_cb      headerCb;
};

LOCAL z_stream z_strm;
LOCAL ARKIME_LOCK_DEFINE(z_strm);

LOCAL gboolean arkime_http_send_timer_callback(gpointer);
LOCAL void arkime_http_add_request(ArkimeHttpServer_t *server, ArkimeHttpRequest_t *request, int priority);

/******************************************************************************/
LOCAL int arkime_http_conn_cmp(const void *keyv, const ArkimeHttpConn_t *conn)
{
    return memcmp(keyv, conn->sessionId, MIN(((uint8_t *)keyv)[0], conn->sessionId[0])) == 0;
}
/******************************************************************************/
LOCAL size_t arkime_http_curl_write_callback(void *contents, size_t size, size_t nmemb, void *requestP)
{
    ArkimeHttpRequest_t *request = requestP;

    size_t sz = size * nmemb;

    if (!request->dataIn) {
        double cl;
        curl_easy_getinfo(request->easy, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &cl);
        request->used = sz;
        request->size = MAX(sz, cl);
        request->dataIn = malloc(request->size + 1);
        memcpy(request->dataIn, contents, sz);
        return sz;
    }

    if (request->used + sz >= request->size) {
        request->size += request->used + sz;
        request->dataIn = realloc(request->dataIn, request->size + 1);
    }

    memcpy(request->dataIn + request->used, contents, sz);
    request->used += sz;
    return sz;
}
/******************************************************************************/
uint8_t *arkime_http_send_sync(void *serverV, const char *method, const char *key, int32_t key_len, char *data, uint32_t data_len, char **headers, size_t *return_len, int *code)
{
    ArkimeHttpServer_t        *server = serverV;
    struct curl_slist         *headerList = NULL;

    if (return_len)
        *return_len = 0;

    CURL *easy;

    if (headers) {
        int i;
        for (i = 0; headers[i]; i++) {
            headerList = curl_slist_append(headerList, headers[i]);
        }
    }

    if (server->defaultHeaders) {
        int i;
        for (i = 0; server->defaultHeaders[i]; i++) {
            headerList = curl_slist_append(headerList, server->defaultHeaders[i]);
        }
    }

    ARKIME_LOCK(server->syncRequest);
    if (!server->syncRequest.easy) {
        easy = server->syncRequest.easy = curl_easy_init();
        if (config.debug >= 2) {
            curl_easy_setopt(easy, CURLOPT_VERBOSE, 1);
        }
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, arkime_http_curl_write_callback);
        curl_easy_setopt(easy, CURLOPT_WRITEDATA, (void *)&server->syncRequest);
        curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(easy, CURLOPT_TIMEOUT, 120L);
        curl_easy_setopt(easy, CURLOPT_TCP_KEEPALIVE, 1L);
    } else {
        easy = server->syncRequest.easy;
    }

    if (config.insecure) {
        curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    if (config.caTrustFile) {
        curl_easy_setopt(easy, CURLOPT_CAINFO, config.caTrustFile);
    }

    // Send client certs if so configured
    if(server->clientAuth) {
       curl_easy_setopt(easy, CURLOPT_SSLCERT, server->clientAuth->clientCert);
       curl_easy_setopt(easy, CURLOPT_SSLKEY, server->clientAuth->clientKey);
       if(server->clientAuth->clientKeyPass) {
          curl_easy_setopt(easy, CURLOPT_SSLKEYPASSWD, server->clientAuth->clientKeyPass);
       }
    }

    if (method[0] != 'G') {
        curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, method);
        curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE, data_len);
        curl_easy_setopt(easy, CURLOPT_POSTFIELDS, data);
    } else {
        curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, NULL);
        curl_easy_setopt(easy, CURLOPT_HTTPGET, 1L);
    }

    curl_easy_setopt(easy, CURLOPT_USERAGENT, "arkime");

    if (headerList) {
        curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headerList);
    }

    if (key_len == -1)
        key_len = strlen(key);

    if (key_len > 1000) {
        LOGEXIT("ERROR - URL too long %.*s", key_len, key);
    }

    memcpy(server->syncRequest.key, key, key_len);
    server->syncRequest.key[key_len] = 0;
    server->syncRequest.retries = server->maxRetries;

    while (1) {
        ARKIME_LOCK(requests);
        arkime_http_add_request(server, &server->syncRequest, -1);
        ARKIME_UNLOCK(requests);

        server->syncRequest.used = 0;
        int res = curl_easy_perform(easy);

        if (res != CURLE_OK) {
            if (server->syncRequest.retries >= 0) {
                struct timeval now;
                gettimeofday(&now, NULL);
                server->snames[server->syncRequest.snamePos].allowedAtSeconds = now.tv_sec + 30;
                LOG("Retry %s error '%s'", server->syncRequest.url, curl_easy_strerror(res));
                server->syncRequest.retries--;
                continue;
            }
            LOG("libcurl failure %s error '%s'", server->syncRequest.url, curl_easy_strerror(res));
            ARKIME_UNLOCK(server->syncRequest);

            if (headerList) {
                curl_slist_free_all(headerList);
            }
            return 0;
        }
        break;
    }

    if (headerList) {
        curl_slist_free_all(headerList);
    }

    if (server->syncRequest.dataIn)
        server->syncRequest.dataIn[server->syncRequest.used] = 0;

    if (return_len)
        *return_len = server->syncRequest.used;

    long responseCode;
    curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &responseCode);
    if (code)
        *code = responseCode;

    if (config.logESRequests || (server->printErrors && responseCode/100 != 2)) {
        double totalTime;
        double connectTime;
        double uploadSize;
        double downloadSize;

        curl_easy_getinfo(easy, CURLINFO_TOTAL_TIME, &totalTime);
        curl_easy_getinfo(easy, CURLINFO_CONNECT_TIME, &connectTime);
        curl_easy_getinfo(easy, CURLINFO_SIZE_UPLOAD, &uploadSize);
        curl_easy_getinfo(easy, CURLINFO_SIZE_DOWNLOAD, &downloadSize);

        LOG("%d/%d SYNC %ld %s %.0lf/%0.lf %.0lfms %.0lfms",
           1, 1,
           responseCode,
           server->syncRequest.url,
           uploadSize,
           downloadSize,
           connectTime * 1000,
           totalTime * 1000);
    }

    uint8_t *dataIn = server->syncRequest.dataIn;
    server->syncRequest.dataIn = 0;

    ARKIME_UNLOCK(server->syncRequest);
    return dataIn;
}
/******************************************************************************/
LOCAL void arkime_http_add_request(ArkimeHttpServer_t *server, ArkimeHttpRequest_t *request, int priority)
{
    struct timeval now;
    gettimeofday(&now, NULL);

    int startPos = server->snamesPos;
    int offset = 0;

    while (server->snames[server->snamesPos].allowedAtSeconds > now.tv_sec) {
        server->snames[server->snamesPos].allowedAtSeconds -= offset;
        server->snamesPos = (server->snamesPos + 1) % server->snamesCnt;
        if (startPos == server->snamesPos)
            offset = 1;
    }

    request->snamePos = server->snamesPos;
    server->snamesPos = (server->snamesPos + 1) % server->snamesCnt;

    char *host = server->snames[request->snamePos].name;
    snprintf(request->url, sizeof(request->url), "%s%s", host, request->key);

    curl_easy_setopt(request->easy, CURLOPT_URL, request->url);

    if (priority >= 0) {
        if (priority > PRIORITY_MAX) {
            priority = PRIORITY_MAX;
        }
        curl_easy_setopt(request->easy, CURLOPT_OPENSOCKETDATA, &server->snames[request->snamePos]);
        curl_easy_setopt(request->easy, CURLOPT_CLOSESOCKETDATA, &server->snames[request->snamePos]);

#ifdef ARKIME_HTTP_DEBUG
        LOG("HTTPDEBUG INCR %p %d %s", request, server->outstanding, request->url);
#endif
        server->outstanding++;

        DLL_PUSH_TAIL(rqt_, &requests[priority], request);

        if (!requestsTimer)
            requestsTimer = g_timeout_add(0, arkime_http_send_timer_callback, NULL);
    }
}
/******************************************************************************/
LOCAL void arkime_http_curlm_check_multi_info(ArkimeHttpServer_t *server)
{
    char *eff_url;
    CURLMsg *msg;
    int msgs_left;
    ArkimeHttpRequest_t *request;
    CURL *easy;

    while ((msg = curl_multi_info_read(server->multi, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            easy = msg->easy_handle;
            curl_easy_getinfo(easy, CURLINFO_PRIVATE, (void*)&request);
            curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &eff_url);

            long   responseCode;
            curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &responseCode);

            if (config.logESRequests || (server->printErrors && responseCode/100 != 2)) {
                double totalTime;
                double connectTime;
                double uploadSize;
                double downloadSize;

                curl_easy_getinfo(easy, CURLINFO_TOTAL_TIME, &totalTime);
                curl_easy_getinfo(easy, CURLINFO_CONNECT_TIME, &connectTime);
                curl_easy_getinfo(easy, CURLINFO_SIZE_UPLOAD, &uploadSize);
                curl_easy_getinfo(easy, CURLINFO_SIZE_DOWNLOAD, &downloadSize);

                LOG("%d/%d ASYNC %ld %s %.0lf/%.0lf %.0lfms %.0lfms",
                   request->server->outstanding,
                   request->server->connections,
                   responseCode,
                   request->url,
                   uploadSize,
                   downloadSize,
                   connectTime * 1000,
                   totalTime * 1000);
            }

#ifdef ARKIME_HTTP_DEBUG
            LOG("HTTPDEBUG DECR %p %d %s", request, server->outstanding, request->url);
#endif

            if (responseCode == 0 && request->retries >= 0) {
                curl_multi_remove_handle(server->multi, easy);

                request->retries--;
                struct timeval now;
                gettimeofday(&now, NULL);
                ARKIME_LOCK(requests);
                server->snames[request->snamePos].allowedAtSeconds = now.tv_sec + 30;
                server->outstanding--;
                arkime_http_add_request(server, request, request->priority);
                ARKIME_UNLOCK(requests);
            } else {

                if (server->printErrors && responseCode/100 != 2) {
                    if (arkime_memstr((char *)request->dataIn, MIN(request->used, 1000), "version conflict, current version", 33)) {
                        LOG("See the FAQ - https://arkime.com/faq#version-conflict");
                    }
                    LOG("Response length=%u :>\n%.*s", request->used, MIN(request->used, 4000), request->dataIn);
                }

                if (request->func) {
                    if (request->dataIn)
                        request->dataIn[request->used] = 0;
                    request->func(responseCode, request->dataIn, request->used, request->uw);
                }
                if (request->dataIn) {
                    free(request->dataIn);
                    request->dataIn = 0;
                }
                if (request->dataOut) {
                    ARKIME_SIZE_FREE(buffer, request->dataOut);
                }
                if (request->headerList) {
                    curl_slist_free_all(request->headerList);
                }
                ARKIME_TYPE_FREE(ArkimeHttpRequest_t, request);

                curl_multi_remove_handle(server->multi, easy);
                curl_easy_cleanup(easy);
                ARKIME_LOCK(requests);
                server->outstanding--;
                ARKIME_UNLOCK(requests);
            }
        }
    }
}
/******************************************************************************/
LOCAL gboolean arkime_http_watch_callback(int fd, GIOCondition condition, gpointer serverV)
{
    ArkimeHttpServer_t        *server = serverV;

    int action = ((condition & G_IO_IN) ? CURL_CSELECT_IN : 0) |
                 ((condition & G_IO_OUT) ? CURL_CSELECT_OUT : 0) |
                 ((condition & (G_IO_HUP | G_IO_ERR)) ? CURL_CSELECT_ERR : 0);

    curl_multi_socket_action(server->multi, fd, action, &server->multiRunning);
    arkime_http_curlm_check_multi_info(server);
    return TRUE;
}
/******************************************************************************/
LOCAL int arkime_http_curlm_socket_callback(CURL *UNUSED(easy), curl_socket_t fd, int what, void *serverV, void *evP)
{
    ArkimeHttpServer_t        *server = serverV;
    long                       ev = (long)evP;

    switch (what) {
    case CURL_POLL_REMOVE:
        g_source_remove(ev);
        curl_multi_assign(server->multi, fd, 0);
        break;
    default:
        if (ev != 0) {
            g_source_remove(ev);
        }

        ev = arkime_watch_fd(fd, ((what&CURL_POLL_IN)?ARKIME_GIO_READ_COND:0)|((what&CURL_POLL_OUT)?ARKIME_GIO_WRITE_COND:0), arkime_http_watch_callback, server);
        curl_multi_assign(server->multi, fd, (void*)ev);
    }

    return 0;
}
/******************************************************************************/
/* Called by glib when our timeout expires */
LOCAL gboolean arkime_http_timer_callback(gpointer serverV)
{
    ArkimeHttpServer_t        *server = serverV;

    curl_multi_socket_action(server->multi, CURL_SOCKET_TIMEOUT, 0, &server->multiRunning);
    arkime_http_curlm_check_multi_info(server);

    server->multiTimer = 0;
    return G_SOURCE_REMOVE;
}
/******************************************************************************/
LOCAL int arkime_http_curlm_timeout_callback(CURLM *UNUSED(multi), long timeout_ms, void *serverV)
{
    ArkimeHttpServer_t        *server = serverV;

    if (timeout_ms == -1) {
        if (server->multiTimer) {
            g_source_remove(server->multiTimer);
            server->multiTimer = 0;
        }
        return CURLE_OK;
    }

    if (server->multiTimer) {
        g_source_remove(server->multiTimer);
    }
    server->multiTimer = g_timeout_add(timeout_ms, arkime_http_timer_callback, server);

    return CURLE_OK;
}

/******************************************************************************/
size_t arkime_http_curlm_header_function(char *buffer, size_t size, size_t nitems, void *requestP)
{
    ArkimeHttpRequest_t *request = requestP;
    int sz = size*nitems;
    int i = sz;

    while (i > 0 && (buffer[i - 1] == '\r' || buffer[i - 1] == '\n')) {
        buffer[i - 1] = 0;
        i--;
    }

    char *colon = memchr(buffer, ':', sz);
    if (!colon)
        return sz;

    *colon = 0;
    colon++;
    while (isspace(*colon)) colon++;

    request->server->headerCb(request->url, buffer, colon, buffer+i-colon, request->uw);
    return sz;
}
/******************************************************************************/
LOCAL gboolean arkime_http_curl_watch_open_callback(int fd, GIOCondition condition, gpointer snameV)
{
    ArkimeHttpServerName_t    *sname = snameV;
    ArkimeHttpServer_t        *server = sname->server;


    struct sockaddr_storage localAddressStorage, remoteAddressStorage;

    socklen_t addressLength = sizeof(localAddressStorage);
    int rc = getsockname(fd, (struct sockaddr*)&localAddressStorage, &addressLength);
    if (rc != 0)
        return CURLE_OK;

    addressLength = sizeof(remoteAddressStorage);
    rc = getpeername(fd, (struct sockaddr*)&remoteAddressStorage, &addressLength);
    if (rc != 0)
        return CURLE_OK;

    uint8_t sessionId[ARKIME_SESSIONID_LEN];
    int  localPort, remotePort;
    char remoteIp[INET6_ADDRSTRLEN + 2];
    if (localAddressStorage.ss_family == AF_INET) {
        struct sockaddr_in *localAddress = (struct sockaddr_in *)&localAddressStorage;
        struct sockaddr_in *remoteAddress = (struct sockaddr_in *)&remoteAddressStorage;
        arkime_session_id(sessionId, localAddress->sin_addr.s_addr, localAddress->sin_port,
                          remoteAddress->sin_addr.s_addr, remoteAddress->sin_port);
        localPort = ntohs(localAddress->sin_port);
        remotePort = ntohs(remoteAddress->sin_port);
        inet_ntop(AF_INET, &remoteAddress->sin_addr, remoteIp, sizeof(remoteIp));
    } else {
        struct sockaddr_in6 *localAddress = (struct sockaddr_in6 *)&localAddressStorage;
        struct sockaddr_in6 *remoteAddress = (struct sockaddr_in6 *)&remoteAddressStorage;
        arkime_session_id6(sessionId, localAddress->sin6_addr.s6_addr, localAddress->sin6_port,
                          remoteAddress->sin6_addr.s6_addr, remoteAddress->sin6_port);
        localPort = ntohs(localAddress->sin6_port);
        remotePort = ntohs(remoteAddress->sin6_port);
        inet_ntop(AF_INET6, &remoteAddress->sin6_addr, remoteIp + 1, sizeof(remoteIp) - 2);
        remoteIp[0] = '[';
        g_strlcat(remoteIp, "]", sizeof(remoteIp));
    }

    if (config.logHTTPConnections) {
        LOG("Connected %d/%d - %s   %d->%s:%d - fd:%d",
                server->outstanding,
                server->connections,
                sname->name,
                localPort,
                remoteIp,
                remotePort,
                fd);
    }

    ArkimeHttpConn_t *conn;

    ARKIME_LOCK(connections);
    BIT_SET(fd, connectionsSet);
    HASH_FIND(h_, connections, sessionId, conn);
    if (!conn) {
        conn = ARKIME_TYPE_ALLOC0(ArkimeHttpConn_t);

        HASH_ADD(h_, connections, sessionId, conn);
        memcpy(&conn->sessionId, sessionId, sessionId[0]);
        server->connections++;
    } else {
        char buf[1000];
        LOG("ERROR - Already added %x %s", condition, arkime_session_id_string(sessionId, buf));
    }
    ARKIME_UNLOCK(connections);

    arkime_http_curlm_check_multi_info(server);

    return CURLE_OK;
}
/******************************************************************************/
curl_socket_t arkime_http_curl_open_callback(void *snameV, curlsocktype UNUSED(purpose), struct curl_sockaddr *addr)
{
    ArkimeHttpServerName_t    *sname = snameV;
    ArkimeHttpServer_t        *server = sname->server;

    int fd = socket(addr->family, addr->socktype, addr->protocol);

    long ev = arkime_watch_fd(fd, G_IO_OUT | G_IO_IN, arkime_http_curl_watch_open_callback, snameV);
    g_hash_table_insert(server->fd2ev, (void *)(long)fd, (void *)(long)ev);
    return fd;
}
/******************************************************************************/
int arkime_http_curl_close_callback(void *snameV, curl_socket_t fd)
{
    ArkimeHttpServerName_t    *sname = snameV;
    ArkimeHttpServer_t        *server = sname->server;

    if (! BIT_ISSET(fd, connectionsSet)) {
        long ev = (long)g_hash_table_lookup(server->fd2ev, (void *)(long)fd);
        LOG("Couldn't connect %s (%d, %ld) ", sname->name, fd, ev);
        close(fd);
        GSource *source = g_main_context_find_source_by_id (NULL, ev);
        if (source)
            g_source_destroy (source);
        g_hash_table_remove(server->fd2ev, (void *)(long)fd);
        return 0;
    }

    struct sockaddr_storage localAddressStorage, remoteAddressStorage;

    socklen_t addressLength = sizeof(localAddressStorage);
    int rc = getsockname(fd, (struct sockaddr*)&localAddressStorage, &addressLength);
    if (rc != 0) {
        close(fd);
        return 0;
    }

    addressLength = sizeof(remoteAddressStorage);
    rc = getpeername(fd, (struct sockaddr*)&remoteAddressStorage, &addressLength);
    if (rc != 0) {
        close(fd);
        return 0;
    }

    uint8_t sessionId[ARKIME_SESSIONID_LEN];
    int  localPort, remotePort;
    char remoteIp[INET6_ADDRSTRLEN + 2];
    if (localAddressStorage.ss_family == AF_INET) {
        struct sockaddr_in *localAddress = (struct sockaddr_in *)&localAddressStorage;
        struct sockaddr_in *remoteAddress = (struct sockaddr_in *)&remoteAddressStorage;
        arkime_session_id(sessionId, localAddress->sin_addr.s_addr, localAddress->sin_port,
                          remoteAddress->sin_addr.s_addr, remoteAddress->sin_port);
        localPort = ntohs(localAddress->sin_port);
        remotePort = ntohs(remoteAddress->sin_port);
        inet_ntop(AF_INET, &remoteAddress->sin_addr, remoteIp, sizeof(remoteIp));
    } else {
        struct sockaddr_in6 *localAddress = (struct sockaddr_in6 *)&localAddressStorage;
        struct sockaddr_in6 *remoteAddress = (struct sockaddr_in6 *)&remoteAddressStorage;
        arkime_session_id6(sessionId, localAddress->sin6_addr.s6_addr, localAddress->sin6_port,
                          remoteAddress->sin6_addr.s6_addr, remoteAddress->sin6_port);
        localPort = ntohs(localAddress->sin6_port);
        remotePort = ntohs(remoteAddress->sin6_port);
        inet_ntop(AF_INET6, &remoteAddress->sin6_addr, remoteIp + 1, sizeof(remoteIp) - 2);
        remoteIp[0] = '[';
        g_strlcat(remoteIp, "]", sizeof(remoteIp));
    }


    ArkimeHttpConn_t *conn;
    BIT_CLR(fd, connectionsSet);

    ARKIME_LOCK(connections);
    HASH_FIND(h_, connections, sessionId, conn);
    if (conn) {
        HASH_REMOVE(h_, connections, conn);
        ARKIME_TYPE_FREE(ArkimeHttpConn_t, conn);
    }
    ARKIME_UNLOCK(connections);

    server->connections--;

    if (config.logHTTPConnections) {
        LOG("Close %d/%d - %s   %d->%s:%d fd:%d removed: %s",
                server->outstanding,
                server->connections,
                sname->name,
                localPort,
                remoteIp,
                remotePort,
                fd,
                conn?"true":"false");
    }

    close (fd);
    return 0;
}
/******************************************************************************/
LOCAL gboolean arkime_http_send_timer_callback(gpointer UNUSED(unused))
{
    while (1) {
        ArkimeHttpRequest_t *request;
        ARKIME_LOCK(requests);
        for (int r = 0; r <= PRIORITY_MAX; r++) {
            DLL_POP_HEAD(rqt_, &requests[r], request);
            if (request)
                break;
        }

        if (!request) {
            requestsTimer = 0;
            ARKIME_UNLOCK(requests);
            return G_SOURCE_REMOVE;
        }
        ARKIME_UNLOCK(requests);

#ifdef ARKIME_HTTP_DEBUG
        LOG("HTTPDEBUG DO %p %d %s", request, request->server->outstanding, request->url);
#endif
        curl_multi_add_handle(request->server->multi, request->easy);
    }

    return G_SOURCE_REMOVE;
}
/******************************************************************************/
gboolean arkime_http_send(void *serverV, const char *method, const char *key, int32_t key_len, char *data, uint32_t data_len, char **headers, gboolean dropable, ArkimeHttpResponse_cb func, gpointer uw)
{
   return arkime_http_schedule(serverV, method, key, key_len, data, data_len, headers, dropable ? ARKIME_HTTP_PRIORITY_DROPABLE : ARKIME_HTTP_PRIORITY_NORMAL, func, uw);
}
/******************************************************************************/
gboolean arkime_http_schedule(void *serverV, const char *method, const char *key, int32_t key_len, char *data, uint32_t data_len, char **headers, int priority, ArkimeHttpResponse_cb func, gpointer uw)
{
    ArkimeHttpServer_t        *server = serverV;

    if (key_len == -1)
        key_len = strlen(key);

    if (key_len > 1000) {
        LOGEXIT("ERROR - URL too long %.*s", key_len, key);
    }

    // Are we overloaded
    if (!config.quitting && server->outstanding > server->maxOutstandingRequests) {
        int drop = FALSE;
        if (priority == ARKIME_HTTP_PRIORITY_DROPABLE) {
            LOG("ERROR - Dropping request (https://arkime.com/faq#error-dropping-request) %.*s of size %u queue %u is too big", key_len, key, data_len, server->outstanding);
            drop = TRUE;
        } else if (priority == ARKIME_HTTP_PRIORITY_NORMAL && server->outstanding > server->maxOutstandingRequests * 2) {
            LOG("ERROR - Dropping request (https://arkime.com/faq#error-dropping-request) %.*s of size %u queue %u is WAY too big", key_len, key, data_len, server->outstanding);
            drop = TRUE;
        }

        if (drop) {
            ARKIME_THREAD_INCR(server->dropped);

            if (data) {
                ARKIME_SIZE_FREE(buffer, data);
            }
            return 1;
        }
    }

    ArkimeHttpRequest_t       *request = ARKIME_TYPE_ALLOC0(ArkimeHttpRequest_t);

    if (headers) {
        int i;
        for (i = 0; headers[i]; i++) {
            request->headerList = curl_slist_append(request->headerList, headers[i]);
        }
    }

    request->priority = priority;

    if (priority == ARKIME_HTTP_PRIORITY_DROPABLE)
        request->retries = 0;
    else
        request->retries = server->maxRetries;

    if (server->defaultHeaders) {
        int i;
        for (i = 0; server->defaultHeaders[i]; i++) {
            request->headerList = curl_slist_append(request->headerList, server->defaultHeaders[i]);
        }
    }

    // Do we need to compress item
    if (server->compress && data && data_len > 860) {
        char            *buf = arkime_http_get_buffer(data_len);
        int              ret;

        ARKIME_LOCK(z_strm);
        z_strm.avail_in   = data_len;
        z_strm.next_in    = (uint8_t *)data;
        z_strm.avail_out  = data_len;
        z_strm.next_out   = (uint8_t *)buf;
        ret = deflate(&z_strm, Z_FINISH);
        if (ret == Z_STREAM_END) {
            request->headerList = curl_slist_append(request->headerList, "Content-Encoding: gzip");
            ARKIME_SIZE_FREE(buffer, data);
            data_len = data_len - z_strm.avail_out;
            data     = buf;
        } else {
            ARKIME_SIZE_FREE(buffer, buf);
        }

        deflateReset(&z_strm);
        ARKIME_UNLOCK(z_strm);
    }

    request->server     = server;
    request->func       = func;
    request->uw         = uw;
    request->dataOut    = data;
    request->dataOutLen = data_len;

    request->easy = curl_easy_init();
    if (config.debug >= 2) {
        curl_easy_setopt(request->easy, CURLOPT_VERBOSE, 1);
    }

    if (config.insecure) {
        curl_easy_setopt(request->easy, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(request->easy, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    if (config.caTrustFile) {
        curl_easy_setopt(request->easy, CURLOPT_CAINFO, config.caTrustFile);
    }

    // Send client certs if so configured
    if(server->clientAuth) {
       curl_easy_setopt(request->easy, CURLOPT_SSLCERT, server->clientAuth->clientCert);
       curl_easy_setopt(request->easy, CURLOPT_SSLKEY, server->clientAuth->clientKey);
       if(server->clientAuth->clientKeyPass) {
          curl_easy_setopt(request->easy, CURLOPT_SSLKEYPASSWD, server->clientAuth->clientKeyPass);
       }
    }

    curl_easy_setopt(request->easy, CURLOPT_WRITEFUNCTION, arkime_http_curl_write_callback);
    curl_easy_setopt(request->easy, CURLOPT_WRITEDATA, (void *)request);
    curl_easy_setopt(request->easy, CURLOPT_PRIVATE, (void *)request);
    curl_easy_setopt(request->easy, CURLOPT_OPENSOCKETFUNCTION, arkime_http_curl_open_callback);
    curl_easy_setopt(request->easy, CURLOPT_CLOSESOCKETFUNCTION, arkime_http_curl_close_callback);
    curl_easy_setopt(request->easy, CURLOPT_ACCEPT_ENCODING, ""); // https://curl.haxx.se/libcurl/c/CURLOPT_ACCEPT_ENCODING.html
    curl_easy_setopt(request->easy, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(request->easy, CURLOPT_USERAGENT, "arkime");

    if (request->headerList) {
        curl_easy_setopt(request->easy, CURLOPT_HTTPHEADER, request->headerList);
    }

    if (method[0] != 'G') {
        curl_easy_setopt(request->easy, CURLOPT_CUSTOMREQUEST, method);
        curl_easy_setopt(request->easy, CURLOPT_INFILESIZE, data_len);
        curl_easy_setopt(request->easy, CURLOPT_POSTFIELDSIZE, data_len);
        curl_easy_setopt(request->easy, CURLOPT_POSTFIELDS, data);
    } else {
        curl_easy_setopt(request->easy, CURLOPT_CUSTOMREQUEST, NULL);
        curl_easy_setopt(request->easy, CURLOPT_HTTPGET, 1L);
    }


    if (server->headerCb) {
        curl_easy_setopt(request->easy, CURLOPT_HEADERFUNCTION, arkime_http_curlm_header_function);
        curl_easy_setopt(request->easy, CURLOPT_HEADERDATA, request);
    }

    curl_easy_setopt(request->easy, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(request->easy, CURLOPT_TIMEOUT, 120L);

    memcpy(request->key, key, key_len);
    request->key[key_len] = 0;

    ARKIME_LOCK(requests);
    arkime_http_add_request(server, request, priority);
    ARKIME_UNLOCK(requests);
    return 0;
}

/******************************************************************************/
uint8_t *arkime_http_get(void *serverV, char *key, int key_len, size_t *mlen)
{
    return arkime_http_send_sync(serverV, "GET", key, key_len, NULL, 0, NULL, mlen, NULL);
}

/******************************************************************************/
int arkime_http_queue_length(void *serverV)
{
    ArkimeHttpServer_t        *server = serverV;
    return server?server->outstanding:0;
}
/******************************************************************************/
uint64_t arkime_http_dropped_count(void *serverV)
{
    ArkimeHttpServer_t        *server = serverV;
    return server?server->dropped:0;
}
/******************************************************************************/
void arkime_http_set_header_cb(void *serverV, ArkimeHttpHeader_cb cb)
{
    ArkimeHttpServer_t        *server = serverV;
    server->headerCb                  = cb;
}
/******************************************************************************/
void arkime_http_free_server(void *serverV)
{
    ArkimeHttpServer_t        *server = serverV;

    if (server->multiTimer) {
        g_source_remove(server->multiTimer);
    }

    // Finish any still running requests
    while (server->multiRunning) {
        curl_multi_socket_action(server->multi, CURL_SOCKET_TIMEOUT, 0, &server->multiRunning);
        arkime_http_curlm_check_multi_info(server);
    }

    // Free sync info
    if (server->syncRequest.easy) {
        curl_easy_cleanup(server->syncRequest.easy);
        server->syncRequest.easy = 0;
    }

    if (server->syncRequest.dataIn)
        free(server->syncRequest.dataIn);

    if (server->clientAuth) {
        ARKIME_TYPE_FREE(ArkimeClientAuth_t, server->clientAuth);
    }
    // Free multi info
    curl_multi_cleanup(server->multi);


    for (int i = 0; i < server->snamesCnt; i++){
        g_free(server->snames[i].name);
    }
    free(server->snames);

    g_hash_table_destroy(server->fd2ev);

    ARKIME_TYPE_FREE(ArkimeHttpServer_t, server);
}
/******************************************************************************/
void arkime_http_set_headers(void *serverV, char **headers)
{
    ArkimeHttpServer_t        *server = serverV;

    server->defaultHeaders = headers;
}
/******************************************************************************/
void arkime_http_set_retries(void *serverV, uint16_t retries)
{
    ArkimeHttpServer_t        *server = serverV;

    server->maxRetries = retries;
}
/******************************************************************************/
void arkime_http_set_client_cert(void *serverV, char *clientCert,
                                char *clientKey, char *clientKeyPass)
{
    ArkimeHttpServer_t        *server = serverV;
    if(server->clientAuth != NULL) {
        ARKIME_TYPE_FREE(ArkimeClientAuth_t, server->clientAuth);
    }
    ArkimeClientAuth_t *clientAuth = ARKIME_TYPE_ALLOC0(ArkimeClientAuth_t);

    clientAuth->clientCert = clientCert;
    clientAuth->clientKey  = clientKey;
    clientAuth->clientKeyPass = clientKeyPass;

    server->clientAuth = clientAuth;
}
/******************************************************************************/
void arkime_http_set_print_errors(void *serverV)
{
    ArkimeHttpServer_t        *server = serverV;

    server->printErrors = 1;
}
/******************************************************************************/
gboolean arkime_http_is_arkime(uint32_t hash, uint8_t *sessionId)
{
    ArkimeHttpConn_t *conn;

    ARKIME_LOCK(connections);
    HASH_FIND_HASH(h_, connections, hash, sessionId, conn);
    ARKIME_UNLOCK(connections);
    return (conn?1:0);
}
/******************************************************************************/
void *arkime_http_create_server(const char *hostnames, int maxConns, int maxOutstandingRequests, int compress)
{
    ArkimeHttpServer_t *server = ARKIME_TYPE_ALLOC0(ArkimeHttpServer_t);

    int i;
    char **names = g_strsplit(hostnames, ",", 0);
    for (i = 0; names[i]; i++) {
        // Count entries
    }
    server->snames = malloc(i * sizeof(ArkimeHttpServerName_t));
    server->maxConns = maxConns;
    server->maxOutstandingRequests = maxOutstandingRequests;
    server->compress = compress;
    server->maxRetries = 2;
    server->clientAuth = NULL;

    for (i = 0; names[i]; i++) {
        g_strstrip(names[i]);
        if (!names[i][0])
            continue;
        server->snames[server->snamesCnt].server            = server;
        server->snames[server->snamesCnt].name              = g_strdup(names[i]);
        server->snames[server->snamesCnt].allowedAtSeconds  = 0;
        server->snamesCnt++;
    }
    g_strfreev(names);

    if (server->snamesCnt == 0) {
        LOGEXIT("ERROR - No valid endpoints in string '%s'", hostnames);
    }

    server->multi = curl_multi_init();
    curl_multi_setopt(server->multi, CURLMOPT_SOCKETFUNCTION, arkime_http_curlm_socket_callback);
    curl_multi_setopt(server->multi, CURLMOPT_SOCKETDATA, server);
    curl_multi_setopt(server->multi, CURLMOPT_TIMERFUNCTION, arkime_http_curlm_timeout_callback);
    curl_multi_setopt(server->multi, CURLMOPT_TIMERDATA, server);
    curl_multi_setopt(server->multi, CURLMOPT_MAX_TOTAL_CONNECTIONS, server->maxConns);
    curl_multi_setopt(server->multi, CURLMOPT_MAXCONNECTS, server->maxConns);

    server->multiTimer = g_timeout_add(50, arkime_http_timer_callback, server);

    server->fd2ev = g_hash_table_new(NULL, NULL);

    ARKIME_LOCK_INIT(server->syncRequest);

    return server;
}
/******************************************************************************/
void arkime_http_init()
{
    z_strm.zalloc = Z_NULL;
    z_strm.zfree  = Z_NULL;
    z_strm.opaque = Z_NULL;
    deflateInit2(&z_strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + 15, 8, Z_DEFAULT_STRATEGY);

    curl_global_init(CURL_GLOBAL_SSL);

    HASH_INIT(h_, connections, arkime_session_hash, (HASH_CMP_FUNC)arkime_http_conn_cmp);
    for (int r = 0; r <= PRIORITY_MAX; r++) {
        DLL_INIT(rqt_, &requests[r]);
    }
}
/******************************************************************************/
void arkime_http_exit()
{
    curl_global_cleanup();
}
