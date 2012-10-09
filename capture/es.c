/* es.c  -- Functions dealing with elasticsearch.
 * 
 * Copyright 2012 AOL Inc. All rights reserved.
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
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <string.h>
#include <string.h>
#include <sys/time.h>
#include <netdb.h>
#include <netdb.h>
#include "glib.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "moloch.h"

/******************************************************************************/
MolochConfig_t         config;

/******************************************************************************/
static http_parser_settings  parserSettings;

typedef struct molochrequest_t {
    struct molochrequest_t *r_next, *r_prev;

    char                    key[200];
    char                    method[20];
    int                     key_len;
    char                   *data;
    uint32_t                data_len;
    MolochResponse_cb       func;
    gpointer                uw;
} MolochRequest_t;

typedef struct {
    struct molochrequest_t *r_next, *r_prev;
    uint32_t r_count;
} MolochRequestHead_t;

typedef struct moloches_t {
    struct moloches_t *e_next, *e_prev;

    char               line[1000];
    struct timeval     startTime;
    struct timeval     sendTime;
    struct timeval     endTime;
    char               hp_data[400000];
    uint32_t           sent;
    uint32_t           hp_len;
    uint16_t           hp_complete;
    GSocket           *elastic;
    http_parser        parser;
    MolochRequest_t   *request;
} MolochES_t;

typedef struct {
    MolochES_t        *e_next, *e_prev;
    uint32_t           e_count;
} MolochESHead_t;

typedef struct molochmem_t {
    struct molochmem_t *m_next, *m_prev;
    int                 m_count;
} MolochMem_t;




MolochES_t                  *syncES;
static MolochESHead_t        esQ;
time_t                       lastFailedConnect = 0;
static MolochRequestHead_t   requestQ[2];
static MolochMem_t           bufferQ[3];

