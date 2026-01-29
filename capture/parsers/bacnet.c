/* Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

/*
 * BACnet/IP (Building Automation and Control Networks)
 * BVLC (BACnet Virtual Link Control) header followed by NPDU and optionally APDU
 *
 * BVLC Header (4 bytes minimum):
 *   Type (1 byte): 0x81 = BACnet/IP, 0x82 = BACnet/IPv6
 *   Function (1 byte): See bvlc_functions below
 *   Length (2 bytes): Total length including BVLC header
 *
 * References:
 *   - ASHRAE 135 (BACnet standard)
 *   - http://www.bacnet.org
 */

#define BVLC_TYPE_BACNET_IP     0x81
#define BVLC_TYPE_BACNET_IPV6   0x82

// BVLC Functions for BACnet/IP
#define BVLC_RESULT                         0x00
#define BVLC_WRITE_BDT                      0x01
#define BVLC_READ_BDT                       0x02
#define BVLC_READ_BDT_ACK                   0x03
#define BVLC_FORWARDED_NPDU                 0x04
#define BVLC_REGISTER_FOREIGN_DEVICE        0x05
#define BVLC_READ_FDT                       0x06
#define BVLC_READ_FDT_ACK                   0x07
#define BVLC_DELETE_FDT_ENTRY               0x08
#define BVLC_DISTRIBUTE_BROADCAST           0x09
#define BVLC_ORIGINAL_UNICAST               0x0a
#define BVLC_ORIGINAL_BROADCAST             0x0b
#define BVLC_SECURED_BVLL                   0x0c

// NPDU Control flags
#define NPDU_CONTROL_DNET_PRESENT           0x20
#define NPDU_CONTROL_SNET_PRESENT           0x08

// APDU Types (high nibble)
#define APDU_TYPE_CONFIRMED_REQ             0x00
#define APDU_TYPE_UNCONFIRMED_REQ           0x10
#define APDU_TYPE_SIMPLE_ACK                0x20
#define APDU_TYPE_COMPLEX_ACK               0x30
#define APDU_TYPE_SEGMENT_ACK               0x40
#define APDU_TYPE_ERROR                     0x50
#define APDU_TYPE_REJECT                    0x60
#define APDU_TYPE_ABORT                     0x70

LOCAL const char *bvlcFunctions[] = {
    [0x00] = "result",
    [0x01] = "write-bdt",
    [0x02] = "read-bdt",
    [0x03] = "read-bdt-ack",
    [0x04] = "forwarded-npdu",
    [0x05] = "register-foreign-device",
    [0x06] = "read-fdt",
    [0x07] = "read-fdt-ack",
    [0x08] = "delete-fdt-entry",
    [0x09] = "distribute-broadcast",
    [0x0a] = "original-unicast",
    [0x0b] = "original-broadcast",
    [0x0c] = "secured-bvll"
};

LOCAL const char *apduServices[] = {
    // Confirmed services
    [0]  = "acknowledge-alarm",
    [1]  = "confirmed-cov-notification",
    [2]  = "confirmed-event-notification",
    [3]  = "get-alarm-summary",
    [4]  = "get-enrollment-summary",
    [5]  = "subscribe-cov",
    [6]  = "atomic-read-file",
    [7]  = "atomic-write-file",
    [8]  = "add-list-element",
    [9]  = "remove-list-element",
    [10] = "create-object",
    [11] = "delete-object",
    [12] = "read-property",
    [13] = "read-property-conditional",
    [14] = "read-property-multiple",
    [15] = "write-property",
    [16] = "write-property-multiple",
    [17] = "device-communication-control",
    [18] = "confirmed-private-transfer",
    [19] = "confirmed-text-message",
    [20] = "reinitialize-device",
    [21] = "vt-open",
    [22] = "vt-close",
    [23] = "vt-data",
    [24] = "authenticate",
    [25] = "request-key",
    [26] = "read-range",
    [27] = "life-safety-operation",
    [28] = "subscribe-cov-property",
    [29] = "get-event-information"
};

