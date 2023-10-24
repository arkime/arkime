/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <arpa/inet.h>

extern ArkimeConfig_t        config;
LOCAL  int userField;
LOCAL  int macField;
LOCAL  int endpointIpField;
LOCAL  int framedIpField;

/******************************************************************************/
LOCAL int radius_udp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    BSB bsb;

    BSB_INIT(bsb, data, len);

    BSB_IMPORT_skip(bsb, 20);

    uint8_t type = 0, length = 0;
    uint8_t *value;
    char str[256];
    struct in_addr in;
    int i;

    while (BSB_REMAINING(bsb) > 2) {
        BSB_IMPORT_u08(bsb, type);
        BSB_IMPORT_u08(bsb, length);
        if (length < 3)
            break;
        length -= 2; // length includes the type/length
        BSB_IMPORT_ptr(bsb, value, length);
        if (BSB_IS_ERROR(bsb)) {
            break;
        }
        switch (type) {
        case 1:
            arkime_field_string_add(userField, session, (char *)value, length, TRUE);
            break;
        /*    case 4:
                LOG("NAS-IP-Address: %d %d %u.%u.%u.%u", type, length, value[0], value[1], value[2], value[3]);
                break;*/
        case 8:
            if (length != 4)
                return 0;
            memcpy(&in.s_addr, value, 4);
            arkime_field_ip4_add(framedIpField, session, in.s_addr);
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
                arkime_field_string_add(macField, session, str, 17, TRUE);
            }
            break;
        case 66:
            memcpy(str, value, length);
            str[length] = 0;
            arkime_field_ip_add_str(endpointIpField, session, str);
            break;

            /*        default:
                        LOG("%d %d %.*s", type, length, length, value);*/
        }
    }
    return 0;
}
/******************************************************************************/
LOCAL void radius_udp_classify(ArkimeSession_t *session, const uint8_t *UNUSED(data), int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 4 || len != ((data[2] << 8) | data[3])) {
        return;
    }

    if ((session->port1 >= 1812 && session->port1 <= 1813) ||
        (session->port1 >= 1645 && session->port1 <= 1646) ||
        (session->port2 >= 1812 && session->port2 <= 1813) ||
        (session->port2 >= 1645 && session->port2 <= 1646)) {
        arkime_parsers_register(session, radius_udp_parser, 0, 0);
        arkime_session_add_protocol(session, "radius");
    }
}
/******************************************************************************/
void arkime_parser_init()
{
    userField = arkime_field_define("radius", "termfield",
                                    "radius.user", "User", "radius.user",
                                    "RADIUS user",
                                    ARKIME_FIELD_TYPE_STR_GHASH,  0,
                                    "category", "user",
                                    (char *)NULL);

    macField = arkime_field_define("radius", "lotermfield",
                                   "radius.mac", "MAC", "radius.mac",
                                   "Radius Mac",
                                   ARKIME_FIELD_TYPE_STR_GHASH,  ARKIME_FIELD_FLAG_CNT,
                                   (char *)NULL);

    endpointIpField = arkime_field_define("radius", "ip",
                                          "radius.endpoint-ip", "Endpoint IP", "radius.endpointIp",
                                          "Radius endpoint ip addresses for session",
                                          ARKIME_FIELD_TYPE_IP_GHASH,  ARKIME_FIELD_FLAG_CNT,
                                          (char *)NULL);

    framedIpField = arkime_field_define("radius", "ip",
                                        "radius.framed-ip", "Framed IP", "radius.framedIp",
                                        "Radius framed ip addresses for session",
                                        ARKIME_FIELD_TYPE_IP_GHASH,  ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);


    arkime_parsers_classifier_register_udp("radius", NULL, 0, (const uint8_t *)"\x01", 1, radius_udp_classify);
    arkime_parsers_classifier_register_udp("radius", NULL, 0, (const uint8_t *)"\x02", 1, radius_udp_classify);
    arkime_parsers_classifier_register_udp("radius", NULL, 0, (const uint8_t *)"\x03", 1, radius_udp_classify);
    arkime_parsers_classifier_register_udp("radius", NULL, 0, (const uint8_t *)"\x04", 1, radius_udp_classify);
    arkime_parsers_classifier_register_udp("radius", NULL, 0, (const uint8_t *)"\x05", 1, radius_udp_classify);
}