gboolean moloch_es_process_send(MolochES_t *es, gboolean sync);
/******************************************************************************/
int
moloch_es_hp_cb_on_message_begin (http_parser *parser)
{
    MolochES_t *info = parser->data;

    info->hp_len      = 0;
    info->hp_complete = 0;
    return 0;
}
/******************************************************************************/
int
moloch_es_hp_cb_on_body (http_parser *parser, const char *at, size_t length)
{
    MolochES_t *info = parser->data;

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
moloch_es_hp_cb_on_message_complete (http_parser *parser)
{
    MolochES_t *info = parser->data;

    info->hp_complete = 1;
    return 0;
}
/******************************************************************************/
void moloch_es_finish(MolochES_t *es, gboolean sync);

gboolean moloch_es_write_cb(gint UNUSED(fd), GIOCondition UNUSED(cond), gpointer data) {
    MolochES_t          *es = data;
    GError              *gerror = 0;

    /*struct timeval startTime;
    struct timeval endTime;
    gettimeofday(&startTime, 0); */
    if (!es->request)
        return FALSE;

    int sent = g_socket_send(es->elastic, es->request->data+es->sent, es->request->data_len-es->sent, NULL, &gerror);
    es->sent += sent;

    /*gettimeofday(&endTime, 0);
    LOG("%s WRITE %d %d %ldms", es->line, sent, es->sent,
       (endTime.tv_sec - startTime.tv_sec)*1000 + (endTime.tv_usec/1000 - startTime.tv_usec/1000));*/


    if (gerror) {
        /* Should free stuff here */
        LOG("ERROR: %p: Receive Error: %s", es, gerror->message);
        return FALSE;
    }

    gboolean finished = es->sent == es->request->data_len;
    if (finished)
        moloch_es_finish(es, FALSE);


    return !finished;
}
/******************************************************************************/
gboolean moloch_es_read_cb(gint UNUSED(fd), GIOCondition cond, gpointer data) {
    MolochES_t          *es = data;
    char                 buffer[0xffff];
    int                  len;
    GError              *gerror = 0;

    len = g_socket_receive(es->elastic, buffer, sizeof(buffer)-1, NULL, &gerror);

    if (gerror || cond & (G_IO_HUP | G_IO_ERR) || len <= 0) {
        if (gerror)
            LOG("ERROR: %p: Receive Error: %s", es, gerror->message);
        else if (cond & (G_IO_HUP | G_IO_ERR))
            LOG("ERROR: %p: Lost connection to es", es);
        else if (len <= 0)
            LOG("ERROR: %p: len: %d cond: %x", es, len, cond);

        g_object_unref (es->elastic);
        es->elastic = 0;
        if (es != syncES && es->request) {
            DLL_PUSH_TAIL(e_, &esQ, es);
        }
        es->request = 0;
        return FALSE;
    }

    http_parser_execute(&es->parser, &parserSettings, buffer, len);

    if (es->hp_complete) {
        es->hp_data[es->hp_len] = 0;

        /* Must save, free, then call function because of recursive sync GETs */
        MolochResponse_cb  func = es->request->func;
        gpointer           uw = es->request->uw;

        free(es->request);

        if (func) {
            func((unsigned char*)es->hp_data, es->hp_len, uw);
        }


        if (es == syncES)
            return TRUE;


        int q;
        for (q = 0; q < 2; q++) {
            DLL_POP_HEAD(r_, &requestQ[q], es->request);
            if (es->request) {
                if (!moloch_es_process_send(es, 0)) {
                    DLL_PUSH_HEAD(r_, &requestQ[q], es->request);
                    es->request = 0;
                    DLL_PUSH_TAIL(e_, &esQ, es);
                }
                return TRUE;
            }
        }
        DLL_PUSH_TAIL(e_, &esQ, es);
    }

    return TRUE;
}
/******************************************************************************/
int moloch_es_connect(MolochES_t *es)
{
    GError                   *error = 0;
    GSocketConnectable       *connectable;
    GSocketAddressEnumerator *enumerator;
    GSocketAddress           *sockaddr;

    if (config.logESRequests)
        LOG("Connecting %p", es);


    connectable = g_network_address_parse(config.elasticsearch, 9200, &error);

    if (error) {
        LOG("%p: Couldn't parse connect string of %s", es, config.elasticsearch);
        exit(0);
    }

    enumerator = g_socket_connectable_enumerate (connectable);
    g_object_unref(connectable);

    while (!es->elastic && (sockaddr = g_socket_address_enumerator_next (enumerator, NULL, &error)))
    {
        es->elastic = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, &error);
        if (!error) {
            g_socket_connect(es->elastic, sockaddr, NULL, &error);
        }
        if (error) {
            g_object_unref (es->elastic);
            es->elastic = NULL;
        }
        g_object_unref (sockaddr);
    }
    g_object_unref (enumerator);

    if (es->elastic) {
        if (error)
            g_error_free(error);
    } else if (error) {
        LOG("%p: Error: %s", es, error->message);
    }

    if (error || !es->elastic) {
        lastFailedConnect = time(0);
        return 1;
    }


    //g_object_ref (es->elastic);
    g_socket_set_keepalive(es->elastic, TRUE);
    int fd = g_socket_get_fd(es->elastic);
    moloch_watch_fd(fd, MOLOCH_GIO_READ_COND, moloch_es_read_cb, es);

    int sendbuff = 0;
    socklen_t optlen = sizeof(sendbuff);

    int res = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendbuff, &optlen);
    if(res != -1 && sendbuff < 300000) {
        sendbuff = 300000;
        setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendbuff, sizeof(sendbuff));
    }

    res = getsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &sendbuff, &optlen);
    if(res != -1 && sendbuff > 60*8) {
        sendbuff = 60*8;
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &sendbuff, sizeof(sendbuff));
    }

    return 0;
}
/******************************************************************************/
void moloch_es_finish( MolochES_t *es, gboolean sync)
{
    char line[1000];
    strcpy(line, es->line);

    es->hp_complete = 0;
    http_parser_init(&es->parser, HTTP_RESPONSE);

    if (!sync && es->request->data) {
        MolochMem_t *mem = (MolochMem_t *)(es->request->data-8);
        int          b   = es->request->data[-8];

        DLL_PUSH_TAIL(m_, &bufferQ[b], mem);
        es->request->data = 0;
    }

    while (sync) {
        moloch_es_read_cb(0, 0, es);
        if (es->hp_complete)
            break;
    }

    gettimeofday(&es->endTime, NULL);
    if (config.logESRequests)
        LOG("%s %ldms %ldms", 
           line,
           (es->sendTime.tv_sec - es->startTime.tv_sec)*1000 + (es->sendTime.tv_usec/1000 - es->startTime.tv_usec/1000),
           (es->endTime.tv_sec - es->startTime.tv_sec)*1000 + (es->endTime.tv_usec/1000 - es->startTime.tv_usec/1000)
           );

}
/******************************************************************************/
gboolean moloch_es_process_send(MolochES_t *es, gboolean sync)
{
    char                 buffer[3000];
    uint32_t             len;
    GError              *gerror = 0;
    MolochRequest_t     *request = es->request;

    if (es->elastic == 0) {
        if (moloch_es_connect(es)) {
            LOG("%p: Couldn't connect from process", es);
            return FALSE;
        }
    }

    len = snprintf(buffer, sizeof(buffer),
                          "%s %.*s HTTP/1.1\r\n"
                          "Host: this.host\r\n"
                          "Content-Type: application/json\r\n"
                          "Content-Length: %d\r\n"
                          "Connection: keep-alive\r\n"
                          "\r\n",
                          request->method,
                          request->key_len,
                          request->key,
                          (int)request->data_len);

    gettimeofday(&es->startTime, NULL);
    snprintf(es->line, sizeof(es->line), "%15.15s %d/%d/%d %p %s %s %.*s %d", 
           ctime(&es->startTime.tv_sec)+4,
           esQ.e_count,
           requestQ[0].r_count,
           requestQ[1].r_count,
           es,
           request->method,
           sync?"SYNC":"ASYNC",
           request->key_len,
           request->key,
           request->data_len);

    uint32_t sent = 0;
    while (!gerror && sent < len) {
        sent += g_socket_send(es->elastic, buffer+sent, len-sent, NULL, &gerror);
    }


    /* If async and we have data to send, send it when writable */
    if (!sync && request->data_len) {
        es->sent = 0;
        gettimeofday(&es->sendTime, NULL);
        moloch_watch_fd(g_socket_get_fd(es->elastic), G_IO_OUT, moloch_es_write_cb, es);
        return TRUE;
    }

    sent = 0;
    while (!gerror && sent < request->data_len) {
        sent += g_socket_send(es->elastic, request->data+sent, request->data_len-sent, NULL, &gerror);
    }
    
    gettimeofday(&es->sendTime, NULL);

    if (gerror) {
        LOG("%p: Send Error: %d %s", es, sync, gerror->message);
        es->elastic = 0;
        return FALSE;
    }

    moloch_es_finish(es, sync);

    return TRUE;
}
/******************************************************************************/
void moloch_es_set(char *key, int key_len, char *data, size_t data_len, MolochResponse_cb func, gpointer uw)
{
    moloch_es_send("POST", key, key_len, data, data_len, 0, FALSE, func, uw);
}
/******************************************************************************/
unsigned char *moloch_es_send(char *method, char *key, uint32_t key_len, char *data, size_t data_len, size_t *return_len, gboolean sync, MolochResponse_cb func, gpointer uw)
{
    MolochRequest_t     *request;
    MolochES_t          *es;
    gboolean             sent = FALSE;

    request = malloc(sizeof(MolochRequest_t));
    memcpy(request->key, key, MIN(key_len, sizeof(request->key)));
    strncpy(request->method, method, sizeof(request->method));
    request->key_len  = key_len;
    request->data_len = data_len;
    request->data     = data;
    request->func     = func;
    request->uw       = uw;

    int q = data_len > MOLOCH_ES_BUFFER_SIZE_S?1:0;

    if (return_len)
        *return_len = 0;


    if (sync) {
        syncES->request = request;
        sent = moloch_es_process_send(syncES, TRUE);

        if (sent)  {
            if (return_len)
                *return_len = syncES->hp_len;
            return (unsigned char*)syncES->hp_data;
        }

        // Couldn't send sync, so add to head of async
        request->data = moloch_es_get_buffer(data_len);
        memcpy(request->data, data, data_len);
        DLL_PUSH_HEAD(r_, &requestQ[q], request);
        return 0;
    }

    // Already have outstanding requests, see if we can process them
    if (requestQ[q].r_count && esQ.e_count && time(0) - lastFailedConnect > 0 ) {
        while (DLL_POP_HEAD(e_, &esQ, es)) {
            DLL_POP_HEAD(r_, &requestQ[q], es->request);
            if (es->request) {
                if (!moloch_es_process_send(es, 0)) {
                    LOG("ERROR - %p: Couldn't send %.*s", es, es->request->key_len, es->request->key);
                    DLL_PUSH_HEAD(r_, &requestQ[q], es->request);
                    es->request = 0;
                    DLL_PUSH_TAIL(e_, &esQ, es);
                    break;
                }
            }
            else {
                DLL_PUSH_TAIL(e_, &esQ, es);
                break;
            }
        }
    }

    // Now see if we can send something new
    if (DLL_POP_HEAD(e_, &esQ, es)) {
        es->request = request;
        if (!moloch_es_process_send(es, FALSE)) {
            es->request = 0;
            DLL_PUSH_TAIL(r_, &requestQ[q], request);
            DLL_PUSH_TAIL(e_, &esQ, es);
        }
    } else {
        request->data = data;
        if (!func && requestQ[q].r_count > config.maxESRequests) {
            LOG("ERROR - Dropping request %.*s of size %ld queue[%d] %d is too big", key_len, key, data_len, q, requestQ[q].r_count);

            MolochMem_t *mem = (MolochMem_t *)(request->data-8);
            int          b   = request->data[-8];
            DLL_PUSH_TAIL(m_, &bufferQ[b], mem);
            free(request);
        } else {
            DLL_PUSH_TAIL(r_, &requestQ[q], request);
        }
    }

    return 0;
}
/******************************************************************************/
unsigned char *moloch_es_get(char *key, int key_len, size_t *mlen)
{
    return moloch_es_send("GET", key, key_len, NULL, 0, mlen, TRUE, 0, 0);
}
/******************************************************************************/
MolochES_t *
moloch_es_create() {
    MolochES_t *es;

    es = malloc(sizeof(MolochES_t));
    memset(es, 0, sizeof(MolochES_t));
    es->parser.data = es;

    if (moloch_es_connect(es)) {
        printf("Couldn't connect to elastic search at '%s'", config.elasticsearch);
        exit (1);
    }
    return es;
}
/******************************************************************************/
void moloch_es_init()
{
    uint32_t i;

    g_type_init();

    memset(&parserSettings, 0, sizeof(parserSettings));

    parserSettings.on_message_begin    = moloch_es_hp_cb_on_message_begin;
    parserSettings.on_body             = moloch_es_hp_cb_on_body;
    parserSettings.on_message_complete = moloch_es_hp_cb_on_message_complete;

    DLL_INIT(m_,&bufferQ[0]);
    DLL_INIT(m_,&bufferQ[1]);
    DLL_INIT(m_,&bufferQ[2]);
    DLL_INIT(r_, &requestQ[0]);
    DLL_INIT(r_, &requestQ[1]);
    DLL_INIT(e_, &esQ);

    syncES = moloch_es_create();
    for (i = 0; i < config.maxESConns; i++) {
        MolochES_t *es =moloch_es_create();
        DLL_PUSH_TAIL(e_, &esQ, es);
    }
}
/******************************************************************************/
void moloch_es_exit()
{
    int num = requestQ[1].r_count;

    int q;

    for (q = 0; q < 2; q++) {
        while (requestQ[q].r_count > 0 || esQ.e_count != config.maxESConns) {
            if (num >= 0 && (int)requestQ[q].r_count < num) {
                num -= 100;
            }
            g_main_context_iteration (g_main_context_default(), FALSE);
        }
    }

    int i;
    for (i = 0; i < 3; i++) {
        MolochMem_t *mem = 0;

        while (DLL_POP_HEAD(m_, &bufferQ[i], mem))
            free(mem);
    }


    MolochES_t *es = 0;
    while (DLL_POP_HEAD(e_, &esQ, es)) {
        free(es);
    }

    free(syncES);
    syncES = 0;
}
/******************************************************************************/
int moloch_es_queue_length() 
{
    return requestQ[0].r_count + requestQ[1].r_count;
}
/******************************************************************************/

char *moloch_es_get_buffer(int size) 
{
    MolochMem_t *mem;
    char        *buf = 0;

    int   i;
    if (size <= MOLOCH_ES_BUFFER_SIZE_S)
        i = 0;
    else if (size <= MOLOCH_ES_BUFFER_SIZE_M)
        i = 1;
    else
        i = 2;

    if (DLL_POP_HEAD(m_, &bufferQ[i], mem)) {
        buf = (char *)mem;
        buf[0] = i;
        return buf + 8;
    }

    if (i == 0)
        buf = malloc(MOLOCH_ES_BUFFER_SIZE_S + 9);
    else if (i == 1)
        buf = malloc(MOLOCH_ES_BUFFER_SIZE_M + 9);
    else if (i == 2)
        buf = malloc(MOLOCH_ES_BUFFER_SIZE_L + 9);
    else {
        LOG("ERROR - bad i %d\n", i);
    }


    buf[0] = i;

    return buf + 8;
}
