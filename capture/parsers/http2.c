/* Copyright 2012-2017 AOL Inc. All rights reserved.
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
 * https://http2.github.io/http2-spec/
 */
#include "moloch.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include "nghttp2/nghttp2.h"

//#define HTTPDEBUG 1

#define MAX_URL_LENGTH 4096
#define MAX_HTTP2_SIZE 8192
#define MAX_STREAMS 16

typedef struct {
    uint32_t                 id;
    nghttp2_hd_inflater     *hd_inflater[2];
    uint8_t                  ended;
    const char              *magicString[2];
} HTTP2Stream_t;

typedef enum {
    HTTP2_STATE_NORMAL,
    HTTP2_STATE_IN_DATA
} HTTP2InfoState_t;

typedef struct {
    unsigned char            data[2][MAX_HTTP2_SIZE];
    int                      used[2];
    int                      dataNeeded[2];
    int                      which;

    int                      numStreams;
    HTTP2Stream_t            streams[MAX_STREAMS];
} HTTP2Info_t;

#ifdef HTTPDEBUG
LOCAL const char *http2_frameNames[] = {"DATA", "HEADERS", "PRIORITY", "RST_STREAM", "SETTINGS", "PUSH_PROMISE", "PING", "GOAWAY", "WINDOW_UPDATE", "CONTINUATION", "ALTSVC", "ORIGIN"};
#endif


extern MolochConfig_t        config;

LOCAL  int statuscodeField;
LOCAL  int methodField;
LOCAL  int hostField;
LOCAL  int magicField;


void http_common_parse_cookie(MolochSession_t *session, char *cookie, int len);
void http_common_add_header(MolochSession_t *session, int pos, int isReq, const char *name, int namelen, const char *value, int valuelen);
void http_common_parse_url(MolochSession_t *session, char *url, int len);

