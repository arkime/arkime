/* camel.c - CAMEL/CAP parser for SS7/SIGTRAN
 *
 * Handles TCAP messages containing CAP/CAMEL operations.
 * Registers as a sub-parser with tcap using CAMEL application context OIDs.
 *
 * Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL int camelOpField;
LOCAL int camelCallingField;
LOCAL int camelCalledField;
LOCAL int camelImsiField;

// CAP/CAMEL Operation Codes (from ETSI TS 101 046)
LOCAL const char *camel_op_names[] = {
    [0]  = "initialDP",
    [1]  = "assistRequestInstructions",
    [2]  = "establishTemporaryConnection",
    [3]  = "disconnectForwardConnection",
    [4]  = "connectToResource",
    [5]  = "connect",
    [6]  = "releaseCall",               // CAP v1
    [7]  = "requestReportBCSMEvent",    // CAP v1
    [8]  = "eventReportBCSM",           // CAP v1
    [9]  = "requestNotificationChargingEvent", // CAP v1
    [10] = "eventNotificationCharging", // CAP v1
    [11] = "cancel",                    // CAP v1
    [12] = "furnishChargingInformation",
    [13] = "applyCharging",             // CAP v1
    [14] = "applyChargingReport",       // CAP v1
    [16] = "callGap",
    [17] = "connect",                   // CAP v2+
    [20] = "continue",
    [21] = "initiateCallAttempt",
    [22] = "releaseCall",               // CAP v2+
    [23] = "requestReportBCSMEvent",    // CAP v2+
    [24] = "eventReportBCSM",           // CAP v2+
    [31] = "resetTimer",
    [32] = "sendChargingInformation",
    [33] = "connectToResource",         // CAP v2+
    [34] = "furnishChargingInformation",// CAP v2+
    [35] = "applyCharging",             // CAP v2+
    [36] = "applyChargingReport",       // CAP v2+
    [44] = "callInformationRequest",
    [45] = "callInformationReport",
    [46] = "playAnnouncement",
    [47] = "promptAndCollectUserInformation",
    [48] = "specializedResourceReport",
    [49] = "cancel",                    // CAP v2+
    [56] = "activityTest",
    [60] = "continueWithArgument",
    [61] = "disconnectLeg",
    [62] = "initiateSMS",               // SMS operations
    [63] = "releaseSMS",
    [64] = "connectSMS",
    [65] = "requestReportSMSEvent",
    [66] = "eventReportSMS",
    [67] = "continueSMS",
    [70] = "initialDPSMS",
    [71] = "furnishChargingInformationSMS",
    [80] = "initialDPGPRS",             // GPRS operations
    [81] = "requestReportGPRSEvent",
    [82] = "eventReportGPRS",
    [83] = "applyChargingGPRS",
    [84] = "applyChargingReportGPRS",
    [85] = "furnishChargingInformationGPRS",
    [86] = "cancelGPRS",
    [87] = "connectGPRS",
    [88] = "continueGPRS",
    [89] = "releaseGPRS",
    [90] = "resetTimerGPRS",
    [91] = "sendChargingInformationGPRS",
    [93] = "activityTestGPRS",
};

/******************************************************************************/
// Decode TBCD (Telephony BCD) to string
LOCAL void decode_tbcd(const uint8_t *data, int len, BSB *bsb)
{
    for (int i = 0; i < len && !BSB_IS_ERROR(*bsb) && BSB_REMAINING(*bsb) > 0; i++) {
        uint8_t lo = data[i] & 0x0f;
        uint8_t hi = (data[i] >> 4) & 0x0f;

        if (lo < 10) {
            BSB_EXPORT_u08(*bsb, '0' + lo);
        } else if (lo == 0x0a) {
            BSB_EXPORT_u08(*bsb, '*');
        } else if (lo == 0x0b) {
            BSB_EXPORT_u08(*bsb, '#');
        } else if (lo == 0x0f) {
            break;
        }

        if (hi < 10) {
            BSB_EXPORT_u08(*bsb, '0' + hi);
        } else if (hi == 0x0a) {
            BSB_EXPORT_u08(*bsb, '*');
        } else if (hi == 0x0b) {
            BSB_EXPORT_u08(*bsb, '#');
        } else if (hi == 0x0f) {
            break;
        }
    }
    BSB_EXPORT_u08(*bsb, 0);
}

