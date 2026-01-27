/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

#define TDS_MAX_SIZE 4096
typedef struct {
    uint8_t  data[2][TDS_MAX_SIZE];
    int      pos[2];
    uint8_t  version;  // 0 = TDS 4.2/5.0, 7 = TDS 7.0+
} TDSInfo_t;

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
    TDSInfo_t *tds = uw;

    remaining = MIN(remaining, TDS_MAX_SIZE - tds->pos[which]);
    memcpy(tds->data[which] + tds->pos[which], data, remaining);
    tds->pos[which] += remaining;

    // Lots of info from http://www.freetds.org/tds.html
    if (tds->version == 7) {
        // TDS 7.0+ - look for LOGIN7 packet (type 0x10) in the stream
        // May have prelogin (0x12) packets before LOGIN7
        const uint8_t *ptr = tds->data[0];
        int pos = 0;

        while (pos + 8 <= tds->pos[0]) {
            uint8_t pktType = ptr[pos];
            uint16_t pktLen = (ptr[pos + 2] << 8) | ptr[pos + 3];

            if (pktLen < 8 || pos + pktLen > tds->pos[0]) {
                // Need more data
                break;
            }

            if (pktType == 0x10) {
                // LOGIN7 packet found
                // Need at least 8-byte header + 86 bytes of fixed login7 structure
                if (pktLen >= 8 + 86) {
                    const uint8_t *pkt = ptr + pos + 8;  // Skip 8-byte TDS header
                    int pktDataLen = pktLen - 8;

                    // Offsets are at fixed positions in LOGIN7 structure (relative to start of login7 data)
                    // Username: offset at byte 40, length at byte 42
                    uint16_t userOffset = pkt[40] | (pkt[41] << 8);
                    uint16_t userLen    = pkt[42] | (pkt[43] << 8);

                    tds7_add_string_field(session, userField, pkt, pktDataLen, userOffset, userLen);
                }
                arkime_parsers_unregister(session, uw);
                return 0;
            }

            pos += pktLen;
        }
    } else {
        // TDS 4.2/5.0 login packet
        if (tds->pos[0] > 598) {
#if 0
            LOG("host:%.*s user:%.*s pass:%.*s process:%.*s app:%.*s server:%.*s lib:%.*s",
                tds->data[0][38], tds->data[0] + 8,
                tds->data[0][69], tds->data[0] + 39,
                tds->data[0][100], tds->data[0] + 70,
                tds->data[0][131], tds->data[0] + 101,
                tds->data[0][178], tds->data[0] + 148,
                tds->data[0][209], tds->data[0] + 179,
                tds->data[0][480], tds->data[0] + 470
               );
#endif
            arkime_field_string_add_lower(userField, session, (const char *)tds->data[0] + 39, tds->data[0][69]);
            arkime_parsers_unregister(session, uw);
        }
    }

    return 0;
}
/******************************************************************************/
LOCAL void tds_free(ArkimeSession_t UNUSED(*session), void *uw)
{
    TDSInfo_t            *tds          = uw;

    ARKIME_TYPE_FREE(TDSInfo_t, tds);
}
/******************************************************************************/
LOCAL void tds_classify(ArkimeSession_t *session, const uint8_t *data, int len, int which, void *UNUSED(uw))
{
    if (which != 0 || arkime_session_has_protocol(session, "tds"))
        return;

    TDSInfo_t *tds = ARKIME_TYPE_ALLOC(TDSInfo_t);
    tds->pos[0] = tds->pos[1] = 0;

    // Determine TDS version based on packet type
    if (data[0] == 0x10 || data[0] == 0x12) {
        // TDS 7.0+ LOGIN7 packet (0x10) or prelogin packet (0x12)
        if (len < 50) {
            ARKIME_TYPE_FREE(TDSInfo_t, tds);
            return;
        }
        tds->version = 7;
    } else if (data[0] == 0x02) {
        // TDS 4.2/5.0 login packet - need at least 512 bytes
        if (len < 512) {
            ARKIME_TYPE_FREE(TDSInfo_t, tds);
            return;
        }
        tds->version = 0;
    } else {
        ARKIME_TYPE_FREE(TDSInfo_t, tds);
        return;
    }

    arkime_session_add_protocol(session, "tds");
    arkime_parsers_register(session, tds_parser, tds, tds_free);
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

