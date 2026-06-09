/* Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * STUN (Session Traversal Utilities for NAT) and TURN parser
 * RFC 5389, RFC 5766
 */
#include "arkime.h"
#include <arpa/inet.h>
#include <sys/socket.h>

extern ArkimeConfig_t config;

LOCAL int messageTypeField;
LOCAL int mappedIpField;
LOCAL int mappedPortField;
LOCAL int xorMappedIpField;
LOCAL int xorMappedPortField;
LOCAL int xorRelayedIpField;
LOCAL int xorRelayedPortField;
LOCAL int xorPeerIpField;
LOCAL int xorPeerPortField;
LOCAL int usernameField;
LOCAL int softwareField;
LOCAL int realmField;
LOCAL int errorCodeField;
LOCAL int attributesField;

// STUN magic cookie
#define STUN_MAGIC_COOKIE 0x2112A442

// STUN/TURN message types (method | class)
// Class: 0x00=request, 0x10=indication, 0x100=success, 0x110=error
#define STUN_BINDING_REQUEST        0x0001
#define STUN_BINDING_RESPONSE       0x0101
#define STUN_BINDING_ERROR          0x0111
#define STUN_BINDING_INDICATION     0x0011

// TURN message types
#define TURN_ALLOCATE_REQUEST       0x0003
#define TURN_ALLOCATE_RESPONSE      0x0103
#define TURN_ALLOCATE_ERROR         0x0113
#define TURN_REFRESH_REQUEST        0x0004
#define TURN_REFRESH_RESPONSE       0x0104
#define TURN_REFRESH_ERROR          0x0114
#define TURN_SEND_INDICATION        0x0016
#define TURN_DATA_INDICATION        0x0017
#define TURN_CREATEPERM_REQUEST     0x0008
#define TURN_CREATEPERM_RESPONSE    0x0108
#define TURN_CREATEPERM_ERROR       0x0118
#define TURN_CHANNELBIND_REQUEST    0x0009
#define TURN_CHANNELBIND_RESPONSE   0x0109
#define TURN_CHANNELBIND_ERROR      0x0119

// STUN/TURN attribute types
#define STUN_ATTR_MAPPED_ADDRESS    0x0001
#define STUN_ATTR_USERNAME          0x0006
#define STUN_ATTR_ERROR_CODE        0x0009
#define STUN_ATTR_LIFETIME          0x000D
#define STUN_ATTR_XOR_PEER_ADDRESS  0x0012
#define STUN_ATTR_DATA              0x0013
#define STUN_ATTR_REALM             0x0014
#define STUN_ATTR_XOR_RELAYED_ADDR  0x0016
#define STUN_ATTR_REQ_TRANSPORT     0x0019
#define STUN_ATTR_XOR_MAPPED_ADDR   0x0020
#define STUN_ATTR_USE_CANDIDATE     0x0025
#define STUN_ATTR_SOFTWARE          0x8022
#define STUN_ATTR_FINGERPRINT       0x8028
#define STUN_ATTR_ICE_CONTROLLED    0x8029
#define STUN_ATTR_ICE_CONTROLLING   0x802A
#define STUN_ATTR_XOR_MAPPED_ADDR2  0x8020  // Legacy

// Message type lookup -- methods scattered above 9 (Connect/ConnectionBind/
// ConnectionAttempt for TURN-TCP per RFC 6062, and GooglePing for WebRTC) so
// use a switch rather than an indexed table.
LOCAL const char *stun_method_name(int method)
{
    switch (method) {
    case 0x01:
        return "Binding";
    case 0x03:
        return "Allocate";
    case 0x04:
        return "Refresh";
    case 0x06:
        return "Send";
    case 0x07:
        return "Data";
    case 0x08:
        return "CreatePermission";
    case 0x09:
        return "ChannelBind";
    case 0x0A:
        return "Connect";
    case 0x0B:
        return "ConnectionBind";
    case 0x0C:
        return "ConnectionAttempt";
    case 0x80:
        return "GooglePing";
    default:
        return NULL;
    }
}

LOCAL const char *stunClasses[4] = {
    "Request",
    "Indication",
    "Response",
    "Error"
};

