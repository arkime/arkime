/* http.c  -- Functions dealing with http connections.
 *
 * Copyright 2012-2015 AOL Inc. All rights reserved.
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
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include "moloch.h"
#include "zlib.h"

extern MolochConfig_t        config;

struct molochhttpserver_t;
typedef struct molochhttpserver_t MolochHttpServer_t;

typedef struct molochttprequest_t {
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

typedef struct molochhttpconn_t {
    struct molochhttpconn_t *h_next, *h_prev;
    uint32_t                 h_hash;
    short                    h_bucket;

    uint64_t                 sessionIda;
    uint32_t                 sessionIdb;
} MolochHttpConn_t;

typedef struct molochhttpconnhead_t {
    struct molochhttpconn_t *h_next, *h_prev;
    short                    h_count;

} MolochHttpConnHead_t;

HASH_VAR(s_, connections, MolochHttpConnHead_t, 119);

uint64_t connectionsSet[2048];
#define BIT_ISSET(bit, bits) ((bits[bit/64] & (1 << (bit % 64))) != 0)
#define BIT_SET(bit, bits) bits[bit/64] |= (1 << (bit % 64))
#define BIT_CLR(bit, bits) bits[bit/64] &= ~(1 << (bit % 64))

struct molochhttpserver_t {
    char                **names;
    int                   namesCnt;
    int                   namesPos;
    char                  compress;
    char                  https;
    int                   defaultPort;
    uint16_t              maxConns;
    uint16_t              maxOutstandingRequests;
    uint16_t              outstanding;
    uint16_t              connections;

    MolochHttpRequest_t   syncRequest;
    CURL                 *multi;
    guint                 multiTimer;
    int                   multiRunning;

    MolochHttpHeader_cb   headerCb;
};

static z_stream z_strm;

/******************************************************************************/
int moloch_http_conn_cmp(const void *keyv, const void *elementv)
{
    MolochHttpConn_t *conn = (MolochHttpConn_t *)elementv;

    return (*(uint64_t *)keyv     == conn->sessionIda && 
            *(uint32_t *)(keyv+8) == conn->sessionIdb);
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
}
/******************************************************************************/
void moloch_http_exit()
{
    curl_global_cleanup();
}
/******************************************************************************/
void *moloch_http_create_server(char *hostnames, int defaultPort, int maxConns, int maxOutstandingRequests, int compress)
{
    MolochHttpServer_t *server = MOLOCH_TYPE_ALLOC0(MolochHttpServer_t);

    server->names = g_strsplit(hostnames, ",", 0);
    uint32_t i;
    for (i = 0; server->names[i]; i++) {
        if (strncmp(server->names[i], "http://", 7) == 0) {
            char *tmp = g_strdup(server->names[i] + 7);
            g_free(server->names[i]);
            server->names[i] = tmp;
        } else if (strncmp(server->names[i], "https://", 8) == 0) {
            char *tmp = g_strdup(server->names[i] + 8);
            g_free(server->names[i]);
            server->names[i] = tmp;
            server->https = TRUE;
        }
    }
    server->namesCnt = i;
    server->defaultPort = defaultPort;
    server->maxConns = maxConns;
    server->maxOutstandingRequests = maxOutstandingRequests;
    server->compress = compress;
    LOG("https = %d", server->https);

    return server;
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
unsigned char *moloch_http_send_sync(void *serverV, char *method, char *key, uint32_t key_len, char *data, uint32_t data_len, char **UNUSED(headers), size_t *return_len)
{
    MolochHttpServer_t        *server = serverV;

    if (return_len)
        *return_len = 0;

    CURL *easy;

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

    if (strchr(host, ':') == 0) {
        snprintf(url, sizeof(url), "%s://%s:%d%.*s", (server->https?"https":"http"), host, server->defaultPort, key_len, key);
    } else {
        snprintf(url, sizeof(url), "%s://%s%.*s", (server->https?"https":"http"), host, key_len, key);
    }
    curl_easy_setopt(easy, CURLOPT_URL, url);

    server->syncRequest.used = 0;
    int res = curl_easy_perform(easy);

    if (res != CURLE_OK) {
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
    return (unsigned char *)server->syncRequest.dataIn;
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
            server->outstanding--;
        }
    }
}
/******************************************************************************/
static gboolean moloch_http_curlm_watch_callback(int fd, GIOCondition condition, gpointer serverV)
{
    MolochHttpServer_t        *server = serverV;

    int action = (condition & G_IO_IN ? CURL_CSELECT_IN : 0) |
                 (condition & G_IO_OUT ? CURL_CSELECT_OUT : 0);
    CURLMcode rc;
    while ((rc = curl_multi_socket_action(server->multi, fd, action, &server->multiRunning)) == CURLM_CALL_MULTI_PERFORM);
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

        ev = moloch_watch_fd(fd, (what&CURL_POLL_IN?G_IO_IN:0)|(what&CURL_POLL_OUT?G_IO_OUT:0), moloch_http_curlm_watch_callback, server);
        curl_multi_assign(server->multi, fd, (void*)ev);
    }

    return 0;
}
/******************************************************************************/
/* Called by glib when our timeout expires */
static gboolean moloch_http_curlm_timer_callback(gpointer serverV)
{
    MolochHttpServer_t        *server = serverV;
    CURLMcode rc;

    while ((rc = curl_multi_socket_action(server->multi, CURL_SOCKET_TIMEOUT, 0, &server->multiRunning)) == CURLM_CALL_MULTI_PERFORM);
    moloch_http_curlm_check_multi_info(server);
    server->multiTimer = 0;
    return G_SOURCE_REMOVE;
}
/******************************************************************************/
/* Update the event timer after curl_multi library calls */
static int moloch_http_curlm_timeout_callback(CURLM *UNUSED(multi), long timeout_ms, void *serverV)
{
  MolochHttpServer_t        *server = serverV;

  if (server->multiTimer)
      g_source_remove(server->multiTimer);

  if (timeout_ms != -1)
      server->multiTimer = g_timeout_add(timeout_ms, moloch_http_curlm_timer_callback, server);
  else
      server->multiTimer = 0;
  return 0;
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
    LOG("enter");
    MolochHttpServer_t        *server = serverV;

    BIT_SET(fd, connectionsSet);

    struct sockaddr_in localAddress, remoteAddress;
    socklen_t addressLength = sizeof(localAddress);
    getsockname(fd, (struct sockaddr*)&localAddress, &addressLength);
    addressLength = sizeof(remoteAddress);
    getpeername(fd, (struct sockaddr*)&remoteAddress, &addressLength);
    char sessionId[MOLOCH_SESSIONID_LEN];

    moloch_session_id(sessionId, localAddress.sin_addr.s_addr, localAddress.sin_port,
                      remoteAddress.sin_addr.s_addr, remoteAddress.sin_port);

    MolochHttpConn_t *conn;
    HASH_FIND(h_, connections, sessionId, conn);
    if (!conn) {
        conn = MOLOCH_TYPE_ALLOC0(MolochHttpConn_t);

        HASH_ADD(h_, connections, sessionId, conn);
        memcpy(&conn->sessionIda, sessionId, 8);
        memcpy(&conn->sessionIdb, sessionId+8, 4);
        server->connections++;
    } else {
        LOG("ERROR - Already added %x %s", condition, moloch_friendly_session_id(6, localAddress.sin_addr.s_addr, htons(localAddress.sin_port),
                                                                                 remoteAddress.sin_addr.s_addr, htons(remoteAddress.sin_port)));
    }

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
        close (fd);
        return 0;
    }

    struct sockaddr_in localAddress, remoteAddress;
    socklen_t addressLength = sizeof(localAddress);
    getsockname(fd, (struct sockaddr*)&localAddress, &addressLength);
    addressLength = sizeof(remoteAddress);
    getpeername(fd, (struct sockaddr*)&remoteAddress, &addressLength);

    char sessionId[MOLOCH_SESSIONID_LEN];

    moloch_session_id(sessionId, localAddress.sin_addr.s_addr, localAddress.sin_port,
                      remoteAddress.sin_addr.s_addr, remoteAddress.sin_port);

    MolochHttpConn_t *conn;
    HASH_FIND(h_, connections, sessionId, conn);
    BIT_CLR(fd, connectionsSet);
    if (conn) {
        HASH_REMOVE(h_, connections, conn);
        MOLOCH_TYPE_FREE(MolochHttpConn_t, conn);
    }

    server->connections--;

    close (fd);
    return 0;
}
/******************************************************************************/
gboolean moloch_http_send(void *serverV, char *method, char *key, uint32_t key_len, char *data, uint32_t data_len, char **headers, gboolean dropable, MolochHttpResponse_cb func, gpointer uw)
{
    MolochHttpServer_t        *server = serverV;

    // Are we overloaded
    if (!config.exiting && dropable && server->outstanding > server->maxOutstandingRequests) {
        LOG("ERROR - Dropping request %.*s of size %d queue %d is too big", key_len, key, data_len, server->outstanding);

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
    }

    request->server     = server;
    request->func       = func;
    request->uw         = uw;
    request->dataOut    = data;
    request->dataOutLen = data_len;

    if (!server->multi) {
        server->multi = curl_multi_init();
        curl_multi_setopt(server->multi, CURLMOPT_SOCKETFUNCTION, moloch_http_curlm_socket_callback);
        curl_multi_setopt(server->multi, CURLMOPT_SOCKETDATA, server);
        curl_multi_setopt(server->multi, CURLMOPT_TIMERFUNCTION, moloch_http_curlm_timeout_callback);
        curl_multi_setopt(server->multi, CURLMOPT_TIMERDATA, server);
        LOG("maxConns = %d", server->maxConns);
        curl_multi_setopt(server->multi, CURLMOPT_MAX_HOST_CONNECTIONS, server->maxConns);
    }

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

    curl_easy_setopt(request->easy, CURLOPT_CONNECTTIMEOUT, 30);

    char *host = server->names[server->namesPos];
    server->namesPos = (server->namesPos + 1) % server->namesCnt;

    if (strchr(host, ':') == 0) {
        snprintf(request->url, sizeof(request->url), "%s://%s:%d%.*s", (server->https?"https":"http"), host, server->defaultPort, key_len, key);
    } else {
        snprintf(request->url, sizeof(request->url), "%s://%s%.*s", (server->https?"https":"http"), host, key_len, key);
    }

    curl_easy_setopt(request->easy, CURLOPT_URL, request->url);

    server->outstanding++;
    curl_multi_add_handle(server->multi, request->easy);
    curl_multi_socket_action(server->multi, CURL_SOCKET_TIMEOUT, 0, &server->multiRunning);


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
void moloch_http_set_header_cb(void *serverV, MolochHttpHeader_cb cb)
{
    MolochHttpServer_t        *server = serverV;
    server->headerCb                  = cb;
}
/******************************************************************************/
void moloch_http_free_server(void *serverV)
{
    MolochHttpServer_t        *server = serverV;

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

    HASH_FIND_HASH(h_, connections, hash, key, conn);
    return (conn?1:0);
}
