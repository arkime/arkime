/* http.c  -- Functions dealing with http connections.
 *
 * Copyright 2012-2014 AOL Inc. All rights reserved.
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
#include "glib.h"
#include "zlib.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "moloch.h"

/******************************************************************************/
extern MolochConfig_t        config;

/******************************************************************************/
static http_parser_settings  parserSettings;

typedef struct molochrequest_t {
    struct molochrequest_t *r_next, *r_prev;

    MolochResponse_cb       func;
    gpointer                uw;
    char                    key[200];
    char                    method[16];
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
} MolochConn_t;

typedef struct {
    MolochConn_t        *e_next, *e_prev;
    uint32_t           e_count;
} MolochConnHead_t;

typedef struct molochhttp_t {
    MolochConn_t         *syncConn;
    char                **names;
    int                   namesCnt;
    int                   namesPos;
    char                  compress;
    int                   port;
    uint16_t              maxConns;
    uint16_t              maxOutstandingRequests;
    MolochConnHead_t      connQ;
    time_t                lastFailedConnect;
    MolochRequestHead_t   requestQ[2];
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
void moloch_http_finish(MolochConn_t *conn, gboolean sync);

gboolean moloch_http_write_cb(gint UNUSED(fd), GIOCondition UNUSED(cond), gpointer data) {
    MolochConn_t        *conn = data;
    GError              *gerror = 0;

    if (!conn->request)
        return FALSE;

    int sent = g_socket_send(conn->conn, conn->request->data+conn->sent, conn->request->data_len-conn->sent, NULL, &gerror);
    conn->sent += sent;

    if (gerror) {
        /* Should free stuff here */
        LOG("ERROR: %p: Receive Error: %s", (void*)conn, gerror->message);
        g_error_free(gerror);
        return FALSE;
    }

    gboolean finished = conn->sent == conn->request->data_len;
    if (finished)
        moloch_http_finish(conn, FALSE);


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
            LOG("ERROR: %p:%p Receive Error: %s", (void*)conn, conn->request, gerror->message);
            g_error_free(gerror);
        } else if (cond & (G_IO_HUP | G_IO_ERR))
            LOG("ERROR: %p:%p Lost connection to %s", (void*)conn, conn->request, conn->name);
        else if (len <= 0)
            LOG("ERROR: %p:%p len: %d cond: %x", (void*)conn, conn->request, len, cond);
        else
            LOG("ERROR HMM: %p:%p len: %d cond: %x", (void*)conn, conn->request, len, cond);

        if (conn->request) {
            // Must save, free, then call function because of recursive sync GETs
            MolochResponse_cb  func = conn->request->func;
            gpointer           uw = conn->request->uw;

            MOLOCH_TYPE_FREE(MolochRequest_t, conn->request);

            if (func) {
                func(0, 0, uw);
            }
        }

        g_object_unref (conn->conn);
        conn->conn = 0;
        if (conn != conn->server->syncConn && conn->request) {
            DLL_PUSH_TAIL(e_, &conn->server->connQ, conn);
        }
        conn->request = 0;
        return FALSE;
    }

    http_parser_execute(&conn->parser, &parserSettings, buffer, len);

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


        if (conn == conn->server->syncConn)
            return TRUE;


        int q;
        for (q = 0; q < 2; q++) {
            DLL_POP_HEAD(r_, &conn->server->requestQ[q], conn->request);
            if (conn->request) {
                if (!moloch_http_process_send(conn, 0)) {
                    DLL_PUSH_HEAD(r_, &conn->server->requestQ[q], conn->request);
                    conn->request = 0;
                    DLL_PUSH_TAIL(e_, &conn->server->connQ, conn);
                }
                return TRUE;
            }
        }
        DLL_PUSH_TAIL(e_, &conn->server->connQ, conn);
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

    if (config.logESRequests)
        LOG("Connecting %p %s", (void*)conn, name);


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
        conn->server->lastFailedConnect = time(0);
        return 1;
    }

    //g_object_ref (conn->conn);
    g_socket_set_keepalive(conn->conn, TRUE);
    int fd = g_socket_get_fd(conn->conn);
    moloch_watch_fd(fd, MOLOCH_GIO_READ_COND, moloch_http_read_cb, conn);

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

    return 0;
}
/******************************************************************************/
void moloch_http_finish( MolochConn_t *conn, gboolean sync)
{
    char line[1000];
    strcpy(line, conn->line);

    conn->hp_complete = 0;
    http_parser_init(&conn->parser, HTTP_RESPONSE);

    if (!sync && conn->request->data) {
        MOLOCH_SIZE_FREE(buffer, conn->request->data);
        conn->request->data = 0;
    }

    while (sync) {
        moloch_http_read_cb(0, 0, conn);
        if (conn->hp_complete)
            break;
    }

    gettimeofday(&conn->sentTime, NULL);
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
    char                 buffer[3000];
    uint32_t             len;
    GError              *gerror = 0;
    MolochRequest_t     *request = conn->request;

    if (conn->conn == 0) {
        if (moloch_http_connect(conn, moloch_http_get_name(conn->server), conn->server->port, TRUE)) {
            LOG("%p: Couldn't connect %s", (void*)conn, conn->name);
            return FALSE;
        }
    }

    len = snprintf(buffer, sizeof(buffer),
                          "%s %.*s HTTP/1.1\r\n"
                          "Host: this.host\r\n"
                          "Content-Type: application/json\r\n"
                          "%s"
                          "Content-Length: %d\r\n"
                          "Connection: keep-alive\r\n"
                          "\r\n",
                          request->method,
                          request->key_len,
                          request->key,
                          request->compress?"Content-Encoding: deflate\r\n":"",
                          (int)request->data_len);

    gettimeofday(&conn->startTime, NULL);
    snprintf(conn->line, sizeof(conn->line), "%15.15s %d/%d/%d %p %s %s %.*s %d",
           ctime(&conn->startTime.tv_sec)+4,
           conn->server->connQ.e_count,
           conn->server->requestQ[0].r_count,
           conn->server->requestQ[1].r_count,
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
        moloch_watch_fd(g_socket_get_fd(conn->conn), G_IO_OUT, moloch_http_write_cb, conn);
        return TRUE;
    }

    sent = 0;
    while (!gerror && sent < request->data_len) {
        sent += g_socket_send(conn->conn, request->data+sent, request->data_len-sent, NULL, &gerror);
    }

    gettimeofday(&conn->sendTime, NULL);

    if (gerror) {
        LOG("%p: Send Error: %d %s", (void*)conn, sync, gerror->message);
        conn->conn = 0;
        g_error_free(gerror);
        return FALSE;
    }

    moloch_http_finish(conn, sync);

    return TRUE;
}
/******************************************************************************/
gboolean moloch_http_set(void *server, char *key, int key_len, char *data, uint32_t data_len, MolochResponse_cb func, gpointer uw)
{
    // If no func then this request is dropable
    return moloch_http_send(server, "POST", key, key_len, data, data_len, func == 0, func, uw);
}
/******************************************************************************/
unsigned char *moloch_http_send_sync(void *serverV, char *method, char *key, uint32_t key_len, char *data, uint32_t data_len, size_t *return_len)
{
    MolochRequest_t     *request;
    gboolean             sent = FALSE;
    MolochHttp_t        *server = serverV;

    request = MOLOCH_TYPE_ALLOC(MolochRequest_t);
    memcpy(request->key, key, MIN(key_len, sizeof(request->key)));
    strncpy(request->method, method, sizeof(request->method));
    request->method[sizeof(request->method)-1] = 0;
    request->key_len  = key_len;
    request->data_len = data_len;
    request->data     = data;
    request->func     = 0;
    request->uw       = 0;
    request->compress = 0;

    if (return_len)
        *return_len = 0;

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
static z_stream z_strm;
gboolean moloch_http_send(void *serverV, char *method, char *key, uint32_t key_len, char *data, uint32_t data_len, gboolean dropable, MolochResponse_cb func, gpointer uw)
{
    MolochRequest_t     *request;
    MolochConn_t        *conn;
    MolochHttp_t        *server = serverV;

    request = MOLOCH_TYPE_ALLOC(MolochRequest_t);
    memcpy(request->key, key, MIN(key_len, sizeof(request->key)));
    strncpy(request->method, method, sizeof(request->method));
    request->key_len  = key_len;
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

    int q = data_len > MOLOCH_HTTP_BUFFER_SIZE?1:0;

    // Already have outstanding requests, see if we can process them
    if (server->requestQ[q].r_count && server->connQ.e_count && time(0) - server->lastFailedConnect > 0 ) {
        while (DLL_POP_HEAD(e_, &server->connQ, conn)) {
            DLL_POP_HEAD(r_, &server->requestQ[q], conn->request);
            if (conn->request) {
                if (!moloch_http_process_send(conn, 0)) {
                    LOG("ERROR - %p: Couldn't send %.*s", (void*)conn, conn->request->key_len, conn->request->key);
                    DLL_PUSH_HEAD(r_, &server->requestQ[q], conn->request);
                    conn->request = 0;
                    DLL_PUSH_TAIL(e_, &server->connQ, conn);
                    break;
                }
            }
            else {
                DLL_PUSH_TAIL(e_, &server->connQ, conn);
                break;
            }
        }
    }

    // Now see if we can send something new
    if (DLL_POP_HEAD(e_, &server->connQ, conn)) {
        conn->request = request;
        if (!moloch_http_process_send(conn, FALSE)) {
            conn->request = 0;
            DLL_PUSH_TAIL(r_, &server->requestQ[q], request);
            DLL_PUSH_TAIL(e_, &server->connQ, conn);
        }
    } else {
        request->data = data;
        if (!config.exiting && dropable && server->requestQ[q].r_count > server->maxOutstandingRequests) {
            LOG("ERROR - Dropping request %.*s of size %d queue[%d] %d is too big", key_len, key, data_len, q, server->requestQ[q].r_count);

            if (data) {
                MOLOCH_SIZE_FREE(buffer, data);
            }
            MOLOCH_TYPE_FREE(MolochRequest_t, request);
            return 1;
        } else {
            DLL_PUSH_TAIL(r_, &server->requestQ[q], request);
        }
    }

    return 0;
}
/******************************************************************************/
unsigned char *moloch_http_get(void *server, char *key, int key_len, size_t *mlen)
{
    return moloch_http_send_sync(server, "GET", key, key_len, NULL, 0, mlen);
}
/******************************************************************************/
MolochConn_t *
moloch_http_create(MolochHttp_t *server, int blocking) {
    MolochConn_t *conn;
    int           tries = server->namesCnt;

    conn = MOLOCH_TYPE_ALLOC0(MolochConn_t);
    conn->parser.data = conn;
    conn->server = server;

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

    DLL_INIT(r_, &server->requestQ[0]);
    DLL_INIT(r_, &server->requestQ[1]);
    DLL_INIT(e_, &server->connQ);

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
    server->port = defaultPort;
    server->maxConns = maxConns;
    server->maxOutstandingRequests = maxOutstandingRequests;
    server->compress = compress;

    server->syncConn = moloch_http_create(server, TRUE);
    for (i = 0; i < server->maxConns; i++) {
        MolochConn_t *conn = moloch_http_create(server, FALSE);
        DLL_PUSH_TAIL(e_, &server->connQ, conn);
    }

    return server;
}
/******************************************************************************/
void moloch_http_init()
{
    g_type_init();

    z_strm.zalloc = Z_NULL;
    z_strm.zfree  = Z_NULL;
    z_strm.opaque = Z_NULL;
    deflateInit(&z_strm, Z_DEFAULT_COMPRESSION);

    memset(&parserSettings, 0, sizeof(parserSettings));

    parserSettings.on_message_begin    = moloch_http_hp_cb_on_message_begin;
    parserSettings.on_body             = moloch_http_hp_cb_on_body;
    parserSettings.on_message_complete = moloch_http_hp_cb_on_message_complete;
}
/******************************************************************************/
void moloch_http_free_server(void *serverV)
{
    MolochHttp_t *server = serverV;

    int q;
    for (q = 0; q < 2; q++) {
        while (server->requestQ[q].r_count > 0 || server->connQ.e_count != server->maxConns) {
            g_main_context_iteration (g_main_context_default(), FALSE);
        }
    }

    MolochConn_t *es = 0;
    while (DLL_POP_HEAD(e_, &server->connQ, es)) {
        MOLOCH_TYPE_FREE(MolochConn_t, es);
    }

    MOLOCH_TYPE_FREE(MolochConn_t, server->syncConn);
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

    return server->requestQ[0].r_count + server->requestQ[1].r_count;
}
