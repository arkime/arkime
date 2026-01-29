/* Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * STUN (Session Traversal Utilities for NAT) parser
 * RFC 5389
 */
#include "arkime.h"
#include <arpa/inet.h>

extern ArkimeConfig_t config;

LOCAL int messageTypeField;
LOCAL int mappedIpField;
LOCAL int mappedPortField;
LOCAL int xorMappedIpField;
LOCAL int xorMappedPortField;
LOCAL int usernameField;
LOCAL int softwareField;
LOCAL int realmField;
LOCAL int errorCodeField;

// STUN magic cookie
#define STUN_MAGIC_COOKIE 0x2112A442

// STUN message types
#define STUN_BINDING_REQUEST        0x0001
#define STUN_BINDING_RESPONSE       0x0101
#define STUN_BINDING_ERROR          0x0111
#define STUN_ALLOCATE_REQUEST       0x0003
#define STUN_ALLOCATE_RESPONSE      0x0103
#define STUN_ALLOCATE_ERROR         0x0113

// STUN attribute types
#define STUN_ATTR_MAPPED_ADDRESS    0x0001
#define STUN_ATTR_USERNAME          0x0006
#define STUN_ATTR_ERROR_CODE        0x0009
#define STUN_ATTR_REALM             0x0014
#define STUN_ATTR_XOR_MAPPED_ADDR   0x0020
#define STUN_ATTR_SOFTWARE          0x8022
#define STUN_ATTR_XOR_MAPPED_ADDR2  0x8020  // Legacy

/******************************************************************************/
LOCAL const char *stun_message_type_str(uint16_t type)
{
    switch (type) {
    case STUN_BINDING_REQUEST:
        return "Binding Request";
    case STUN_BINDING_RESPONSE:
        return "Binding Response";
    case STUN_BINDING_ERROR:
        return "Binding Error";
    case STUN_ALLOCATE_REQUEST:
        return "Allocate Request";
    case STUN_ALLOCATE_RESPONSE:
        return "Allocate Response";
    case STUN_ALLOCATE_ERROR:
        return "Allocate Error";
    default:
        return NULL;
    }
}

/******************************************************************************/
LOCAL void stun_parse_address(ArkimeSession_t *session, BSB *bsb, int attrLen,
                              int ipField, int portField, uint32_t xorMagic, const uint8_t *xorTxnId)
{
    if (attrLen < 8)
        return;

    BSB_IMPORT_skip(*bsb, 1);  // reserved

    uint8_t family = 0;
    BSB_IMPORT_u08(*bsb, family);

    uint16_t port = 0;
    BSB_IMPORT_u16(*bsb, port);

    if (xorMagic) {
        port ^= (xorMagic >> 16);
    }

    arkime_field_int_add(portField, session, port);

    if (family == 0x01) {  // IPv4
        uint32_t ip = 0;
        BSB_IMPORT_u32(*bsb, ip);

        if (xorMagic) {
            ip ^= xorMagic;
        }

        struct in_addr addr;
        addr.s_addr = htonl(ip);
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr, ipStr, sizeof(ipStr));
        arkime_field_ip_add_str(ipField, session, ipStr);
    } else if (family == 0x02 && attrLen >= 20) {  // IPv6
        uint8_t ip6[16];
        const uint8_t *ip6ptr;
        BSB_IMPORT_ptr(*bsb, ip6ptr, 16);
        if (BSB_IS_ERROR(*bsb))
            return;
        memcpy(ip6, ip6ptr, 16);

        if (xorMagic && xorTxnId) {
            // XOR with magic cookie (4 bytes) + transaction ID (12 bytes)
            uint8_t xorBytes[16];
            xorBytes[0] = (xorMagic >> 24) & 0xFF;
            xorBytes[1] = (xorMagic >> 16) & 0xFF;
            xorBytes[2] = (xorMagic >> 8) & 0xFF;
            xorBytes[3] = xorMagic & 0xFF;
            memcpy(xorBytes + 4, xorTxnId, 12);

            for (int i = 0; i < 16; i++) {
                ip6[i] ^= xorBytes[i];
            }
        }

        char ipStr[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, ip6, ipStr, sizeof(ipStr));
        arkime_field_ip_add_str(ipField, session, ipStr);
    }
}

