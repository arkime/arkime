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
 * Needs to be rewritten
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "zlib.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "moloch.h"

#define DEBUGCONN(...)
//#define DEBUGCONN(...) do {LOG(__VA_ARGS__);fflush(stdout); } while(0)

/******************************************************************************/
extern MolochConfig_t        config;

/******************************************************************************/

typedef struct molochrequest_t {
    struct molochrequest_t *r_next, *r_prev;

    MolochResponse_cb       func;
    gpointer                uw;
    char                    key[300];
    char                    method[16];
    char                    headers[2000];
    int                     key_len;
    char                   *data;
    uint32_t                data_len;
    char                    compress;
} MolochRequest_t;

typedef struct {
    struct molochrequest_t *r_next, *r_prev;
    uint32_t r_count;
} MolochRequestHead_t;

struct molochthttp_t;

typedef struct molochconn_t {
    struct molochconn_t *e_next, *e_prev;
    struct molochconn_t *h_next, *h_prev;
    uint32_t             h_hash;
    short                h_bucket;

    uint64_t             sessionIda;
    uint32_t             sessionIdb;

    gint                 readWatch;
    gint                 writeWatch;

    char                 header[1000];
    char                 line[1000];
    struct timeval       startTime;
    struct timeval       sendTime;
    struct timeval       sentTime;
    struct timeval       endTime;
    char                 hp_data[10000000];
    uint32_t             sent;
    uint32_t             hp_len;
    uint16_t             hp_complete;
    GSocket             *conn;
    http_parser          parser;
    MolochRequest_t     *request;
    struct molochhttp_t *server;
    char                *name;
    char                 doClose;
} MolochConn_t;

typedef struct {
    MolochConn_t        *e_next, *e_prev;
    MolochConn_t        *h_next, *h_prev;
    short                e_count;
    short                h_count;
} MolochConnHead_t;

HASH_VAR(s_, connections, MolochConnHead_t, 119);

typedef struct molochhttp_t {
    MolochConn_t         *syncConn;
    char                **names;
    int                   namesCnt;
    int                   namesPos;
    char                  compress;
    int                   port;
    int                   inProgress;
    uint16_t              maxConns;
    uint16_t              numConns;
    uint16_t              maxOutstandingRequests;
    MolochConnHead_t      connQ;
    time_t                lastFailedConnect;
    MolochRequestHead_t   requestQ;
    http_parser_settings  parserSettings;
    MolochHttpHeader_cb   headerCb;
} MolochHttp_t;




gboolean moloch_http_process_send(MolochConn_t *conn, gboolean sync);
/******************************************************************************/
int
moloch_http_hp_cb_on_message_begin (http_parser *parser)
{
    MolochConn_t *info = parser->data;

    info->hp_len      = 0;
    info->hp_complete = 0;
    return 0;
}
/******************************************************************************/
int
moloch_http_hp_cb_on_header_field (http_parser *parser, const char *at, size_t length)
{
    MolochConn_t *conn = parser->data;
    memcpy(conn->header, at, MIN(length, 999));
    conn->header[MIN(length, 999)] = 0;
    return 0;
}
/******************************************************************************/
int
moloch_http_hp_cb_on_header_value (http_parser *parser, const char *at, size_t length)
{
    MolochConn_t *conn = parser->data;
    if (conn->server->headerCb) {
        conn->server->headerCb(conn->request->key, conn->header, at, length, conn->request->uw);
    }
    if (length == 5 && strcasecmp(conn->header, "connection") == 0 && memcmp(at, "close", 5) == 0 ) {
        DEBUGCONN("AAA conn setclose %s %p ww:%d", conn->server->names[0], conn, conn->writeWatch);
        conn->doClose = 1;
    }
    return 0;
}
/******************************************************************************/
int
moloch_http_hp_cb_on_body (http_parser *parser, const char *at, size_t length)
{
    MolochConn_t *info = parser->data;

    if (info->hp_len + length >= sizeof(info->hp_data)) {
        LOG("HP ERROR: Too much data to parse");
        return 0;
    }
    memcpy(info->hp_data + info->hp_len, at, length);
    info->hp_len += length;
    return 0;
}

