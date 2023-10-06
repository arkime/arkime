/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

LOCAL  int userField;
LOCAL  int hostField;
LOCAL  int serviceField;
extern ArkimeConfig_t        config;

// Lots of info from https://www.pythian.com/blog/repost-oracle-protocol/

/******************************************************************************/
LOCAL char *oracle_get_item(const uint8_t *data, char *needle, int needle_len, int *len)
{
    const uint8_t *start = data + data[27];

    uint8_t *item = (uint8_t *)g_strstr_len((char *)start, data[25], (gchar *)needle);
    if (item) {
        uint8_t *paren = (uint8_t *)g_strstr_len((char *)item, data[25] - (item - start), ")");
        if (paren) {
            *len = (paren-item)-needle_len;
            if (*len == 0)
                return NULL;

            return g_ascii_strdown((char *)item+needle_len, *len);
        }
    }
    return NULL;
}
/******************************************************************************/
LOCAL void oracle_classify(ArkimeSession_t *session, const uint8_t *data, int len, int which, void *UNUSED(uw))
{
    if (which != 0 || len <= 27 || len != (data[0] << 8 | data[1]) || (data[25] + data[27] != len)) {
        return;
    }

    char *buf;  // can't be more then 1 byte big
    int  blen;

    buf = oracle_get_item(data, "HOST=", 5, &blen); // Already lowercases
    if (buf && !arkime_field_string_add(hostField, session, buf, blen, FALSE)) {
        g_free(buf);
    }

    buf = oracle_get_item(data, "USER=", 5, &blen); // Already lowercases
    if (buf && !arkime_field_string_add(userField, session, buf, blen, FALSE)) {
        g_free(buf);
    }

    buf = oracle_get_item(data, "SERVICE_NAME=", 13, &blen); // Already lowercases
    if (buf && !arkime_field_string_add(serviceField, session, buf, blen, FALSE)) {
        g_free(buf);
    }

    arkime_session_add_protocol(session, "oracle");
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_parsers_classifier_register_tcp("oracle", NULL, 2, (uint8_t *)"\x00\x00\x01\x00\x00\x00", 6, oracle_classify);

    userField = arkime_field_define("oracle", "lotermfield",
        "oracle.user", "User", "oracle.user",
        "Oracle User",
        ARKIME_FIELD_TYPE_STR,  ARKIME_FIELD_FLAG_LINKED_SESSIONS,
        "category", "user",
        (char *)NULL);

    hostField = arkime_field_define("oracle", "lotermfield",
        "oracle.host", "Host", "oracle.host",
        "Oracle Host",
        ARKIME_FIELD_TYPE_STR,  ARKIME_FIELD_FLAG_LINKED_SESSIONS,
        "category", "host",
        "aliases", "[\"host.oracle\"]",
        (char *)NULL);

    arkime_field_define("oracle", "lotextfield",
        "oracle.host.tokens", "Hostname Tokens", "oracle.hostTokens",
        "Oracle Hostname Tokens",
        ARKIME_FIELD_TYPE_STR,  ARKIME_FIELD_FLAG_FAKE,
        "aliases", "[\"host.oracle.tokens\"]",
        (char *)NULL);

    serviceField = arkime_field_define("oracle", "lotermfield",
        "oracle.service", "Service", "oracle.service",
        "Oracle Service",
        ARKIME_FIELD_TYPE_STR,  ARKIME_FIELD_FLAG_LINKED_SESSIONS,
        (char *)NULL);
}