/******************************************************************************/
LOCAL int stun_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    if (len < 20)
        return 0;

    BSB bsb;
    BSB_INIT(bsb, data, len);

    uint16_t msgType = 0;
    BSB_IMPORT_u16(bsb, msgType);

    uint16_t msgLen = 0;
    BSB_IMPORT_u16(bsb, msgLen);

    uint32_t magicCookie = 0;
    BSB_IMPORT_u32(bsb, magicCookie);

    // Save transaction ID for XOR-MAPPED-ADDRESS IPv6
    const uint8_t *txnId = BSB_WORK_PTR(bsb);
    BSB_IMPORT_skip(bsb, 12);

    // Verify it's STUN
    if (magicCookie != STUN_MAGIC_COOKIE) {
        // Could be classic STUN (RFC 3489) without magic cookie
        if (msgLen + 20 != len)
            return 0;
    }

    const char *typeStr = stun_message_type_str(msgType);
    if (typeStr) {
        arkime_field_string_add(messageTypeField, session, typeStr, -1, TRUE);
    }

    // Parse attributes
    while (BSB_REMAINING(bsb) >= 4) {
        uint16_t attrType = 0;
        BSB_IMPORT_u16(bsb, attrType);

        uint16_t attrLen = 0;
        BSB_IMPORT_u16(bsb, attrLen);

        if (attrLen > BSB_REMAINING(bsb))
            break;

        const uint8_t *attrStart = BSB_WORK_PTR(bsb);
        BSB attrBsb;
        BSB_INIT(attrBsb, attrStart, attrLen);

        switch (attrType) {
        case STUN_ATTR_MAPPED_ADDRESS:
            stun_parse_address(session, &attrBsb, attrLen, mappedIpField, mappedPortField, 0, NULL);
            break;

        case STUN_ATTR_XOR_MAPPED_ADDR:
        case STUN_ATTR_XOR_MAPPED_ADDR2:
            stun_parse_address(session, &attrBsb, attrLen, xorMappedIpField, xorMappedPortField,
                               STUN_MAGIC_COOKIE, txnId);
            break;

        case STUN_ATTR_USERNAME:
            if (attrLen > 0 && attrLen < 513) {
                arkime_field_string_add(usernameField, session, (char *)attrStart, attrLen, TRUE);
            }
            break;

        case STUN_ATTR_SOFTWARE:
            if (attrLen > 0 && attrLen < 763) {
                arkime_field_string_add(softwareField, session, (char *)attrStart, attrLen, TRUE);
            }
            break;

        case STUN_ATTR_REALM:
            if (attrLen > 0 && attrLen < 763) {
                arkime_field_string_add(realmField, session, (char *)attrStart, attrLen, TRUE);
            }
            break;

        case STUN_ATTR_ERROR_CODE:
            if (attrLen >= 4) {
                BSB_IMPORT_skip(attrBsb, 2);  // reserved
                uint8_t errClass = 0, errNumber = 0;
                BSB_IMPORT_u08(attrBsb, errClass);
                BSB_IMPORT_u08(attrBsb, errNumber);
                int errorCode = (errClass * 100) + errNumber;
                arkime_field_int_add(errorCodeField, session, errorCode);
            }
            break;
        }

        // Skip to next attribute (4-byte aligned)
        int padded = (attrLen + 3) & ~3;
        BSB_IMPORT_skip(bsb, padded);
    }

    return 0;
}

