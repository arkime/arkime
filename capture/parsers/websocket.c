/* websocket.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * WebSocket Protocol Parser (RFC 6455)
 *
 * The websocket parser is not registered as a TCP classifier; the http
 * parser detects the HTTP/1.1 101 Switching Protocols handshake with
 * "Upgrade: websocket" and dispatches to this parser via the "http"
 * sub-parser registry under the key "websocket".
 */

#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL int opcodeField;
LOCAL int closeCodeField;
LOCAL int closeReasonField;
LOCAL int payloadBytesField;
LOCAL int frameCntField;
LOCAL int textSampleField;
LOCAL int maskedFromClientField;

#define WEBSOCKET_TEXT_SAMPLE_MAX 256
LOCAL uint32_t websocketTextSampleCnt;

LOCAL const char *opcodeNames[] = {
    "Continuation", // 0
    "Text",         // 1
    "Binary",       // 2
    "0x3",
    "0x4",
    "0x5",
    "0x6",
    "0x7",
    "Close",        // 8
    "Ping",         // 9
    "Pong",         // 10
    "0xb",
    "0xc",
    "0xd",
    "0xe",
    "0xf"
};

typedef struct {
    ArkimeParserBuf_t *buf;
    uint64_t           payloadBytes;
    uint32_t           frameCnt;
    uint32_t           maskedFrames;
    uint16_t           textSampleCnt;
    uint8_t            curOpcode[2];   // last non-continuation opcode per direction
    uint8_t            sawAny;
} WebSocketInfo_t;

