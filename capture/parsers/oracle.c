/* Copyright 2012-2016 AOL Inc. All rights reserved.
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

static int userField;
static int hostField;
static int serviceField;
extern MolochConfig_t        config;

// Lots of info from https://www.pythian.com/blog/repost-oracle-protocol/

/******************************************************************************/
void oracle_get_item(const char *data, char *needle, int needle_len, char *buf, int *len)
{
    const char *start = data + data[27];
    char *item, *paren;

    buf[0] = 0;

    item = g_strstr_len((char *)start, data[25], (gchar *)needle);
    if (item) {
        paren = g_strstr_len(item, data[25] - (item - start), ")");
        if (paren) {
            *len = (paren-item)-needle_len;
            memcpy(buf, item+needle_len, *len);
            buf[*len] = 0;
            g_ascii_strdown(buf, *len);
        }
    }
}
/******************************************************************************/
void oracle_classify(MolochSession_t *session, const unsigned char *data, int len, int which, void *UNUSED(uw))
{
    if (which != 0 || len <= 27 || len != data[1] || (data[25] + data[27] != len))
        return;

    char buf[257];  // can't be more then 1 byte big
    int  blen;

    oracle_get_item((const char *)data, "HOST=", 5, buf, &blen);
    if (buf[0])
        moloch_field_string_add(hostField, session, buf, blen, TRUE);

    oracle_get_item((const char *)data, "USER=", 5, buf, &blen);
    if (buf[0])
        moloch_field_string_add(userField, session, buf, blen, TRUE);

    oracle_get_item((const char *)data, "SERVICE_NAME=", 13, buf, &blen);
    if (buf[0])
        moloch_field_string_add(serviceField, session, buf, blen, TRUE);

    moloch_session_add_protocol(session, "oracle");
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_parsers_classifier_register_tcp("oracle", NULL, 2, (unsigned char*)"\x00\x00\x01\x00\x00\x00", 6, oracle_classify);

    userField = moloch_field_define("oracle", "lotermfield",
        "oracle.user", "User", "oracle.user-term",
        "Oracle User",
        MOLOCH_FIELD_TYPE_STR,  MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        "category", "user",
        NULL);

    hostField = moloch_field_define("oracle", "lotermfield",
        "oracle.host", "Host", "oracle.host-term",
        "Oracle Host",
        MOLOCH_FIELD_TYPE_STR,  MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);

    serviceField = moloch_field_define("oracle", "lotermfield",
        "oracle.service", "Service", "oracle.service-term",
        "Oracle Service",
        MOLOCH_FIELD_TYPE_STR,  MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);
}