/******************************************************************************/
// Parse CAP InitialDP parameters
LOCAL void camel_parse_cap_params(ArkimeSession_t *session, const uint8_t *data, int len)
{
    BSB bsb;
    BSB_INIT(bsb, data, len);
    char numBuf[32];
    BSB numbsb;

    while (BSB_REMAINING(bsb) >= 2) {
        int tag = 0;
        uint8_t tagByte = 0, tagLen = 0;
        BSB_IMPORT_u08(bsb, tagByte);

        if ((tagByte & 0x1f) == 0x1f) {
            tag = tagByte;
            do {
                if (BSB_IS_ERROR(bsb) || BSB_REMAINING(bsb) == 0)
                    return;
                BSB_IMPORT_u08(bsb, tagByte);
                tag = (tag << 8) | tagByte;
            } while (tagByte & 0x80);
        } else {
            tag = tagByte;
        }

        BSB_IMPORT_u08(bsb, tagLen);

        int lenVal = tagLen;
        if (tagLen & 0x80) {
            int numBytes = tagLen & 0x7f;
            lenVal = 0;
            for (int i = 0; i < numBytes && BSB_REMAINING(bsb) > 0; i++) {
                uint8_t b = 0;
                BSB_IMPORT_u08(bsb, b);
                lenVal = (lenVal << 8) | b;
            }
        }

        if (BSB_IS_ERROR(bsb) || lenVal > BSB_REMAINING(bsb))
            break;

        const uint8_t *valData = BSB_WORK_PTR(bsb);

        switch (tag) {
        case 0x82: // [2] calledPartyNumber
        case 0xA2:
            if (lenVal > 2) {
                int skip = (valData[0] & 0x80) ? 1 : 2;
                if (lenVal > skip) {
                    BSB_INIT(numbsb, numBuf, sizeof(numBuf));
                    decode_tbcd(valData + skip, lenVal - skip, &numbsb);
                    if (numBuf[0] && !BSB_IS_ERROR(numbsb))
                        arkime_field_string_add(camelCalledField, session, numBuf, -1, TRUE);
                }
            }
            break;

        case 0x83: // [3] callingPartyNumber
        case 0xA3:
            if (lenVal > 2) {
                int skip = (valData[0] & 0x80) ? 1 : 2;
                if (lenVal > skip) {
                    BSB_INIT(numbsb, numBuf, sizeof(numBuf));
                    decode_tbcd(valData + skip, lenVal - skip, &numbsb);
                    if (numBuf[0] && !BSB_IS_ERROR(numbsb))
                        arkime_field_string_add(camelCallingField, session, numBuf, -1, TRUE);
                }
            }
            break;

        case 0x8A: // [10] iMSI
        case 0xAA:
        case 0x9f32: // [50] iMSI (long form)
            if (lenVal >= 3 && lenVal <= 8) {
                BSB_INIT(numbsb, numBuf, sizeof(numBuf));
                decode_tbcd(valData, lenVal, &numbsb);
                if (numBuf[0] && !BSB_IS_ERROR(numbsb))
                    arkime_field_string_add(camelImsiField, session, numBuf, -1, TRUE);
            }
            break;
        }

        BSB_IMPORT_skip(bsb, lenVal);
    }
}

