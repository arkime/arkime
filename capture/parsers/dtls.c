/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

//#define DTLSDEBUG 1

extern ArkimeConfig_t        config;

LOCAL uint32_t tls_process_server_certificate_func;

/******************************************************************************/
LOCAL int dtls_udp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    BSB bbuf;

    // 22 is handshake
    if (data[0] != 22) {
        arkime_parsers_unregister(session, uw);
        return 0;
    }

    BSB_INIT(bbuf, data, len);

    while (BSB_NOT_ERROR(bbuf) && BSB_REMAINING(bbuf) > 11) {
        BSB_IMPORT_skip(bbuf, 11);
        uint16_t tlen = 0;
        BSB_IMPORT_u16(bbuf, tlen);

        if (tlen > BSB_REMAINING(bbuf))
            return 0;

        BSB msgBuf;
        BSB_INIT(msgBuf, BSB_WORK_PTR(bbuf), tlen);
        BSB_IMPORT_skip(bbuf, tlen);

        while (BSB_NOT_ERROR(bbuf) && BSB_NOT_ERROR(msgBuf) && BSB_REMAINING(msgBuf) > 12) {
            uint8_t handshakeType = 0;
            BSB_IMPORT_u08(msgBuf, handshakeType);
            uint32_t handshakeLen = 0;
            BSB_IMPORT_u24(msgBuf, handshakeLen);
            BSB_IMPORT_skip(msgBuf, 2); // msgSeq
            uint32_t frameOffset = 0;
            BSB_IMPORT_u24(msgBuf, frameOffset);
            BSB_IMPORT_skip(msgBuf, 3); // frameLength
            // ALW fix - don't handle fragmented packets yet
            if (frameOffset != 0) {
                BSB_IMPORT_skip(msgBuf, handshakeLen);
                continue;
            }

            // Not enough data left
            if (BSB_IS_ERROR(msgBuf) || handshakeLen > BSB_REMAINING(msgBuf))
                break;

            switch (handshakeType) {
            case 11: // Certificate
                arkime_parsers_call_named_func(tls_process_server_certificate_func, session, BSB_WORK_PTR(msgBuf), handshakeLen, NULL);
                BSB_IMPORT_skip(msgBuf, handshakeLen);
                break;
            default:
                BSB_IMPORT_skip(msgBuf, handshakeLen);
            }
        }
    }

    return 0;
}
/******************************************************************************/
LOCAL void dtls_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 100 || data[13] != 1)
        return;
    arkime_session_add_protocol(session, "dtls");
    arkime_parsers_register(session, dtls_udp_parser, uw, 0);
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_parsers_classifier_register_udp("dtls", NULL, 0, (const uint8_t *)"\x16\x01\x00", 3, dtls_udp_classify);
    arkime_parsers_classifier_register_udp("dtls", NULL, 0, (const uint8_t *)"\x16\xfe\xff", 3, dtls_udp_classify);
    arkime_parsers_classifier_register_udp("dtls", NULL, 0, (const uint8_t *)"\x16\xfe\xfe", 3, dtls_udp_classify);
    arkime_parsers_classifier_register_udp("dtls", NULL, 0, (const uint8_t *)"\x16\xfe\xfd", 3, dtls_udp_classify);

    tls_process_server_certificate_func = arkime_parsers_get_named_func("tls_process_server_certificate");
}
