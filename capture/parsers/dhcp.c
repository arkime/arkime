/* Copyright 2018, Oath Inc.. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <arpa/inet.h>
#include <netinet/udp.h>

extern ArkimeConfig_t        config;

LOCAL int dhcpMProtocol;
LOCAL int dhcpv6MProtocol;

LOCAL int typeField;
LOCAL int hostField;
LOCAL int macField;
LOCAL int ouiField;
LOCAL int idField;
LOCAL int classIdField;
LOCAL int requestIpField;

LOCAL int dhcp_packet_func;

LOCAL const char *const namesv6[] = {
    "",
    "SOLICIT",
    "ADVERTISE",
    "REQUEST",
    "CONFIRM",
    "RENEW",
    "REBIND",
    "REPLY",
    "RELEASE",
    "DECLINE",
    "RECONFIGURE",
    "INFORMATION_REQUEST",
    "RELAY_FORW",
    "RELAY_REPL",
    "LEASEQUERY",
    "LEASEQUERY_REPLY",
    "LEASEQUERY_DONE",
    "LEASEQUERY_DATA",
    "LEASEQUERY_NO_DATA",
    "LEASEQUERY_STATUS",
    "LEASEQUERY_RECONF",
    "LEASEQUERY_RECONF_REPLY"
};

LOCAL char *const namesv4[] = {
    "",
    "DISCOVER",
    "OFFER",
    "REQUEST",
    "DECLINE",
    "ACK",
    "NAK",
    "RELEASE",
    "INFORM",
    "FORCERENEW",
    "LEASEQUERY",
    "LEASEUNASSIGNED",
    "LEASEUNKNOWN",
    "LEASEACTIVE",
    "BULKLEASEQUERY",
    "LEASEQUERYDONE",
    "ACTIVELEASEQUERY",
    "LEASEQUERYSTATUS",
    "TLS"
};

/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL void dhcpv6_create_sessionid(uint8_t *sessionId, ArkimePacket_t *packet)
{
    const uint8_t *data = packet->pkt + packet->payloadOffset;

    sessionId[0] = 8;
    sessionId[1] = dhcpv6MProtocol;
    memcpy(sessionId + 2, data + 1, 3);
    sessionId[5] = sessionId[6] = sessionId[7] = 0;
}
/******************************************************************************/
LOCAL int dhcpv6_pre_process(ArkimeSession_t *session, ArkimePacket_t *const packet, int isNewSession)
{
    const struct udphdr *udphdr = (struct udphdr *)(packet->pkt + packet->payloadOffset - 8);

    if (isNewSession) {
        session->port1 = ntohs(udphdr->uh_sport);
        session->port2 = ntohs(udphdr->uh_dport);
        arkime_session_add_protocol(session, "udp");
        arkime_session_add_protocol(session, "dhcpv6");
    }

    // Determine direction based on ports matching session->port1/port2
    packet->direction = (session->port1 == ntohs(udphdr->uh_sport) &&
                         session->port2 == ntohs(udphdr->uh_dport)) ? 0 : 1;

    // Count databytes
    session->databytes[packet->direction] += packet->payloadLen;

    return 0;
}
/******************************************************************************/
LOCAL int dhcpv6_process(ArkimeSession_t *session, ArkimePacket_t *const packet)
{
    const uint8_t *data = packet->pkt + packet->payloadOffset;
    int len = packet->payloadLen;

    if (len < 4 || data[0] == 0 || data[0] >= ARRAY_LEN(namesv6))
        return 0;

    arkime_field_string_add(typeField, session, namesv6[data[0]], -1, TRUE);

    // DHCPv6 transaction ID
    char str[100];
    uint32_t id = (data[1] << 16) | (data[2] << 8) | data[3];
    snprintf(str, sizeof(str), "%x", id);
    arkime_field_string_add(idField, session, str, -1, TRUE);

    // Parse DHCPv6 options
    BSB bsb;
    BSB_INIT(bsb, data + 4, len - 4);
    while (BSB_REMAINING(bsb) >= 4) {
        uint16_t optionCode = 0;
        uint16_t optionLen = 0;
        uint8_t *optionData = 0;
        BSB_IMPORT_u16(bsb, optionCode);
        BSB_IMPORT_u16(bsb, optionLen);
        if (BSB_IS_ERROR(bsb) || optionLen > BSB_REMAINING(bsb))
            break;
        switch (optionCode) {
        case 16: // Vendor Class
            BSB_IMPORT_ptr(bsb, optionData, optionLen);
            if (optionData && optionLen > 0)
                arkime_field_string_add(classIdField, session, (char *)optionData, optionLen, TRUE);
            break;
        default:
            BSB_IMPORT_skip(bsb, optionLen);
            break;
        }
    }

    arkime_parsers_call_named_func(dhcp_packet_func, session, data, len, NULL);

    return 1;
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL void dhcp_create_sessionid(uint8_t *sessionId, ArkimePacket_t *packet)
{
    const uint8_t *data = packet->pkt + packet->payloadOffset;

    sessionId[0] = 8;
    sessionId[1] = dhcpMProtocol;
    memcpy(sessionId + 2, data + 28, 6);   // Copy 6-byte client MAC address
}
/******************************************************************************/
LOCAL int dhcp_pre_process(ArkimeSession_t *session, ArkimePacket_t *const packet, int isNewSession)
{
    const struct udphdr *udphdr = (struct udphdr *)(packet->pkt + packet->payloadOffset - 8);

    if (isNewSession) {
        session->port1 = ntohs(udphdr->uh_sport);
        session->port2 = ntohs(udphdr->uh_dport);
        arkime_session_add_protocol(session, "udp");
        arkime_session_add_protocol(session, "dhcp");
    }

    // Determine direction based on ports matching session->port1/port2
    packet->direction = (session->port1 == ntohs(udphdr->uh_sport) &&
                         session->port2 == ntohs(udphdr->uh_dport)) ? 0 : 1;

    // Count databytes (DHCP payload length)
    session->databytes[packet->direction] += packet->payloadLen;

    return 0;
}
/******************************************************************************/
LOCAL int dhcp_process(ArkimeSession_t *session, ArkimePacket_t *const packet)
{
    const uint8_t *data = packet->pkt + packet->payloadOffset;
    int len = packet->payloadLen;

    if (len < 256)
        return 0;

    BSB bsb;
    BSB_INIT(bsb, data, len);

    int hardwareType = data[1];
    if (hardwareType == 1) {
        arkime_field_macoui_add(session, macField, ouiField, data + 28);
    }

    char str[100];
    uint32_t id = 0;
    BSB_IMPORT_skip(bsb, 4);
    BSB_IMPORT_u32(bsb, id);
    snprintf(str, sizeof(str), "%x", id);
    arkime_field_string_add(idField, session, str, -1, TRUE);

    // 236 offset + magic len - 4 skip - u32 import
    BSB_IMPORT_skip(bsb, 236 + 4 - 4 - 4);
    while (BSB_REMAINING(bsb) >= 2) {
        int t = 0;
        int l = 0;
        uint32_t value = 0;
        uint8_t *valueStr = 0;
        BSB_IMPORT_u08(bsb, t);
        if (t == 255) // End Tag, no length
            break;
        BSB_IMPORT_u08(bsb, l);
        if (BSB_IS_ERROR(bsb) || l > BSB_REMAINING(bsb) || l == 0)
            break;
        switch (t) {
        case 12: // Host Name
            BSB_IMPORT_ptr(bsb, valueStr, l);
            arkime_field_string_add_lower(hostField, session, (char *)valueStr, l);
            break;
        case 53: // Message Type
            if (l == 1) {
                BSB_IMPORT_u08(bsb, value);
                if (value > 0 && value < ARRAY_LEN(namesv4))
                    arkime_field_string_add(typeField, session, namesv4[value], -1, TRUE);
            } else {
                BSB_IMPORT_skip(bsb, l);
            }
            break;
        case 60: // Vendor Class Identifier
            BSB_IMPORT_ptr(bsb, valueStr, l);
            if (valueStr)
                arkime_field_string_add(classIdField, session, (char *)valueStr, l, TRUE);
            break;
        case 50: // Requested IP Address
            if (l == 4) {
                BSB_IMPORT_ptr(bsb, valueStr, 4);
                if (valueStr)
                    arkime_field_ip4_add(requestIpField, session, *(uint32_t *)valueStr);
            } else {
                BSB_IMPORT_skip(bsb, l);
            }
            break;
        case 61: // Client identifier
            BSB_IMPORT_u08(bsb, value);
            if (l == 7 && value == 1) {
                BSB_IMPORT_ptr(bsb, valueStr, 6);
                if (valueStr)
                    arkime_field_macoui_add(session, macField, ouiField, valueStr);
            } else {
                BSB_IMPORT_skip(bsb, l - 1);
            }
            break;
        case 81: // FQDN
            if (l < 3) {
                BSB_IMPORT_skip(bsb, l);
                break;
            }
            BSB_IMPORT_u08(bsb, value);
            BSB_IMPORT_skip(bsb, 2);
            if (value != 0) // Don't support any encodings right now
                BSB_IMPORT_skip(bsb, l - 1);
            else {
                BSB_IMPORT_ptr(bsb, valueStr, l - 3);
                arkime_field_string_add_lower(hostField, session, (char *)valueStr, l - 3);
            }
            break;

        default:
            BSB_IMPORT_skip(bsb, l);
        }
    }

    arkime_parsers_call_named_func(dhcp_packet_func, session, data, len, NULL);
    return 1;
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL ArkimePacketRC dhcp_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[ARKIME_SESSIONID_LEN];

    // Validate: minimum size, op field (1=request, 2=reply), magic cookie
    if (len < 256 || (data[0] != 1 && data[0] != 2) || memcmp(data + 236, "\x63\x82\x53\x63", 4) != 0)
        return ARKIME_PACKET_UNKNOWN;

    // Validate hardware type (1=Ethernet) and address length (6 bytes)
    if (data[1] != 1 || data[2] != 6)
        return ARKIME_PACKET_UNKNOWN;

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    dhcp_create_sessionid(sessionId, packet);

    packet->hash = arkime_session_hash(sessionId);
    packet->mProtocol = dhcpMProtocol;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL ArkimePacketRC dhcpv6_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[ARKIME_SESSIONID_LEN];

    if (len < 4 || data[0] == 0 || data[0] >= ARRAY_LEN(namesv6))
        return ARKIME_PACKET_UNKNOWN;

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    dhcpv6_create_sessionid(sessionId, packet);

    packet->hash = arkime_session_hash(sessionId);
    packet->mProtocol = dhcpv6MProtocol;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
void arkime_parser_init()
{
    typeField = arkime_field_define("dhcp", "uptermfield",
                                    "dhcp.type", "DHCP Type", "dhcp.type",
                                    "DHCP Type",
                                    ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                    (char *)NULL);

    hostField = arkime_field_define("dhcp", "lotermfield",
                                    "dhcp.host", "DHCP Host", "dhcp.host",
                                    "DHCP Host",
                                    ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                    "aliases", "[\"host.dhcp\"]",
                                    "category", "host",
                                    (char *)NULL);

    arkime_field_define("dhcp", "lotextfield",
                        "dhcp.host.tokens", "Hostname Tokens", "dhcp.hostTokens",
                        "DHCP Hostname Tokens",
                        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_FAKE,
                        "aliases", "[\"host.dhcp.tokens\"]",
                        (char *)NULL);

    macField = arkime_field_define("dhcp", "lotermfield",
                                   "dhcp.mac", "DHCP Client MAC", "dhcp.mac",
                                   "Client ethernet MAC ",
                                   ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                   (char *)NULL);

    ouiField = arkime_field_define("dhcp", "termfield",
                                   "dhcp.oui", "DHCP Client OUI", "dhcp.oui",
                                   "Client ethernet OUI ",
                                   ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                   (char *)NULL);

    idField = arkime_field_define("dhcp", "lotermfield",
                                  "dhcp.id", "DHCP Transaction id", "dhcp.id",
                                  "DHCP Transaction Id",
                                  ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                  (char *)NULL);

    classIdField = arkime_field_define("dhcp", "termfield",
                                       "dhcp.classId", "DHCP Vendor Class", "dhcp.classId",
                                       "DHCP Vendor Class Identifier",
                                       ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    requestIpField = arkime_field_define("dhcp", "ip",
                                         "dhcp.requestIp", "DHCP Request IP", "dhcp.requestIp",
                                         "DHCP Requested IP Address",
                                         ARKIME_FIELD_TYPE_IP_GHASH,  ARKIME_FIELD_FLAG_CNT,
                                         (char *)NULL);

    arkime_packet_set_udpport_enqueue_cb(67, dhcp_packet_enqueue);
    arkime_packet_set_udpport_enqueue_cb(68, dhcp_packet_enqueue);
    arkime_packet_set_udpport_enqueue_cb(546, dhcpv6_packet_enqueue);
    arkime_packet_set_udpport_enqueue_cb(547, dhcpv6_packet_enqueue);

    dhcpMProtocol = arkime_mprotocol_register("dhcp",
                                              SESSION_OTHER,
                                              dhcp_create_sessionid,
                                              dhcp_pre_process,
                                              dhcp_process,
                                              NULL,
                                              NULL,
                                              arkime_config_int(NULL, "dhcpTimeout", 60, 10, 0xffff));

    dhcpv6MProtocol = arkime_mprotocol_register("dhcpv6",
                                                SESSION_OTHER,
                                                dhcpv6_create_sessionid,
                                                dhcpv6_pre_process,
                                                dhcpv6_process,
                                                NULL,
                                                NULL,
                                                arkime_config_int(NULL, "dhcpTimeout", 60, 10, 0xffff));

    dhcp_packet_func = arkime_parsers_get_named_func("dhcp_packet");
}
