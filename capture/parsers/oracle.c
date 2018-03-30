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

LOCAL  int userField;
LOCAL  int hostField;
LOCAL  int serviceField;
extern MolochConfig_t        config;

// Lots of info from https://www.pythian.com/blog/repost-oracle-protocol/

/******************************************************************************/
LOCAL char *oracle_get_item(const char *data, char *needle, int needle_len, int *len)
{
    const char *start = data + data[27];
    char *item, *paren;

    item = g_strstr_len((char *)start, data[25], (gchar *)needle);
    if (item) {
        paren = g_strstr_len(item, data[25] - (item - start), ")");
        if (paren) {
            *len = (paren-item)-needle_len;
            if (*len == 0)
                return NULL;

            return g_ascii_strdown(item+needle_len, *len);
        }
    }
    return NULL;
}
/******************************************************************************/
LOCAL void oracle_classify(MolochSession_t *session, const unsigned char *data, int len, int which, void *UNUSED(uw))
{
    if (which != 0 || len <= 27 || len != data[1] || (data[25] + data[27] != len))
        return;

    char *buf;  // can't be more then 1 byte big
    int  blen;

    buf = oracle_get_item((const char *)data, "HOST=", 5, &blen); // Already lowercases
    if (buf && !moloch_field_string_add(hostField, session, buf, blen, FALSE)) {
        g_free(buf);
    }

    buf = oracle_get_item((const char *)data, "USER=", 5, &blen); // Already lowercases
    if (buf && !moloch_field_string_add(userField, session, buf, blen, FALSE)) {
        g_free(buf);
    }

    buf = oracle_get_item((const char *)data, "SERVICE_NAME=", 13, &blen); // Already lowercases
    if (buf && !moloch_field_string_add(serviceField, session, buf, blen, FALSE)) {
        g_free(buf);
    }

    moloch_session_add_protocol(session, "oracle");
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_parsers_classifier_register_tcp("oracle", NULL, 2, (unsigned char*)"\x00\x00\x01\x00\x00\x00", 6, oracle_classify);

    userField = moloch_field_define("oracle", "lotermfield",
        "oracle.user", "User", "oracle.user",
        "Oracle User",
        MOLOCH_FIELD_TYPE_STR,  MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        "category", "user",
        NULL);

    hostField = moloch_field_define("oracle", "lotermfield",
        "oracle.host", "Host", "oracle.host",
        "Oracle Host",
        MOLOCH_FIELD_TYPE_STR,  MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);

    serviceField = moloch_field_define("oracle", "lotermfield",
        "oracle.service", "Service", "oracle.service",
        "Oracle Service",
        MOLOCH_FIELD_TYPE_STR,  MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);
}

