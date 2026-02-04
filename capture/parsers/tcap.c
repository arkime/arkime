/* tcap.c - SCCP/TCAP parser for SS7/SIGTRAN
 *
 * Handles SCCP messages (extracting point codes) and TCAP messages,
 * dispatching to sub-parsers based on application context OID (CAMEL, MAP, INAP, etc.)
 *
 * Registers as a sub-parser with m3ua for SI=03 (SCCP).
 *
 * Copyright 2024 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL GHashTable *subParsers;
LOCAL int opCodeField;
LOCAL int srcField;
LOCAL int dstField;

// SCCP Message Types
#define SCCP_MSG_UDT 0x09    // Unitdata

/******************************************************************************/
// Parse application context OID and return hex string
LOCAL int tcap_get_app_context(const uint8_t *data, int len, char *out, int outlen)
{
    BSB bsb;
    BSB_INIT(bsb, data, len);

    // Skip transaction portion tag and length (Begin=0x62, End=0x64, Continue=0x65)
    uint8_t tag = 0, lenByte = 0;
    BSB_IMPORT_u08(bsb, tag);
    if (tag != 0x62 && tag != 0x64 && tag != 0x65)
        return 0;
    BSB_IMPORT_u08(bsb, lenByte);
    if (lenByte & 0x80) {
        int numBytes = lenByte & 0x7f;
        BSB_IMPORT_skip(bsb, numBytes);
    }

    // Look for dialogue portion (tag 0x6B)
    while (BSB_REMAINING(bsb) >= 2) {
        BSB_IMPORT_u08(bsb, tag);
        BSB_IMPORT_u08(bsb, lenByte);

        int elemLen = lenByte;
        if (lenByte & 0x80) {
            int numBytes = lenByte & 0x7f;
            elemLen = 0;
            for (int i = 0; i < numBytes && BSB_REMAINING(bsb) > 0; i++) {
                uint8_t b = 0;
                BSB_IMPORT_u08(bsb, b);
                elemLen = (elemLen << 8) | b;
            }
        }

        if (BSB_IS_ERROR(bsb) || elemLen > BSB_REMAINING(bsb))
            return 0;

        // Dialogue portion (0x6B)
        if (tag == 0x6B && elemLen > 0) {
            // Search for OID 0x06 with prefix 04 00 00 01 (CAMEL/MAP prefix)
            // The structure is: External(0x28) -> context[0](0xA0) -> AARQ/AARE(0x60/0x61) -> context[1](0xA1) -> OID
            const uint8_t *dlgData = BSB_WORK_PTR(bsb);
            // Scan for OID tags in the dialogue portion
            for (int i = 0; i < elemLen - 8; i++) {
                if (dlgData[i] == 0x06) {
                    int oidLen = dlgData[i + 1];
                    if (oidLen > 0 && oidLen < 20 && i + 2 + oidLen <= elemLen) {
                        // Check if it looks like CAMEL/MAP/INAP OID (starts with 04 00 00 01)
                        if (oidLen >= 4 && dlgData[i + 2] == 0x04 && dlgData[i + 3] == 0x00 &&
                            dlgData[i + 4] == 0x00 && dlgData[i + 5] == 0x01) {
                            const uint8_t *oid = dlgData + i + 2;
                            if (oidLen * 2 >= outlen)
                                return 0;
                            arkime_sprint_hex_string(out, oid, oidLen);
                            return oidLen * 2;
                        }
                    }
                }
            }
        }

        // Component portion - stop looking for dialogue
        if (tag == 0x6C) {
            break;
        }

        BSB_IMPORT_skip(bsb, elemLen);
    }

    return 0;
}

/******************************************************************************/
// Parse SCCP and extract point codes, returns pointer to TCAP data
LOCAL const uint8_t *sccp_parse(ArkimeSession_t *session, const uint8_t *data, int len, int *userLen)
{
    if (len < 6)
        return NULL;

    uint8_t msgType = data[0];

    // Only handle UDT for now
    if (msgType != SCCP_MSG_UDT)
        return NULL;

    // UDT format: msgType(1) + class(1) + ptr1(1) + ptr2(1) + ptr3(1) + variable data
    uint8_t ptr1 = data[2];
    uint8_t ptr2 = data[3];
    uint8_t ptr3 = data[4];

    // Called Party address
    int calledOffset = 2 + ptr1;
    if (calledOffset < len) {
        uint8_t calledLen = data[calledOffset];
        if (calledOffset + 1 + calledLen <= len && calledLen >= 3) {
            uint8_t ai = data[calledOffset + 1];
            if (ai & 0x01) { // PC present
                uint16_t pc = data[calledOffset + 2] | (data[calledOffset + 3] << 8);
                arkime_field_int_add(dstField, session, pc);
            }
        }
    }

    // Calling Party address
    int callingOffset = 3 + ptr2;
    if (callingOffset < len) {
        uint8_t callingLen = data[callingOffset];
        if (callingOffset + 1 + callingLen <= len && callingLen >= 3) {
            uint8_t ai = data[callingOffset + 1];
            if (ai & 0x01) { // PC present
                uint16_t pc = data[callingOffset + 2] | (data[callingOffset + 3] << 8);
                arkime_field_int_add(srcField, session, pc);
            }
        }
    }

    // Data pointer
    int dataOffset = 4 + ptr3;
    if (dataOffset >= len)
        return NULL;

    uint8_t dataLen = data[dataOffset];
    if (dataOffset + 1 + dataLen > len)
        return NULL;

    *userLen = dataLen;
    return data + dataOffset + 1;
}

/******************************************************************************/
// Called by m3ua with MTP3 user data for SI=03 (SCCP)
LOCAL int sccp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int which)
{
    if (len < 6)
        return ARKIME_PARSER_UNREGISTER;

    // Parse SCCP to get TCAP user data
    int tcapLen = 0;
    const uint8_t *tcapData = sccp_parse(session, data, len, &tcapLen);
    if (!tcapData || tcapLen < 2)
        return ARKIME_PARSER_UNREGISTER;

    arkime_session_add_protocol(session, "sccp");

    // Try to extract application context OID from TCAP
    char appCtx[64];
    if (tcap_get_app_context(tcapData, tcapLen, appCtx, sizeof(appCtx)) > 0) {
        // Look up sub-parser by application context
        ArkimeParserInfo_t *info = g_hash_table_lookup(subParsers, appCtx);
        if (info && info->parserFunc) {
            // Pass opCodeField address as uw if none provided
            void *subUw = info->uw ? info->uw : &opCodeField;
            return info->parserFunc(session, subUw, tcapData, tcapLen, which);
        }
    }

    return 0;
}

/******************************************************************************/
void arkime_parser_init()
{
    subParsers = arkime_parsers_get_sub("tcap");

    // Register with m3ua for SI=03 (SCCP)
    arkime_parsers_register_sub("m3ua", "03", sccp_parser, NULL);

    srcField = arkime_field_define("sccp", "integer",
                                   "sccp.src", "Src Point Code", "sccp.src",
                                   "SCCP Source Point Code",
                                   ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                   (char *)NULL);

    dstField = arkime_field_define("sccp", "integer",
                                   "sccp.dst", "Dst Point Code", "sccp.dst",
                                   "SCCP Destination Point Code",
                                   ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                   (char *)NULL);

    opCodeField = arkime_field_define("tcap", "integer",
                                      "tcap.opcode", "Op Code", "tcap.opcode",
                                      "TCAP Operation Code",
                                      ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                      (char *)NULL);
}