/******************************************************************************/
int
moloch_http_hp_cb_on_message_complete (http_parser *parser)
{
    MolochConn_t *info = parser->data;

    info->hp_complete = 1;
    return 0;
}
/******************************************************************************/
gboolean moloch_http_finish(MolochConn_t *conn, gboolean sync);

void moloch_http_do_requests(MolochHttp_t *server);
gboolean moloch_http_conn_cb(gint UNUSED(fd), GIOCondition UNUSED(cond), gpointer data) {
    MolochConn_t        *conn = data;

    DLL_PUSH_TAIL(e_, &conn->server->connQ, conn);
    DEBUGCONN("AAA conn ww clear %s %p fd:%d,%d ww:%d", conn->server->names[0], conn, fd, g_socket_get_fd(conn->conn), conn->writeWatch);
    conn->writeWatch = 0;
    moloch_http_do_requests(conn->server);
    return FALSE;
}

/******************************************************************************/
void moloch_http_free_conn(MolochConn_t *conn, gboolean process)
{
    DEBUGCONN("AAA free %s %p read: %d write: %d", conn->server->names[0], conn, conn->readWatch, conn->writeWatch);

    if (conn->readWatch)
        g_source_remove(conn->readWatch);
    if (conn->writeWatch)
        g_source_remove(conn->writeWatch);

    MolochHttp_t *server = conn->server;
    if (conn->conn) {
        g_object_unref (conn->conn);
        conn->conn = 0;
    }
    conn->request = 0;
    conn->server->numConns--;

    if (conn->h_next) {
        HASH_REMOVE(h_, connections, conn);
    }

    if (conn->e_next) {
        DLL_REMOVE(e_, &conn->server->connQ, conn);
    }

    MOLOCH_TYPE_FREE(MolochConn_t, conn);
    if (process)
        moloch_http_do_requests(server);
}
/******************************************************************************/
gboolean moloch_http_write_cb(gint UNUSED(fd), GIOCondition UNUSED(cond), gpointer data) {
    MolochConn_t        *conn = data;
    GError              *gerror = 0;

    if (!conn->request) {
        DEBUGCONN("AAA ww clear %s %p fd:%d,%d ww:%d", conn->server->names[0], conn, fd, g_socket_get_fd(conn->conn), conn->writeWatch);
        conn->writeWatch = 0;
        return FALSE;
    }

    int sent = g_socket_send(conn->conn, conn->request->data+conn->sent, conn->request->data_len-conn->sent, NULL, &gerror);
    conn->sent += sent;

    if (gerror) {
        /* Should free stuff here */
        LOG("ERROR: %p: Write Error: %s", (void*)conn, gerror->message);
        g_error_free(gerror);
        DEBUGCONN("AAA ww clear %s %p fd:%d,%d ww:%d", conn->server->names[0], conn, fd, g_socket_get_fd(conn->conn), conn->writeWatch);
        conn->writeWatch = 0;
        return FALSE;
    }

    gboolean finished = conn->sent == conn->request->data_len;
    if (finished) {
        moloch_http_finish(conn, FALSE);
        DEBUGCONN("AAA ww finish %s %p fd:%d ww:%d", conn->server->names[0], conn, g_socket_get_fd(conn->conn), conn->writeWatch);
        conn->writeWatch = 0;
    }

    return !finished;
}
/******************************************************************************/
gboolean moloch_http_read_cb(gint UNUSED(fd), GIOCondition cond, gpointer data) {
    MolochConn_t        *conn = data;
    char                 buffer[0xffff];
    int                  len;
    GError              *gerror = 0;

    len = g_socket_receive(conn->conn, buffer, sizeof(buffer)-1, NULL, &gerror);

    if (gerror || cond & (G_IO_HUP | G_IO_ERR) || len <= 0) {
        if (gerror) {
            LOG("ERROR: %s %p:%p Receive Error: %s", conn->name, (void*)conn, conn->request, gerror->message);
            g_error_free(gerror);
        } else if (cond & (G_IO_HUP | G_IO_ERR))
            LOG("ERROR: %s %p:%p Lost connection to %s", conn->name, (void*)conn, conn->request, conn->name);
        else if (len <= 0)
            LOG("ERROR: %s %p:%p len: %d cond: %x", conn->name, (void*)conn, conn->request, len, cond);
        else
            LOG("ERROR HMM: %s %p:%p len: %d cond: %x", conn->name, (void*)conn, conn->request, len, cond);

        if (conn == conn->server->syncConn) {
            if (!conn->request) {
                DEBUGCONN("AAA zerosync %s %p", conn->server->names[0], conn);
                conn->server->syncConn = 0;
                moloch_http_free_conn(conn, FALSE);
            } else {
                DEBUGCONN("AAA complete %s %p", conn->server->names[0], conn);
                conn->hp_complete = 1;
            }
            return FALSE;
        }

        if (conn->request && conn->request->data) {
            conn->server->inProgress--;
            DLL_PUSH_HEAD(r_, &conn->server->requestQ, conn->request);
        } else if (conn->request) {
            conn->server->inProgress--;
            // Must save, free, then call function because of recursive sync GETs
            MolochResponse_cb  func = conn->request->func;
            gpointer           uw = conn->request->uw;

            MOLOCH_TYPE_FREE(MolochRequest_t, conn->request);

            if (func) {
                func(0, 0, uw);
            }
        }

        moloch_http_free_conn(conn, TRUE);
        return FALSE;
    }

    http_parser_execute(&conn->parser, &conn->server->parserSettings, buffer, len);

    if (conn->hp_complete) {
        gettimeofday(&conn->endTime, NULL);
        if (config.logESRequests)
            LOG("%s %ldms %ldms %ldms",
               conn->line,
               (conn->sendTime.tv_sec - conn->startTime.tv_sec)*1000 + (conn->sendTime.tv_usec - conn->startTime.tv_usec)/1000,
               (conn->sentTime.tv_sec - conn->startTime.tv_sec)*1000 + (conn->sentTime.tv_usec - conn->startTime.tv_usec)/1000,
               (conn->endTime.tv_sec - conn->startTime.tv_sec)*1000 + (conn->endTime.tv_usec - conn->startTime.tv_usec)/1000
               );

        conn->hp_data[conn->hp_len] = 0;

        /* Must save, free, then call function because of recursive sync GETs */
        MolochResponse_cb  func = conn->request->func;
        gpointer           uw = conn->request->uw;

        MOLOCH_TYPE_FREE(MolochRequest_t, conn->request);
        conn->request = 0;

        if (func) {
            func((unsigned char*)conn->hp_data, conn->hp_len, uw);
        }

        if (conn == conn->server->syncConn) {
            if (conn->doClose) {
                DEBUGCONN("AAA zerosync %s %p", conn->server->names[0], conn);
                conn->server->syncConn = 0;
                moloch_http_free_conn(conn, FALSE);
                return FALSE;
            }

            return TRUE;
        }

        // Not syncConn below this

        if (conn->doClose) {
            moloch_http_free_conn(conn, TRUE);
            return FALSE;
        }

        conn->server->inProgress--;
        DLL_PUSH_TAIL(e_, &conn->server->connQ, conn);
        moloch_http_do_requests(conn->server);
    }

    return TRUE;
}
/******************************************************************************/
int moloch_http_connect(MolochConn_t *conn, char *name, int defaultport, int blocking)
{
    GError                   *error = 0;
    GSocketConnectable       *connectable;
    GSocketAddressEnumerator *enumerator;
    GSocketAddress           *sockaddr;
    struct timeval            startTime;
    struct timeval            stopTime;

    gettimeofday(&startTime, NULL);

    connectable = g_network_address_parse(name, defaultport, &error);

    if (error) {
        LOG("%p: Couldn't parse connect string of %s", (void*)conn, name);
        exit(0);
    }

    conn->name = name;

    enumerator = g_socket_connectable_enumerate (connectable);
    g_object_unref(connectable);

    while (!conn->conn && (sockaddr = g_socket_address_enumerator_next (enumerator, NULL, &error)))
    {
        conn->conn = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, &error);

        if (!error) {
            GValue value = G_VALUE_INIT;
            g_value_init (&value, G_TYPE_BOOLEAN);
            g_value_set_boolean (&value, blocking);
            g_object_set_property(G_OBJECT(conn->conn), "blocking", &value);
            g_socket_connect(conn->conn, sockaddr, NULL, &error);
        }

        if (error && error->code != G_IO_ERROR_PENDING) {
            g_object_unref (conn->conn);
            conn->conn = NULL;
        } else {
            struct sockaddr_in localAddress, remoteAddress;
            socklen_t addressLength = sizeof(localAddress);
            getsockname(g_socket_get_fd(conn->conn), (struct sockaddr*)&localAddress, &addressLength);
            g_socket_address_to_native(sockaddr, &remoteAddress, addressLength, NULL);

            
            char sessionId[MOLOCH_SESSIONID_LEN];
            moloch_session_id(sessionId, localAddress.sin_addr.s_addr, localAddress.sin_port,
                              remoteAddress.sin_addr.s_addr, remoteAddress.sin_port);

            DEBUGCONN("AAA connected %s %p %s", conn->server->names[0], conn, 
                      moloch_friendly_session_id(17, localAddress.sin_addr.s_addr, htons(localAddress.sin_port),
                                                 remoteAddress.sin_addr.s_addr, htons(remoteAddress.sin_port)));

            HASH_ADD(h_, connections, sessionId, conn);
            memcpy(&conn->sessionIda, sessionId, 8);
            memcpy(&conn->sessionIdb, sessionId+8, 4);
            conn->server->numConns++;
        }
        g_object_unref (sockaddr);
    }
    g_object_unref (enumerator);

    if (conn->conn) {
        if (error) {
            g_error_free(error);
            error = 0;
        }
    } else if (error) {
        LOG("%p: Error: %s", (void*)conn, error->message);
    }

    if (error || !conn->conn) {
        if (config.logESRequests)
            LOG("Connecting %p %s %d %d FAIL", (void*)conn, name, defaultport, blocking);
        conn->server->lastFailedConnect = time(0);
        return 1;
    }

    //g_object_ref (conn->conn);
    g_socket_set_keepalive(conn->conn, TRUE);
    int fd = g_socket_get_fd(conn->conn);

    conn->readWatch = moloch_watch_fd(fd, MOLOCH_GIO_READ_COND, moloch_http_read_cb, conn);
    if (!blocking) {
        conn->writeWatch = moloch_watch_fd(fd, G_IO_OUT, moloch_http_conn_cb, conn);
        DEBUGCONN("AAA connwritewatch %s %p fd:%d ww:%d", conn->server->names[0], conn, fd, conn->writeWatch);
    }

    int sendbuff = 0;
    socklen_t optlen = sizeof(sendbuff);

    int res = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendbuff, &optlen);
    if(res != -1 && sendbuff < 300000) {
        sendbuff = 300000;
        setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendbuff, sizeof(sendbuff));
    }

