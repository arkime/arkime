/******************************************************************************/
/* field.c  -- Functions dealing with declaring fields
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <stdarg.h>
#include <arpa/inet.h>
#include <math.h>
#include "patricia.h"

extern ArkimeConfig_t        config;

LOCAL HASH_VAR(d_, fieldsByDb, ArkimeFieldInfo_t, 307);
LOCAL HASH_VAR(e_, fieldsByExp, ArkimeFieldInfo_t, 307);

#define ARKIME_FIELD_SPECIAL_STOP_SPI      -2
#define ARKIME_FIELD_SPECIAL_STOP_PCAP     -3
#define ARKIME_FIELD_SPECIAL_MIN_SAVE      -4
#define ARKIME_FIELD_SPECIAL_DROP_SRC      -5
#define ARKIME_FIELD_SPECIAL_DROP_DST      -6
#define ARKIME_FIELD_SPECIAL_DROP_SESSION  -7
#define ARKIME_FIELD_SPECIAL_STOP_YARA     -8
#define ARKIME_FIELD_SPECIAL_MIN           -8

LOCAL va_list empty_va_list;

#define ARKIME_FIELD_MAX_ELEMENT_SIZE 16384

GHashTable *groupName2Num;

int16_t fieldOpsRemap[ARKIME_FIELDS_MAX][ARKIME_FIELDS_MAX];

#define FIELD_MAX_JSON_SIZE 20000

/******************************************************************************/
LOCAL void arkime_field_by_exp_add_special(const char *exp, int pos)
{
    ArkimeFieldInfo_t *info = ARKIME_TYPE_ALLOC0(ArkimeFieldInfo_t);
    info->expression = g_strdup(exp);
    info->pos = pos;
    HASH_ADD(e_, fieldsByExp, info->expression, info);
}
/******************************************************************************/
int arkime_field_by_exp_add_internal(const char *exp, ArkimeFieldType type, ArkimeFieldGetFunc getCb, ArkimeFieldSetFunc setCb)
{
    ArkimeFieldInfo_t *info = ARKIME_TYPE_ALLOC0(ArkimeFieldInfo_t);
    info->expression   = g_strdup(exp);
    info->pos          = ARKIME_THREAD_DECRNEW(config.minInternalField);
    info->type         = type;
    info->getCb        = getCb;
    info->setCb        = setCb;
    config.fields[info->pos] = info;
    HASH_ADD(e_, fieldsByExp, info->expression, info);

    return info->pos;
}
/******************************************************************************/
LOCAL void arkime_field_by_exp_add_count(const char *exp, int cntForPos)
{
    // Fake fields don't actully have a cnt
    if (cntForPos == -1) {
        return;
    }

    ArkimeFieldInfo_t *info = ARKIME_TYPE_ALLOC0(ArkimeFieldInfo_t);
    info->expression   = g_strdup(exp);
    info->pos          = ARKIME_THREAD_DECRNEW(config.minInternalField);
    info->cntForPos    = cntForPos;
    info->type         = ARKIME_FIELD_TYPE_INT;
    config.fields[info->pos] = info;
    HASH_ADD(e_, fieldsByExp, info->expression, info);
}
/******************************************************************************/
LOCAL int arkime_field_exp_cmp(const char *key, const ArkimeFieldInfo_t *element)
{
    return strcmp(key, element->expression) == 0;
}
/******************************************************************************/
LOCAL void arkime_field_free_info(ArkimeFieldInfo_t *info)
{
    g_free(info->dbFieldFull);
    g_free(info->expression);
    g_free(info->group);
    g_free(info->kind);
    g_free(info->category);
    g_free(info->transform);
    g_free(info->aliases);
    ARKIME_TYPE_FREE(ArkimeFieldInfo_t, info);
}
/******************************************************************************/
void arkime_field_define_json(const uint8_t *expression, int expression_len, const uint8_t *data, int data_len)
{
    ArkimeFieldInfo_t *info = ARKIME_TYPE_ALLOC0(ArkimeFieldInfo_t);
    int                i;
    uint32_t           out[4 * 100]; // Can have up to 100 elements at any level
    int                disabled = 0;

    if (js0n(data, data_len, out, sizeof(out)) != 0) {
        LOGEXIT("ERROR - Parse error for >%.*s<\n", data_len, data);
    }

    info->expression = g_strndup((char *)expression, expression_len);
    for (i = 0; out[i]; i += 4) {
        if (strncmp("group", (char * )data + out[i], 5) == 0) {
            g_free(info->group);
            info->group = g_strndup((char *)data + out[i + 2], out[i + 3]);
        } else if (strncmp("dbField2", (char * )data + out[i], 7) == 0) {
            g_free(info->dbFieldFull);
            info->dbFieldFull = info->dbField = g_strndup((char *)data + out[i + 2], out[i + 3]);
            info->dbFieldLen  = out[i + 3];
        } else if (strncmp("fieldECS", (char * )data + out[i], 7) == 0) {
            g_free(info->dbFieldFull);
            info->dbFieldFull = info->dbField = g_strndup((char *)data + out[i + 2], out[i + 3]);
            info->dbFieldLen  = out[i + 3];
        } else if (strncmp("type", (char * )data + out[i], 4) == 0) {
            g_free(info->kind);
            info->kind = g_strndup((char *)data + out[i + 2], out[i + 3]);
        } else if (strncmp("category", (char * )data + out[i], 8) == 0) {
            g_free(info->category);
            info->category = g_strndup((char *)data + out[i + 2], out[i + 3]);
        } else if (strncmp("transform", (char * )data + out[i], 8) == 0) {
            g_free(info->transform);
            info->transform = g_strndup((char *)data + out[i + 2], out[i + 3]);
        } else if (strncmp("aliases", (char * )data + out[i], 7) == 0) {
            g_free(info->aliases);
            info->aliases = g_strndup((char *)data + out[i + 2], out[i + 3]);
        } else if (strncmp("disabled", (char * )data + out[i], 8) == 0) {
            if (strncmp((char *)data + out[i + 2], "true", 4) == 0) {
                disabled = 1;
            }
        }
    }

    // Ignore old style http.request/http.response, will remove in the future
    if (g_str_has_prefix(info->dbFieldFull, "http.request-") && !g_str_has_prefix(info->expression, "http.request.")) {
        arkime_db_delete_field(info->expression);
        arkime_field_free_info(info);
        return;
    }

    if (g_str_has_prefix(info->dbFieldFull, "http.response-") && !g_str_has_prefix(info->expression, "http.response.")) {
        arkime_db_delete_field(info->expression);
        arkime_field_free_info(info);
        return;
    }

    if (disabled)
        info->flags    |= ARKIME_FIELD_FLAG_DISABLED;
    else
        info->flags    &= ~ARKIME_FIELD_FLAG_DISABLED;

    info->pos = -1;
    HASH_ADD(d_, fieldsByDb, info->dbField, info);
    HASH_ADD(e_, fieldsByExp, info->expression, info);

    return;
}
/******************************************************************************/
int arkime_field_define_text_full(char *field, const char *text, int *shortcut)
{
    int count = 0;
    int nolinked = 0;
    int noutf8 = 0;
    int fake = 0;
    const char *kind = 0;
    const char *help = 0;
    const char *db = 0;
    const char *group = 0;
    const char *friendly = 0;
    const char *category = 0;
    const char *transform = 0;
    const char *aliases = 0;

    if (config.debug)
        LOG("Parsing %s", text);
    char **elements = g_strsplit(text, ";", 0);
    int e;
    for (e = 0; elements[e]; e++) {
        char *colon = strchr(elements[e], ':');
        if (!colon)
            continue;
        *colon = 0;
        colon++;
        if (strcmp(elements[e], "field") == 0)
            field = colon;
        else if (strcmp(elements[e], "kind") == 0)
            kind = colon;
        else if (strcmp(elements[e], "group") == 0)
            group = colon;
        else if (strcmp(elements[e], "count") == 0)
            count = strcmp(colon, "true") == 0;
        else if (strcmp(elements[e], "nolinked") == 0)
            nolinked = strcmp(colon, "true") == 0;
        else if (strcmp(elements[e], "noutf8") == 0)
            noutf8 = strcmp(colon, "true") == 0;
        else if (strcmp(elements[e], "fake") == 0 || strcmp(elements[e], "viewerOnly") == 0)
            fake = strcmp(colon, "true") == 0;
        else if (strcmp(elements[e], "friendly") == 0)
            friendly = colon;
        else if (strcmp(elements[e], "db") == 0)
            db = colon;
        else if (strcmp(elements[e], "help") == 0)
            help = colon;
        else if (strcmp(elements[e], "category") == 0)
            category = colon;
        else if (strcmp(elements[e], "transform") == 0)
            transform = colon;
        else if (strcmp(elements[e], "aliases") == 0)
            aliases = colon;
        else if (strcmp(elements[e], "shortcut") == 0) {
            if (shortcut)
                *shortcut = atoi(colon);
        }

    }

    if (!field) {
        LOG("Didn't find field 'field:' in '%s'", text);
        g_strfreev(elements);
        return -1;
    }

    if (!db) {
        int pos = arkime_field_by_exp(field);
        g_strfreev(elements);
        if (pos != -1)
            return pos;

        LOG("Didn't find field 'db:' in '%s'", text);
        return -1;
    }

    if (!kind) {
        LOG("Didn't find field 'kind:' in '%s'", text);
        g_strfreev(elements);
        return -1;
    }

    if (strstr(kind, "termfield") != 0 && strstr(db, "-term") != 0) {
        LOGEXIT("ERROR - db field %s for %s should NOT end with -term in '%s' with Arkime 1.0", kind, db, text);
    }

    char groupbuf[100];
    if (!group) {
        const char *dot = strchr(field, '.');
        if (dot) {
            if (dot - field >= (int)sizeof(groupbuf) - 1)
                LOGEXIT("ERROR - field '%s' too long", field);
            memcpy(groupbuf, field, dot - field);
            groupbuf[dot - field] = 0;
            group = groupbuf;
        } else {
            group = "general";
        }
    }

    if (!friendly)
        friendly = field;

    if (!help)
        help = field;

    ArkimeFieldType type;
    int flags = 0;
    if (strcmp(kind, "integer") == 0 ||
        strcmp(kind, "seconds") == 0) {
        type = ARKIME_FIELD_TYPE_INT_GHASH;
    } else if (strcmp(kind, "ip") == 0) {
        type = ARKIME_FIELD_TYPE_IP_GHASH;
        if (!category)
            category = "ip";
    } else if (strcmp(kind, "float") == 0) {
        type = ARKIME_FIELD_TYPE_FLOAT_GHASH;
    } else {
        type = ARKIME_FIELD_TYPE_STR_HASH;
    }

    if (count)
        flags |= ARKIME_FIELD_FLAG_CNT;

    if (!nolinked)
        flags |= ARKIME_FIELD_FLAG_LINKED_SESSIONS;

    if (!noutf8 && type == ARKIME_FIELD_TYPE_STR_HASH)
        flags |= ARKIME_FIELD_FLAG_FORCE_UTF8;

    if (fake)
        flags |= ARKIME_FIELD_FLAG_FAKE;

    int pos =  arkime_field_define(group, kind, field, friendly, db, help, type, flags, "category", category, "transform", transform, "aliases", aliases, (char *)NULL);
    g_strfreev(elements);
    return pos;
}
/******************************************************************************/
int arkime_field_define_text(const char *text, int *shortcut)
{
    return arkime_field_define_text_full(NULL, text, shortcut);
}
/******************************************************************************/
/* Changes ... to va_list */
/*static void arkime_session_add_field_proxy(char *group, char *kind, char *expression, char *friendlyName, char *dbField, char *help, ...)
{
    va_list args;
    va_start(args, help);
    arkime_db_add_field(group, kind, expression, friendlyName, dbField, help, TRUE, args);
    va_end(args);
}*/
/******************************************************************************/
LOCAL int arkime_field_group_num(const char *group, int len)
{
    static int maxGroupNum = 0;
    char       groupName[100];

    if (len + 1 >= (int)sizeof(groupName)) {
        LOGEXIT("ERROR - field '%s' too long", group);
        return 0;
    }
    memcpy(groupName, group, len);
    groupName[len] = 0;

    long groupNum = (long)g_hash_table_lookup(groupName2Num, groupName);
    if (groupNum != 0) {
        return groupNum;
    }

    groupNum = ARKIME_THREAD_INCRNEW(maxGroupNum);
    g_hash_table_insert(groupName2Num, g_strdup(groupName), (gpointer)groupNum);
    return groupNum;
}
/******************************************************************************/
int arkime_field_define(const char *group, const char *kind, const char *expression, const char *friendlyName, const char *dbField, const char *help, ArkimeFieldType type, int flags, ...)
{
    char dbField2[100];
    char expression2[1000];
    char friendlyName2[1000];
    char help2[1000];

    ArkimeFieldInfo_t *minfo = 0;
    HASH_FIND(d_, fieldsByDb, dbField, minfo);

    const char *category = NULL;
    const char *transform = NULL;
    const char *aliases = NULL;
    va_list args;
    va_start(args, flags);
    while (1) {
        const char *field = va_arg(args, char *);
        if (!field) break;
        const char *value = va_arg(args, char *);
        if (strcmp(field, "category") == 0 && value) {
            category = value;
        } else if (strcmp(field, "transform") == 0 && value) {
            transform = value;
        } else if (strcmp(field, "aliases") == 0 && value) {
            aliases = value;
        }
    }
    va_end(args);

    if (!minfo) {
        minfo = ARKIME_TYPE_ALLOC0(ArkimeFieldInfo_t);
        minfo->dbFieldFull = g_strdup(dbField);
        minfo->dbField     = minfo->dbFieldFull;
        minfo->dbFieldLen  = strlen(minfo->dbField);
        minfo->pos         = -1;
        minfo->expression  = g_strdup(expression);
        minfo->group       = g_strdup(group);
        minfo->kind        = g_strdup(kind);
        if (aliases)
            minfo->aliases = g_strdup(aliases);
        if (category)
            minfo->category = g_strdup(category);
        if (transform)
            minfo->transform = g_strdup(transform);
        HASH_ADD(d_, fieldsByDb, minfo->dbField, minfo);
        HASH_ADD(e_, fieldsByExp, minfo->expression, minfo);

        if ((flags & ARKIME_FIELD_FLAG_NODB) == 0) {
            va_list args2;
            va_start(args2, flags);
            arkime_db_add_field(group, kind, expression, friendlyName, dbField, help, TRUE, args2);
            va_end(args2);
        }
    } else {
        flags |= (minfo->flags & ARKIME_FIELD_FLAG_DISABLED);

        // If we already have a pos can't be a fake field later
        if (minfo->pos != -1 && (flags & ARKIME_FIELD_FLAG_FAKE)) {
            return minfo->pos;
        }

        if (strcmp(kind, minfo->kind) != 0) {
            LOG("WARNING - Field kind in db %s doesn't match field kind %s in capture for field %s", minfo->kind, kind, expression);
        }

        if (category && (!minfo->category || strcmp(category, minfo->category) != 0)) {
            LOG("UPDATING - Field category in db %s doesn't match field category %s in capture for field %s", minfo->category, category, expression);
            arkime_db_update_field(expression, "category", category);
        }
        if (transform && (!minfo->transform || strcmp(transform, minfo->transform) != 0)) {
            LOG("UPDATING - Field transform in db %s doesn't match field transform %s in capture for field %s", minfo->transform, transform, expression);
            arkime_db_update_field(expression, "transform", transform);
        }
        if (aliases && (!minfo->aliases || strcmp(aliases, minfo->aliases) != 0)) {
            LOG("UPDATING - Field aliases in db %s doesn't match field aliases %s in capture for field %s", minfo->aliases, aliases, expression);
            arkime_db_update_field(expression, "aliases", aliases);
        }
    }

    minfo->type     = type;
    minfo->flags    = flags;

    if ((flags & ARKIME_FIELD_FLAG_FAKE) == 0) {
        if (minfo->pos == -1) {
            minfo->pos = ARKIME_THREAD_INCROLD(config.maxDbField);
            if (config.maxDbField >= config.minInternalField) {
                LOGEXIT("ERROR - Max Fields is too large %d", config.maxDbField);
            }
        }

        config.fields[minfo->pos] = minfo;

        // Change leading part to dbGroup
        const char *firstdot = strchr(minfo->dbField, '.');
        if (firstdot) {
            minfo->dbGroupNum = arkime_field_group_num(minfo->dbField, (firstdot - minfo->dbField) + 1);
            minfo->dbGroup = minfo->dbField;
            minfo->dbGroupLen = firstdot - minfo->dbField;
            minfo->dbField += (firstdot - minfo->dbField) + 1;
            minfo->dbFieldLen = strlen(minfo->dbField);
        }
    }

    if (flags & ARKIME_FIELD_FLAG_NODB)
        return minfo->pos;

    ArkimeFieldInfo_t *info;
    if (flags & ARKIME_FIELD_FLAG_CNT) {
        snprintf(dbField2, sizeof(dbField2), "%sCnt", dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            snprintf(expression2, sizeof(expression2), "%s.cnt", expression);
            snprintf(friendlyName2, sizeof(friendlyName2), "%s Cnt", friendlyName);
            snprintf(help2, sizeof(help2), "Unique number of %s", help);
            arkime_db_add_field(group, "integer", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
            arkime_field_by_exp_add_count(expression2, minfo->pos);
        }
    } else if (flags & ARKIME_FIELD_FLAG_ECS_CNT) {
        snprintf(dbField2, sizeof(dbField2), "%s-cnt", dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            snprintf(expression2, sizeof(expression2), "%s.cnt", expression);
            snprintf(friendlyName2, sizeof(friendlyName2), "%s Cnt", friendlyName);
            snprintf(help2, sizeof(help2), "Unique number of %s", help);
            arkime_db_add_field(group, "integer", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
            arkime_field_by_exp_add_count(expression2, minfo->pos);
        }
    }

    if (flags & ARKIME_FIELD_FLAG_FAKE) {
        HASH_REMOVE(d_, fieldsByDb, minfo);
        HASH_REMOVE(e_, fieldsByExp, minfo);
        arkime_field_free_info(minfo);
        return -1;
    }

    if (flags & ARKIME_FIELD_FLAG_IPPRE) {
        int l = strlen(dbField) - 2;
        int fnlen = strlen(friendlyName);
        snprintf(dbField2, sizeof(dbField2), "%.*sGEO", l, dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            snprintf(expression2, sizeof(expression2), "country.%s", expression + 3);
            snprintf(friendlyName2, sizeof(friendlyName2), "%.*s GEO", fnlen - 2, friendlyName);
            snprintf(help2, sizeof(help2), "GeoIP country string calculated from the %s", help);
            arkime_db_add_field(group, "uptermfield", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
        }

        snprintf(dbField2, sizeof(dbField2), "%.*sASN", l, dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            snprintf(expression2, sizeof(expression2), "asn.%s", expression + 3);
            snprintf(friendlyName2, sizeof(friendlyName2), "%.*s ASN", fnlen - 2, friendlyName);
            snprintf(help2, sizeof(help2), "GeoIP ASN string calculated from the %s", help);
            arkime_db_add_field(group, "termfield", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
        }

        snprintf(dbField2, sizeof(dbField2), "%.*sRIR", l, dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            snprintf(expression2, sizeof(expression2), "rir.%s", expression + 3);
            snprintf(friendlyName2, sizeof(friendlyName2), "%.*s RIR", fnlen - 2, friendlyName);
            snprintf(help2, sizeof(help2), "Regional Internet Registry string calculated from %s", help);
            arkime_db_add_field(group, "uptermfield", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
        }
    } else if (type == ARKIME_FIELD_TYPE_IP || type == ARKIME_FIELD_TYPE_IP_GHASH) {
        int l = strlen(dbField) - 2;
        snprintf(dbField2, sizeof(dbField2), "%.*sGEO", l, dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            snprintf(expression2, sizeof(expression2), "%s.country", expression);
            snprintf(friendlyName2, sizeof(friendlyName2), "%s GEO", friendlyName);
            snprintf(help2, sizeof(help2), "GeoIP country string calculated from the %s", help);
            arkime_db_add_field(group, "uptermfield", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
        }

        snprintf(dbField2, sizeof(dbField2), "%.*sASN", l, dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            snprintf(expression2, sizeof(expression2), "%s.asn", expression);
            snprintf(friendlyName2, sizeof(friendlyName2), "%s ASN", friendlyName);
            snprintf(help2, sizeof(help2), "GeoIP ASN string calculated from the %s", help);
            arkime_db_add_field(group, "termfield", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
        }

        snprintf(dbField2, sizeof(dbField2), "%.*sRIR", l, dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            snprintf(expression2, sizeof(expression2), "%s.rir", expression);
            snprintf(friendlyName2, sizeof(friendlyName2), "%s RIR", friendlyName);
            snprintf(help2, sizeof(help2), "Regional Internet Registry string calculated from %s", help);
            arkime_db_add_field(group, "uptermfield", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
        }
    }
    return minfo->pos;
}
/******************************************************************************/
int arkime_field_by_db(const char *dbField)
{
    ArkimeFieldInfo_t *info = 0;
    HASH_FIND(d_, fieldsByDb, dbField, info);
    if (!info || info->pos == -1)
        LOGEXIT("ERROR - dbField %s wasn't defined", dbField);

    return info->pos;
}
/******************************************************************************/
int arkime_field_by_exp(const char *exp)
{
    ArkimeFieldInfo_t *info = 0;
    HASH_FIND(e_, fieldsByExp, exp, info);
    if (info) {
        if (info->pos != -1)
            return info->pos;

        // Need to change from field we just know about to real field
        if (strcmp(info->kind, "integer") == 0 || strcmp(info->kind, "seconds") == 0) {
            info->type = ARKIME_FIELD_TYPE_INT_HASH;
        } else if (strcmp(info->kind, "ip") == 0) {
            info->type = ARKIME_FIELD_TYPE_IP_GHASH;
        } else {
            info->type = ARKIME_FIELD_TYPE_STR_HASH;
        }
        info->pos = ARKIME_THREAD_INCROLD(config.maxDbField);
        config.fields[info->pos] = info;
        return info->pos;
    }

    LOGEXIT("ERROR - expr %s wasn't defined", exp);
}
/******************************************************************************/
void arkime_field_truncated(ArkimeSession_t *session, const ArkimeFieldInfo_t *info)
{
    char str[1024];
    snprintf(str, sizeof(str), "truncated-field-%s", info->dbField);
    arkime_session_add_tag(session, str);
}
/******************************************************************************/
const char *arkime_field_string_add(int pos, ArkimeSession_t *session, const char *string, int len, gboolean copy)
{
    ArkimeField_t                    *field;
    ArkimeStringHashStd_t            *hash;
    ArkimeString_t                   *hstring;
    const ArkimeFieldInfo_t          *info = config.fields[pos];

    if (pos >= session->maxFields || info->flags & ARKIME_FIELD_FLAG_DISABLED)
        return NULL;

    if (!session->fields[pos]) {
        field = ARKIME_TYPE_ALLOC(ArkimeField_t);
        session->fields[pos] = field;
        if (len < 0)
            len = strlen(string);

        if (len > ARKIME_FIELD_MAX_ELEMENT_SIZE) {
            len = ARKIME_FIELD_MAX_ELEMENT_SIZE;
            arkime_field_truncated(session, info);
        }

        field->jsonSize = info->dbFieldLen;
        if (copy)
            string = g_strndup(string, len);
        switch (info->type) {
        case ARKIME_FIELD_TYPE_STR:
            field->str = (char *)string;
            goto added;
        case ARKIME_FIELD_TYPE_STR_ARRAY:
            field->sarray = g_ptr_array_new_with_free_func(g_free);
            g_ptr_array_add(field->sarray, (char *)string);
            goto added;
        case ARKIME_FIELD_TYPE_STR_HASH:
            hash = ARKIME_TYPE_ALLOC(ArkimeStringHashStd_t);
            HASH_INIT(s_, *hash, arkime_string_hash, arkime_string_ncmp);
            field->shash = hash;
            hstring = ARKIME_TYPE_ALLOC(ArkimeString_t);
            hstring->str = (char *)string;
            hstring->len = len;
            hstring->utf8 = 0;
            hstring->uw = 0;
            HASH_ADD(s_, *hash, hstring->str, hstring);
            goto added;
        case ARKIME_FIELD_TYPE_STR_GHASH:
            field->ghash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
            g_hash_table_add(field->ghash, (gpointer)string);
            goto added;
        default:
            LOGEXIT("ERROR - Not a string, expression: %s field: %s, tried to set '%.*s'", info->expression, info->dbFieldFull, len, string);
        }
    }

    if (len < 0)
        len = strlen(string);

    if (len > ARKIME_FIELD_MAX_ELEMENT_SIZE) {
        len = ARKIME_FIELD_MAX_ELEMENT_SIZE;
        arkime_field_truncated(session, info);
    }

    field = session->fields[pos];

    switch (info->type) {
    case ARKIME_FIELD_TYPE_STR:
        if (copy)
            string = g_strndup(string, len);
        g_free(field->str);
        field->str = (char *)string;
        goto added;
    case ARKIME_FIELD_TYPE_STR_ARRAY:
        if (copy)
            string = g_strndup(string, len);
        if (info->flags & ARKIME_FIELD_FLAG_DIFF_FROM_LAST) {
            if (strcmp(field->sarray->pdata[field->sarray->len - 1], string) == 0) {
                if (copy)
                    g_free((char *)string);
                return NULL;
            }
        }
        g_ptr_array_add(field->sarray, (char *)string);
        goto added;
    case ARKIME_FIELD_TYPE_STR_HASH:
        if (copy)
            string = g_strndup(string, len);

        HASH_FIND_HASH(s_, *(field->shash), arkime_string_hash_len(string, len), string, hstring);

        if (hstring) {
            if (copy)
                g_free((gpointer)string);
            return NULL;
        }
        hstring = ARKIME_TYPE_ALLOC(ArkimeString_t);
        hstring->str = (char *)string;
        hstring->len = len;
        hstring->utf8 = 0;
        hstring->uw = 0;
        HASH_ADD(s_, *(field->shash), hstring->str, hstring);
        goto added;
    case ARKIME_FIELD_TYPE_STR_GHASH:
        if (copy)
            string = g_strndup(string, len);
        if (g_hash_table_lookup(field->ghash, string)) {
            if (copy)
                g_free((gpointer)string);
            return NULL;
        }
        g_hash_table_add(field->ghash, (gpointer)string);
        goto added;
    default:
        LOGEXIT("ERROR - Not a string, expression: %s field: %s, tried to set '%.*s'", info->expression, info->dbFieldFull, len, string);
    }

added:
    field->jsonSize += (6 + 2 * len);

    if (field->jsonSize > FIELD_MAX_JSON_SIZE)
        session->midSave = 1;

    if (info->ruleEnabled)
        arkime_rules_run_field_set(session, pos, (const gpointer) string);

    return string;
}
/******************************************************************************/
gboolean arkime_field_string_add_lower(int pos, ArkimeSession_t *session, const char *string, int len)
{
    if (len < 0)
        len = strlen(string);

    if (len > ARKIME_FIELD_MAX_ELEMENT_SIZE) {
        len = ARKIME_FIELD_MAX_ELEMENT_SIZE;
        arkime_field_truncated(session, config.fields[pos]);
    }

    char *lower = g_ascii_strdown(string, len);
    if (!arkime_field_string_add(pos, session, lower, len, FALSE)) {
        g_free(lower);
        return FALSE;
    }
    return TRUE;
}
/******************************************************************************/
gboolean arkime_field_string_add_host(int pos, ArkimeSession_t *session, char *string, int len)
{
    char *host;

    if (len < 0) {
        len = strlen(string);
    }

    if (len > ARKIME_FIELD_MAX_ELEMENT_SIZE) {
        len = ARKIME_FIELD_MAX_ELEMENT_SIZE;
        arkime_field_truncated(session, config.fields[pos]);
    }

    if (string[len] == 0)
        host = g_hostname_to_unicode(string);
    else {
        char ch = string[len];
        string[len] = 0;
        host = g_hostname_to_unicode(string);
        string[len] = ch;
    }
    if (!host || g_utf8_validate(host, -1, NULL) == 0) {
        if (len > 4 && arkime_memstr((const char *)string, len, "xn--", 4)) {
            arkime_session_add_tag(session, "bad-punycode");
        } else {
            arkime_session_add_tag(session, "bad-hostname");
        }
        if (host)
            g_free(host);
        return FALSE;
    }

    if (!arkime_field_string_add(pos, session, host, -1, FALSE)) {
        g_free(host);
        return FALSE;
    }
    return TRUE;
}
/******************************************************************************/
const char *arkime_field_string_uw_add(int pos, ArkimeSession_t *session, const char *string, int len, gpointer uw, gboolean copy)
{
    ArkimeField_t                    *field;
    ArkimeStringHashStd_t            *hash;
    ArkimeString_t                   *hstring;
    const ArkimeFieldInfo_t          *info = config.fields[pos];

    if (pos >= session->maxFields || info->flags & ARKIME_FIELD_FLAG_DISABLED)
        return NULL;

    if (!session->fields[pos]) {
        field = ARKIME_TYPE_ALLOC(ArkimeField_t);
        session->fields[pos] = field;
        if (len < 0)
            len = strlen(string);
        if (len > ARKIME_FIELD_MAX_ELEMENT_SIZE) {
            len = ARKIME_FIELD_MAX_ELEMENT_SIZE;
            arkime_field_truncated(session, info);
        }
        field->jsonSize = 6 + info->dbFieldLen + 2 * len;
        if (copy)
            string = g_strndup(string, len);
        switch (info->type) {
        case ARKIME_FIELD_TYPE_STR_HASH:
            hash = ARKIME_TYPE_ALLOC(ArkimeStringHashStd_t);
            HASH_INIT(s_, *hash, arkime_string_hash, arkime_string_ncmp);
            field->shash = hash;
            hstring = ARKIME_TYPE_ALLOC(ArkimeString_t);
            hstring->str = (char *)string;
            hstring->len = len;
            hstring->utf8 = 0;
            hstring->uw = uw;
            HASH_ADD(s_, *hash, hstring->str, hstring);
            if (info->ruleEnabled)
                arkime_rules_run_field_set(session, pos, (const gpointer) string);
            return string;
        default:
            LOGEXIT("ERROR - Not a string hash, expression: %s field: %s, tried to set '%.*s'", info->expression, info->dbFieldFull, len, string);
        }
    }

    if (len < 0)
        len = strlen(string);

    field = session->fields[pos];
    field->jsonSize += (6 + 2 * len);

    if (len > ARKIME_FIELD_MAX_ELEMENT_SIZE) {
        len = ARKIME_FIELD_MAX_ELEMENT_SIZE;
        arkime_field_truncated(session, info);
    }

    if (field->jsonSize > FIELD_MAX_JSON_SIZE)
        session->midSave = 1;

    switch (info->type) {
    case ARKIME_FIELD_TYPE_STR_HASH:
        HASH_FIND_HASH(s_, *(field->shash), arkime_string_hash_len(string, len), string, hstring);

        if (hstring) {
            field->jsonSize -= (6 + 2 * len);
            return NULL;
        }
        hstring = ARKIME_TYPE_ALLOC(ArkimeString_t);
        if (copy)
            string = g_strndup(string, len);
        hstring->str = (char *)string;
        hstring->len = len;
        hstring->utf8 = 0;
        hstring->uw = uw;
        HASH_ADD(s_, *(field->shash), hstring->str, hstring);
        if (info->ruleEnabled)
            arkime_rules_run_field_set(session, pos, (const gpointer) string);
        return string;
    default:
        LOGEXIT("ERROR - Not a string hash, expression: %s field: %s, tried to set '%.*s'", info->expression, info->dbFieldFull, len, string);
    }
}
/******************************************************************************/
gboolean arkime_field_int_add(int pos, ArkimeSession_t *session, int i)
{
    ArkimeField_t                    *field;
    ArkimeIntHashStd_t               *hash;
    ArkimeInt_t                      *hint;
    const ArkimeFieldInfo_t          *info = config.fields[pos];

    if (pos >= session->maxFields || info->flags & ARKIME_FIELD_FLAG_DISABLED)
        return FALSE;

    if (!session->fields[pos]) {
        field = ARKIME_TYPE_ALLOC(ArkimeField_t);
        session->fields[pos] = field;
        field->jsonSize = info->dbFieldLen;
        switch (info->type) {
        case ARKIME_FIELD_TYPE_INT:
            field->i = i;
            goto added;
        case ARKIME_FIELD_TYPE_INT_ARRAY:
            field->iarray = g_array_new(FALSE, FALSE, 4);
            g_array_append_val(field->iarray, i);
            goto added;
        case ARKIME_FIELD_TYPE_INT_HASH:
            hash = ARKIME_TYPE_ALLOC(ArkimeIntHashStd_t);
            HASH_INIT(i_, *hash, arkime_int_hash, arkime_int_cmp);
            field->ihash = hash;
            hint = ARKIME_TYPE_ALLOC(ArkimeInt_t);
            HASH_ADD(i_, *hash, (void *)(long)i, hint);
            goto added;
        case ARKIME_FIELD_TYPE_INT_GHASH:
            field->ghash = g_hash_table_new(NULL, NULL);
            g_hash_table_add(field->ghash, (void *)(long)i);
            goto added;
        default:
            LOGEXIT("ERROR - Not an integer, expression: %s field: %s, tried to set '%d'", info->expression, info->dbFieldFull, i);
        }
    }

    field = session->fields[pos];
    switch (info->type) {
    case ARKIME_FIELD_TYPE_INT:
        field->i = i;
        goto added;
    case ARKIME_FIELD_TYPE_INT_ARRAY:
        g_array_append_val(field->iarray, i);
        goto added;
    case ARKIME_FIELD_TYPE_INT_HASH:
        HASH_FIND_INT(i_, *(field->ihash), i, hint);
        if (hint) {
            return FALSE;
        }
        hint = ARKIME_TYPE_ALLOC(ArkimeInt_t);
        HASH_ADD(i_, *(field->ihash), (void *)(long)i, hint);
        goto added;
    case ARKIME_FIELD_TYPE_INT_GHASH:
        if (!g_hash_table_add(field->ghash, (void *)(long)i)) {
            return FALSE;
        }
        goto added;
    default:
        LOGEXIT("ERROR - Not an integer, expression: %s field: %s, tried to set '%d'", info->expression, info->dbFieldFull, i);
    }

added:
    field->jsonSize += 13;

    if (field->jsonSize > FIELD_MAX_JSON_SIZE)
        session->midSave = 1;

    if (info->ruleEnabled)
        arkime_rules_run_field_set(session, pos, (gpointer)(long)i);

    return TRUE;
}
/******************************************************************************/
gboolean arkime_field_float_add(int pos, ArkimeSession_t *session, float f)
{
    ArkimeField_t                    *field;
    uint32_t                          fint;
    const ArkimeFieldInfo_t          *info = config.fields[pos];

    if (pos >= session->maxFields || info->flags & ARKIME_FIELD_FLAG_DISABLED)
        return FALSE;

    if (!session->fields[pos]) {
        field = ARKIME_TYPE_ALLOC(ArkimeField_t);
        session->fields[pos] = field;
        field->jsonSize = info->dbFieldLen;
        switch (info->type) {
        case ARKIME_FIELD_TYPE_FLOAT:
            field->f = f;
            goto added;
        case ARKIME_FIELD_TYPE_FLOAT_ARRAY:
            field->farray = g_array_new(FALSE, FALSE, 4);
            g_array_append_val(field->farray, f);
            goto added;
        case ARKIME_FIELD_TYPE_FLOAT_GHASH:
            field->ghash = g_hash_table_new(NULL, NULL);
            memcpy(&fint, &f, 4);
            g_hash_table_add(field->ghash, (gpointer)(long)fint);
            goto added;
        default:
            LOGEXIT("ERROR - Not a float, expression: %s field: %s, tried to set '%f'", info->expression, info->dbFieldFull, f);
        }
    }

    field = session->fields[pos];
    switch (info->type) {
    case ARKIME_FIELD_TYPE_FLOAT:
        field->f = f;
        goto added;
    case ARKIME_FIELD_TYPE_FLOAT_ARRAY:
        g_array_append_val(field->farray, f);
        goto added;
    case ARKIME_FIELD_TYPE_FLOAT_GHASH:
        memcpy(&fint, &f, 4);
        g_hash_table_add(field->ghash, (gpointer)(long)fint);
        if (!g_hash_table_add(field->ghash, (gpointer)(long)fint)) {
            return FALSE;
        }
        goto added;
    default:
        LOGEXIT("ERROR - Not a float, expression: %s field: %s, tried to set '%f'", info->expression, info->dbFieldFull, f);
    }

added:
    field->jsonSize += 15;

    if (field->jsonSize > FIELD_MAX_JSON_SIZE)
        session->midSave = 1;

    if (info->ruleEnabled) {
        memcpy(&fint, &f, 4);
        arkime_rules_run_field_set(session, pos, (gpointer)(long)fint);
    }

    return TRUE;
}
/******************************************************************************/
gboolean arkime_field_ip_equal (gconstpointer v1, gconstpointer v2)
{
    return memcmp (v1, v2, 16) == 0;
}
/******************************************************************************/
SUPPRESS_UNSIGNED_INTEGER_OVERFLOW
guint arkime_field_ip_hash (gconstpointer v)
{
    const uint8_t *p;
    guint32 h = 5381;
    unsigned int i;

    for (i = 0, p = v; i < 16; i++, p++) {
        h = (h << 5) + h + *p;
    }

    return h;
}

/******************************************************************************/
void *arkime_field_parse_ip(const char *str)
{

    struct in6_addr *v = g_malloc(sizeof(struct in6_addr));

    if (strchr(str, '.')) {
        struct in_addr addr;
        if (inet_aton(str, &addr) == 0) {
            g_free(v);
            return NULL;
        }

        memset(v->s6_addr, 0, 8);
        ((uint32_t *)v->s6_addr)[2] = htonl(0xffff);
        ((uint32_t *)v->s6_addr)[3] = addr.s_addr;
    } else {
        if (inet_pton(AF_INET6, str, v) == 0) {
            g_free(v);
            return NULL;
        }
    }

    return v;
}
/******************************************************************************/
gboolean arkime_field_ip_add_str(int pos, ArkimeSession_t *session, const char *str)
{
    ArkimeField_t                    *field;
    const ArkimeFieldInfo_t          *info = config.fields[pos];

    if (pos >= session->maxFields || info->flags & ARKIME_FIELD_FLAG_DISABLED)
        return FALSE;

    int len = strlen(str);
    struct in6_addr *v = arkime_field_parse_ip(str);

    if (!v) {
        return FALSE;
    }

    if (!session->fields[pos]) {
        field = ARKIME_TYPE_ALLOC(ArkimeField_t);
        session->fields[pos] = field;
        field->jsonSize = info->dbFieldLen;
        switch (info->type) {
        case ARKIME_FIELD_TYPE_IP:
            field->ip = v;
            goto added;
        case ARKIME_FIELD_TYPE_IP_GHASH:
            field->ghash = g_hash_table_new_full(arkime_field_ip_hash, arkime_field_ip_equal, g_free, NULL);
            g_hash_table_add(field->ghash, v);
            goto added;
        default:
            LOGEXIT("ERROR - Not an ip, expression: %s field: %s, tried to set '%s'", info->expression, info->dbFieldFull, str);
        }
    }

    field = session->fields[pos];
    switch (info->type) {
    case ARKIME_FIELD_TYPE_IP:
        g_free(field->ip);
        field->ip = v;
        goto added;
    case ARKIME_FIELD_TYPE_IP_GHASH:
        if (!g_hash_table_add(field->ghash, v)) {
            return FALSE;
        } else {
            goto added;
        }
    default:
        LOGEXIT("ERROR - Not an ip, expression: %s field: %s, tried to set '%s'", info->expression, info->dbFieldFull, str);
    }

added:
    field->jsonSize += (3 + len + 100);

    if (field->jsonSize > FIELD_MAX_JSON_SIZE)
        session->midSave = 1;

    if (info->ruleEnabled)
        arkime_rules_run_field_set(session, pos, v);

    return TRUE;
}
/******************************************************************************/
gboolean arkime_field_ip4_add(int pos, ArkimeSession_t *session, uint32_t i)
{
    ArkimeField_t                    *field;
    const ArkimeFieldInfo_t          *info = config.fields[pos];
    char                              ipbuf[INET6_ADDRSTRLEN];

    if (pos >= session->maxFields || info->flags & ARKIME_FIELD_FLAG_DISABLED)
        return FALSE;

    struct in6_addr *v = g_malloc(sizeof(struct in6_addr));

    memset(v->s6_addr, 0, 8);
    ((uint32_t *)v->s6_addr)[2] = htonl(0xffff);
    ((uint32_t *)v->s6_addr)[3] = i;

    if (!session->fields[pos]) {
        field = ARKIME_TYPE_ALLOC(ArkimeField_t);
        session->fields[pos] = field;
        field->jsonSize = info->dbFieldLen;
        switch (info->type) {
        case ARKIME_FIELD_TYPE_IP:
            field->ip = v;
            goto added;
        case ARKIME_FIELD_TYPE_IP_GHASH:
            field->ghash = g_hash_table_new_full(arkime_field_ip_hash, arkime_field_ip_equal, g_free, NULL);
            g_hash_table_add(field->ghash, v);
            goto added;
        default:
            snprintf(ipbuf, sizeof(ipbuf), "%u.%u.%u.%u", i & 0xff, (i >> 8) & 0xff, (i >> 16) & 0xff, (i >> 24) & 0xff);
            LOGEXIT("ERROR - Not an ip, expression: %s field: %s, tried to set '%s'", info->expression, info->dbFieldFull, ipbuf);
        }
    }

    field = session->fields[pos];
    switch (info->type) {
    case ARKIME_FIELD_TYPE_IP:
        g_free(field->ip);
        field->ip = v;
        goto added;
    case ARKIME_FIELD_TYPE_IP_GHASH:
        if (!g_hash_table_add(field->ghash, v)) {
            return FALSE;
        } else {
            goto added;
        }
    default:
        snprintf(ipbuf, sizeof(ipbuf), "%u.%u.%u.%u", i & 0xff, (i >> 8) & 0xff, (i >> 16) & 0xff, (i >> 24) & 0xff);
        LOGEXIT("ERROR - Not an ip, expression: %s field: %s, tried to set '%s'", info->expression, info->dbFieldFull, ipbuf);
    }

added:
    field->jsonSize += (3 + 15 + 100);

    if (field->jsonSize > FIELD_MAX_JSON_SIZE)
        session->midSave = 1;

    if (info->ruleEnabled)
        arkime_rules_run_field_set(session, pos, v);

    return TRUE;
}
/******************************************************************************/
gboolean arkime_field_ip6_add(int pos, ArkimeSession_t *session, const uint8_t *val)
{
    ArkimeField_t                    *field;
    const ArkimeFieldInfo_t          *info = config.fields[pos];
    char                              ipbuf[INET6_ADDRSTRLEN];

    if (pos >= session->maxFields || info->flags & ARKIME_FIELD_FLAG_DISABLED)
        return FALSE;

    struct in6_addr *v = g_memdup(val, sizeof(struct in6_addr));

    if (!session->fields[pos]) {
        field = ARKIME_TYPE_ALLOC(ArkimeField_t);
        session->fields[pos] = field;
        field->jsonSize = info->dbFieldLen;
        switch (info->type) {
        case ARKIME_FIELD_TYPE_IP:
            field->ip = v;
            goto added;
        case ARKIME_FIELD_TYPE_IP_GHASH:
            field->ghash = g_hash_table_new_full(arkime_field_ip_hash, arkime_field_ip_equal, g_free, NULL);
            g_hash_table_add(field->ghash, v);
            goto added;
        default:
            inet_ntop(AF_INET6, v, ipbuf, sizeof(ipbuf));
            LOGEXIT("ERROR - Not an ip, expression: %s field: %s, tried to set '%s'", info->expression, info->dbFieldFull, ipbuf);
        }
    }

    field = session->fields[pos];
    switch (info->type) {
    case ARKIME_FIELD_TYPE_IP:
        g_free(field->ip);
        field->ip = v;
        goto added;
    case ARKIME_FIELD_TYPE_IP_GHASH:
        if (!g_hash_table_add(field->ghash, v)) {
            return FALSE;
        } else {
            goto added;
        }
    default:
        inet_ntop(AF_INET6, v, ipbuf, sizeof(ipbuf));
        LOGEXIT("ERROR - Not an ip, expression: %s field: %s, tried to set '%s'", info->expression, info->dbFieldFull, ipbuf);
    }

added:
    field->jsonSize += (3 + 30 + 100);

    if (field->jsonSize > FIELD_MAX_JSON_SIZE)
        session->midSave = 1;

    if (info->ruleEnabled)
        arkime_rules_run_field_set(session, pos, v);

    return TRUE;
}
/******************************************************************************/
void arkime_field_macoui_add(ArkimeSession_t *session, int macField, int ouiField, const uint8_t *mac)
{
    char str[20];

    snprintf(str, sizeof(str), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0],
             mac[1],
             mac[2],
             mac[3],
             mac[4],
             mac[5]);

    if (arkime_field_string_add(macField, session, str, 17, TRUE))
        arkime_db_oui_lookup(ouiField, session, mac);
}
/******************************************************************************/
void arkime_field_free(ArkimeSession_t *session)
{
    int                         pos;
    ArkimeString_t             *hstring;
    ArkimeStringHashStd_t      *shash;
    ArkimeInt_t                *hint;
    ArkimeIntHashStd_t         *ihash;
    ArkimeFieldObject_t        *ho;
    ArkimeFieldObjectHashStd_t *ohash;
    ArkimeFieldObjectFreeFunc   freeCB;

    for (pos = 0; pos < session->maxFields; pos++) {
        ArkimeField_t        *field;

        if (!(field = session->fields[pos]))
            continue;

        switch (config.fields[pos]->type) {
        case ARKIME_FIELD_TYPE_STR:
            g_free(field->str);
            break;
        case ARKIME_FIELD_TYPE_STR_ARRAY:
            g_ptr_array_free(field->sarray, TRUE);
            break;
        case ARKIME_FIELD_TYPE_STR_HASH:
            shash = session->fields[pos]->shash;
            HASH_FORALL_POP_HEAD2(s_, *shash, hstring) {
                g_free(hstring->str);
                ARKIME_TYPE_FREE(ArkimeString_t, hstring);
            }
            ARKIME_TYPE_FREE(ArkimeStringHashStd_t, shash);
            break;
        case ARKIME_FIELD_TYPE_INT:
            break;
        case ARKIME_FIELD_TYPE_INT_ARRAY:
            g_array_free(field->iarray, TRUE);
            break;
        case ARKIME_FIELD_TYPE_INT_HASH:
            ihash = session->fields[pos]->ihash;
            HASH_FORALL_POP_HEAD2(i_, *ihash, hint) {
                ARKIME_TYPE_FREE(ArkimeInt_t, hint);
            }
            ARKIME_TYPE_FREE(ArkimeIntHashStd_t, ihash);
            break;
        case ARKIME_FIELD_TYPE_FLOAT:
            break;
        case ARKIME_FIELD_TYPE_FLOAT_ARRAY:
            g_array_free(field->farray, TRUE);
            break;
        case ARKIME_FIELD_TYPE_IP:
            g_free(session->fields[pos]->ip);
            break;
        case ARKIME_FIELD_TYPE_IP_GHASH:
        case ARKIME_FIELD_TYPE_INT_GHASH:
        case ARKIME_FIELD_TYPE_STR_GHASH:
        case ARKIME_FIELD_TYPE_FLOAT_GHASH:
            g_hash_table_destroy(session->fields[pos]->ghash);
            break;
        case ARKIME_FIELD_TYPE_OBJECT: {
            freeCB = config.fields[pos]->object_free;
            ohash = session->fields[pos]->ohash;
            HASH_FORALL_POP_HEAD2(o_, *ohash, ho) {
                freeCB(ho);
            }
            ARKIME_TYPE_FREE(ArkimeFieldObjectHashStd_t, ohash);
        }
        break;
        } // switch
        ARKIME_TYPE_FREE(ArkimeField_t, session->fields[pos]);
    }
    ARKIME_SIZE_FREE(fields, session->fields);
    session->fields = 0;
}
/******************************************************************************/
int arkime_field_object_register(const char *name, const char *help, ArkimeFieldObjectSaveFunc save, ArkimeFieldObjectFreeFunc free, ArkimeFieldObjectHashFunc hash, ArkimeFieldObjectCmpFunc cmp)
{
    int object_pos;
    ArkimeFieldInfo_t *object_info;

    object_pos = arkime_field_define(name, "notreal",
                                     name, name, name,
                                     help,
                                     ARKIME_FIELD_TYPE_OBJECT, ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_NODB,
                                     (char *)NULL);

    // This should never be the case but better safe than sorry
    if (object_pos == -1) {
        LOGEXIT("ERROR - Field object position is %d", object_pos);
    }

    object_info = config.fields[object_pos];

    // This shouldn't happen but lets be sure
    if (!object_info) {
        LOGEXIT("ERROR - Field object info is null");
    }

    object_info->object_save = save;
    object_info->object_free = free;
    object_info->object_hash = hash;
    object_info->object_cmp = cmp;

    return object_info->pos;
}
/******************************************************************************/
gboolean arkime_field_object_add(int pos, ArkimeSession_t *session, ArkimeFieldObject_t *object, int len)
{
    ArkimeField_t               *field;
    ArkimeFieldObjectHashStd_t  *hash;
    ArkimeFieldObject_t         *ho;

    if (!session->fields[pos]) {
        field = ARKIME_TYPE_ALLOC(ArkimeField_t);
        session->fields[pos] = field;
        // 3 for the quotes and colon
        // length of the object name
        // 4 for the brackets and braces
        // len should be the length of the contents of the object
        field->jsonSize = 3 + config.fields[pos]->dbFieldLen + 4 + len;
        switch (config.fields[pos]->type) {
        case ARKIME_FIELD_TYPE_OBJECT:
            hash = ARKIME_TYPE_ALLOC(ArkimeFieldObjectHashStd_t);
            HASH_INIT(o_, *hash, config.fields[pos]->object_hash, config.fields[pos]->object_cmp);
            field->ohash = hash;
            HASH_ADD(o_, *hash, object->object, object);
            return TRUE;
        default:
            LOGEXIT("ERROR - Not a field object %s field", config.fields[pos]->dbField);
        }
    }

    field = session->fields[pos];
    switch (config.fields[pos]->type) {
    case ARKIME_FIELD_TYPE_OBJECT:
        HASH_FIND(o_, *(field->ohash), object->object, ho);
        if (ho) {
            field->jsonSize += len;
            return FALSE;
        }
        // 3 for braces and comma
        // len should be the length of contents of the object
        field->jsonSize += 3 + len;
        HASH_ADD(o_, *(field->ohash), object->object, object);
        return TRUE;
    default:
        LOGEXIT("ERROR - Not a field object %s field", config.fields[pos]->dbField);
    }
}

/******************************************************************************/
int arkime_field_count(int pos, ArkimeSession_t *session)
{
    ArkimeField_t         *field;

    if (pos >= session->maxFields)
        return 0;

    if (!session->fields[pos])
        return 0;

    field = session->fields[pos];

    switch (config.fields[pos]->type) {
    case ARKIME_FIELD_TYPE_INT:
    case ARKIME_FIELD_TYPE_STR:
    case ARKIME_FIELD_TYPE_IP:
        return 1;
    case ARKIME_FIELD_TYPE_STR_ARRAY:
        return field->sarray->len;
    case ARKIME_FIELD_TYPE_INT_ARRAY:
        return field->iarray->len;
    case ARKIME_FIELD_TYPE_STR_HASH:
        return HASH_COUNT(s_, *(field->shash));
    case ARKIME_FIELD_TYPE_INT_HASH:
        return HASH_COUNT(s_, *(field->ihash));
    case ARKIME_FIELD_TYPE_IP_GHASH:
    case ARKIME_FIELD_TYPE_INT_GHASH:
    case ARKIME_FIELD_TYPE_STR_GHASH:
        return g_hash_table_size(field->ghash);
    case ARKIME_FIELD_TYPE_OBJECT:
        return HASH_COUNT(o_, *(field->ohash));
    default:
        LOGEXIT("ERROR - Unknown field type for counting %s %d", config.fields[pos]->dbField, config.fields[pos]->type);
    }
}
/******************************************************************************/
LOCAL int arkime_field_ops_should_run_int_op(const ArkimeFieldOp_t *op, int value)
{
    switch (op->set) {
    case ARKIME_FIELD_OP_SET:
        if (op->strLenOrInt == value)
            return 0; // Don't run since value is the same
        return 1;
    case ARKIME_FIELD_OP_SET_IF_MORE:
        if (op->strLenOrInt > value)
            return 1;
        return 0;
    case ARKIME_FIELD_OP_SET_IF_LESS:
        if (op->strLenOrInt < value)
            return 1;
        return 0;
    }
    return 0;
}
/******************************************************************************/
void arkime_field_ops_run_match(ArkimeSession_t *session, ArkimeFieldOps_t *ops, int matchPos)
{
    int i;

    for (i = 0; i < ops->num; i++) {
        const ArkimeFieldOp_t *op = &(ops->ops[i]);
        int16_t fieldPos = op->fieldPos;

        // Special field pos that really are setting a field in sessions
        if (fieldPos < 0) {
            switch (fieldPos) {
            case ARKIME_FIELD_SPECIAL_STOP_SPI:
                if (arkime_field_ops_should_run_int_op(op, session->stopSPI))
                    arkime_session_set_stop_spi(session, op->strLenOrInt);
                break;
            case ARKIME_FIELD_SPECIAL_STOP_PCAP:
                if (arkime_field_ops_should_run_int_op(op, session->stopSaving)) {
                    session->stopSaving = op->strLenOrInt;
                    if (session->packets[0] + session->packets[1] >= session->stopSaving)
                        arkime_session_add_tag(session, "truncated-pcap");
                }
                break;
            case ARKIME_FIELD_SPECIAL_MIN_SAVE:
                if (arkime_field_ops_should_run_int_op(op, session->minSaving)) {
                    session->minSaving = op->strLenOrInt;
                }
                break;
            case ARKIME_FIELD_SPECIAL_DROP_SRC:
                arkime_packet_drophash_add(session, 0, op->strLenOrInt);
                break;
            case ARKIME_FIELD_SPECIAL_DROP_DST:
                arkime_packet_drophash_add(session, 1, op->strLenOrInt);
                break;
            case ARKIME_FIELD_SPECIAL_DROP_SESSION:
                arkime_packet_drophash_add(session, -1, op->strLenOrInt);
                break;
            case ARKIME_FIELD_SPECIAL_STOP_YARA:
                if (arkime_field_ops_should_run_int_op(op, session->stopYara)) {
                    session->stopYara = op->strLenOrInt;
                }
                break;
            }
            continue;
        }

        // If matchPos isn't -1 then we are doing a match and we need to remap the fieldPos
        if (matchPos >= 0 && fieldOpsRemap[fieldPos][matchPos] >= 0) {
            fieldPos = fieldOpsRemap[fieldPos][matchPos];
        }

        // Internal Fields
        if (fieldPos >= config.minInternalField) {
            // ALW TODO
            continue;
        }

        switch (config.fields[fieldPos]->type) {
        case ARKIME_FIELD_TYPE_INT:
            if (arkime_field_count(fieldPos, session) == 0 ||
                arkime_field_ops_should_run_int_op(op, session->fields[fieldPos]->i)) {

                arkime_field_int_add(fieldPos, session, op->strLenOrInt);
            }
            break;
        case ARKIME_FIELD_TYPE_INT_HASH:
        case ARKIME_FIELD_TYPE_INT_GHASH:
        case ARKIME_FIELD_TYPE_INT_ARRAY:
            arkime_field_int_add(fieldPos, session, op->strLenOrInt);
            break;
        case ARKIME_FIELD_TYPE_FLOAT_GHASH:
        case ARKIME_FIELD_TYPE_FLOAT:
        case ARKIME_FIELD_TYPE_FLOAT_ARRAY:
            arkime_field_float_add(fieldPos, session, op->f);
            break;
        case ARKIME_FIELD_TYPE_IP:
        case ARKIME_FIELD_TYPE_IP_GHASH:
            arkime_field_ip_add_str(fieldPos, session, op->str);
            break;
        case ARKIME_FIELD_TYPE_STR:
        case ARKIME_FIELD_TYPE_STR_ARRAY:
        case ARKIME_FIELD_TYPE_STR_HASH:
        case ARKIME_FIELD_TYPE_STR_GHASH:
            arkime_field_string_add(fieldPos, session, op->str, op->strLenOrInt, TRUE);
            break;
        case ARKIME_FIELD_TYPE_OBJECT:
            // Unsupported
            break;
        }
    }
}
/******************************************************************************/
void arkime_field_ops_run(ArkimeSession_t *session, ArkimeFieldOps_t *ops)
{
    arkime_field_ops_run_match(session, ops, -1);
}
/******************************************************************************/
void arkime_field_ops_free(ArkimeFieldOps_t *ops)
{
    if (ops->flags & ARKIME_FIELD_OPS_FLAGS_COPY) {
        int i;
        for (i = 0; i < ops->num; i++) {
            g_free(ops->ops[i].str);
        }
    }
    if (ops->ops)
        free(ops->ops);
    ops->ops = NULL;
    ops->size = 0;
    ops->num = 0;
}
/******************************************************************************/
void arkime_field_ops_init(ArkimeFieldOps_t *ops, int numOps, uint16_t flags)
{
    ops->num   = 0;
    ops->size  = numOps;
    ops->flags = flags;

    if (numOps > 0)
        ops->ops = malloc(numOps * sizeof(ArkimeFieldOp_t));
    else
        ops->ops = NULL;
}


/******************************************************************************/
LOCAL void arkime_field_ops_int_parse(ArkimeFieldOp_t *op, const char *value)
{
    int len;
    switch (value[0]) {
    case '<':
        op->set = ARKIME_FIELD_OP_SET_IF_LESS;
        op->strLenOrInt = atoi(value + 1);
        break;
    case '>':
        op->set = ARKIME_FIELD_OP_SET_IF_MORE;
        op->strLenOrInt = atoi(value + 1);
        break;
    case '=':
        op->set = ARKIME_FIELD_OP_SET;
        op->strLenOrInt = atoi(value + 1);
        break;
    case 'm':
        len = strlen(value);
        if (len < 5) {
            op->set = ARKIME_FIELD_OP_SET;
            op->strLenOrInt = 0;
        } else if (strncmp(value, "min ", 4) == 0) {
            op->set = ARKIME_FIELD_OP_SET_IF_LESS;
            op->strLenOrInt = atoi(value + 4);
        } else if (strncmp(value, "max ", 4) == 0) {
            op->set = ARKIME_FIELD_OP_SET_IF_MORE;
            op->strLenOrInt = atoi(value + 4);
        } else {
            op->set = ARKIME_FIELD_OP_SET;
            op->strLenOrInt = 0;
        }
        break;
    default:
        op->set = ARKIME_FIELD_OP_SET;
        op->strLenOrInt = atoi(value);
        break;
    }
}
/******************************************************************************/
void arkime_field_ops_add_match(ArkimeFieldOps_t *ops, int fieldPos, char *value, int valuelen, int matchPos)
{
    if (ops->num >= ops->size) {
        ops->size = ceil (ops->size * 1.6);
        ops->ops = realloc(ops->ops, ops->size * sizeof(ArkimeFieldOp_t));
    }

    if (fieldPos == -1 || fieldPos > config.maxDbField) {
        LOG("WARNING - Not adding %d %s %d", fieldPos, value, valuelen);
        return;
    }

    ArkimeFieldOp_t *op = &(ops->ops[ops->num]);

    op->fieldPos = fieldPos;
    op->matchPos = matchPos;

    if (fieldPos < 0) {
        switch (op->fieldPos) {
        case ARKIME_FIELD_SPECIAL_STOP_SPI:
            arkime_field_ops_int_parse(op, value);
            if (op->strLenOrInt > 1) op->strLenOrInt = 1;
            if (op->strLenOrInt < 0) op->strLenOrInt = 0;
            op->str = 0;
            break;
        case ARKIME_FIELD_SPECIAL_STOP_PCAP:
            arkime_field_ops_int_parse(op, value);
            if (op->strLenOrInt > 0xffff) op->strLenOrInt = 0xffff;
            if (op->strLenOrInt < 0) op->strLenOrInt = 0;
            op->str = 0;
            break;
        case ARKIME_FIELD_SPECIAL_MIN_SAVE:
            arkime_field_ops_int_parse(op, value);
            if (op->strLenOrInt > 0xff) op->strLenOrInt = 0xff;
            if (op->strLenOrInt < 0) op->strLenOrInt = 0;
            op->str = 0;
            break;
        case ARKIME_FIELD_SPECIAL_DROP_SRC:
        case ARKIME_FIELD_SPECIAL_DROP_DST:
        case ARKIME_FIELD_SPECIAL_DROP_SESSION:
            op->strLenOrInt = atoi(value);
            if (op->strLenOrInt < 0) op->strLenOrInt = 0;
            op->str = 0;
            break;
        case ARKIME_FIELD_SPECIAL_STOP_YARA:
            arkime_field_ops_int_parse(op, value);
            if (op->strLenOrInt > 1) op->strLenOrInt = 1;
            if (op->strLenOrInt < 0) op->strLenOrInt = 0;
            op->str = 0;
            break;
        default:
            LOG("WARNING - Unknown special field pos %d", fieldPos);
            break;
        }
    } else {
        if (fieldPos >= config.minInternalField && !config.fields[fieldPos]->setCb) {
            LOG("WARNING - not allow to set %s", config.fields[fieldPos]->expression);
            return;
        }

        switch (config.fields[fieldPos]->type) {
        case  ARKIME_FIELD_TYPE_INT:
            op->str = 0;
            arkime_field_ops_int_parse(op, value);
            break;
        case  ARKIME_FIELD_TYPE_INT_HASH:
        case  ARKIME_FIELD_TYPE_INT_GHASH:
        case  ARKIME_FIELD_TYPE_INT_ARRAY:
            op->str = 0;
            op->strLenOrInt = atoi(value);
            break;
        case  ARKIME_FIELD_TYPE_FLOAT_GHASH:
        case  ARKIME_FIELD_TYPE_FLOAT:
        case  ARKIME_FIELD_TYPE_FLOAT_ARRAY:
            op->str = 0;
            op->f = atof(value);
            break;
        case  ARKIME_FIELD_TYPE_STR:
        case  ARKIME_FIELD_TYPE_STR_ARRAY:
        case  ARKIME_FIELD_TYPE_STR_HASH:
        case  ARKIME_FIELD_TYPE_STR_GHASH:
        case  ARKIME_FIELD_TYPE_IP:
        case  ARKIME_FIELD_TYPE_IP_GHASH:
            if (valuelen == -1)
                valuelen = strlen(value);
            if (ops->flags & ARKIME_FIELD_OPS_FLAGS_COPY)
                op->str = g_strndup(value, valuelen);
            else
                op->str = value;
            op->strLenOrInt = valuelen;
            break;
        default:
            LOG("WARNING - Unsupported expression type %d for %s", config.fields[fieldPos]->type, value);
            return;
        }
    }
    ops->num++;
}
/******************************************************************************/
void arkime_field_ops_add(ArkimeFieldOps_t *ops, int fieldPos, char *value, int valuelen)
{
    arkime_field_ops_add_match(ops, fieldPos, value, valuelen, -1);
}
/******************************************************************************/
// Parse oldExpr=matchExpr1=newExpr1;matchExpr2=newExpr2
gboolean arkime_field_load_field_remap (gpointer UNUSED(user_data))
{
    gsize keys_len;
    int   i;

    gchar **keys = arkime_config_section_keys(NULL, "custom-fields-remap", &keys_len);

    if (!keys)
        return G_SOURCE_REMOVE;

    for (i = 0; i < (int)keys_len; i++) {
        int oldPos = arkime_field_by_exp(keys[i]);
        if (oldPos == -1) {
            LOG("WARNING - Unknown field '%s', not remapping", keys[i]);
            continue;
        }
        gchar *info = arkime_config_section_str(NULL, "custom-fields-remap", keys[i], NULL);

        char **kvs = g_strsplit(info, ";", 0);
        for (int k = 0; kvs[k]; k++) {
            char *key = kvs[k];
            char *value = strchr(key, '=');
            if (!value)
                continue;
            *value = 0;
            value++;
            while (isspace(*key)) key++;
            g_strchomp(key);
            while (isspace(*value)) value++;
            g_strchomp(value);
            int matchPos = arkime_field_by_exp(key);
            if (matchPos == -1) {
                LOG("WARNING - Unknown field '%s', not remapping", key);
                continue;
            }
            int newPos = arkime_field_by_exp(value);
            if (newPos == -1) {
                LOG("WARNING - Unknown field '%s', not remapping", value);
                continue;
            }
            fieldOpsRemap[oldPos][matchPos] = newPos;
        }
        g_strfreev(kvs);
        g_free(info);
    }
    g_strfreev(keys);
    return G_SOURCE_REMOVE;
}
/******************************************************************************/
LOCAL void *arkime_field_getcb_src_ip(ArkimeSession_t *session, int UNUSED(pos))
{
    return &session->addr1;
}
/******************************************************************************/
LOCAL void *arkime_field_getcb_src_port(ArkimeSession_t *session, int UNUSED(pos))
{
    return (void *)(long)session->port1;
}
/******************************************************************************/
LOCAL void *arkime_field_getcb_dst_ip(ArkimeSession_t *session, int UNUSED(pos))
{
    return &session->addr2;
}
/******************************************************************************/
LOCAL void *arkime_field_getcb_dst_port(ArkimeSession_t *session, int UNUSED(pos))
{
    return (void *)(long)session->port2;
}
/******************************************************************************/
LOCAL void *arkime_field_getcb_tcpflags_syn(ArkimeSession_t *session, int UNUSED(pos))
{
    return (void *)(long)session->tcpFlagCnt[ARKIME_TCPFLAG_SYN];
}
/******************************************************************************/
LOCAL void *arkime_field_getcb_tcpflags_syn_ack(ArkimeSession_t *session, int UNUSED(pos))
{
    return (void *)(long)session->tcpFlagCnt[ARKIME_TCPFLAG_SYN_ACK];
}
/******************************************************************************/
LOCAL void *arkime_field_getcb_tcpflags_ack(ArkimeSession_t *session, int UNUSED(pos))
{
    return (void *)(long)session->tcpFlagCnt[ARKIME_TCPFLAG_ACK];
}
/******************************************************************************/
LOCAL void *arkime_field_getcb_tcpflags_psh(ArkimeSession_t *session, int UNUSED(pos))
{
    return (void *)(long)session->tcpFlagCnt[ARKIME_TCPFLAG_PSH];
}
/******************************************************************************/
LOCAL void *arkime_field_getcb_tcpflags_rst(ArkimeSession_t *session, int UNUSED(pos))
{
    return (void *)(long)session->tcpFlagCnt[ARKIME_TCPFLAG_RST];
}
/******************************************************************************/
LOCAL void *arkime_field_getcb_tcpflags_fin(ArkimeSession_t *session, int UNUSED(pos))
{
    return (void *)(long)session->tcpFlagCnt[ARKIME_TCPFLAG_FIN];
}
/******************************************************************************/
LOCAL void *arkime_field_getcb_tcpflags_urg(ArkimeSession_t *session, int UNUSED(pos))
{
    return (void *)(long)session->tcpFlagCnt[ARKIME_TCPFLAG_URG];
}
/******************************************************************************/
LOCAL void *arkime_field_getcb_packets_src(ArkimeSession_t *session, int UNUSED(pos))
{
    return (void *)(long)session->packets[0];
}
/******************************************************************************/
LOCAL void *arkime_field_getcb_packets_dst(ArkimeSession_t *session, int UNUSED(pos))
{
    return (void *)(long)session->packets[1];
}
/******************************************************************************/
LOCAL void *arkime_field_getcb_databytes_src(ArkimeSession_t *session, int UNUSED(pos))
{
    return (void *)(long)session->databytes[0];
}
/******************************************************************************/
LOCAL void *arkime_field_getcb_databytes_dst(ArkimeSession_t *session, int UNUSED(pos))
{
    return (void *)(long)session->databytes[1];
}
/******************************************************************************/
LOCAL void *arkime_field_getcb_community_id(ArkimeSession_t *session, int UNUSED(pos))
{

    if (session->ses == SESSION_OTHER) {
        return NULL;
    }
    char *communityId;
    if (session->ses == SESSION_ICMP) {
        communityId = arkime_db_community_id_icmp(session);
    } else {
        communityId = arkime_db_community_id(session);
    }
    arkime_free_later(communityId, g_free);

    return communityId;
}
/******************************************************************************/
LOCAL void *arkime_field_getcb_dst_ip_port(ArkimeSession_t *session, int UNUSED(pos))
{
    char *ipstr = g_malloc(INET6_ADDRSTRLEN + 10);

    if (IN6_IS_ADDR_V4MAPPED(&session->addr2)) {
        uint32_t ip = ARKIME_V6_TO_V4(session->addr2);
        snprintf(ipstr, INET6_ADDRSTRLEN + 10, "%u.%u.%u.%u:%d", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff, session->port2);
    } else {
        inet_ntop(AF_INET6, &session->addr2, ipstr, sizeof(ipstr));
        int len = strlen(ipstr);
        snprintf(ipstr + len, INET6_ADDRSTRLEN + 10 - len, ".%d", session->port2);
    }

    arkime_free_later(ipstr, g_free);

    return ipstr;
}
/******************************************************************************/
void arkime_field_init()
{
    config.maxDbField = 0;
    config.minInternalField = ARKIME_FIELDS_MAX;
    HASH_INIT(d_, fieldsByDb, arkime_string_hash, arkime_string_cmp);
    HASH_INIT(e_, fieldsByExp, arkime_string_hash, (HASH_CMP_FUNC)arkime_field_exp_cmp);
    groupName2Num = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    arkime_field_by_exp_add_special("dontSaveSPI", ARKIME_FIELD_SPECIAL_STOP_SPI);
    arkime_field_by_exp_add_special("_dontSaveSPI", ARKIME_FIELD_SPECIAL_STOP_SPI);
    arkime_field_by_exp_add_special("_maxPacketsToSave", ARKIME_FIELD_SPECIAL_STOP_PCAP);
    arkime_field_by_exp_add_special("_minPacketsBeforeSavingSPI", ARKIME_FIELD_SPECIAL_MIN_SAVE);
    arkime_field_by_exp_add_special("_dropBySrc", ARKIME_FIELD_SPECIAL_DROP_SRC);
    arkime_field_by_exp_add_special("_dropByDst", ARKIME_FIELD_SPECIAL_DROP_DST);
    arkime_field_by_exp_add_special("_dropBySession", ARKIME_FIELD_SPECIAL_DROP_SESSION);
    arkime_field_by_exp_add_special("_dontCheckYara", ARKIME_FIELD_SPECIAL_STOP_YARA);

    arkime_field_by_exp_add_internal("ip.src", ARKIME_FIELD_TYPE_IP, arkime_field_getcb_src_ip, NULL);
    arkime_field_by_exp_add_internal("port.src", ARKIME_FIELD_TYPE_INT, arkime_field_getcb_src_port, NULL);
    arkime_field_by_exp_add_internal("ip.dst", ARKIME_FIELD_TYPE_IP, arkime_field_getcb_dst_ip, NULL);
    arkime_field_by_exp_add_internal("port.dst", ARKIME_FIELD_TYPE_INT, arkime_field_getcb_dst_port, NULL);

    arkime_field_by_exp_add_internal("tcpflags.syn", ARKIME_FIELD_TYPE_INT, arkime_field_getcb_tcpflags_syn, NULL);
    arkime_field_by_exp_add_internal("tcpflags.syn-ack", ARKIME_FIELD_TYPE_INT, arkime_field_getcb_tcpflags_syn_ack, NULL);
    arkime_field_by_exp_add_internal("tcpflags.ack", ARKIME_FIELD_TYPE_INT, arkime_field_getcb_tcpflags_ack, NULL);
    arkime_field_by_exp_add_internal("tcpflags.psh", ARKIME_FIELD_TYPE_INT, arkime_field_getcb_tcpflags_psh, NULL);
    arkime_field_by_exp_add_internal("tcpflags.rst", ARKIME_FIELD_TYPE_INT, arkime_field_getcb_tcpflags_rst, NULL);
    arkime_field_by_exp_add_internal("tcpflags.fin", ARKIME_FIELD_TYPE_INT, arkime_field_getcb_tcpflags_fin, NULL);
    arkime_field_by_exp_add_internal("tcpflags.urg", ARKIME_FIELD_TYPE_INT, arkime_field_getcb_tcpflags_urg, NULL);

    arkime_field_by_exp_add_internal("packets.src", ARKIME_FIELD_TYPE_INT, arkime_field_getcb_packets_src, NULL);
    arkime_field_by_exp_add_internal("packets.dst", ARKIME_FIELD_TYPE_INT, arkime_field_getcb_packets_dst, NULL);
    arkime_field_by_exp_add_internal("databytes.src", ARKIME_FIELD_TYPE_INT, arkime_field_getcb_databytes_src, NULL);
    arkime_field_by_exp_add_internal("databytes.dst", ARKIME_FIELD_TYPE_INT, arkime_field_getcb_databytes_dst, NULL);
    arkime_field_by_exp_add_internal("communityId", ARKIME_FIELD_TYPE_STR, arkime_field_getcb_community_id, NULL);

    arkime_field_by_exp_add_internal("ip.dst:port", ARKIME_FIELD_TYPE_STR, arkime_field_getcb_dst_ip_port, NULL);
    arkime_field_by_exp_add_internal("dst.ip:port", ARKIME_FIELD_TYPE_STR, arkime_field_getcb_dst_ip_port, NULL);

    // Wait until about to start listening to remap
    memset(fieldOpsRemap, -1, sizeof(fieldOpsRemap));
    g_timeout_add(0, arkime_field_load_field_remap, 0);
}
/******************************************************************************/
void arkime_field_exit()
{
    ArkimeFieldInfo_t *info;

    // Remove those are in both db & exp hash
    HASH_FORALL_POP_HEAD2(d_, fieldsByDb, info) {
        HASH_REMOVE(e_, fieldsByExp, info);
        arkime_field_free_info(info);
    }

    // Remove those are only in exp
    HASH_FORALL_POP_HEAD2(d_, fieldsByExp, info) {
        g_free(info->expression);
    }
}