/******************************************************************************/
// Parse TCAP components and extract operation codes
LOCAL int camel_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int UNUSED(which))
{
    const int *opcodeField = uw;

    if (len < 2)
        return ARKIME_PARSER_UNREGISTER;

    int foundOpcode = 0;
    BSB bsb;
    BSB_INIT(bsb, data, len);

    // Skip transaction portion tag and length
    uint8_t tag = 0, lenByte = 0;
    BSB_IMPORT_u08(bsb, tag);
    BSB_IMPORT_u08(bsb, lenByte);
    if (lenByte & 0x80) {
        int numBytes = lenByte & 0x7f;
        BSB_IMPORT_skip(bsb, numBytes);
    }

    // Look for component portion (tag 0x6C)
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

        if (BSB_IS_ERROR(bsb))
            return ARKIME_PARSER_UNREGISTER;

        if (tag == 0x6C) {
            BSB compBsb;
            BSB_INIT(compBsb, BSB_WORK_PTR(bsb), MIN(elemLen, BSB_REMAINING(bsb)));

            while (BSB_REMAINING(compBsb) >= 4) {
                uint8_t compTag = 0;
                BSB_IMPORT_u08(compBsb, compTag);
                BSB_IMPORT_u08(compBsb, lenByte);

                int compLen = lenByte;
                if (lenByte & 0x80) {
                    int numBytes = lenByte & 0x7f;
                    compLen = 0;
                    for (int i = 0; i < numBytes && BSB_REMAINING(compBsb) > 0; i++) {
                        uint8_t b = 0;
                        BSB_IMPORT_u08(compBsb, b);
                        compLen = (compLen << 8) | b;
                    }
                }

                if (compTag == 0xA1 || compTag == 0xA2) {
                    BSB invBsb;
                    BSB_INIT(invBsb, BSB_WORK_PTR(compBsb), MIN(compLen, BSB_REMAINING(compBsb)));

                    int currentOpcode = -1;
                    while (BSB_REMAINING(invBsb) >= 3) {
                        uint8_t invTag = 0;
                        BSB_IMPORT_u08(invBsb, invTag);
                        BSB_IMPORT_u08(invBsb, lenByte);

                        if (invTag == 0x02 && lenByte >= 1 && lenByte <= 2) {
                            int opcode = 0;
                            for (int i = 0; i < lenByte && BSB_REMAINING(invBsb) > 0; i++) {
                                uint8_t b = 0;
                                BSB_IMPORT_u08(invBsb, b);
                                opcode = (opcode << 8) | b;
                            }
                            if (opcodeField)
                                arkime_field_int_add(*opcodeField, session, opcode);
                            if (opcode >= 0 && (size_t)opcode < ARRAY_LEN(camel_op_names) && camel_op_names[opcode]) {
                                arkime_field_string_add(camelOpField, session, camel_op_names[opcode], -1, TRUE);
                            }
                            currentOpcode = opcode;
                            foundOpcode = 1;
                        } else if (invTag == 0x30 && lenByte > 0) {
                            const uint8_t *caps = 0;
                            BSB_IMPORT_ptr(invBsb, caps, lenByte);
                            if (caps && (currentOpcode == 0 || currentOpcode == 70 || currentOpcode == 80)) {
                                camel_parse_cap_params(session, caps, lenByte);
                            }
                        } else {
                            BSB_IMPORT_skip(invBsb, lenByte);
                        }
                    }
                }

                BSB_IMPORT_skip(compBsb, compLen);
            }

            if (foundOpcode) {
                arkime_session_add_protocol(session, "camel");
            }
            return 0;
        }

        BSB_IMPORT_skip(bsb, elemLen);
    }
    return 0;
}

/******************************************************************************/
void arkime_parser_init()
{
    // Register for CAMEL application contexts with tcap
    // CAP v1: 0.4.0.0.1.0.50.0 = 04000001003200
    // CAP v2: 0.4.0.0.1.0.50.1 = 04000001003201
    // CAP v3: 0.4.0.0.1.21.3.4 = 04000001150304
    // CAP v4: 0.4.0.0.1.23.3.4 = 04000001170304
    arkime_parsers_register_sub("tcap", "04000001003200", camel_parser, NULL);
    arkime_parsers_register_sub("tcap", "04000001003201", camel_parser, NULL);
    arkime_parsers_register_sub("tcap", "04000001150304", camel_parser, NULL);
    arkime_parsers_register_sub("tcap", "04000001170304", camel_parser, NULL);

    camelOpField = arkime_field_define("camel", "lotermfield",
                                       "camel.op", "Operation", "camel.op",
                                       "CAP/CAMEL Operation Name",
                                       ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    camelCallingField = arkime_field_define("camel", "termfield",
                                            "camel.calling", "Calling", "camel.calling",
                                            "CAP Calling Party Number",
                                            ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                            (char *)NULL);

    camelCalledField = arkime_field_define("camel", "termfield",
                                           "camel.called", "Called", "camel.called",
                                           "CAP Called Party Number",
                                           ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                           (char *)NULL);

    camelImsiField = arkime_field_define("camel", "termfield",
                                         "camel.imsi", "IMSI", "camel.imsi",
                                         "CAP IMSI",
                                         ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                         (char *)NULL);
}
