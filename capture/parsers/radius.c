/* Copyright 2012-2017 AOL Inc. All rights reserved.
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
LOCAL  int userField;
LOCAL  int macField;
LOCAL  int endpointIpField;
LOCAL  int framedIpField;

/******************************************************************************/
LOCAL int radius_udp_parser(MolochSession_t *session, void *UNUSED(uw), const unsigned char *data, int len, int UNUSED(which))
{
    BSB bsb;

    BSB_INIT(bsb, data, len);

    BSB_IMPORT_skip(bsb, 20);

    unsigned char type = 0, length = 0;
    unsigned char *value;
    char str[256];
    struct in_addr in;
    int i;

    while (BSB_REMAINING(bsb) > 2) {
        BSB_IMPORT_u08(bsb, type);
        BSB_IMPORT_u08(bsb, length);
        length -= 2; // length includes the type/length
        BSB_IMPORT_ptr(bsb, value, length);
        if (BSB_IS_ERROR(bsb)) {
            break;
        }
        switch (type) {
        case 1:
            moloch_field_string_add(userField, session, (char *)value, length, TRUE);
            break;
    /*    case 4:
            LOG("NAS-IP-Address: %d %d %u.%u.%u.%u", type, length, value[0], value[1], value[2], value[3]);
            break;*/
        case 8:
            memcpy(&in.s_addr, value, 4);
            moloch_field_ip4_add(framedIpField, session, in.s_addr);
            break;
        case 31:
            if (length == 12) {
                snprintf(str, sizeof(str), "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
                        value[0], value[1],
                        value[2], value[3],
                        value[4], value[5],
                        value[6], value[7],
                        value[8], value[9],
                        value[10], value[11]);
                for (i = 0; i < 17; i++) {
                    if (isupper (str[i]))
                      str[i] = tolower (str[i]);
                }
                moloch_field_string_add(macField, session, str, 17, TRUE);
            }
            break;
        case 66:
            memcpy(str, value, length);
            str[length] = 0;
            moloch_field_ip_add_str(endpointIpField, session, str);
            break;

/*        default:
            LOG("%d %d %.*s", type, length, length, value);*/
        }
    }
    return 0;
}
/******************************************************************************/
LOCAL void radius_udp_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len != ((data[2] << 8) | data[3])) {
        return;
    }

    if ((session->port1 >= 1812 && session->port1 <= 1813) ||
        (session->port1 >= 1645 && session->port1 <= 1646) ||
        (session->port2 >= 1812 && session->port2 <= 1813) ||
        (session->port2 >= 1645 && session->port2 <= 1646)) {
        moloch_parsers_register(session, radius_udp_parser, 0, 0);
        moloch_session_add_protocol(session, "radius");
    }
}
/******************************************************************************/
void moloch_parser_init()
{
    userField = moloch_field_define("radius", "termfield",
        "radius.user", "User", "radius.user",
        "RADIUS user",
        MOLOCH_FIELD_TYPE_STR_HASH,     0, 
        "category", "user",
        NULL);

    macField = moloch_field_define("radius", "lotermfield",
        "radius.mac", "MAC", "radius.mac",
        "Radius Mac",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL); 

    endpointIpField = moloch_field_define("radius", "ip",
        "radius.endpoint-ip", "Endpoint IP", "radius.endpointIp",
        "Radius endpoint ip addresses for session",
        MOLOCH_FIELD_TYPE_IP_GHASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL);

    framedIpField = moloch_field_define("radius", "ip",
        "radius.framed-ip", "Framed IP", "radius.framedIp",
        "Radius framed ip addresses for session",
        MOLOCH_FIELD_TYPE_IP_GHASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL);


    moloch_parsers_classifier_register_udp("radius", NULL, 0, (const unsigned char *)"\x01", 1, radius_udp_classify);
    moloch_parsers_classifier_register_udp("radius", NULL, 0, (const unsigned char *)"\x02", 1, radius_udp_classify);
    moloch_parsers_classifier_register_udp("radius", NULL, 0, (const unsigned char *)"\x03", 1, radius_udp_classify);
    moloch_parsers_classifier_register_udp("radius", NULL, 0, (const unsigned char *)"\x04", 1, radius_udp_classify);
    moloch_parsers_classifier_register_udp("radius", NULL, 0, (const unsigned char *)"\x05", 1, radius_udp_classify);
}