#ifdef TCP_KEEPIDLE
    res = getsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &sendbuff, &optlen);
    if(res != -1 && sendbuff > 60*8) {
        sendbuff = 60*8;
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &sendbuff, sizeof(sendbuff));
    }
#endif

    gettimeofday(&stopTime, NULL);
    if (config.logESRequests)
        LOG("Connecting %p %s %d %d %ldms", (void*)conn, name, defaultport, blocking, 
            (stopTime.tv_sec - startTime.tv_sec)*1000 + (stopTime.tv_usec - startTime.tv_usec)/1000);

    return 0;
}
/******************************************************************************/
gboolean moloch_http_finish( MolochConn_t *conn, gboolean sync)
{
    conn->hp_complete = 0;
    http_parser_init(&conn->parser, HTTP_RESPONSE);

    if (!sync && conn->request->data) {
        MOLOCH_SIZE_FREE(buffer, conn->request->data);
        conn->request->data = 0;
    }

    while (sync) {
        if (!moloch_http_read_cb(0, 0, conn)) {
            gettimeofday(&conn->sentTime, NULL);
            return FALSE;
        }

        if (conn->hp_complete)
            break;
    }

    gettimeofday(&conn->sentTime, NULL);
    return TRUE;
}
/******************************************************************************/
char *moloch_http_get_name(MolochHttp_t *server)
{
    char *name;
    if (server->names[server->namesPos]) {
        name = server->names[server->namesPos];
        server->namesPos++;
        return name;
    }
    server->namesPos = 0;
    return server->names[0];
}
/******************************************************************************/
gboolean moloch_http_process_send(MolochConn_t *conn, gboolean sync)
{
    char                 buffer[6000];
    uint32_t             len;
    GError              *gerror = 0;
    MolochRequest_t     *request = conn->request;

    len = snprintf(buffer, sizeof(buffer),
                          "%s %.*s HTTP/1.1\r\n"
                          "Host: %s\r\n"
                          "Content-Type: application/json\r\n"
                          "%s"
                          "Content-Length: %d\r\n"
                          "Connection: keep-alive\r\n"
                          "Keep-Alive: timeout=6000\r\n"
                          "%s"
                          "\r\n",
                          request->method,
                          request->key_len,
                          request->key,
                          conn->name,
                          request->compress?"Content-Encoding: deflate\r\n":"",
                          (int)request->data_len,
                          request->headers);

    gettimeofday(&conn->startTime, NULL);
    snprintf(conn->line, sizeof(conn->line), "%15.15s %d/%d %p %s %s %.*s %d",
           ctime(&conn->startTime.tv_sec)+4,
           conn->server->connQ.e_count,
           conn->server->requestQ.r_count,
           (void*)conn,
           request->method,
           sync?"SYNC":"ASYNC",
           request->key_len,
           request->key,
           request->data_len);

    uint32_t sent = 0;
    while (!gerror && sent < len) {
        sent += g_socket_send(conn->conn, buffer+sent, len-sent, NULL, &gerror);
    }


    /* If async and we have data to send, send it when writable */
    if (!sync && request->data_len) {
        conn->sent = 0;
        gettimeofday(&conn->sendTime, NULL);
        conn->writeWatch = moloch_watch_fd(g_socket_get_fd(conn->conn), G_IO_OUT, moloch_http_write_cb, conn);
        DEBUGCONN("AAA writewatch %s %p fd:%d ww:%d", conn->server->names[0], conn, g_socket_get_fd(conn->conn), conn->writeWatch);
        return TRUE;
    }

    sent = 0;
    while (!gerror && sent < request->data_len) {
        sent += g_socket_send(conn->conn, request->data+sent, request->data_len-sent, NULL, &gerror);
    }

    gettimeofday(&conn->sendTime, NULL);

    if (gerror) {
        LOG("%p: Send Error: %d %s", (void*)conn, sync, gerror->message);
        g_error_free(gerror);
        return FALSE;
    }

    return moloch_http_finish(conn, sync);
}

