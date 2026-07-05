/* m3ua.c - M2UA/M3UA parser for SS7/SIGTRAN
 *
 * Handles:
 * - M2UA (SCTP protocol ID 2): MTP2 User Adaptation
 * - M3UA (SCTP protocol ID 3): MTP3 User Adaptation
 *
 * Detects upper layer protocols based on MTP3 Service Indicator:
 * - SI=3 (SCCP) -> Calls sub-parsers (tcap -> camel/map)
 * - SI=5 (ISUP) -> Marks as ISUP
 * - SI=4 (TUP)  -> Marks as TUP
 *
 * Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL int siField;
LOCAL GHashTable *subParsers;

// M2UA constants
#define M2UA_VERSION 1
#define M2UA_MSG_CLASS_MAUP 6   // MTP2 User Adaptation messages
#define M2UA_MSG_TYPE_DATA 1

// M3UA constants
#define M3UA_VERSION 1
#define M3UA_MSG_CLASS_TRANSFER 1
#define M3UA_MSG_TYPE_DATA 1

/******************************************************************************/
// Parse M2UA header, returns pointer to MTP3 data or NULL
LOCAL const uint8_t *m3ua_parse_m2ua(const uint8_t *data, int len, int *mtp3Len)
{
    BSB bsb;
    BSB_INIT(bsb, data, len);

    uint8_t version = 0, msgClass = 0, msgType = 0;
    uint32_t msgLen = 0;

    BSB_IMPORT_u08(bsb, version);
    BSB_IMPORT_skip(bsb, 1); // reserved
    BSB_IMPORT_u08(bsb, msgClass);
    BSB_IMPORT_u08(bsb, msgType);
    BSB_IMPORT_u32(bsb, msgLen);

    if (BSB_IS_ERROR(bsb))
        return NULL;

    if (version != M2UA_VERSION)
        return NULL;

    if (msgClass != M2UA_MSG_CLASS_MAUP || msgType != M2UA_MSG_TYPE_DATA)
        return NULL;

    if (msgLen < 8 || msgLen > (uint32_t)len)
        return NULL;

    BSB_INIT(bsb, data + 8, msgLen - 8);

    // Parse parameters looking for Protocol Data
    while (BSB_REMAINING(bsb) >= 4) {
        uint16_t paramTag = 0, paramLen = 0;
        BSB_IMPORT_u16(bsb, paramTag);
        BSB_IMPORT_u16(bsb, paramLen);

        if (paramLen < 4 || BSB_IS_ERROR(bsb))
            return NULL;

        int dataLen = paramLen - 4;

        // Protocol Data 1 (0x0300) or Protocol Data 2 (0x0301)
        if (paramTag == 0x0300 || paramTag == 0x0301) {
            if (BSB_REMAINING(bsb) >= dataLen) {
                if (paramTag == 0x0301) {
                    // RFC 3331: Protocol Data 2 starts with a 1-byte Length
                    // Indicator (LI) octet before the MTP2-user message
                    if (dataLen < 2)
                        return NULL;
                    *mtp3Len = dataLen - 1;
                    return BSB_WORK_PTR(bsb) + 1;
                }
                *mtp3Len = dataLen;
                return BSB_WORK_PTR(bsb);
            }
        }

        // Skip to next parameter (4-byte aligned)
        int skip = (dataLen + 3) & ~3;
        BSB_IMPORT_skip(bsb, skip);
    }

    return NULL;
}

/******************************************************************************/
LOCAL const uint8_t *m3ua_parse_mtp3(const uint8_t *data, int len, int *userLen, int *si);
/******************************************************************************/
// Parse M3UA header, returns pointer to user data or NULL
// For M3UA, the Protocol Data parameter contains routing info + user data
LOCAL const uint8_t *m3ua_parse_m3ua(const uint8_t *data, int len, int *userLen, int *si)
{
    BSB bsb;
    BSB_INIT(bsb, data, len);

    uint8_t version = 0, msgClass = 0, msgType = 0;
    uint32_t msgLen = 0;

    BSB_IMPORT_u08(bsb, version);
    BSB_IMPORT_skip(bsb, 1); // reserved
    BSB_IMPORT_u08(bsb, msgClass);
    BSB_IMPORT_u08(bsb, msgType);
    BSB_IMPORT_u32(bsb, msgLen);

    if (BSB_IS_ERROR(bsb))
        return NULL;

    if (version != M3UA_VERSION)
        return NULL;

    if (msgClass != M3UA_MSG_CLASS_TRANSFER || msgType != M3UA_MSG_TYPE_DATA)
        return NULL;

    if (msgLen < 8 || msgLen > (uint32_t)len)
        return NULL;

    BSB_INIT(bsb, data + 8, msgLen - 8);

    // Parse parameters looking for Protocol Data (0x0210 per RFC 4666, 0x0002/0x0003 draft v6)
    while (BSB_REMAINING(bsb) >= 4) {
        uint16_t paramTag = 0, paramLen = 0;
        BSB_IMPORT_u16(bsb, paramTag);
        BSB_IMPORT_u16(bsb, paramLen);

        if (paramLen < 4 || BSB_IS_ERROR(bsb))
            return NULL;

        int dataLen = paramLen - 4;

        // Protocol Data (0x0210 per RFC 4666)
        if (paramTag == 0x0210) {
            // M3UA Protocol Data: OPC(4) + DPC(4) + SI(1) + NI(1) + MP(1) + SLS(1) + user data
            if (dataLen >= 12 && BSB_REMAINING(bsb) >= dataLen) {
                BSB_IMPORT_skip(bsb, 8); // Skip OPC, DPC
                uint8_t siVal = 0;
                BSB_IMPORT_u08(bsb, siVal);
                *si = siVal;
                BSB_IMPORT_skip(bsb, 3); // Skip NI, MP, SLS
                *userLen = dataLen - 12;
                return BSB_WORK_PTR(bsb);
            }
        }

        // Draft v6 Protocol Data 1 (0x0002) is a raw MTP3 message; Protocol
        // Data 2 (0x0003) is the same with a 1-byte Length Indicator first
        if (paramTag == 0x0002 || paramTag == 0x0003) {
            int liLen = (paramTag == 0x0003) ? 1 : 0;
            if (dataLen > liLen && BSB_REMAINING(bsb) >= dataLen) {
                return m3ua_parse_mtp3(BSB_WORK_PTR(bsb) + liLen, dataLen - liLen, userLen, si);
            }
        }

        // Skip to next parameter (4-byte aligned)
        int skip = (dataLen + 3) & ~3;
        BSB_IMPORT_skip(bsb, skip);
    }

    return NULL;
}

