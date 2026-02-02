/* Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

/*
 * PTP (Precision Time Protocol) - IEEE 1588v2
 *
 * PTP Header (34 bytes minimum):
 *   Byte 0: messageType (low nibble) | transportSpecific (high nibble)
 *   Byte 1: versionPTP (low nibble should be 2)
 *   Bytes 2-3: messageLength
 *   Byte 4: domainNumber
 *   Byte 5: reserved1
 *   Bytes 6-7: flags
 *   Bytes 8-15: correctionField
 *   Bytes 16-19: reserved2
 *   Bytes 20-27: clockIdentity (8 bytes)
 *   Bytes 28-29: sourcePortId
 *   Bytes 30-31: sequenceId
 *   Byte 32: controlField
 *   Byte 33: logMessageInterval
 *
 * Port 319 = Event messages (time-critical: Sync, Delay_Req, etc.)
 * Port 320 = General messages (Announce, Follow_Up, Management, etc.)
 */

LOCAL const char *ptpMsgTypes[] = {
    [0x0] = "sync",
    [0x1] = "delay-req",
    [0x2] = "pdelay-req",
    [0x3] = "pdelay-resp",
    [0x8] = "follow-up",
    [0x9] = "delay-resp",
    [0xA] = "pdelay-resp-follow-up",
    [0xB] = "announce",
    [0xC] = "signaling",
    [0xD] = "management"
};

extern ArkimeConfig_t        config;

LOCAL  int msgTypeField;
LOCAL  int clockIdField;

/******************************************************************************/
LOCAL int ptp_udp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    BSB bsb;
    BSB_INIT(bsb, data, len);

    if (BSB_REMAINING(bsb) < 34)
        return 0;

    uint8_t byte0 = 0;
    BSB_IMPORT_u08(bsb, byte0);

    uint8_t msgType = byte0 & 0x0f;

    // Add message type
    if (msgType < ARRAY_LEN(ptpMsgTypes) && ptpMsgTypes[msgType]) {
        arkime_field_string_add(msgTypeField, session, ptpMsgTypes[msgType], -1, TRUE);
    }

    // Skip version, messageLength, domainNumber, reserved1, flags, correctionField, reserved2
    BSB_IMPORT_skip(bsb, 1 + 2 + 1 + 1 + 2 + 8 + 4);

    // Clock identity (8 bytes) - display as hex
    if (BSB_REMAINING(bsb) >= 8) {
        const uint8_t *clockId = BSB_WORK_PTR(bsb);
        char clockIdStr[24];
        snprintf(clockIdStr, sizeof(clockIdStr), "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                 clockId[0], clockId[1], clockId[2], clockId[3],
                 clockId[4], clockId[5], clockId[6], clockId[7]);
        arkime_field_string_add(clockIdField, session, clockIdStr, -1, TRUE);
    }

    return 0;
}
/******************************************************************************/
LOCAL void ptp_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "ptp"))
        return;

    // Minimum PTP packet size
    if (len < 34)
        return;

    // Validate version (byte 1 low nibble should be 2 for PTPv2)
    if ((data[1] & 0x0f) != 2)
        return;

    // Validate message type (byte 0 low nibble should be valid)
    uint8_t msgType = data[0] & 0x0f;
    if (msgType > 0x0D || (msgType > 0x03 && msgType < 0x08))
        return;

    arkime_session_add_protocol(session, "ptp");
    arkime_parsers_register(session, ptp_udp_parser, 0, 0);
}
/******************************************************************************/
void arkime_parser_init()
{
    msgTypeField = arkime_field_define("ptp", "termfield",
                                       "ptp.msgType", "Message Type", "ptp.msgType",
                                       "PTP message type",
                                       ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    clockIdField = arkime_field_define("ptp", "termfield",
                                       "ptp.clockId", "Clock ID", "ptp.clockId",
                                       "PTP source clock identity",
                                       ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    // PTP uses UDP ports 319 (event) and 320 (general)
    arkime_parsers_classifier_register_port("ptp", NULL, 319, ARKIME_PARSERS_PORT_UDP, ptp_udp_classify);
    arkime_parsers_classifier_register_port("ptp", NULL, 320, ARKIME_PARSERS_PORT_UDP, ptp_udp_classify);
}