/******************************************************************************/
gboolean moloch_http_set(void *server, char *key, int key_len, char *data, uint32_t data_len, MolochResponse_cb func, gpointer uw)
{
    // If no func then this request is dropable
    return moloch_http_send(server, "POST", key, key_len, data, data_len, NULL, func == 0, func, uw);
}
/******************************************************************************/
MolochConn_t *moloch_http_create(MolochHttp_t *server, int blocking);
unsigned char *moloch_http_send_sync(void *serverV, char *method, char *key, uint32_t key_len, char *data, uint32_t data_len, char *headers, size_t *return_len)
{
    MolochRequest_t     *request;
    gboolean             sent = FALSE;
    MolochHttp_t        *server = serverV;

    request = MOLOCH_TYPE_ALLOC(MolochRequest_t);
    request->key_len  = MIN(key_len, sizeof(request->key)-1);
    memcpy(request->key, key, request->key_len);
    request->key[request->key_len] = 0;
    strncpy(request->method, method, sizeof(request->method));
    request->method[sizeof(request->method)-1] = 0;
    request->data_len = data_len;
    request->data     = data;
    request->func     = 0;
    request->uw       = 0;
    request->compress = 0;
    if (headers)
        strcpy(request->headers, headers);
    else
        request->headers[0] = 0;

    if (return_len)
        *return_len = 0;

    if (!server->syncConn)
        server->syncConn = moloch_http_create(server, TRUE);

    server->syncConn->request = request;
    sent = moloch_http_process_send(server->syncConn, TRUE);

    if (sent)  {
        if (return_len)
            *return_len = server->syncConn->hp_len;
        return (unsigned char*)server->syncConn->hp_data;
    }

    return 0;
}
/******************************************************************************/
void moloch_http_do_requests(MolochHttp_t *server)
{
    // Something waiting
    while (DLL_COUNT(r_, &server->requestQ)) {

        // No free connections
        if (DLL_COUNT(e_, &server->connQ) == 0) {

            // Not at our max, create one a second
            if (server->numConns < server->maxConns && time(0) - server->lastFailedConnect > 0) {
                moloch_http_create(server, FALSE);
            }
            return;
        }

        // Have a free conn and a request
        MolochConn_t *conn;
        DLL_POP_HEAD(e_, &server->connQ, conn);
        DLL_POP_HEAD(r_, &server->requestQ, conn->request);
        server->inProgress++;
        if (!moloch_http_process_send(conn, FALSE)) {
            server->inProgress--;
            LOG("ERROR - %p: Couldn't send %.*s", (void*)conn, conn->request->key_len, conn->request->key);
            DLL_PUSH_HEAD(r_, &server->requestQ, conn->request);

            moloch_http_free_conn(conn, FALSE);
        }
    }
}