/******************************************************************************/
LOCAL void stun_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 20)
        return;

    if (arkime_session_has_protocol(session, "stun"))
        return;

    // Check for STUN magic cookie at offset 4
    if (memcmp(data + 4, "\x21\x12\xa4\x42", 4) != 0) {
        // Classic STUN check: message length should match
        uint16_t msgLen = (data[2] << 8) | data[3];
        if (msgLen + 20 != len)
            return;

        // Additional sanity check for classic STUN
        if (data[0] != 0x00 && data[0] != 0x01)
            return;
    }

    if (arkime_session_has_protocol(session, "stun"))
        return;

    arkime_session_add_protocol(session, "stun");
    arkime_parsers_register(session, stun_parser, NULL, NULL);
}

/******************************************************************************/
// Text-based RSP format: "RSP/x.x STUN..."
LOCAL void stun_rsp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "stun"))
        return;

    if (len > 7 && arkime_memstr((const char *)data + 7, len - 7, "STUN", 4)) {
        arkime_session_add_protocol(session, "stun");
    }
}

/******************************************************************************/
void arkime_parser_init()
{
    messageTypeField = arkime_field_define("stun", "termfield",
                                           "stun.type", "Message Type", "stun.type",
                                           "STUN message type",
                                           ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                           (char *)NULL);

    mappedIpField = arkime_field_define("stun", "ip",
                                        "stun.mapped-ip", "Mapped IP", "stun.mappedIp",
                                        "STUN MAPPED-ADDRESS IP",
                                        ARKIME_FIELD_TYPE_IP_GHASH, ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);

    mappedPortField = arkime_field_define("stun", "integer",
                                          "stun.mapped-port", "Mapped Port", "stun.mappedPort",
                                          "STUN MAPPED-ADDRESS port",
                                          ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                          (char *)NULL);

    xorMappedIpField = arkime_field_define("stun", "ip",
                                           "stun.xor-mapped-ip", "XOR Mapped IP", "stun.xorMappedIp",
                                           "STUN XOR-MAPPED-ADDRESS IP (external/NAT IP)",
                                           ARKIME_FIELD_TYPE_IP_GHASH, ARKIME_FIELD_FLAG_CNT,
                                           (char *)NULL);

    xorMappedPortField = arkime_field_define("stun", "integer",
                                             "stun.xor-mapped-port", "XOR Mapped Port", "stun.xorMappedPort",
                                             "STUN XOR-MAPPED-ADDRESS port",
                                             ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                             (char *)NULL);

    usernameField = arkime_field_define("stun", "termfield",
                                        "stun.username", "Username", "stun.username",
                                        "STUN username (ICE credentials)",
                                        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                        "category", "user",
                                        (char *)NULL);

    softwareField = arkime_field_define("stun", "termfield",
                                        "stun.software", "Software", "stun.software",
                                        "STUN software attribute",
                                        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);

    realmField = arkime_field_define("stun", "termfield",
                                     "stun.realm", "Realm", "stun.realm",
                                     "STUN realm",
                                     ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                     (char *)NULL);

    errorCodeField = arkime_field_define("stun", "integer",
                                         "stun.error", "Error Code", "stun.error",
                                         "STUN error code",
                                         ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                         (char *)NULL);

    // UDP classifiers for binary STUN
    arkime_parsers_classifier_register_udp("stun", NULL, 0, (uint8_t *)"\x00\x01\x00", 3, stun_classify);
    arkime_parsers_classifier_register_udp("stun", NULL, 0, (uint8_t *)"\x00\x03\x00", 3, stun_classify);
    arkime_parsers_classifier_register_udp("stun", NULL, 0, (uint8_t *)"\x01\x01\x00", 3, stun_classify);

    // UDP/TCP classifiers for text RSP format
    arkime_parsers_classifier_register_udp("stun", NULL, 0, (uint8_t *)"RSP/", 4, stun_rsp_classify);
    arkime_parsers_classifier_register_tcp("stun", NULL, 0, (uint8_t *)"RSP/", 4, stun_rsp_classify);
}
