/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

extern ArkimeConfig_t        config;

typedef struct {
    int       which;
} Info_t;

LOCAL  int userField;
LOCAL  int dbField;
LOCAL  int appField;

/******************************************************************************/
LOCAL int postgresql_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int which)
{
    Info_t *info = uw;
    if (which != info->which)
        return 0;

    if (len == 8 && memcmp(data, "\x00\x00\x00\x08\x04\xd2\x16\x2f", 8) == 0) {
        arkime_session_add_protocol(session, "postgresql");
        return 0;
    }

    BSB bsb;

    BSB_INIT(bsb, data, len);

    int plen = 0;
    BSB_IMPORT_u32(bsb, plen);
    if (plen > len || plen < 16) {
        goto cleanup;
    }

    uint32_t version = 0;
    BSB_IMPORT_u32(bsb, version);
    if (version >> 16 != 3) {
        goto cleanup;
    }

    while (BSB_NOT_ERROR(bsb) && BSB_REMAINING(bsb) > 1 && *(BSB_WORK_PTR(bsb)) != 0) {
        char *key = (char *)BSB_WORK_PTR(bsb);
        int klen = strnlen(key, BSB_REMAINING(bsb));
        BSB_IMPORT_skip(bsb, klen + 1);

        if (BSB_IS_ERROR(bsb))
            break;

        char *value = (char *)BSB_WORK_PTR(bsb);
        int vlen = strnlen(value, BSB_REMAINING(bsb));
        BSB_IMPORT_skip(bsb, vlen + 1);

        if (BSB_IS_ERROR(bsb))
            break;

        if (strcmp(key, "user") == 0) {
            arkime_field_string_add(userField, session, value, vlen, TRUE);
            arkime_session_add_protocol(session, "postgresql");
        } else if (strcmp(key, "database") == 0)
            arkime_field_string_add(dbField, session, value, vlen, TRUE);
        else if (strcmp(key, "application_name") == 0)
            arkime_field_string_add(appField, session, value, vlen, TRUE);
    }

cleanup:
    arkime_parsers_unregister(session, info);
    return 0;
}
/******************************************************************************/
LOCAL void postgresql_free(ArkimeSession_t UNUSED(*session), void *uw)
{
    Info_t *info = uw;

    ARKIME_TYPE_FREE(Info_t, info);
}
/******************************************************************************/
LOCAL void postgresql_classify(ArkimeSession_t *session, const uint8_t UNUSED(*data), int UNUSED(len), int which, void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "postgresql"))
        return;

    if ((len == 8 && memcmp(data + 3, "\x08\x04\xd2\x16\x2f", 5) == 0) ||
        (len > 8 && data[3] <= len && data[4] == 0 && data[5] == 3 && data[6] == 0)) {

        Info_t *info = ARKIME_TYPE_ALLOC0(Info_t);
        info->which = which;
        arkime_parsers_register(session, postgresql_parser, info, postgresql_free);
    }
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_parsers_classifier_register_tcp("postgresql", NULL, 0, (uint8_t *)"\x00\x00\x00", 3, postgresql_classify);

    userField = arkime_field_define("postgresql", "termfield",
                                    "postgresql.user", "User", "postgresql.user",
                                    "Postgresql user name",
                                    ARKIME_FIELD_TYPE_STR,  ARKIME_FIELD_FLAG_LINKED_SESSIONS,
                                    "category", "user",
                                    (char *)NULL);

    dbField = arkime_field_define("postgresql", "termfield",
                                  "postgresql.db", "Database", "postgresql.db",
                                  "Postgresql database",
                                  ARKIME_FIELD_TYPE_STR,  ARKIME_FIELD_FLAG_LINKED_SESSIONS,
                                  (char *)NULL);

    appField = arkime_field_define("postgresql", "termfield",
                                   "postgresql.app", "Application", "postgresql.app",
                                   "Postgresql application",
                                   ARKIME_FIELD_TYPE_STR,  ARKIME_FIELD_FLAG_LINKED_SESSIONS,
                                   (char *)NULL);
}