/******************************************************************************/
static z_stream z_strm;
gboolean moloch_http_send(void *serverV, char *method, char *key, uint32_t key_len, char *data, uint32_t data_len, char *headers, gboolean dropable, MolochResponse_cb func, gpointer uw)
{
    MolochRequest_t     *request;
    MolochHttp_t        *server = serverV;

    if (!config.exiting && dropable && server->requestQ.r_count > server->maxOutstandingRequests) {
        LOG("ERROR - Dropping request %.*s of size %d queue %d is too big", key_len, key, data_len, server->requestQ.r_count);

        if (data) {
            MOLOCH_SIZE_FREE(buffer, data);
        }
        return 1;
    }

    request = MOLOCH_TYPE_ALLOC(MolochRequest_t);
    request->key_len  = MIN(key_len, sizeof(request->key)-1);
    memcpy(request->key, key, request->key_len);
    request->key[request->key_len] = 0;
    strncpy(request->method, method, sizeof(request->method));
    request->compress = 0;

    if (server->compress && data && data_len > 1000) {
        char            *buf = moloch_http_get_buffer(data_len);
        int              ret;

        z_strm.avail_in   = data_len;
        z_strm.next_in    = (unsigned char *)data;
        z_strm.avail_out  = data_len;
        z_strm.next_out   = (unsigned char *)buf;
        ret = deflate(&z_strm, Z_FINISH);
        if (ret == Z_STREAM_END) {
            request->compress = 1;
            MOLOCH_SIZE_FREE(buffer, data);
            data_len = data_len - z_strm.avail_out;
            data     = buf;
        } else {
            MOLOCH_SIZE_FREE(buffer, buf);
        }

        deflateReset(&z_strm);
    }

    request->data_len = data_len;
    request->data     = data;
    request->func     = func;
    request->uw       = uw;
    if (headers)
        strcpy(request->headers, headers);
    else
        request->headers[0] = 0;

    DLL_PUSH_TAIL(r_, &server->requestQ, request);
    moloch_http_do_requests(server);
    return 0;
}