/******************************************************************************/
LOCAL int http2_spos_get(HTTP2Info_t *http2, uint32_t streamId, int create)
{
    streamId &= 0x7fffffff;
    streamId++;

    for (int i = 0; i < http2->numStreams; i++) {
        if (streamId == http2->streams[i].id)
            return i;
    }

    // Not found, if we aren't creating then return error
    if (!create)
        return -1;

    // See if any slots are free and use that
    for (int i = 0; i < http2->numStreams; i++) {
        if (http2->streams[i].id == 0) {
            http2->streams[http2->numStreams].id = streamId;
            return i;
        }
    }

    // See if we can add to the end
    if (http2->numStreams == MAX_STREAMS)
        return -1;
    http2->streams[http2->numStreams].id = streamId;
    http2->numStreams++;
    return http2->numStreams-1;
}
/******************************************************************************/
LOCAL void http2_spos_free(HTTP2Info_t *http2, uint32_t streamId)
{
    streamId &= 0x7fffffff;
    streamId++;

    for (int i = 0; i < http2->numStreams; i++) {
        if (streamId == http2->streams[i].id) {
            http2->streams[i].id = 0;
            http2->streams[i].ended = 0;
            if (http2->streams[i].hd_inflater[0]) {
                nghttp2_hd_inflate_del(http2->streams[i].hd_inflater[0]);
                http2->streams[i].hd_inflater[0] = NULL;
            }
            if (http2->streams[i].hd_inflater[1]) {
                nghttp2_hd_inflate_del(http2->streams[i].hd_inflater[1]);
                http2->streams[i].hd_inflater[1] = NULL;
            }
            return;
        }
    }
}
/******************************************************************************/
LOCAL void http2_parse_frame_headers(MolochSession_t *session, HTTP2Info_t *http2, int which, uint8_t flags, uint32_t streamId, unsigned char *in, uint32_t inlen)
{
    int spos = http2_spos_get(http2, streamId, TRUE);
    if (spos == -1)
        return;

    if (!http2->streams[spos].hd_inflater[which])
        nghttp2_hd_inflate_new(&http2->streams[spos].hd_inflater[which]);

    int final = flags & NGHTTP2_FLAG_END_HEADERS;

#ifdef HTTPDEBUG
    LOG("%d,%d: %d final:%d %.*s", streamId, spos, inlen, final, inlen, in);
#endif

    // https://nghttp2.org/documentation/nghttp2_hd_inflate_hd2.html
    for(;;) {
        nghttp2_nv nv;
        int inflate_flags = 0;

        ssize_t rv = nghttp2_hd_inflate_hd2(http2->streams[spos].hd_inflater[which], &nv, &inflate_flags,
                                    in, inlen, final);

        if(rv < 0) {
            LOG("inflate failed with error code %zd", rv);
            return;
        }

        in += rv;
        inlen -= rv;

        if(inflate_flags & NGHTTP2_HD_INFLATE_EMIT) {
            if (nv.name[0] == ':') {
                if (memcmp(nv.name, ":method", 7) == 0) {
                    moloch_field_string_add(methodField, session, (char *)nv.value, nv.valuelen, TRUE);
                } else if (memcmp(nv.name, ":authority", 10) == 0) {
                    uint8_t *colon = memchr(nv.value, ':', nv.valuelen);
                    if (colon) {
                        moloch_field_string_add(hostField, session, (char *)nv.value, colon - nv.value, TRUE);
                    } else {
                        moloch_field_string_add(hostField, session, (char *)nv.value, nv.valuelen, TRUE);
                    }
                } else if (memcmp(nv.name, ":path", 5) == 0) {
                    http_common_parse_url(session, (char *)nv.value, nv.valuelen);
                } else if (memcmp(nv.name, ":status", 7) == 0) {
                    moloch_field_int_add(statuscodeField, session, atoi((const char *)nv.value));
                }
            } else {
                http_common_add_header(session, 0, which == http2->which, (const char *)nv.name, nv.namelen, (const char *)nv.value, nv.valuelen);

                if (memcmp(nv.name, "cookie", 6) == 0) {
                    http_common_parse_cookie(session, (char *)nv.value, nv.valuelen);
                }
            }
#ifdef HTTPDEBUG
            fwrite(nv.name, nv.namelen, 1, stderr);
            fprintf(stderr, ": ");
            fwrite(nv.value, nv.valuelen, 1, stderr);
            fprintf(stderr, "\n");
#endif
        }
        if(inflate_flags & NGHTTP2_HD_INFLATE_FINAL) {
            nghttp2_hd_inflate_end_headers(http2->streams[spos].hd_inflater[which]);
            break;
        }
        if((inflate_flags & NGHTTP2_HD_INFLATE_EMIT) == 0 &&
           inlen == 0) {
           break;
        }
    }

}
/******************************************************************************/
LOCAL int http2_parse_frame(MolochSession_t *session, HTTP2Info_t *http2, int which)
{
    BSB bsb;
    BSB_INIT(bsb, http2->data[which], http2->used[which]);

    uint32_t            len = 0;
    nghttp2_frame_type  type = 0;
    uint8_t             flags = 0;
    uint32_t            streamId = 0;

    BSB_IMPORT_u24(bsb, len);
    uint32_t olen = len;
    BSB_IMPORT_u08(bsb, type);
    BSB_IMPORT_u08(bsb, flags);
    BSB_IMPORT_u32(bsb, streamId);

    if (flags & NGHTTP2_FLAG_PADDED) {
        uint8_t padding = 0;
        BSB_IMPORT_u08(bsb, padding);
        BSB_IMPORT_skip(bsb, padding);
    }

    if (flags & NGHTTP2_FLAG_PRIORITY) {
        BSB_IMPORT_skip(bsb, 5); // weight
        len -= 5;
    }

    if (BSB_IS_ERROR(bsb))
        goto cleanup;

    // type will only be DATA if this is the first part of a data frame, anything else will be shortcutted in http_parse
    if (type == NGHTTP2_DATA) {
        int spos = http2_spos_get(http2, streamId, FALSE);
        http2->streams[spos].magicString[which] = moloch_parsers_magic(session, magicField, (char *)BSB_WORK_PTR(bsb), MIN(len, BSB_REMAINING(bsb)));
    }

    if (len > BSB_REMAINING(bsb)) {
        if (type != NGHTTP2_DATA) {
            return 1;
        } else {
            //TODO: Call data callback with (data, BSB_REMAINING(bsb));
            http2->dataNeeded[which] = len - BSB_REMAINING(bsb);
            http2->used[which] = 0;
            return 0;
        }
    }

#ifdef HTTPDEBUG
    LOG("which: %d len: %u, type: %u (%s), flags: 0x%x, streamId: %u", which, len, type, http2_frameNames[type], flags, streamId);
#endif
    switch(type) {
    case NGHTTP2_HEADERS:
        http2_parse_frame_headers(session, http2, which, flags, streamId, BSB_WORK_PTR(bsb), len);
        break;
    case NGHTTP2_RST_STREAM:
        http2_spos_free(http2, streamId);
        break;
    default:
        break;
    }

    if (flags & NGHTTP2_FLAG_END_STREAM) {
        int spos = http2_spos_get(http2, streamId, FALSE);
        if (spos != -1) {
            http2->streams[spos].ended |= (1 << which);
            if (http2->streams[spos].ended == 0x3) {
                http2_spos_free(http2, streamId);
            }
        }
    }

cleanup:
    http2->used[which] -= 9 + olen;
    memmove(http2->data[which], http2->data[which] + 9 + olen, http2->used[which]);

    return 0;
}
/******************************************************************************/
LOCAL int http2_parse(MolochSession_t *session, void *uw, const unsigned char *data, int len, int which)
{
    HTTP2Info_t            *http2          = uw;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG which: %d used: %d len: %d", which, http2->used[which], len);
#endif


    if (http2->dataNeeded[which] > 0) {
        int used = MIN(http2->dataNeeded[which], len);
        //TODO: Call data callback with (data, used);

        http2->dataNeeded[which] -= used;
        if (used == len)
            return 0;

        data += used;
        len -= used;
    }

    if (len > MAX_HTTP2_SIZE - http2->used[which]) {
#ifdef HTTPDEBUG
        moloch_print_hex_string(http2->data[which], http2->used[which]);
        LOG("TOO MUCH DATA");
#endif
        return MOLOCH_PARSER_UNREGISTER;
    }
    memcpy(http2->data[which] + http2->used[which], data, len);
    http2->used[which] += len;

    if (http2->used[which] > 24 && http2->data[which][0] == 'P' && memcmp(http2->data[which], "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n", 24) == 0) {
        http2->used[which] -= 24;
        memmove(http2->data[which], http2->data[which] + 24, http2->used[which]);
    }

    while (http2->used[which] >= 9) {
        if (http2_parse_frame(session, http2, which))
            break;
    }

    return 0;
}
/******************************************************************************/
void http2_save(MolochSession_t UNUSED(*session), void *UNUSED(uw), int final)
{
    if (!final)
        return;

//    HTTP2Info_t            *http2          = uw;

#ifdef HTTPDEBUG
    LOG("Save callback %d", final);
#endif
}
/******************************************************************************/
LOCAL void http2_free(MolochSession_t UNUSED(*session), void *uw)
{
    HTTP2Info_t            *http2          = uw;

    for (int i = 0; i < http2->numStreams; i++) {
        if (http2->streams[i].hd_inflater[0])
            nghttp2_hd_inflate_del(http2->streams[i].hd_inflater[0]);
        if (http2->streams[i].hd_inflater[1])
            nghttp2_hd_inflate_del(http2->streams[i].hd_inflater[1]);
    }
    MOLOCH_TYPE_FREE(HTTP2Info_t, http2);
}
/******************************************************************************/
LOCAL void http2_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len), int which, void *UNUSED(uw))
{
    if (moloch_session_has_protocol(session, "http2"))
        return;
    moloch_session_add_protocol(session, "http2");

    HTTP2Info_t            *http2          = MOLOCH_TYPE_ALLOC0(HTTP2Info_t);
    http2->which = which;

    moloch_parsers_register2(session, http2_parse, http2, http2_free, http2_save);
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_parsers_classifier_register_tcp("http2", NULL, 0, (unsigned char *)"PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n", 24, http2_classify);

    methodField = moloch_field_define("http", "termfield",
        "http.method", "Request Method", "http.method",
        "HTTP Request Method",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);
    statuscodeField = moloch_field_define("http", "integer",
        "http.statuscode", "Status Code", "http.statuscode",
        "Response HTTP numeric status code",
        MOLOCH_FIELD_TYPE_INT_GHASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);
    hostField = moloch_field_define("http", "lotermfield",
        "host.http", "Hostname", "http.host",
        "HTTP host header field",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        "aliases", "[\"http.host\"]",
        "category", "host",
        (char *)NULL);
    magicField = moloch_field_define("http", "termfield",
        "http.bodymagic", "Body Magic", "http.bodyMagic",
        "The content type of body determined by libfile/magic",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);
}
