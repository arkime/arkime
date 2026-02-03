/* dnp3.c
 *
 * Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * DNP3 (Distributed Network Protocol 3) parser for SCADA/ICS systems
 */
#include <string.h>
#include "arkimeconfig.h"
#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL  int srcField;
LOCAL  int dstField;
LOCAL  int funcField;

#define DNP3_START_BYTES    0x0564
#define DNP3_MIN_LEN        10      // Start(2) + Len(1) + Ctrl(1) + Dst(2) + Src(2) + CRC(2)

/******************************************************************************/
LOCAL void dnp3_parse_frame(ArkimeSession_t *session, BSB *bsb)
{
    BSB_IMPORT_skip(*bsb, 1);  // ctrl

    uint16_t dst = 0, src = 0;
    BSB_LIMPORT_u16(*bsb, dst);
    BSB_LIMPORT_u16(*bsb, src);

    arkime_field_int_add(srcField, session, src);
    arkime_field_int_add(dstField, session, dst);

    // Skip header CRC (2 bytes) to get to transport + application layer
    BSB_IMPORT_skip(*bsb, 2);

    // Parse transport header and application layer if present
    if (BSB_REMAINING(*bsb) >= 3) {
        BSB_IMPORT_skip(*bsb, 1);  // transport header
        BSB_IMPORT_skip(*bsb, 1);  // app control

        uint8_t funcCode = 0;
        BSB_IMPORT_u08(*bsb, funcCode);

        arkime_field_int_add(funcField, session, funcCode);
    }
}
/******************************************************************************/
LOCAL int dnp3_tcp_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int which)
{
    ArkimeParserBuf_t *dnp3 = uw;

    arkime_parser_buf_add(dnp3, which, data, len);

    while (dnp3->len[which] >= DNP3_MIN_LEN) {
        BSB bsb;
        BSB_INIT(bsb, dnp3->buf[which], dnp3->len[which]);

        uint16_t start = 0;
        BSB_IMPORT_u16(bsb, start);

        if (start != DNP3_START_BYTES) {
            // Lost sync, try to find start bytes
            const uint8_t *buf = dnp3->buf[which];
            int bufLen = dnp3->len[which];
            int i;
            for (i = 1; i < bufLen - 1; i++) {
                if (buf[i] == 0x05 && buf[i + 1] == 0x64) {
                    break;
                }
            }
            if (i >= bufLen - 1) {
                arkime_parser_buf_del(dnp3, which, bufLen - 1);
                return 0;
            }
            arkime_parser_buf_del(dnp3, which, i);
            continue;
        }

        uint8_t pduLen = 0;
        BSB_IMPORT_u08(bsb, pduLen);

        if (pduLen < 5) {
            // Invalid length
            arkime_parser_buf_del(dnp3, which, 2);
            continue;
        }

        // Calculate total frame length: 10 bytes header + user data with CRCs
        // Every 16 bytes of user data has a 2-byte CRC
        int userDataLen = pduLen - 5;  // Subtract ctrl(1) + dst(2) + src(2)
        int numCrcBlocks = (userDataLen + 15) / 16;
        int totalLen = 10 + userDataLen + (numCrcBlocks * 2);

        if (totalLen > (int)dnp3->len[which]) {
            return 0;  // Need more data
        }

        dnp3_parse_frame(session, &bsb);

        arkime_parser_buf_del(dnp3, which, totalLen);
    }

    return 0;
}
/******************************************************************************/
LOCAL int dnp3_udp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    if (len < DNP3_MIN_LEN) {
        return 0;
    }

    BSB bsb;
    BSB_INIT(bsb, data, len);

    uint16_t start = 0;
    BSB_IMPORT_u16(bsb, start);

    if (start != DNP3_START_BYTES) {
        return 0;
    }

    uint8_t pduLen = 0;
    BSB_IMPORT_u08(bsb, pduLen);

    if (pduLen < 5) {
        return 0;
    }

    dnp3_parse_frame(session, &bsb);

    return 0;
}
/******************************************************************************/
LOCAL void dnp3_tcp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void UNUSED(*uw))
{
    if (len < DNP3_MIN_LEN) {
        return;
    }

    // Check for DNP3 start bytes: 0x05 0x64
    if (data[0] != 0x05 || data[1] != 0x64) {
        return;
    }

    uint8_t pduLen = data[2];
    if (pduLen < 5) {
        return;
    }

    if (arkime_session_has_protocol(session, "dnp3")) {
        return;
    }
    arkime_session_add_protocol(session, "dnp3");

    ArkimeParserBuf_t *info = arkime_parser_buf_create();
    arkime_parsers_register(session, dnp3_tcp_parser, info, arkime_parser_buf_session_free);
}
/******************************************************************************/
LOCAL void dnp3_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void UNUSED(*uw))
{
    if (len < DNP3_MIN_LEN) {
        return;
    }

    // Check for DNP3 start bytes: 0x05 0x64
    if (data[0] != 0x05 || data[1] != 0x64) {
        return;
    }

    uint8_t pduLen = data[2];
    if (pduLen < 5) {
        return;
    }

    if (arkime_session_has_protocol(session, "dnp3")) {
        return;
    }
    arkime_session_add_protocol(session, "dnp3");

    arkime_parsers_register(session, dnp3_udp_parser, NULL, NULL);
}
/******************************************************************************/
void arkime_parser_init()
{
    // DNP3 typically runs on port 20000 TCP and UDP
    arkime_parsers_classifier_register_port("dnp3", NULL, 20000, ARKIME_PARSERS_PORT_TCP, dnp3_tcp_classify);
    arkime_parsers_classifier_register_port("dnp3", NULL, 20000, ARKIME_PARSERS_PORT_UDP, dnp3_udp_classify);

    srcField = arkime_field_define("dnp3", "integer",
                                   "dnp3.src", "DNP3 Source Address", "dnp3.src",
                                   "DNP3 Source Addresses",
                                   ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                   (char *)NULL);

    dstField = arkime_field_define("dnp3", "integer",
                                   "dnp3.dst", "DNP3 Destination Address", "dnp3.dst",
                                   "DNP3 Destination Addresses",
                                   ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                   (char *)NULL);

    funcField = arkime_field_define("dnp3", "integer",
                                    "dnp3.func", "DNP3 Function Code", "dnp3.func",
                                    "DNP3 Function Codes",
                                    ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                    (char *)NULL);
}
