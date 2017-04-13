/* http.c  -- Functions dealing with http connections.
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
#include "moloch.h"
#include "zlib.h"
#include <errno.h>

//#define MOLOCH_HTTP_DEBUG

extern MolochConfig_t        config;

struct molochhttpserver_t;
typedef struct molochhttpserver_t MolochHttpServer_t;

typedef struct molochhttprequest_t {
    struct molochhttprequest_t *rqt_next, *rqt_prev;

    MolochHttpResponse_cb func;
    gpointer              uw;

    MolochHttpServer_t   *server;
    CURL                 *easy;
    char                  url[1024];

    unsigned char        *dataIn;
    uint32_t              used;
    uint32_t              size;

    struct curl_slist    *headerList;
    char                 *dataOut;
    uint32_t              dataOutLen;

} MolochHttpRequest_t;

typedef struct {
    struct molochhttprequest_t *rqt_next, *rqt_prev;
    int                         rqt_count;
} MolochHttpRequestHead_t;

typedef struct molochhttpconn_t {
    struct molochhttpconn_t *h_next, *h_prev;
    uint32_t                 h_hash;
    short                    h_bucket;

    char                     sessionId[MOLOCH_SESSIONID_LEN];
} MolochHttpConn_t;

typedef struct molochhttpconnhead_t {
    struct molochhttpconn_t *h_next, *h_prev;
    short                    h_count;

} MolochHttpConnHead_t;

static HASH_VAR(s_, connections, MolochHttpConnHead_t, 119);
static MOLOCH_LOCK_DEFINE(connections);

static MolochHttpRequestHead_t requests;
static int                     requestsTimer;
static MOLOCH_LOCK_DEFINE(requests);

uint64_t connectionsSet[2048];
#define BIT_ISSET(bit, bits) ((bits[bit/64] & (1 << (bit % 64))) != 0)
#define BIT_SET(bit, bits) bits[bit/64] |= (1 << (bit % 64))
#define BIT_CLR(bit, bits) bits[bit/64] &= ~(1 << (bit % 64))

struct molochhttpserver_t {
    uint64_t              dropped;
    char                **names;
    int                   namesCnt;
    int                   namesPos;
    char                  compress;
    uint16_t              maxConns;
    uint16_t              maxOutstandingRequests;
    uint16_t              outstanding;
    uint16_t              connections;

    MOLOCH_LOCK_EXTERN(syncRequest);
    MolochHttpRequest_t   syncRequest;
    CURL                 *multi;
    guint                 multiTimer;
    int                   multiRunning;

    MolochHttpHeader_cb   headerCb;
};

static z_stream z_strm;
static MOLOCH_LOCK_DEFINE(z_strm);

/******************************************************************************/
int moloch_http_conn_cmp(const void *keyv, const void *elementv)
{
    MolochHttpConn_t *conn = (MolochHttpConn_t *)elementv;

    return memcmp(keyv, conn->sessionId, MIN(((uint8_t *)keyv)[0], conn->sessionId[0])) == 0;
}
/******************************************************************************/
static size_t moloch_http_curl_write_callback(void *contents, size_t size, size_t nmemb, void *requestP)
{
    MolochHttpRequest_t *request = requestP;

    size_t sz = size * nmemb;

    if (!request->dataIn) {
        double cl;
        curl_easy_getinfo(request->easy, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &cl);
        request->used = sz;
        request->size = MAX(sz, cl);
        request->dataIn = malloc(request->size+1);
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
unsigned char *moloch_http_send_sync(void *serverV, const char *method, const char *key, uint32_t key_len, char *data, uint32_t data_len, char **UNUSED(headers), size_t *return_len)
{
    MolochHttpServer_t        *server = serverV;

    if (return_len)
        *return_len = 0;

    CURL *easy;

    MOLOCH_LOCK(server->syncRequest);
    if (!server->syncRequest.easy) {
        easy = server->syncRequest.easy = curl_easy_init();
        if (config.debug >= 2) {
            curl_easy_setopt(easy, CURLOPT_VERBOSE, 1);
        }
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, moloch_http_curl_write_callback);
        curl_easy_setopt(easy, CURLOPT_WRITEDATA, (void *)&server->syncRequest);
    } else {
        easy = server->syncRequest.easy;
    }

    if (method[0] != 'G') {
        curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, method);
        curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE, data_len);
        curl_easy_setopt(easy, CURLOPT_POSTFIELDS, data);
    } else {
        curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, NULL);
        curl_easy_setopt(easy, CURLOPT_HTTPGET, 1L);
    }

    char url[1000];
    char *host = server->names[server->namesPos];
    server->namesPos = (server->namesPos + 1) % server->namesCnt;

    snprintf(url, sizeof(url), "%s%.*s", host, key_len, key);
    curl_easy_setopt(easy, CURLOPT_URL, url);

    server->syncRequest.used = 0;
    int res = curl_easy_perform(easy);

    if (res != CURLE_OK) {
        LOG("libcurl failure %s error '%s'", url, curl_easy_strerror(res));
        MOLOCH_UNLOCK(server->syncRequest);
        return 0;
    }

    if (server->syncRequest.dataIn)
        server->syncRequest.dataIn[server->syncRequest.used] = 0;

    if (return_len)
        *return_len = server->syncRequest.used;

    if (config.logESRequests) {
        long   responseCode;
        double totalTime;
        double connectTime;
        double uploadSize;
        double downloadSize;

        curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &responseCode);
        curl_easy_getinfo(easy, CURLINFO_TOTAL_TIME, &totalTime);
        curl_easy_getinfo(easy, CURLINFO_CONNECT_TIME, &connectTime);
        curl_easy_getinfo(easy, CURLINFO_SIZE_UPLOAD, &uploadSize);
        curl_easy_getinfo(easy, CURLINFO_SIZE_DOWNLOAD, &downloadSize);

        LOG("%d/%d SYNC %ld %s %.0lf/%0.lf %.0lfms %.0lfms",
           1, 1,
           responseCode,
           url,
           uploadSize,
           downloadSize,
           connectTime*1000,
           totalTime*1000);
    }
    uint8_t *dataIn = server->syncRequest.dataIn;
    server->syncRequest.dataIn = 0;
    MOLOCH_UNLOCK(server->syncRequest);
    return dataIn;
}
/******************************************************************************/
static void moloch_http_curlm_check_multi_info(MolochHttpServer_t *server)
{
    char *eff_url;
    CURLMsg *msg;
    int msgs_left;
    MolochHttpRequest_t *request;
    CURL *easy;

    while ((msg = curl_multi_info_read(server->multi, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            easy = msg->easy_handle;
            curl_easy_getinfo(easy, CURLINFO_PRIVATE, (void*)&request);
            curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &eff_url);

            long   responseCode;
            curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &responseCode);

            if (config.logESRequests) {
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
                   connectTime*1000,
                   totalTime*1000);
            }

#ifdef MOLOCH_HTTP_DEBUG
            LOG("HTTPDEBUG DECR %s %p %d %s", server->names[0], request, server->outstanding, request->url);
#endif


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
                MOLOCH_SIZE_FREE(buffer, request->dataOut);
            }
            if (request->headerList) {
                curl_slist_free_all(request->headerList);
            }
            MOLOCH_TYPE_FREE(MolochHttpRequest_t, request);

            curl_multi_remove_handle(server->multi, easy);
            curl_easy_cleanup(easy);
            MOLOCH_LOCK(requests);
            server->outstanding--;
            MOLOCH_UNLOCK(requests);
        }
    }
}
/******************************************************************************/
static gboolean moloch_http_watch_callback(int fd, GIOCondition condition, gpointer serverV)
{
    MolochHttpServer_t        *server = serverV;

    int action = (condition & G_IO_IN ? CURL_CSELECT_IN : 0) |
                 (condition & G_IO_OUT ? CURL_CSELECT_OUT : 0) |
                 (condition & (G_IO_HUP | G_IO_ERR) ? CURL_CSELECT_ERR : 0);

    while (curl_multi_socket_action(server->multi, fd, action, &server->multiRunning) == CURLM_CALL_MULTI_PERFORM) {
    }

    moloch_http_curlm_check_multi_info(server);
    return TRUE;
}
/******************************************************************************/
static int moloch_http_curlm_socket_callback(CURL *UNUSED(easy), curl_socket_t fd, int what, void *serverV, void *evP)
{
    MolochHttpServer_t        *server = serverV;
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

        ev = moloch_watch_fd(fd, (what&CURL_POLL_IN?MOLOCH_GIO_READ_COND:0)|(what&CURL_POLL_OUT?MOLOCH_GIO_WRITE_COND:0), moloch_http_watch_callback, server);
        curl_multi_assign(server->multi, fd, (void*)ev);
    }

    return 0;
}
/******************************************************************************/
/* Called by glib when our timeout expires */
static gboolean moloch_http_timer_callback(gpointer serverV)
{
    MolochHttpServer_t        *server = serverV;

    while (curl_multi_perform(server->multi, &server->multiRunning) == CURLM_CALL_MULTI_PERFORM) {
    }
    moloch_http_curlm_check_multi_info(server);
    return G_SOURCE_CONTINUE;
}
/******************************************************************************/
static int moloch_http_curlm_timeout_callback(CURLM *UNUSED(multi), long timeout_ms, void *serverV)
{
    if (timeout_ms < 10)
        moloch_http_timer_callback(serverV);
    return CURLE_OK;
}

