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
    int       which;
} Info_t;

LOCAL  int userField;
LOCAL  int dbField;
LOCAL  int appField;

/******************************************************************************/
LOCAL int postgresql_parser(MolochSession_t *session, void *uw, const unsigned char *data, int len, int which) 
{
    Info_t *info = uw;
    if (which != info->which)
        return 0;

    if (len == 8 && memcmp(data, "\x00\x00\x00\x08\x04\xd2\x16\x2f", 8) == 0) {
        moloch_session_add_protocol(session, "postgresql");
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

    while (*(BSB_WORK_PTR(bsb)) != 0) {
        char *key = (char*)BSB_WORK_PTR(bsb);
        int klen = strlen(key);
        BSB_IMPORT_skip(bsb, klen+1);

        if (BSB_IS_ERROR(bsb))
            break;

        char *value = (char*)BSB_WORK_PTR(bsb);
        int vlen = strlen(value);
        BSB_IMPORT_skip(bsb, vlen+1);

        if (BSB_IS_ERROR(bsb))
            break;

        if (strcmp(key, "user") == 0) {
            moloch_field_string_add(userField, session, value, vlen, TRUE);
            moloch_session_add_protocol(session, "postgresql");
        } else if (strcmp(key, "database") == 0)
            moloch_field_string_add(dbField, session, value, vlen, TRUE);
        else if (strcmp(key, "application_name") == 0)
            moloch_field_string_add(appField, session, value, vlen, TRUE);
    }

cleanup:
    moloch_parsers_unregister(session, info);
    return 0;
}
/******************************************************************************/
LOCAL void postgresql_free(MolochSession_t UNUSED(*session), void *uw)
{
    Info_t *info = uw;

    MOLOCH_TYPE_FREE(Info_t, info);
}
/******************************************************************************/
LOCAL void postgresql_classify(MolochSession_t *session, const unsigned char UNUSED(*data), int UNUSED(len), int which, void *UNUSED(uw))
{
    if (moloch_session_has_protocol(session, "postgresql"))
        return;

    if ((len == 8 && memcmp(data+3, "\x08\x04\xd2\x16\x2f", 5) == 0) ||
        (len > 8 && data[3] <= len && data[4] == 0 && data[5] == 3 && data[6] == 0)) {

        Info_t *info = MOLOCH_TYPE_ALLOC0(Info_t);
        info->which = which;
        moloch_parsers_register(session, postgresql_parser, info, postgresql_free);
    }
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_parsers_classifier_register_tcp("postgresql", NULL, 0, (unsigned char*)"\x00\x00\x00", 3, postgresql_classify);

    userField = moloch_field_define("postgresql", "termfield",
        "postgresql.user", "User", "postgresql.user",
        "Postgresql user name",
        MOLOCH_FIELD_TYPE_STR,  MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        "category", "user",
        NULL);

    dbField = moloch_field_define("postgresql", "termfield",
        "postgresql.db", "Database", "postgresql.db",
        "Postgresql database",
        MOLOCH_FIELD_TYPE_STR,  MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);

    appField = moloch_field_define("postgresql", "termfield",
        "postgresql.app", "Application", "postgresql.app",
        "Postgresql application",
        MOLOCH_FIELD_TYPE_STR,  MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);
}

