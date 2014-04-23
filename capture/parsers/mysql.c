#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "moloch.h"

typedef struct {
    int       versionLen;
    char     *version;
} Info_t;

static int userField;
static int versionField;

/******************************************************************************/
int mysql_parser(MolochSession_t *session, void *uw, const unsigned char *data, int len) 
{
    Info_t *info = uw;
    if (session->which != 0) {
        return 0;
    }

    if (len < 37 || data[1] != 0 || data[2] != 0 || data[3] != 1) {
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

    moloch_nids_add_protocol(session, "mysql");
    moloch_field_string_add(versionField, session, info->version, info->versionLen, FALSE);
    info->version = 0;

    char *lower = g_ascii_strdown((char *)data+36, ptr - (data + 36));
    moloch_field_string_add(userField, session, lower, ptr - (data + 36), FALSE);

    moloch_parsers_unregister(session, info);
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
void mysql_classify(MolochSession_t *session, const unsigned char *data, int len)
{
    if (session->which != 1)
        return;

    if (moloch_nids_has_protocol(session, "mysql"))
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
    moloch_parsers_classifier_register_tcp("mysql", 1, (unsigned char*)"\x00\x00\x00\x0a", 4, mysql_classify);

    userField = moloch_field_define("mysql", "lotermfield",
        "mysql.user", "User", "mysql.user-term",
        "Mysql user name",
        MOLOCH_FIELD_TYPE_STR,  MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);

    versionField = moloch_field_define("mysql", "termfield",
        "mysql.ver", "Version", "mysql.ver-term",
        "Mysql server version string",
        MOLOCH_FIELD_TYPE_STR,  MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);
}