/******************************************************************************/
// Returns TRUE if ip matches either endpoint of the session.
// ip points to 4 bytes (AF_INET) or 16 bytes (AF_INET6).
LOCAL gboolean stun_ip_matches_session(ArkimeSession_t *session, const uint8_t *ip, int family)
{
    if (family == AF_INET && ARKIME_SESSION_IS_v4(session)) {
        uint32_t v4 = *(const uint32_t *)ip;
        if (v4 == ARKIME_V6_TO_V4(session->addr1)) return TRUE;
        if (v4 == ARKIME_V6_TO_V4(session->addr2)) return TRUE;
    } else if (family == AF_INET6 && !ARKIME_SESSION_IS_v4(session)) {
        if (memcmp(ip, &session->addr1, 16) == 0) return TRUE;
        if (memcmp(ip, &session->addr2, 16) == 0) return TRUE;
    }
    return FALSE;
}
/******************************************************************************/
LOCAL void stun_parse_address(ArkimeSession_t *session, BSB *bsb, int attrLen,
                              int ipField, int portField, uint32_t xorMagic,
                              const uint8_t *xorTxnId, gboolean checkNat)
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

        uint32_t v4 = htonl(ip);
        arkime_field_ip4_add(ipField, session, v4);

        if (checkNat && !stun_ip_matches_session(session, (const uint8_t *)&v4, AF_INET))
            arkime_session_add_tag(session, "stun:nat-discovered");
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

        arkime_field_ip6_add(ipField, session, ip6);

        if (checkNat && !stun_ip_matches_session(session, ip6, AF_INET6))
            arkime_session_add_tag(session, "stun:nat-discovered");
    }
}

/******************************************************************************/
LOCAL int stun_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    if (len < 20)
        return ARKIME_PARSER_UNREGISTER;

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
            return ARKIME_PARSER_UNREGISTER;
    } else if (msgLen + 20 > len) {
        return ARKIME_PARSER_UNREGISTER;
    }

    // Re-init BSB to bound attribute parsing to the declared msgLen
    BSB_INIT(bsb, data + 20, msgLen);

    // Add message type using method/class lookup
    // Method: bits 0-3, 5-8, 11; Class: bits 4, 9
    int method = ((msgType & 0x000F) | ((msgType & 0x00E0) >> 1) | ((msgType & 0x3E00) >> 2));
    int msgClass = ((msgType & 0x0010) >> 4) | ((msgType & 0x0100) >> 7);
    const char *methodName = stun_method_name(method);
    if (methodName) {
        char typeStr[64];
        snprintf(typeStr, sizeof(typeStr), "%s %s", methodName, stunClasses[msgClass]);
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
            stun_parse_address(session, &attrBsb, attrLen, mappedIpField, mappedPortField, 0, NULL, FALSE);
            break;

        case STUN_ATTR_XOR_MAPPED_ADDR:
        case STUN_ATTR_XOR_MAPPED_ADDR2:
            stun_parse_address(session, &attrBsb, attrLen, xorMappedIpField, xorMappedPortField,
                               STUN_MAGIC_COOKIE, txnId, TRUE);
            break;

        case STUN_ATTR_XOR_RELAYED_ADDR:
            stun_parse_address(session, &attrBsb, attrLen, xorRelayedIpField, xorRelayedPortField,
                               STUN_MAGIC_COOKIE, txnId, FALSE);
            break;

        case STUN_ATTR_XOR_PEER_ADDRESS:
            stun_parse_address(session, &attrBsb, attrLen, xorPeerIpField, xorPeerPortField,
                               STUN_MAGIC_COOKIE, txnId, FALSE);
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

        case STUN_ATTR_USE_CANDIDATE:
            arkime_field_string_add(attributesField, session, "use-candidate", -1, TRUE);
            break;

        case STUN_ATTR_ICE_CONTROLLED:
            arkime_field_string_add(attributesField, session, "ice-controlled", -1, TRUE);
            break;

        case STUN_ATTR_ICE_CONTROLLING:
            arkime_field_string_add(attributesField, session, "ice-controlling", -1, TRUE);
            break;

        case STUN_ATTR_FINGERPRINT:
            arkime_field_string_add(attributesField, session, "fingerprint", -1, TRUE);
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

    arkime_session_add_protocol(session, "stun");
    arkime_parsers_register(session, stun_parser, NULL, NULL);
}

/******************************************************************************/
LOCAL int stun_tcp_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int which)
{
    ArkimeParserBuf_t *pb = uw;

    if (arkime_parser_buf_add(pb, which, data, len) < 0) {
        arkime_parsers_unregister(session, pb);
        return 0;
    }

    while (pb->len[which] >= 20) {
        uint16_t msgLen = (pb->buf[which][2] << 8) | pb->buf[which][3];

        if (msgLen + 20 > pb->bufMax) {
            arkime_session_add_tag(session, "stun:message-too-long");
            return ARKIME_PARSER_UNREGISTER;
        }

        if (pb->len[which] < msgLen + 20)
            return 0;

        stun_parser(session, NULL, pb->buf[which], msgLen + 20, which);
        arkime_parser_buf_del(pb, which, msgLen + 20);
    }
    return 0;
}

