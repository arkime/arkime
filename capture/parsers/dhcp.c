/* Copyright 2018, Oath Inc.. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <arpa/inet.h>

extern ArkimeConfig_t        config;
LOCAL  int typeField;
LOCAL  int hostField;
LOCAL  int macField;
LOCAL  int ouiField;
LOCAL  int idField;

/******************************************************************************/
LOCAL void dhcpv6_udp_classify(ArkimeSession_t *session, const uint8_t *data, int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    if ((data[0] != 1 && data[0] != 11) || !ARKIME_SESSION_v6(session))
        return;
    arkime_session_add_protocol(session, "dhcpv6");
}
/******************************************************************************/
LOCAL int dhcp_udp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    static char *names[] = {
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
                if (value <= 18)
                    arkime_field_string_add(typeField, session, names[value], -1, TRUE);
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
    return 0;
}
/******************************************************************************/
LOCAL void dhcp_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{

    if (len < 256 || (data[0] != 1 && data[0] != 2) || ARKIME_SESSION_v6(session) || memcmp(data + 236, "\x63\x82\x53\x63", 4) != 0)
        return;

    arkime_parsers_register(session, dhcp_udp_parser, 0, 0);
    arkime_session_add_protocol(session, "dhcp");
}
/******************************************************************************/
void arkime_parser_init()
{
    typeField = arkime_field_define("dhcp", "uptermfield",
                                    "dhcp.type", "Type", "dhcp.type",
                                    "DHCP Type",
                                    ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                    (char *)NULL);

    hostField = arkime_field_define("dhcp", "lotermfield",
                                    "dhcp.host", "Host", "dhcp.host",
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
                                   "dhcp.mac", "Client MAC", "dhcp.mac",
                                   "Client ethernet MAC ",
                                   ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                   (char *)NULL);

    ouiField = arkime_field_define("dhcp", "termfield",
                                   "dhcp.oui", "Client OUI", "dhcp.oui",
                                   "Client ethernet OUI ",
                                   ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                   (char *)NULL);

    idField = arkime_field_define("dhcp", "lotermfield",
                                  "dhcp.id", "Transaction id", "dhcp.id",
                                  "DHCP Transaction Id",
                                  ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                  (char *)NULL);


    arkime_parsers_classifier_register_port("dhcpv6",  NULL, 547, ARKIME_PARSERS_PORT_UDP, dhcpv6_udp_classify);
    arkime_parsers_classifier_register_port("dhcp",  NULL, 67, ARKIME_PARSERS_PORT_UDP, dhcp_udp_classify);
}
