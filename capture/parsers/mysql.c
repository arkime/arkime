/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

typedef struct {
    int       versionLen;
    char     *version;
    char      ssl;
} Info_t;

LOCAL  int userField;
LOCAL  int versionField;

extern ArkimeConfig_t        config;

/******************************************************************************/
LOCAL int mysql_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int which)
{
    Info_t *info = uw;
    if (which != 0) {
        return 0;
    }

    if (info->ssl) {
        arkime_parsers_classify_tcp(session, data, len, which);
        arkime_parsers_unregister(session, info);
        return 0;
    }

    if ((len < 35 && len != 8) || data[1] != 0 || data[2] != 0 || data[3] > 2) {
        arkime_parsers_unregister(session, info);
        return 0;
    }

    uint8_t *ptr = (uint8_t *)data + 36;
    uint8_t *end = (uint8_t *)data + len;

    while (ptr < end) {
        if (*ptr == 0)
            break;
        if (!isprint(*ptr)) {
            arkime_parsers_unregister(session, info);
            return 0;
        }
        ptr++;
    }

    arkime_session_add_protocol(session, "mysql");
    arkime_field_string_add(versionField, session, info->version, info->versionLen, FALSE);
    info->version = 0;

    if (ptr > data + 36) {
        arkime_field_string_add_lower(userField, session, (char*)data + 36, ptr - (data + 36));
    }

    if (data[5] & 0x08) { //CLIENT_SSL
        info->ssl = 1;
    } else {
        arkime_parsers_unregister(session, info);
    }
    return 0;
}
/******************************************************************************/
LOCAL void mysql_free(ArkimeSession_t UNUSED(*session), void *uw)
{
    Info_t *info = uw;

    if (info->version)
        g_free(info->version);
    ARKIME_TYPE_FREE(Info_t, info);
}
/******************************************************************************/
LOCAL void mysql_classify(ArkimeSession_t *session, const uint8_t *data, int len, int which, void *UNUSED(uw))
{
    if (which != 1)
        return;

    if (arkime_session_has_protocol(session, "mysql"))
        return;

    uint8_t *ptr = (uint8_t *)data + 5;
    uint8_t *end = (uint8_t *)data + len;

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

    Info_t *info = ARKIME_TYPE_ALLOC0(Info_t);
    info->versionLen = ptr - (data + 5);
    info->version = g_strndup((char*)data + 5, info->versionLen);
    arkime_parsers_register(session, mysql_parser, info, mysql_free);
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_parsers_classifier_register_tcp("mysql", NULL, 1, (uint8_t *)"\x00\x00\x00\x0a", 4, mysql_classify);

    userField = arkime_field_define("mysql", "lotermfield",
        "mysql.user", "User", "mysql.user",
        "Mysql user name",
        ARKIME_FIELD_TYPE_STR,  ARKIME_FIELD_FLAG_LINKED_SESSIONS,
        "category", "user",
        (char *)NULL);

    versionField = arkime_field_define("mysql", "termfield",
        "mysql.ver", "Version", "mysql.version",
        "Mysql server version string",
        ARKIME_FIELD_TYPE_STR,  ARKIME_FIELD_FLAG_LINKED_SESSIONS,
        (char *)NULL);
}