LOCAL const char *unconfirmedServices[] = {
    [0] = "i-am",
    [1] = "i-have",
    [2] = "unconfirmed-cov-notification",
    [3] = "unconfirmed-event-notification",
    [4] = "unconfirmed-private-transfer",
    [5] = "unconfirmed-text-message",
    [6] = "time-synchronization",
    [7] = "who-has",
    [8] = "who-is",
    [9] = "utc-time-synchronization"
};

extern ArkimeConfig_t        config;
LOCAL  int functionField;
LOCAL  int serviceField;
LOCAL  int apduTypeField;
LOCAL  int objectNameField;

/******************************************************************************/
// Parse BACnet CharacterString from remaining data and add to field
LOCAL void bacnet_parse_object_name(ArkimeSession_t *session, BSB *bsb)
{
    // Scan for CharacterString tags:
    // - Application tag 7 (0x7x where x is length, or 0x75 for extended length)
    // - Context tag 7 for object-name in Who-Has (0x3d for extended length)
    while (BSB_REMAINING(*bsb) >= 3) {
        uint8_t tag = 0;
        BSB_IMPORT_u08(*bsb, tag);

        // Check for CharacterString:
        // Application tag 7: 0x70-0x77 (high nibble 0x7)
        // Context tag 7: 0x38-0x3F (context 7, primitive or constructed)
        uint8_t isCharString = 0;
        uint8_t lenByte = tag & 0x07;

        if ((tag & 0xF8) == 0x70) {
            // Application tag 7 (CharacterString)
            isCharString = 1;
        } else if ((tag & 0xF8) == 0x38) {
            // Context tag 7 (often used for object-name in requests)
            isCharString = 1;
        }

        if (isCharString) {
            uint32_t strLen = 0;

            if (lenByte == 5) {
                // Extended length - next byte is length
                if (BSB_REMAINING(*bsb) < 1) return;
                uint8_t extLen = 0;
                BSB_IMPORT_u08(*bsb, extLen);
                strLen = extLen;
            } else if (lenByte < 5) {
                strLen = lenByte;
            } else {
                // lenByte 6 or 7 means opening/closing tag
                continue;
            }

            if (strLen < 1 || BSB_REMAINING(*bsb) < strLen)
                return;

            // First byte is character set (0 = UTF-8/ANSI)
            BSB_IMPORT_skip(*bsb, 1);  // charset
            strLen--;

            if (strLen > 0 && strLen <= BSB_REMAINING(*bsb)) {
                const uint8_t *str = BSB_WORK_PTR(*bsb);
                arkime_field_string_add(objectNameField, session, (const char *)str, strLen, TRUE);
            }
            return;
        }
    }
}
LOCAL int bacnet_udp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    BSB bsb;
    BSB_INIT(bsb, data, len);

    if (BSB_REMAINING(bsb) < 4)
        return 0;

    // BVLC Header
    uint8_t bvlcType = 0, bvlcFunction = 0;
    uint16_t bvlcLength = 0;

    BSB_IMPORT_u08(bsb, bvlcType);
    BSB_IMPORT_u08(bsb, bvlcFunction);
    BSB_IMPORT_u16(bsb, bvlcLength);

    if (BSB_IS_ERROR(bsb))
        return 0;

    // Validate length
    if (bvlcLength < 4 || bvlcLength > len)
        return 0;

    // Validate BVLC type
    if (bvlcType != BVLC_TYPE_BACNET_IP && bvlcType != BVLC_TYPE_BACNET_IPV6)
        return 0;

    // Add BVLC function
    if (bvlcFunction < ARRAY_LEN(bvlcFunctions) && bvlcFunctions[bvlcFunction]) {
        arkime_field_string_add(functionField, session, bvlcFunctions[bvlcFunction], -1, TRUE);
    }

    // Handle forwarded NPDU - skip the forwarding address (6 bytes for IPv4)
    if (bvlcFunction == BVLC_FORWARDED_NPDU) {
        if (bvlcType == BVLC_TYPE_BACNET_IP) {
            BSB_IMPORT_skip(bsb, 6);  // 4 bytes IP + 2 bytes port
        } else {
            BSB_IMPORT_skip(bsb, 18); // 16 bytes IPv6 + 2 bytes port
        }
    }

    // Only parse NPDU for functions that contain it
    if (bvlcFunction != BVLC_ORIGINAL_UNICAST &&
        bvlcFunction != BVLC_ORIGINAL_BROADCAST &&
        bvlcFunction != BVLC_FORWARDED_NPDU &&
        bvlcFunction != BVLC_DISTRIBUTE_BROADCAST) {
        return 0;
    }

    if (BSB_REMAINING(bsb) < 2)
        return 0;

    // NPDU Header
    uint8_t npduVersion = 0, npduControl = 0;
    BSB_IMPORT_u08(bsb, npduVersion);
    BSB_IMPORT_u08(bsb, npduControl);

    if (BSB_IS_ERROR(bsb) || npduVersion != 1)
        return 0;

    // Skip destination network info if present
    if (npduControl & NPDU_CONTROL_DNET_PRESENT) {
        uint8_t dlen = 0;
        BSB_IMPORT_skip(bsb, 2);  // DNET
        BSB_IMPORT_u08(bsb, dlen);
        BSB_IMPORT_skip(bsb, dlen);  // Skip DADR
        if (BSB_IS_ERROR(bsb))
            return 0;
    }

    // Skip source network info if present
    if (npduControl & NPDU_CONTROL_SNET_PRESENT) {
        uint8_t slen = 0;
        BSB_IMPORT_skip(bsb, 2);  // SNET
        BSB_IMPORT_u08(bsb, slen);
        BSB_IMPORT_skip(bsb, slen);  // Skip SADR
        if (BSB_IS_ERROR(bsb))
            return 0;
    }

    // Skip hop count if destination present
    if (npduControl & NPDU_CONTROL_DNET_PRESENT) {
        BSB_IMPORT_skip(bsb, 1);
    }

    // Check if this is a network layer message (no APDU)
    if (npduControl & 0x80)
        return 0;

    if (BSB_REMAINING(bsb) < 1)
        return 0;

    // APDU
    uint8_t apduTypeByte = 0;
    BSB_IMPORT_u08(bsb, apduTypeByte);

    uint8_t apduType = apduTypeByte & 0xF0;
    const char *apduTypeStr = NULL;

    switch (apduType) {
    case APDU_TYPE_CONFIRMED_REQ:
        apduTypeStr = "confirmed-request";
        break;
    case APDU_TYPE_UNCONFIRMED_REQ:
        apduTypeStr = "unconfirmed-request";
        break;
    case APDU_TYPE_SIMPLE_ACK:
        apduTypeStr = "simple-ack";
        break;
    case APDU_TYPE_COMPLEX_ACK:
        apduTypeStr = "complex-ack";
        break;
    case APDU_TYPE_SEGMENT_ACK:
        apduTypeStr = "segment-ack";
        break;
    case APDU_TYPE_ERROR:
        apduTypeStr = "error";
        break;
    case APDU_TYPE_REJECT:
        apduTypeStr = "reject";
        break;
    case APDU_TYPE_ABORT:
        apduTypeStr = "abort";
        break;
    }

    if (apduTypeStr) {
        arkime_field_string_add(apduTypeField, session, apduTypeStr, -1, TRUE);
    }

    // Parse service for confirmed and unconfirmed requests
    if (apduType == APDU_TYPE_CONFIRMED_REQ) {
        // Skip max segments/max response, invoke ID
        uint8_t flags = apduTypeByte & 0x0F;
        if (flags & 0x08) BSB_IMPORT_skip(bsb, 1);  // Segmented
        BSB_IMPORT_skip(bsb, 2);  // Max segs/resp + invoke ID

        if (BSB_REMAINING(bsb) >= 1) {
            uint8_t serviceChoice = 0;
            BSB_IMPORT_u08(bsb, serviceChoice);
            if (serviceChoice < ARRAY_LEN(apduServices) && apduServices[serviceChoice]) {
                arkime_field_string_add(serviceField, session, apduServices[serviceChoice], -1, TRUE);
            }
        }
    } else if (apduType == APDU_TYPE_UNCONFIRMED_REQ) {
        if (BSB_REMAINING(bsb) >= 1) {
            uint8_t serviceChoice = 0;
            BSB_IMPORT_u08(bsb, serviceChoice);
            if (serviceChoice < ARRAY_LEN(unconfirmedServices) && unconfirmedServices[serviceChoice]) {
                arkime_field_string_add(serviceField, session, unconfirmedServices[serviceChoice], -1, TRUE);
            }
            // I-Am (0), I-Have (1), and Who-Has (7) contain object names
            if (serviceChoice <= 1 || serviceChoice == 7) {
                bacnet_parse_object_name(session, &bsb);
            }
        }
    } else if (apduType == APDU_TYPE_COMPLEX_ACK) {
        // Complex ACK may contain object names in ReadProperty responses
        BSB_IMPORT_skip(bsb, 1);  // Invoke ID
        if (BSB_REMAINING(bsb) >= 1) {
            uint8_t serviceChoice = 0;
            BSB_IMPORT_u08(bsb, serviceChoice);
            // ReadProperty (12) or ReadPropertyMultiple (14) responses may have object names
            if (serviceChoice == 12 || serviceChoice == 14) {
                bacnet_parse_object_name(session, &bsb);
            }
        }
    }

    return 0;
}
/******************************************************************************/
LOCAL void bacnet_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "bacnet"))
        return;

    // Minimum BVLC header is 4 bytes
    if (len < 4)
        return;

    // Validate BVLC type
    if (data[0] != BVLC_TYPE_BACNET_IP && data[0] != BVLC_TYPE_BACNET_IPV6)
        return;

    // Validate length field matches packet length
    uint16_t bvlcLen = (data[2] << 8) | data[3];
    if (bvlcLen != len)
        return;

    // Validate function is in known range
    if (data[1] > 0x0c)
        return;

    arkime_session_add_protocol(session, "bacnet");
    arkime_parsers_register(session, bacnet_udp_parser, 0, 0);
}
/******************************************************************************/
void arkime_parser_init()
{
    functionField = arkime_field_define("bacnet", "termfield",
                                        "bacnet.function", "Function", "bacnet.function",
                                        "BACnet BVLC function",
                                        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);

    serviceField = arkime_field_define("bacnet", "termfield",
                                       "bacnet.service", "Service", "bacnet.service",
                                       "BACnet APDU service",
                                       ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    apduTypeField = arkime_field_define("bacnet", "termfield",
                                        "bacnet.apdu-type", "APDU Type", "bacnet.apduType",
                                        "BACnet APDU type",
                                        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);

    objectNameField = arkime_field_define("bacnet", "termfield",
                                          "bacnet.object-name", "Object Name", "bacnet.objectName",
                                          "BACnet object name",
                                          ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                          (char *)NULL);

    // BACnet/IP uses UDP port 47808 (0xBAC0) by default
    // Classify based on BVLC type byte
    arkime_parsers_classifier_register_udp("bacnet", NULL, 0, (const uint8_t *)"\x81", 1, bacnet_udp_classify);
    arkime_parsers_classifier_register_udp("bacnet", NULL, 0, (const uint8_t *)"\x82", 1, bacnet_udp_classify);
}