/******************************************************************************/
unsigned char *moloch_http_get(void *server, char *key, int key_len, size_t *mlen)
{
    return moloch_http_send_sync(server, "GET", key, key_len, NULL, 0, NULL, mlen);
}
/******************************************************************************/
MolochConn_t *
moloch_http_create(MolochHttp_t *server, int blocking) {
    MolochConn_t *conn;
    int           tries = server->namesCnt;

    conn = MOLOCH_TYPE_ALLOC0(MolochConn_t);
    conn->parser.data = conn;
    conn->server = server;

    DEBUGCONN("AAA alloc %s %p blocking:%d", conn->server->names[0], conn, blocking);

    while (tries > 0) {
        if (!moloch_http_connect(conn, moloch_http_get_name(server), server->port, blocking)) {
            return conn;
        }
        tries--;
        LOG("Couldn't connect to '%s'", conn->name);
    }
    exit (1);
}
/******************************************************************************/
void *moloch_http_create_server(char *hostnames, int defaultPort, int maxConns, int maxOutstandingRequests, int compress)
{
    MolochHttp_t *server = MOLOCH_TYPE_ALLOC0(MolochHttp_t);

    DLL_INIT(r_, &server->requestQ);
    DLL_INIT(e_, &server->connQ);

    server->parserSettings.on_message_begin    = moloch_http_hp_cb_on_message_begin;
    server->parserSettings.on_body             = moloch_http_hp_cb_on_body;
    server->parserSettings.on_message_complete = moloch_http_hp_cb_on_message_complete;
    server->parserSettings.on_header_field     = moloch_http_hp_cb_on_header_field;
    server->parserSettings.on_header_value     = moloch_http_hp_cb_on_header_value;

    server->names = g_strsplit(hostnames, ",", 0);
    uint32_t i;
    for (i = 0; server->names[i]; i++) {
        if (strncmp(server->names[i], "http://", 7) == 0) {
            char *tmp = g_strdup(server->names[i] + 7);
            g_free(server->names[i]);
            server->names[i] = tmp;
        } else if (strncmp(server->names[i], "https://", 8) == 0) {
            LOG("https not supported yet %s", server->names[i]);
            exit(0);
        }
    }
    server->namesCnt = i;
    server->port     = defaultPort;
    server->maxConns = maxConns;
    server->maxOutstandingRequests = maxOutstandingRequests;
    server->compress = compress;

    return server;
}
/******************************************************************************/
void moloch_http_set_header_cb(void *serverV, MolochHttpHeader_cb cb)
{
    MolochHttp_t *server = serverV;

    server->headerCb                           = cb;
}
/******************************************************************************/
/* Must match moloch_session_cmp and moloch_session_id
 * a1 0-3
 * p1 4-5
 * a2 6-9
 * p2 10-11
 */