/******************************************************************************/
LOCAL void websocket_save(ArkimeSession_t *session, void *uw, int UNUSED(final))
{
    WebSocketInfo_t *ws = uw;
    if (!ws)
        return;
    if (ws->frameCnt) {
        arkime_field_int_add(frameCntField, session, (int)ws->frameCnt);
    }
    if (ws->payloadBytes) {
        int v = (ws->payloadBytes > 0x7fffffff) ? 0x7fffffff : (int)ws->payloadBytes;
        arkime_field_int_add(payloadBytesField, session, v);
    }
    if (ws->maskedFrames) {
        arkime_field_int_add(maskedFromClientField, session, (int)ws->maskedFrames);
    }
}
/******************************************************************************/
LOCAL void websocket_free(ArkimeSession_t *session, void *uw)
{
    WebSocketInfo_t *ws = uw;
    if (!ws)
        return;

    websocket_save(session, ws, 1);

    if (ws->buf) {
        arkime_parser_buf_free(ws->buf);
        ws->buf = NULL;
    }
    ARKIME_TYPE_FREE(WebSocketInfo_t, ws);
}
/******************************************************************************/
// Returns frame header length given header bytes (peek). 0 if more bytes needed.
LOCAL int websocket_header_len(const uint8_t *p, int avail, uint64_t *outPayLen, int *outMasked, int *outOpcode, int *outFin)
{
    if (avail < 2)
        return 0;
    *outFin    = (p[0] & 0x80) ? 1 : 0;
    *outOpcode = p[0] & 0x0F;
    *outMasked = (p[1] & 0x80) ? 1 : 0;
    int len7   = p[1] & 0x7F;

    int hdr = 2;
    uint64_t payLen = 0;

    if (len7 < 126) {
        payLen = len7;
    } else if (len7 == 126) {
        if (avail < hdr + 2)
            return 0;
        payLen = ((uint64_t)p[2] << 8) | p[3];
        hdr += 2;
    } else { // 127
        if (avail < hdr + 8)
            return 0;
        payLen = 0;
        for (int i = 0; i < 8; i++)
            payLen = (payLen << 8) | p[2 + i];
        hdr += 8;
    }

    if (*outMasked) {
        if (avail < hdr + 4)
            return 0;
        hdr += 4;
    }

    *outPayLen = payLen;
    return hdr;
}
/******************************************************************************/
LOCAL int websocket_tcp_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int which)
{
    WebSocketInfo_t   *ws  = uw;
    ArkimeParserBuf_t *buf = ws->buf;

    arkime_parser_buf_add(buf, which, data, len);

    while (buf->len[which] >= 2) {
        uint64_t payLen = 0;
        int masked = 0, opcode = 0, fin = 0;

        int hdrLen = websocket_header_len(buf->buf[which], buf->len[which],
                                          &payLen, &masked, &opcode, &fin);
        if (hdrLen == 0)
            return 0; // need more data for header

        // Sanity: cap payloads we'll attempt to handle. Anything within
        // RFC limits is fine. We just don't want to wedge on absurd values.
        if (payLen > (uint64_t)0x7fffffff) {
            arkime_session_add_tag(session, "websocket:bad-length");
            arkime_parsers_unregister(session, ws);
            return 0;
        }

        const uint8_t *hdr = buf->buf[which];
        uint8_t maskKey[4] = {0};
        if (masked) {
            const int koff = hdrLen - 4;
            for (int i = 0; i < 4; i++) maskKey[i] = hdr[koff + i];
        }

        if (opcode != 0) {
            ws->curOpcode[which & 1] = opcode;
            if (opcode < (int)ARRAY_LEN(opcodeNames))
                arkime_field_string_add(opcodeField, session, opcodeNames[opcode], -1, TRUE);
        }

        ws->frameCnt++;
        ws->payloadBytes += payLen;
        if (masked) ws->maskedFrames++;

        const int effOpcode = (opcode == 0) ? ws->curOpcode[which & 1] : opcode;

        // If this is a text frame and full payload fits in buf, extract a sample
        // (after unmasking).  Same handling for close frames.
        const int needFull = (effOpcode == 1 || opcode == 8);
        if (needFull && (uint64_t)(buf->len[which] - hdrLen) >= payLen) {
            const uint8_t *pay = buf->buf[which] + hdrLen;

            if (effOpcode == 1 && ws->textSampleCnt < websocketTextSampleCnt && payLen > 0) {
                int sampleLen = (payLen > WEBSOCKET_TEXT_SAMPLE_MAX) ?
                                WEBSOCKET_TEXT_SAMPLE_MAX : (int)payLen;
                char sample[WEBSOCKET_TEXT_SAMPLE_MAX];
                for (int i = 0; i < sampleLen; i++) {
                    uint8_t c = pay[i];
                    if (masked) c ^= maskKey[i & 3];
                    if (c < 0x20 && c != '\t' && c != '\n' && c != '\r')
                        c = '.';
                    sample[i] = (char)c;
                }
                // RFC 6455 §5.6 requires Text frames to be UTF-8; skip sample if
                // the peer sent something else (binary blob mislabeled as Text,
                // or a payload truncated mid-multibyte sequence at sampleLen).
                const gchar *utf8End = NULL;
                if (g_utf8_validate(sample, sampleLen, &utf8End)) {
                    arkime_field_string_add(textSampleField, session, sample, sampleLen, TRUE);
                    ws->textSampleCnt++;
                } else if (utf8End && utf8End > sample) {
                    int validLen = (int)(utf8End - sample);
                    arkime_field_string_add(textSampleField, session, sample, validLen, TRUE);
                    ws->textSampleCnt++;
                }
            } else if (opcode == 8) {
                if (payLen >= 2) {
                    uint8_t b0 = pay[0], b1 = pay[1];
                    if (masked) {
                        b0 ^= maskKey[0];
                        b1 ^= maskKey[1];
                    }
                    int code = (b0 << 8) | b1;
                    arkime_field_int_add(closeCodeField, session, code);

                    if (payLen > 2) {
                        int rlen = (int)payLen - 2;
                        if (rlen > 123) rlen = 123; // RFC max for close reason
                        char reason[128];
                        for (int i = 0; i < rlen; i++) {
                            uint8_t c = pay[2 + i];
                            if (masked) c ^= maskKey[(2 + i) & 3];
                            if (c < 0x20 && c != '\t')
                                c = '.';
                            reason[i] = (char)c;
                        }
                        // RFC 6455 §5.5.1 requires close reason text to be UTF-8.
                        const gchar *utf8End = NULL;
                        if (g_utf8_validate(reason, rlen, &utf8End)) {
                            if (rlen > 0)
                                arkime_field_string_add(closeReasonField, session, reason, rlen, TRUE);
                        } else if (utf8End && utf8End > reason) {
                            arkime_field_string_add(closeReasonField, session, reason, (int)(utf8End - reason), TRUE);
                        }
                    }
                }
            }
        }

        // Advance past this frame.  Use _skip so payloads larger than the
        // buffer still drain across subsequent segments.
        uint64_t total = (uint64_t)hdrLen + payLen;
        if (total <= (uint64_t)buf->len[which]) {
            arkime_parser_buf_del(buf, which, (int)total);
        } else {
            arkime_parser_buf_skip(buf, which, (int)total);
            // After a partial skip we have no more bytes buffered; return.
            return 0;
        }
    }

    return 0;
}
/******************************************************************************/
LOCAL int websocket_register_sub(ArkimeSession_t *session,
                                 void *UNUSED(uw),
                                 const uint8_t *UNUSED(data),
                                 int UNUSED(remaining),
                                 int UNUSED(which))
{
    if (arkime_parsers_has_registered(session, websocket_tcp_parser))
        return 0;

    WebSocketInfo_t *ws = ARKIME_TYPE_ALLOC0(WebSocketInfo_t);
    ws->buf = arkime_parser_buf_create();
    arkime_parsers_register2(session, websocket_tcp_parser, ws, websocket_free, websocket_save);
    return 0;
}
/******************************************************************************/
void arkime_parser_init()
{
    opcodeField = arkime_field_define("websocket", "termfield",
                                      "websocket.opcode", "Opcode", "websocket.opcode",
                                      "WebSocket frame opcode (Text, Binary, Ping, Pong, Close, ...)",
                                      ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                      (char *)NULL);

    closeCodeField = arkime_field_define("websocket", "integer",
                                         "websocket.closeCode", "Close Code", "websocket.closeCode",
                                         "WebSocket close status code",
                                         ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                         (char *)NULL);

    closeReasonField = arkime_field_define("websocket", "termfield",
                                           "websocket.closeReason", "Close Reason", "websocket.closeReason",
                                           "WebSocket close reason text",
                                           ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                           (char *)NULL);

    payloadBytesField = arkime_field_define("websocket", "integer",
                                            "websocket.payloadBytes", "Payload Bytes", "websocket.payloadBytes",
                                            "Total WebSocket payload bytes observed",
                                            ARKIME_FIELD_TYPE_INT, 0,
                                            (char *)NULL);

    frameCntField = arkime_field_define("websocket", "integer",
                                        "websocket.frameCnt", "Frame Count", "websocket.frameCnt",
                                        "Total WebSocket frames observed",
                                        ARKIME_FIELD_TYPE_INT, 0,
                                        (char *)NULL);

    textSampleField = arkime_field_define("websocket", "termfield",
                                          "websocket.textSample", "Text Sample", "websocket.textSample",
                                          "Sample of WebSocket text frame payload (truncated to 256 bytes)",
                                          ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                          (char *)NULL);

    maskedFromClientField = arkime_field_define("websocket", "integer",
                                                "websocket.maskedFromClient", "Masked Frames", "websocket.maskedFromClient",
                                                "Number of WebSocket frames with the MASK bit set (must be set on client->server frames per RFC 6455)",
                                                ARKIME_FIELD_TYPE_INT, 0,
                                                (char *)NULL);

    arkime_parsers_register_sub("http", "websocket", websocket_register_sub, NULL);

    websocketTextSampleCnt = arkime_config_int(NULL, "websocketTextSampleCnt", 4, 0, 1024);
}
