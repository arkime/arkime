/* Copyright 2018, Oath Inc.. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "moloch.h"
#include <arpa/inet.h>

extern MolochConfig_t        config;
LOCAL  int typeField;
LOCAL  int hostField;
LOCAL  int macField;
LOCAL  int ouiField;
LOCAL  int idField;

/******************************************************************************/
LOCAL void dhcpv6_udp_classify(MolochSession_t *session, const unsigned char *data, int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    if ((data[0] != 1 && data[0] != 11) || !MOLOCH_SESSION_v6(session))
        return;
    moloch_session_add_protocol(session, "dhcpv6");
}
/******************************************************************************/
LOCAL int dhcp_udp_parser(MolochSession_t *session, void *UNUSED(uw), const unsigned char *data, int len, int UNUSED(which))
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
            "TLS"};

    if (len < 256)
        return 0;

    BSB bsb;

    BSB_INIT(bsb, data, len);
    int hardwareType = data[1];

    if (hardwareType == 1) {
        moloch_field_macoui_add(session, macField, ouiField, data+28);
    }

    char str[100];
    uint32_t id = 0;
    BSB_IMPORT_skip(bsb, 4);
    BSB_IMPORT_u32(bsb, id);
    snprintf(str, sizeof(str), "%x", id);
    moloch_field_string_add(idField, session, str, -1, TRUE);

    // 236 offset + magic len - 4 skip - u32 import
    BSB_IMPORT_skip(bsb, 236 + 4 - 4 - 4);
    while (BSB_REMAINING(bsb) >= 2) {
        int t = 0;
        int l = 0;
        uint32_t value = 0;
        unsigned char *valueStr = 0;
        BSB_IMPORT_u08(bsb, t);
        if (t == 255) // End Tag, no length
            break;
        BSB_IMPORT_u08(bsb, l);
        if (BSB_IS_ERROR(bsb) || l > BSB_REMAINING(bsb))
            break;
        switch(t) {
        case 12: // Host Name
            BSB_IMPORT_ptr(bsb, valueStr, l);
            moloch_field_string_add_lower(hostField, session, (char *)valueStr, l);
            break;
        case 53: // Message Type
            if (l == 1) {
                BSB_IMPORT_u08(bsb, value);
                moloch_field_string_add(typeField, session, names[value], -1, TRUE);
            } else {
                BSB_IMPORT_skip(bsb, l);
            }
            break;
        case 61: // Client identifier
            BSB_IMPORT_u08(bsb, value);
            if (l == 7 && value == 1) {
                BSB_IMPORT_ptr(bsb, valueStr, 6);
                moloch_field_macoui_add(session, macField, ouiField, valueStr);
            } else {
                BSB_IMPORT_skip(bsb, l-1);
            }
            break;
        case 81: // FQDN
            BSB_IMPORT_u08(bsb, value);
            BSB_IMPORT_skip(bsb, 2);
            if (value != 0) // Don't support any encodings right now
                BSB_IMPORT_skip(bsb, l - 1);
            else {
                BSB_IMPORT_ptr(bsb, valueStr, l-3);
                moloch_field_string_add_lower(hostField, session, (char *)valueStr, l-3);
            }
            break;

        default:
            BSB_IMPORT_skip(bsb, l);
        }
    }
    return 0;
}
/******************************************************************************/
LOCAL void dhcp_udp_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{

    if (len < 256 || (data[0] != 1 && data[0] != 2) || MOLOCH_SESSION_v6(session) || memcmp(data+236, "\x63\x82\x53\x63", 4) != 0)
        return;

    moloch_parsers_register(session, dhcp_udp_parser, 0, 0);
    moloch_session_add_protocol(session, "dhcp");
}
/******************************************************************************/
void moloch_parser_init()
{
    typeField = moloch_field_define("dhcp", "uptermfield",
        "dhcp.type", "Type", "dhcp.type",
        "DHCP Type",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL); 

    hostField = moloch_field_define("dhcp", "lotermfield",
        "dhcp.host", "Host", "dhcp.host",
        "DHCP Host",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL); 

    macField = moloch_field_define("dhcp", "lotermfield",
        "dhcp.mac", "Client MAC", "dhcp.mac",
        "Client ethernet MAC ",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL);

    ouiField = moloch_field_define("dhcp", "termfield",
        "dhcp.oui", "Client OUI", "dhcp.oui",
        "Client ethernet OUI ",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL);

    idField = moloch_field_define("dhcp", "lotermfield",
        "dhcp.id", "Transaction id", "dhcp.id",
        "DHCP Transaction Id",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL);


    moloch_parsers_classifier_register_port("dhcpv6",  NULL, 547, MOLOCH_PARSERS_PORT_UDP, dhcpv6_udp_classify);
    moloch_parsers_classifier_register_port("dhcp",  NULL, 67, MOLOCH_PARSERS_PORT_UDP, dhcp_udp_classify);
}