/******************************************************************************/
// Parse MTP3 header (for M2UA), returns SI and pointer to user data
LOCAL const uint8_t *m3ua_parse_mtp3(const uint8_t *data, int len, int *userLen, int *si)
{
    if (len < 5)
        return NULL;

    // Service Information Octet
    uint8_t sio = data[0];
    *si = sio & 0x0f;  // Service Indicator

    // Routing Label is 4 bytes for ITU
    *userLen = len - 5;
    return data + 5;
}


/******************************************************************************/
// Generic sub-parser that just adds uw as the protocol name
LOCAL int m3ua_protocol_parser(ArkimeSession_t *session, void *uw, const uint8_t *UNUSED(data), int UNUSED(len), int UNUSED(which))
{
    arkime_session_add_protocol(session, uw);
    // Keep parsing so mixed-SI associations (e.g. ISUP + SCCP) still get
    // their other service indicators recorded and parsed
    return 0;
}

/******************************************************************************/
LOCAL int m3ua_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int which)
{
    int isM2UA = (uw == GINT_TO_POINTER(2));
    const uint8_t *userData = NULL;
    int userLen = 0;
    int si = -1;

    // Not a valid v1 common header - this isn't M2UA/M3UA after all
    if (len < 8 || data[0] != 0x01)
        return ARKIME_PARSER_UNREGISTER;

    // Valid message but not payload data (ASP handshake, heartbeats,
    // management, ...) - skip it and keep the parser registered
    if (data[2] != (isM2UA ? M2UA_MSG_CLASS_MAUP : M3UA_MSG_CLASS_TRANSFER) ||
        data[3] != (isM2UA ? M2UA_MSG_TYPE_DATA : M3UA_MSG_TYPE_DATA))
        return 0;

    if (isM2UA) {
        // M2UA: parse M2UA header to get MTP3, then parse MTP3
        const uint8_t *mtp3Data = m3ua_parse_m2ua(data, len, &userLen);
        if (!mtp3Data)
            return ARKIME_PARSER_UNREGISTER;

        userData = m3ua_parse_mtp3(mtp3Data, userLen, &userLen, &si);
    } else {
        // M3UA: Protocol Data parameter contains SI directly
        userData = m3ua_parse_m3ua(data, len, &userLen, &si);
    }

    if (!userData || si < 0)
        return ARKIME_PARSER_UNREGISTER;

    // Record the Service Indicator
    arkime_field_int_add(siField, session, si);

    // Look up sub-parser by SI hex value
    char siHex[3];
    snprintf(siHex, sizeof(siHex), "%02x", si);
    ArkimeParserInfo_t *info = g_hash_table_lookup(subParsers, siHex);
    if (info && info->parserFunc) {
        return info->parserFunc(session, info->uw, userData, userLen, which);
    }

    return ARKIME_PARSER_UNREGISTER;
}

/******************************************************************************/
LOCAL void m3ua_classify_m2ua(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void UNUSED(*uw))
{
    if (len < 8 || arkime_session_has_protocol(session, "m2ua"))
        return;

    if (data[0] != 0x01)
        return;

    // The SCTP PPID already identifies M2UA; classify on any valid v1
    // message so associations starting with the ASP handshake are tagged
    arkime_session_add_protocol(session, "m2ua");
    arkime_parsers_register(session, m3ua_parser, GINT_TO_POINTER(2), NULL);
}

/******************************************************************************/
LOCAL void m3ua_classify_m3ua(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void UNUSED(*uw))
{
    if (len < 8 || arkime_session_has_protocol(session, "m3ua"))
        return;

    if (data[0] != 0x01)
        return;

    // The SCTP PPID already identifies M3UA; classify on any valid v1
    // message so associations starting with the ASP handshake are tagged
    arkime_session_add_protocol(session, "m3ua");
    arkime_parsers_register(session, m3ua_parser, GINT_TO_POINTER(3), NULL);
}

/******************************************************************************/
void arkime_parser_init()
{
    // Get sub-parsers table
    subParsers = arkime_parsers_get_sub("m3ua");

    // Register for M2UA (protocol ID 2) and M3UA (protocol ID 3)
    arkime_parsers_classifier_register_sctp_protocol("m2ua", NULL, 2, m3ua_classify_m2ua);
    arkime_parsers_classifier_register_sctp_protocol("m3ua", NULL, 3, m3ua_classify_m3ua);

    // Register protocol-only sub-parsers for known SI values
    arkime_parsers_register_sub("m3ua", "04", m3ua_protocol_parser, "tup");
    arkime_parsers_register_sub("m3ua", "05", m3ua_protocol_parser, "isup");

    siField = arkime_field_define("m3ua", "integer",
                                  "m3ua.si", "Service Indicator", "m3ua.si",
                                  "MTP3 Service Indicator",
                                  ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                  (char *)NULL);
}