/******************************************************************************/
LOCAL void stun_tcp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 20)
        return;

    if (arkime_session_has_protocol(session, "stun"))
        return;

    // Check for STUN magic cookie at offset 4
    if (memcmp(data + 4, "\x21\x12\xa4\x42", 4) != 0)
        return;

    // Verify first 2 bits are 0 (required for STUN)
    if ((data[0] & 0xC0) != 0)
        return;

    arkime_session_add_protocol(session, "stun");
    ArkimeParserBuf_t *pb = arkime_parser_buf_create();
    arkime_parsers_register(session, stun_tcp_parser, pb, arkime_parser_buf_session_free);
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

    xorRelayedIpField = arkime_field_define("stun", "ip",
                                            "stun.xor-relayed-ip", "XOR Relayed IP", "stun.xorRelayedIp",
                                            "TURN XOR-RELAYED-ADDRESS IP (relay server allocated address)",
                                            ARKIME_FIELD_TYPE_IP_GHASH, ARKIME_FIELD_FLAG_CNT,
                                            (char *)NULL);

    xorRelayedPortField = arkime_field_define("stun", "integer",
                                              "stun.xor-relayed-port", "XOR Relayed Port", "stun.xorRelayedPort",
                                              "TURN XOR-RELAYED-ADDRESS port",
                                              ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                              (char *)NULL);

    xorPeerIpField = arkime_field_define("stun", "ip",
                                         "stun.xor-peer-ip", "XOR Peer IP", "stun.xorPeerIp",
                                         "TURN XOR-PEER-ADDRESS IP (remote peer relayed through TURN server)",
                                         ARKIME_FIELD_TYPE_IP_GHASH, ARKIME_FIELD_FLAG_CNT,
                                         (char *)NULL);

    xorPeerPortField = arkime_field_define("stun", "integer",
                                           "stun.xorPeerPort", "XOR Peer Port", "stun.xorPeerPort",
                                           "TURN XOR-PEER-ADDRESS port",
                                           ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                           "category", "port",
                                           (char *)NULL);

    attributesField = arkime_field_define("stun", "termfield",
                                          "stun.attributes", "Attributes", "stun.attributes",
                                          "STUN/TURN protocol attributes/flags (fingerprint, ice-controlled, ice-controlling, use-candidate)",
                                          ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                          (char *)NULL);

    // UDP classifiers for binary STUN
    arkime_parsers_classifier_register_udp("stun", NULL, 0, (uint8_t *)"\x00\x01\x00", 3, stun_classify);
    arkime_parsers_classifier_register_udp("stun", NULL, 0, (uint8_t *)"\x00\x03\x00", 3, stun_classify);
    arkime_parsers_classifier_register_udp("stun", NULL, 0, (uint8_t *)"\x01\x01\x00", 3, stun_classify);

    // TCP classifiers for binary STUN/TURN
    arkime_parsers_classifier_register_tcp("stun", NULL, 0, (uint8_t *)"\x00\x01\x00", 3, stun_tcp_classify);
    arkime_parsers_classifier_register_tcp("stun", NULL, 0, (uint8_t *)"\x00\x03\x00", 3, stun_tcp_classify);
    arkime_parsers_classifier_register_tcp("stun", NULL, 0, (uint8_t *)"\x00\x04\x00", 3, stun_tcp_classify);
    arkime_parsers_classifier_register_tcp("stun", NULL, 0, (uint8_t *)"\x00\x08\x00", 3, stun_tcp_classify);
    arkime_parsers_classifier_register_tcp("stun", NULL, 0, (uint8_t *)"\x00\x09\x00", 3, stun_tcp_classify);
    arkime_parsers_classifier_register_tcp("stun", NULL, 0, (uint8_t *)"\x01\x01\x00", 3, stun_tcp_classify);
    arkime_parsers_classifier_register_tcp("stun", NULL, 0, (uint8_t *)"\x01\x03\x00", 3, stun_tcp_classify);

    // UDP/TCP classifiers for text RSP format
    arkime_parsers_classifier_register_udp("stun", NULL, 0, (uint8_t *)"RSP/", 4, stun_rsp_classify);
    arkime_parsers_classifier_register_tcp("stun", NULL, 0, (uint8_t *)"RSP/", 4, stun_rsp_classify);
}