uint32_t moloch_http_conn_hash(const void *key)
{
    unsigned char *p = (unsigned char *)key;
    //return ((p[2] << 16 | p[3] << 8 | p[4]) * 59) ^ (p[8] << 16 | p[9] << 8 |  p[10]);
    return (((p[1]<<24) ^ (p[2]<<18) ^ (p[3]<<12) ^ (p[4]<<6) ^ p[5]) * 13) ^ (p[8]<<24|p[9]<<16 | p[10]<<8 | p[11]);
}

/******************************************************************************/
int moloch_http_conn_cmp(const void *keyv, const void *elementv)
{
    MolochConn_t *conn = (MolochConn_t *)elementv;

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

    HASH_INIT(h_, connections, moloch_http_conn_hash, moloch_http_conn_cmp);
}
/******************************************************************************/
void moloch_http_free_server(void *serverV)
{
    MolochHttp_t *server = serverV;
    int n=0;

    while (server->requestQ.r_count > 0 || server->inProgress > 0) {
        n++;
        if (n%10000 == 0)
            LOG("%s %d %d", server->names[0], server->requestQ.r_count, server->inProgress);
        g_main_context_iteration (g_main_context_default(), FALSE);
    }

    MolochConn_t *conn = 0;
    while (DLL_POP_HEAD(e_, &server->connQ, conn)) {
        moloch_http_free_conn(conn, FALSE);
    }

    moloch_http_free_conn(server->syncConn, FALSE);
    server->syncConn = 0;
    g_strfreev(server->names);

    MOLOCH_TYPE_FREE(MolochHttp_t, server);
}
/******************************************************************************/
void moloch_http_exit()
{
    deflateEnd(&z_strm);
}
/******************************************************************************/
int moloch_http_queue_length(void *serverV)
{
    MolochHttp_t *server = serverV;
    if (!server) return 0;

    return server->requestQ.r_count;
}
/******************************************************************************/
gboolean moloch_http_is_moloch(uint32_t hash, char *key)
{
    MolochConn_t *conn;

    HASH_FIND_HASH(h_, connections, hash, key, conn);
    return (conn?1:0);
}
