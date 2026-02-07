/* Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * PANA - Protocol for Carrying Authentication for Network Access (RFC 5191)
 */
#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL int userField;
LOCAL int msgTypeField;
LOCAL int sessionIdField;

// PANA Message Types (RFC 5191)
LOCAL const char *pana_msg_types[] = {
    "unknown",                    // 0
    "client-initiation",          // 1
    "auth",                       // 2
    "termination",                // 3
    "notification",               // 4
};

// PANA AVP Codes
#define PANA_AVP_EAP_PAYLOAD    2
#define PANA_AVP_NONCE          5

// EAP Codes
#define EAP_CODE_REQUEST        1
#define EAP_CODE_RESPONSE       2

// EAP Types
#define EAP_TYPE_IDENTITY       1

/******************************************************************************/
// Parse EAP packet embedded in PANA AVP
LOCAL void pana_parse_eap(ArkimeSession_t *session, const uint8_t *data, int len)
{
    if (len < 5)
        return;

    BSB bsb;
    BSB_INIT(bsb, data, len);

    uint8_t code = 0;
    uint16_t eapLen = 0;
    uint8_t type = 0;

    BSB_IMPORT_u08(bsb, code);
    BSB_IMPORT_skip(bsb, 1);  // id
    BSB_IMPORT_u16(bsb, eapLen);
    BSB_IMPORT_u08(bsb, type);

    if (BSB_IS_ERROR(bsb))
        return;

    // Only extract identity from Response/Identity packets
    if ((code == EAP_CODE_REQUEST || code == EAP_CODE_RESPONSE) && type == EAP_TYPE_IDENTITY) {
        int identityLen = eapLen - 5;  // EAP header is 5 bytes (code, id, len, type)
        if (identityLen > 0 && identityLen <= (int)BSB_REMAINING(bsb)) {
            const uint8_t *identity = BSB_WORK_PTR(bsb);
            arkime_field_string_add(userField, session, (char *)identity, identityLen, TRUE);
        }
    }
}

/******************************************************************************/
// Parse PANA AVPs
LOCAL int pana_parse_avps(ArkimeSession_t *session, BSB *bsb)
{
    while (BSB_REMAINING(*bsb) >= 8) {
        uint16_t avpCode = 0;
        uint16_t avpLen = 0;

        BSB_IMPORT_u16(*bsb, avpCode);
        BSB_IMPORT_skip(*bsb, 2);  // flags
        BSB_IMPORT_u16(*bsb, avpLen);
        BSB_IMPORT_skip(*bsb, 2);  // Reserved

        if (BSB_IS_ERROR(*bsb))
            return ARKIME_PARSER_UNREGISTER;

        // avpLen is the data length (not including 8-byte AVP header)
        if (avpLen > BSB_REMAINING(*bsb))
            return ARKIME_PARSER_UNREGISTER;

        const uint8_t *avpData = BSB_WORK_PTR(*bsb);

        switch (avpCode) {
        case PANA_AVP_EAP_PAYLOAD:
            pana_parse_eap(session, avpData, avpLen);
            break;
        }

        // AVPs are padded to 4-byte boundary
        uint16_t paddedLen = (avpLen + 3) & ~3;
        BSB_IMPORT_skip(*bsb, paddedLen);
    }
    return 0;
}

/******************************************************************************/
LOCAL int pana_udp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    if (len < 16)
        return ARKIME_PARSER_UNREGISTER;

    BSB bsb;
    BSB_INIT(bsb, data, len);

    uint16_t msgType = 0;
    uint32_t sessionId = 0;

    BSB_IMPORT_skip(bsb, 2);  // Reserved
    BSB_IMPORT_skip(bsb, 2);  // Message length
    BSB_IMPORT_skip(bsb, 2);  // Flags
    BSB_IMPORT_u16(bsb, msgType);
    BSB_IMPORT_u32(bsb, sessionId);
    BSB_IMPORT_skip(bsb, 4);  // Sequence number

    if (BSB_IS_ERROR(bsb))
        return ARKIME_PARSER_UNREGISTER;

    // Add message type
    if (msgType > 0 && msgType < ARRAY_LEN(pana_msg_types)) {
        arkime_field_string_add(msgTypeField, session, pana_msg_types[msgType], -1, TRUE);
    }

    // Add session ID if non-zero
    if (sessionId != 0) {
        char sessionIdStr[16];
        snprintf(sessionIdStr, sizeof(sessionIdStr), "%08x", sessionId);
        arkime_field_string_add(sessionIdField, session, sessionIdStr, 8, TRUE);
    }

    // Parse AVPs
    return pana_parse_avps(session, &bsb);
}

/******************************************************************************/
LOCAL void pana_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "pana"))
        return;

    // Exclude DNS ports
    ARKIME_RETURN_IF_DNS_PORT;

    // PANA header: 2 bytes reserved (0x0000), 2 bytes length, must match packet length
    if (len < 16)
        return;

    uint16_t msgLen = (data[2] << 8) | data[3];
    if (msgLen != len)
        return;

    // Message type should be 1-4
    uint16_t msgType = (data[6] << 8) | data[7];
    if (msgType < 1 || msgType >= ARRAY_LEN(pana_msg_types))
        return;

    arkime_session_add_protocol(session, "pana");
    arkime_parsers_register(session, pana_udp_parser, 0, 0);
}

/******************************************************************************/
void arkime_parser_init()
{
    userField = arkime_field_define("pana", "termfield",
                                    "pana.user", "User", "pana.user",
                                    "PANA/EAP User Identity",
                                    ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                    "category", "user",
                                    (char *)NULL);

    msgTypeField = arkime_field_define("pana", "termfield",
                                       "pana.msg-type", "Msg Type", "pana.msgType",
                                       "PANA Message Type",
                                       ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    sessionIdField = arkime_field_define("pana", "termfield",
                                         "pana.session-id", "Session ID", "pana.sessionId",
                                         "PANA Session ID",
                                         ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                         (char *)NULL);

    // PANA packets start with 0x0000 (reserved field)
    arkime_parsers_classifier_register_udp("pana", NULL, 0, (const uint8_t *)"\x00\x00", 2, pana_udp_classify);
}
