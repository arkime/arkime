/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

extern ArkimeConfig_t        config;
LOCAL  int userField;

/******************************************************************************/
// Extract a UCS-2LE string from TDS 7 packet, convert to UTF-8 lowercase
LOCAL void tds7_add_string_field(ArkimeSession_t *session, int field, const uint8_t *data, int dataLen, int offset, int charLen)
{
    if (charLen <= 0 || offset < 0)
        return;

    int byteLen = charLen * 2;  // UCS-2LE is 2 bytes per character

    if (offset + byteLen > dataLen)
        return;

    // Convert UCS-2LE to UTF-8
    gsize bread, bwritten;
    GError *error = NULL;
    char *utf8 = g_convert((const char *)data + offset, byteLen, "utf-8", "ucs-2le", &bread, &bwritten, &error);
    if (error) {
        g_error_free(error);
        return;
    }
    if (utf8) {
        arkime_field_string_add_lower(field, session, utf8, bwritten);
        g_free(utf8);
    }
}
/******************************************************************************/
LOCAL int tds_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int remaining, int which)
{
    ArkimeParserBuf_t *tds = uw;

    if (arkime_parser_buf_add(tds, which, data, remaining) < 0) {
        arkime_session_add_tag(session, "tds:packet-too-long");
        return ARKIME_PARSER_UNREGISTER;
    }

    // Lots of info from http://www.freetds.org/tds.html
    if (tds->version == 7) {
        // TDS 7.0+: walk packets in this direction's buffer.
        // Client (which==0): may have PRELOGIN(0x12), LOGIN7(0x10), SSPI(0x11 with Type 3)
        // Server (which==1): may have SSPI(0x11 with Type 2)
        const uint8_t *ptr = tds->buf[which];
        int pos = 0;

        while (pos + 8 <= tds->len[which]) {
            uint8_t pktType = ptr[pos];
            uint16_t pktLen = (ptr[pos + 2] << 8) | ptr[pos + 3];

            if (pktLen < 8 || pktLen > tds->bufMax) {
                arkime_session_add_tag(session, "tds:packet-too-long");
                arkime_parsers_unregister(session, uw);
                return 0;
            }
            if (pos + pktLen > tds->len[which]) {
                break;
            }

            if (pktType == 0x10 && which == 0) {
                // LOGIN7
                if (pktLen >= 8 + 86) {
                    const uint8_t *pkt = ptr + pos + 8;
                    int pktDataLen = pktLen - 8;

                    uint16_t userOffset = pkt[40] | (pkt[41] << 8);
                    uint16_t userLen    = pkt[42] | (pkt[43] << 8);
                    tds7_add_string_field(session, userField, pkt, pktDataLen, userOffset, userLen);

                    // SSPI: offset@78, length@80. cbSSPI==0xFFFF -> use cbSSPILong@90.
                    uint16_t sspiOffset = pkt[78] | (pkt[79] << 8);
                    uint32_t sspiLen    = pkt[80] | (pkt[81] << 8);
                    if (sspiLen == 0xFFFF && pktDataLen >= 94) {
                        sspiLen = pkt[90] | (pkt[91] << 8) |
                                  (pkt[92] << 16) | (pkt[93] << 24);
                    }
                    if (sspiLen >= 12 && sspiOffset <= (uint32_t)pktDataLen &&
                        sspiLen <= (uint32_t)pktDataLen - sspiOffset) {
                        arkime_parsers_ntlm_decode(session, pkt + sspiOffset, sspiLen);
                    }
                }
            } else if (pktType == 0x11) {
                // SSPI message - raw NTLMSSP payload
                if (pktLen > 8) {
                    arkime_parsers_ntlm_decode(session, ptr + pos + 8, pktLen - 8);
                }
            }

            pos += pktLen;
        }
    } else {
        // TDS 4.2/5.0 login packet
        if (tds->len[0] > 598) {
#if 0
            LOG("host:%.*s user:%.*s pass:%.*s process:%.*s app:%.*s server:%.*s lib:%.*s",
                tds->buf[0][38], tds->buf[0] + 8,
                tds->buf[0][69], tds->buf[0] + 39,
                tds->buf[0][100], tds->buf[0] + 70,
                tds->buf[0][131], tds->buf[0] + 101,
                tds->buf[0][178], tds->buf[0] + 148,
                tds->buf[0][209], tds->buf[0] + 179,
                tds->buf[0][480], tds->buf[0] + 470
               );
#endif
            arkime_field_string_add_lower(userField, session, (const char *)tds->buf[0] + 39, tds->buf[0][69]);
            arkime_parsers_unregister(session, uw);
        }
    }

    return 0;
}
/******************************************************************************/
LOCAL void tds_classify(ArkimeSession_t *session, const uint8_t *data, int len, int which, void *UNUSED(uw))
{
    if (which != 0 || arkime_session_has_protocol(session, "tds"))
        return;

    ArkimeParserBuf_t *tds = arkime_parser_buf_create();

    // Determine TDS version based on packet type
    if (data[0] == 0x10 || data[0] == 0x12) {
        // TDS 7.0+ LOGIN7 packet (0x10) or prelogin packet (0x12)
        if (len < 50) {
            arkime_parser_buf_free(tds);
            return;
        }
        tds->version = 7;
    } else if (data[0] == 0x02) {
        // TDS 4.2/5.0 login packet - need at least 512 bytes
        if (len < 512) {
            arkime_parser_buf_free(tds);
            return;
        }
        tds->version = 0;
    } else {
        arkime_parser_buf_free(tds);
        return;
    }

    arkime_session_add_protocol(session, "tds");
    arkime_parsers_register(session, tds_parser, tds, arkime_parser_buf_session_free);
}
/******************************************************************************/
LOCAL void tds7_classify(ArkimeSession_t *session, const uint8_t *data, int len, int which, void *uw)
{
    tds_classify(session, data, len, which, uw);
}
/******************************************************************************/
void arkime_parser_init()
{

    userField = arkime_field_by_db("user");

    // TDS 4.2/5.0 login packet (type 0x02)
    arkime_parsers_classifier_register_tcp("tds", NULL, 0, (uint8_t *)"\x02\x00\x02\x00\x00\x00\x01\x00", 8, tds_classify);

    // TDS 7.0+ LOGIN7 packet (type 0x10)
    // Header: type=0x10, status=0x01 (last packet), followed by length
    arkime_parsers_classifier_register_tcp("tds7", NULL, 0, (uint8_t *)"\x10\x01", 2, tds7_classify);

    // TDS 7.0+ prelogin packet (type 0x12) - comes before LOGIN7
    arkime_parsers_classifier_register_tcp("tds7", NULL, 0, (uint8_t *)"\x12\x01", 2, tds7_classify);
}