/******************************************************************************/
size_t moloch_http_curlm_header_function(char *buffer, size_t size, size_t nitems, void *requestP)
{
    MolochHttpRequest_t *request = requestP;
    int sz = size*nitems;
    int i = sz;

    while (i > 0 && (buffer[i-1] == '\r' || buffer[i-1] == '\n')) {
        buffer[i -1] = 0;
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
static gboolean moloch_http_curl_watch_open_callback(int fd, GIOCondition condition, gpointer serverV)
{
    MolochHttpServer_t        *server = serverV;


    struct sockaddr_storage localAddressStorage, remoteAddressStorage;

    socklen_t addressLength = sizeof(localAddressStorage);
    int rc = getsockname(fd, (struct sockaddr*)&localAddressStorage, &addressLength);
    if (rc != 0)
        return FALSE;

    addressLength = sizeof(remoteAddressStorage);
    rc = getpeername(fd, (struct sockaddr*)&remoteAddressStorage, &addressLength);
    if (rc != 0)
        return FALSE;

    char sessionId[MOLOCH_SESSIONID_LEN];
    int  localPort, remotePort;
    char remoteIp[INET6_ADDRSTRLEN+2];
    if (localAddressStorage.ss_family == AF_INET) {
        struct sockaddr_in *localAddress = (struct sockaddr_in *)&localAddressStorage;
        struct sockaddr_in *remoteAddress = (struct sockaddr_in *)&remoteAddressStorage;
        moloch_session_id(sessionId, localAddress->sin_addr.s_addr, localAddress->sin_port,
                          remoteAddress->sin_addr.s_addr, remoteAddress->sin_port);
        localPort = ntohs(localAddress->sin_port);
        remotePort = ntohs(remoteAddress->sin_port);
        inet_ntop(AF_INET, &remoteAddress->sin_addr, remoteIp, sizeof(remoteIp));
    } else {
        struct sockaddr_in6 *localAddress = (struct sockaddr_in6 *)&localAddressStorage;
        struct sockaddr_in6 *remoteAddress = (struct sockaddr_in6 *)&remoteAddressStorage;
        moloch_session_id6(sessionId, localAddress->sin6_addr.s6_addr, localAddress->sin6_port,
                          remoteAddress->sin6_addr.s6_addr, remoteAddress->sin6_port);
        localPort = ntohs(localAddress->sin6_port);
        remotePort = ntohs(remoteAddress->sin6_port);
        inet_ntop(AF_INET6, &remoteAddress->sin6_addr, remoteIp+1, sizeof(remoteIp)-2);
        remoteIp[0] = '[';
        strcat(remoteIp, "]");
    }

    LOG("Connected %d/%d - %s   %d->%s:%d - fd:%d",
            server->outstanding,
            server->connections,
            server->names[0],
            localPort,
            remoteIp,
            remotePort,
            fd);

    MolochHttpConn_t *conn;

    MOLOCH_LOCK(connections);
    BIT_SET(fd, connectionsSet);
    HASH_FIND(h_, connections, sessionId, conn);
    if (!conn) {
        conn = MOLOCH_TYPE_ALLOC0(MolochHttpConn_t);

        HASH_ADD(h_, connections, sessionId, conn);
        memcpy(&conn->sessionId, sessionId, sessionId[0]);
        server->connections++;
    } else {
        char buf[1000];
        LOG("ERROR - Already added %x %s", condition, moloch_session_id_string(sessionId, buf));
    }
    MOLOCH_UNLOCK(connections);

    moloch_http_curlm_check_multi_info(server);

    return FALSE;
}
/******************************************************************************/
curl_socket_t moloch_http_curl_open_callback(void *serverV, curlsocktype UNUSED(purpose), struct curl_sockaddr *addr)
{
    int fd = socket(addr->family, addr->socktype, addr->protocol);

    moloch_watch_fd(fd, G_IO_OUT | G_IO_IN, moloch_http_curl_watch_open_callback, serverV);
    return fd;
}
/******************************************************************************/
int moloch_http_curl_close_callback(void *serverV, curl_socket_t fd)
{
    MolochHttpServer_t        *server = serverV;

    if (! BIT_ISSET(fd, connectionsSet)) {
        LOG("Couldn't connect %s", server->names[0]);
        return 0;
    }

    struct sockaddr_storage localAddressStorage, remoteAddressStorage;

    socklen_t addressLength = sizeof(localAddressStorage);
    int rc = getsockname(fd, (struct sockaddr*)&localAddressStorage, &addressLength);
    if (rc != 0)
        return 0;

    addressLength = sizeof(remoteAddressStorage);
    rc = getpeername(fd, (struct sockaddr*)&remoteAddressStorage, &addressLength);
    if (rc != 0)
        return 0;

    char sessionId[MOLOCH_SESSIONID_LEN];
    int  localPort, remotePort;
    char remoteIp[INET6_ADDRSTRLEN+2];
    if (localAddressStorage.ss_family == AF_INET) {
        struct sockaddr_in *localAddress = (struct sockaddr_in *)&localAddressStorage;
        struct sockaddr_in *remoteAddress = (struct sockaddr_in *)&remoteAddressStorage;
        moloch_session_id(sessionId, localAddress->sin_addr.s_addr, localAddress->sin_port,
                          remoteAddress->sin_addr.s_addr, remoteAddress->sin_port);
        localPort = ntohs(localAddress->sin_port);
        remotePort = ntohs(remoteAddress->sin_port);
        inet_ntop(AF_INET, &remoteAddress->sin_addr, remoteIp, sizeof(remoteIp));
    } else {
        struct sockaddr_in6 *localAddress = (struct sockaddr_in6 *)&localAddressStorage;
        struct sockaddr_in6 *remoteAddress = (struct sockaddr_in6 *)&remoteAddressStorage;
        moloch_session_id6(sessionId, localAddress->sin6_addr.s6_addr, localAddress->sin6_port,
                          remoteAddress->sin6_addr.s6_addr, remoteAddress->sin6_port);
        localPort = ntohs(localAddress->sin6_port);
        remotePort = ntohs(remoteAddress->sin6_port);
        inet_ntop(AF_INET6, &remoteAddress->sin6_addr, remoteIp+1, sizeof(remoteIp)-2);
        remoteIp[0] = '[';
        strcat(remoteIp, "]");
    }


    MolochHttpConn_t *conn;
    BIT_CLR(fd, connectionsSet);

    MOLOCH_LOCK(connections);
    HASH_FIND(h_, connections, sessionId, conn);
    if (conn) {
        HASH_REMOVE(h_, connections, conn);
        MOLOCH_TYPE_FREE(MolochHttpConn_t, conn);
    }
    MOLOCH_UNLOCK(connections);

    server->connections--;

    LOG("Close %d/%d - %s   %d->%s:%d fd:%d removed: %s",
            server->outstanding,
            server->connections,
            server->names[0],
            localPort,
            remoteIp,
            remotePort,
            fd,
            conn?"true":"false");

    close (fd);
    return 0;
}
/******************************************************************************/
static gboolean moloch_http_send_timer_callback(gpointer UNUSED(unused))
{
    MolochHttpRequest_t       *request;

    while (1) {
        MOLOCH_LOCK(requests);
        DLL_POP_HEAD(rqt_, &requests, request);
        if (!request) {
            requestsTimer = 0;
            MOLOCH_UNLOCK(requests);
            return G_SOURCE_REMOVE;
        }
        MOLOCH_UNLOCK(requests);

#ifdef MOLOCH_HTTP_DEBUG
        LOG("HTTPDEBUG DO %s %p %d %s", request->server->names[0], request, request->server->outstanding, request->url);
#endif
        curl_multi_add_handle(request->server->multi, request->easy);
    }

    return G_SOURCE_REMOVE;
}
/******************************************************************************/
gboolean moloch_http_send(void *serverV, const char *method, const char *key, uint32_t key_len, char *data, uint32_t data_len, char **headers, gboolean dropable, MolochHttpResponse_cb func, gpointer uw)
{
    MolochHttpServer_t        *server = serverV;

    // Are we overloaded
    if (dropable && !config.quitting && server->outstanding > server->maxOutstandingRequests) {
        LOG("ERROR - Dropping request %.*s of size %d queue %d is too big", key_len, key, data_len, server->outstanding);
        server->dropped++;

        if (data) {
            MOLOCH_SIZE_FREE(buffer, data);
        }
        return 1;
    }

    MolochHttpRequest_t       *request = MOLOCH_TYPE_ALLOC0(MolochHttpRequest_t);

    if (headers) {
        int i;
        for (i = 0; headers[i]; i++) {
            request->headerList = curl_slist_append(request->headerList, headers[i]);
        }
    }

    // Do we need to compress item
    if (server->compress && data && data_len > 1000) {
        char            *buf = moloch_http_get_buffer(data_len);
        int              ret;

        MOLOCH_LOCK(z_strm);
        z_strm.avail_in   = data_len;
        z_strm.next_in    = (unsigned char *)data;
        z_strm.avail_out  = data_len;
        z_strm.next_out   = (unsigned char *)buf;
        ret = deflate(&z_strm, Z_FINISH);
        if (ret == Z_STREAM_END) {
            request->headerList = curl_slist_append(request->headerList, "Content-Encoding: deflate");
            MOLOCH_SIZE_FREE(buffer, data);
            data_len = data_len - z_strm.avail_out;
            data     = buf;
        } else {
            MOLOCH_SIZE_FREE(buffer, buf);
        }

        deflateReset(&z_strm);
        MOLOCH_UNLOCK(z_strm);
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

    curl_easy_setopt(request->easy, CURLOPT_WRITEFUNCTION, moloch_http_curl_write_callback);
    curl_easy_setopt(request->easy, CURLOPT_WRITEDATA, (void *)request);
    curl_easy_setopt(request->easy, CURLOPT_PRIVATE, (void *)request);
    curl_easy_setopt(request->easy, CURLOPT_OPENSOCKETFUNCTION, moloch_http_curl_open_callback);
    curl_easy_setopt(request->easy, CURLOPT_OPENSOCKETDATA, server);
    curl_easy_setopt(request->easy, CURLOPT_CLOSESOCKETFUNCTION, moloch_http_curl_close_callback);
    curl_easy_setopt(request->easy, CURLOPT_CLOSESOCKETDATA, server);
    curl_easy_setopt(request->easy, CURLOPT_ACCEPT_ENCODING, ""); // https://curl.haxx.se/libcurl/c/CURLOPT_ACCEPT_ENCODING.html

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
        curl_easy_setopt(request->easy, CURLOPT_HEADERFUNCTION, moloch_http_curlm_header_function);
        curl_easy_setopt(request->easy, CURLOPT_HEADERDATA, request);
    }

    curl_easy_setopt(request->easy, CURLOPT_CONNECTTIMEOUT, 10L);

    char *host = server->names[server->namesPos];
    server->namesPos = (server->namesPos + 1) % server->namesCnt;

    snprintf(request->url, sizeof(request->url), "%s%.*s", host, key_len, key);

    curl_easy_setopt(request->easy, CURLOPT_URL, request->url);

    MOLOCH_LOCK(requests);
#ifdef MOLOCH_HTTP_DEBUG
    LOG("HTTPDEBUG INCR %s %p %d %s", server->names[0], request, server->outstanding, request->url);
#endif
    server->outstanding++;

    DLL_PUSH_TAIL(rqt_, &requests, request);

    if (!requestsTimer)
        requestsTimer = g_timeout_add(0, moloch_http_send_timer_callback, NULL);
    MOLOCH_UNLOCK(requests);

    return 0;
}

/******************************************************************************/
gboolean moloch_http_set(void *serverV, char *key, int key_len, char *data, uint32_t data_len, MolochHttpResponse_cb func, gpointer uw)
{
    // If no func then this request is dropable
    return moloch_http_send(serverV, "POST", key, key_len, data, data_len, NULL, func == 0, func, uw);
}

/******************************************************************************/
unsigned char *moloch_http_get(void *serverV, char *key, int key_len, size_t *mlen)
{
    return moloch_http_send_sync(serverV, "GET", key, key_len, NULL, 0, NULL, mlen);
}

/******************************************************************************/
int moloch_http_queue_length(void *serverV)
{
    MolochHttpServer_t        *server = serverV;
    return server?server->outstanding:0;
}
/******************************************************************************/
uint64_t moloch_http_dropped_count(void *serverV)
{
    MolochHttpServer_t        *server = serverV;
    return server?server->dropped:0;
}
/******************************************************************************/
void moloch_http_set_header_cb(void *serverV, MolochHttpHeader_cb cb)
{
    MolochHttpServer_t        *server = serverV;
    server->headerCb                  = cb;
}
/******************************************************************************/
void moloch_http_free_server(void *serverV)
{
    MolochHttpServer_t        *server = serverV;

    g_source_remove(server->multiTimer);

    // Finish any still running requests
    while (server->multiRunning) {
        curl_multi_perform(server->multi, &server->multiRunning);
        moloch_http_curlm_check_multi_info(server);
    }

    // Free sync info
    if (server->syncRequest.easy) {
        curl_easy_cleanup(server->syncRequest.easy);
        server->syncRequest.easy = 0;
    }

    if (server->syncRequest.dataIn)
        free(server->syncRequest.dataIn);

    // Free multi info
    curl_multi_cleanup(server->multi);


    g_strfreev(server->names);

    MOLOCH_TYPE_FREE(MolochHttpServer_t, server);
}
/******************************************************************************/
gboolean moloch_http_is_moloch(uint32_t hash, char *key)
{
    MolochHttpConn_t *conn;

    MOLOCH_LOCK(connections);
    HASH_FIND_HASH(h_, connections, hash, key, conn);
    MOLOCH_UNLOCK(connections);
    return (conn?1:0);
}
/******************************************************************************/
void *moloch_http_create_server(const char *hostnames, int maxConns, int maxOutstandingRequests, int compress)
{
    MolochHttpServer_t *server = MOLOCH_TYPE_ALLOC0(MolochHttpServer_t);

    server->names = g_strsplit(hostnames, ",", 0);
    uint32_t i;
    for (i = 0; server->names[i]; i++) {
        // Count entries
    }
    server->namesCnt = i;
    server->maxConns = maxConns;
    server->maxOutstandingRequests = maxOutstandingRequests;
    server->compress = compress;

    server->multi = curl_multi_init();
    curl_multi_setopt(server->multi, CURLMOPT_SOCKETFUNCTION, moloch_http_curlm_socket_callback);
    curl_multi_setopt(server->multi, CURLMOPT_SOCKETDATA, server);
    curl_multi_setopt(server->multi, CURLMOPT_TIMERFUNCTION, moloch_http_curlm_timeout_callback);
    curl_multi_setopt(server->multi, CURLMOPT_TIMERDATA, server);
    curl_multi_setopt(server->multi, CURLMOPT_MAX_TOTAL_CONNECTIONS, server->maxConns);
    curl_multi_setopt(server->multi, CURLMOPT_MAXCONNECTS, server->maxConns);

    server->multiTimer = g_timeout_add(50, moloch_http_timer_callback, server);

    MOLOCH_LOCK_INIT(server->syncRequest);

    return server;
}
/******************************************************************************/
void moloch_http_init()
{
    z_strm.zalloc = Z_NULL;
    z_strm.zfree  = Z_NULL;
    z_strm.opaque = Z_NULL;
    deflateInit(&z_strm, Z_DEFAULT_COMPRESSION);

    curl_global_init(CURL_GLOBAL_SSL);

    HASH_INIT(h_, connections, moloch_session_hash, moloch_http_conn_cmp);
    memset(&connectionsSet, 0, sizeof(connectionsSet));
    DLL_INIT(rqt_, &requests);
}
/******************************************************************************/
void moloch_http_exit()
{
    curl_global_cleanup();
}
