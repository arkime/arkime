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

typedef struct {
    int       versionLen;
    char     *version;
    char      ssl;
} Info_t;

static int userField;
static int versionField;

extern MolochConfig_t        config;

/******************************************************************************/
int mysql_parser(MolochSession_t *session, void *uw, const unsigned char *data, int len, int which)
{
    Info_t *info = uw;
    if (which != 0) {
        return 0;
    }

    if (info->ssl) {
        moloch_parsers_classify_tcp(session, data, len, which);
        moloch_parsers_unregister(session, info);
        return 0;
    }

    if (len < 35 || data[1] != 0 || data[2] != 0 || data[3] > 2) {
        moloch_parsers_unregister(session, info);
        return 0;
    }

    unsigned char *ptr = (unsigned char*)data + 36;
    unsigned char *end = (unsigned char*)data + len;

    while (ptr < end) {
        if (*ptr == 0)
            break;
        if (!isprint(*ptr)) {
            moloch_parsers_unregister(session, info);
            return 0;
        }
        ptr++;
    }

    moloch_session_add_protocol(session, "mysql");
    moloch_field_string_add(versionField, session, info->version, info->versionLen, FALSE);
    info->version = 0;

    if (ptr > data + 36) {
        char *lower = g_ascii_strdown((char *)data+36, ptr - (data + 36));
        moloch_field_string_add(userField, session, lower, ptr - (data + 36), FALSE);
    }

    if (data[5] & 0x08) { //CLIENT_SSL
        info->ssl = 1;
    } else {
        moloch_parsers_unregister(session, info);
    }
    return 0;
}
/******************************************************************************/
void mysql_free(MolochSession_t UNUSED(*session), void *uw)
{
    Info_t *info = uw;

    if (info->version)
        g_free(info->version);
    MOLOCH_TYPE_FREE(Info_t, info);
}
/******************************************************************************/
void mysql_classify(MolochSession_t *session, const unsigned char *data, int len, int which, void *UNUSED(uw))
{
    if (which != 1)
        return;

    if (moloch_session_has_protocol(session, "mysql"))
        return;

    unsigned char *ptr = (unsigned char*)data + 5;
    unsigned char *end = (unsigned char*)data + len;

    while (ptr < end) {
        if (*ptr == 0)
            break;
        if (!isprint(*ptr)) {
            return;
        }
        ptr++;
    }

    if (ptr == end || ptr == data + 5) {
        return;
    }

    Info_t *info = MOLOCH_TYPE_ALLOC0(Info_t);
    info->versionLen = ptr - (data + 5);
    info->version = g_strndup((char*)data + 5, info->versionLen);
    moloch_parsers_register(session, mysql_parser, info, mysql_free);
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_parsers_classifier_register_tcp("mysql", NULL, 1, (unsigned char*)"\x00\x00\x00\x0a", 4, mysql_classify);

    userField = moloch_field_define("mysql", "lotermfield",
        "mysql.user", "User", "mysql.user-term",
        "Mysql user name",
        MOLOCH_FIELD_TYPE_STR,  MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        "category", "user",
        NULL);

    versionField = moloch_field_define("mysql", "termfield",
        "mysql.ver", "Version", "mysql.ver-term",
        "Mysql server version string",
        MOLOCH_FIELD_TYPE_STR,  MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);
}

