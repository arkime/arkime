/* Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

/*
 * TFTP (Trivial File Transfer Protocol) - RFC 1350
 *
 * TFTP Packet:
 *   Bytes 0-1: Opcode
 *     1 = RRQ (Read Request)
 *     2 = WRQ (Write Request)
 *     3 = DATA
 *     4 = ACK
 *     5 = ERROR
 *   For RRQ/WRQ: Filename (null-terminated) | Mode (null-terminated)
 */

extern ArkimeConfig_t        config;

LOCAL int opcodeField;
LOCAL int filenameField;

LOCAL const char *tftpOpcodes[] = {
    [1] = "rrq",
    [2] = "wrq",
    [3] = "data",
    [4] = "ack",
    [5] = "error"
};

/******************************************************************************/
LOCAL int tftp_udp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    BSB bsb;
    BSB_INIT(bsb, data, len);

    if (BSB_REMAINING(bsb) < 4)
        return 0;

    uint16_t opcode = 0;
    BSB_IMPORT_u16(bsb, opcode);

    if (opcode < ARRAY_LEN(tftpOpcodes) && tftpOpcodes[opcode])
        arkime_field_string_add(opcodeField, session, tftpOpcodes[opcode], -1, TRUE);

    // Extract filename from RRQ/WRQ
    if (opcode == 1 || opcode == 2) {
        const uint8_t *filename = BSB_WORK_PTR(bsb);
        int maxLen = BSB_REMAINING(bsb);
        int fnLen = 0;

        while (fnLen < maxLen && filename[fnLen] != 0)
            fnLen++;

        if (fnLen > 0)
            arkime_field_string_add(filenameField, session, (const char *)filename, fnLen, TRUE);
    }

    return 0;
}

/******************************************************************************/
LOCAL void tftp_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 4)
        return;

    // Must be on port 69
    if (session->port1 != 69 && session->port2 != 69)
        return;

    uint16_t opcode = (data[0] << 8) | data[1];

    // Valid opcodes are 1-5
    if (opcode < 1 || opcode > 5)
        return;

    // RRQ/WRQ must have null-terminated strings
    if (opcode == 1 || opcode == 2) {
        int hasNull = 0;
        for (int i = 2; i < len; i++) {
            if (data[i] == 0) {
                hasNull = 1;
                break;
            }
        }
        if (!hasNull)
            return;
    }

    arkime_session_add_protocol(session, "tftp");
    arkime_parsers_register(session, tftp_udp_parser, 0, 0);
}

/******************************************************************************/
void arkime_parser_init()
{
    opcodeField = arkime_field_define("tftp", "termfield",
                                      "tftp.opcode", "Opcode", "tftp.opcode",
                                      "TFTP opcode (rrq, wrq, data, ack, error)",
                                      ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT, (char *)NULL);

    filenameField = arkime_field_define("tftp", "termfield",
                                        "tftp.filename", "Filename", "tftp.filename",
                                        "TFTP filename",
                                        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT, (char *)NULL);

    arkime_parsers_classifier_register_port("tftp", NULL, 69, ARKIME_PARSERS_PORT_UDP, tftp_udp_classify);
}
