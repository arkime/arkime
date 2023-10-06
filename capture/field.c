/******************************************************************************/
/* field.c  -- Functions dealing with declaring fields
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
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

/******************************************************************************/
void arkime_field_by_exp_add_special(char *exp, int pos)
{
    ArkimeFieldInfo_t *info = ARKIME_TYPE_ALLOC0(ArkimeFieldInfo_t);
    info->expression = g_strdup(exp);
    info->pos = pos;
    HASH_ADD(e_, fieldsByExp, info->expression, info);
}
/******************************************************************************/
void arkime_field_by_exp_add_special_type(char *exp, int pos, ArkimeFieldType type)
{
    ArkimeFieldInfo_t *info = ARKIME_TYPE_ALLOC0(ArkimeFieldInfo_t);
    info->expression   = g_strdup(exp);
    info->pos          = pos;
    info->type         = type;
    config.fields[pos] = info;
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
void arkime_field_define_json(uint8_t *expression, int expression_len, uint8_t *data, int data_len)
{
    ArkimeFieldInfo_t *info = ARKIME_TYPE_ALLOC0(ArkimeFieldInfo_t);
    int                i;
    uint32_t           out[4 * 100]; // Can have up to 100 elements at any level
    int                disabled = 0;

    if (js0n(data, data_len, out, sizeof(out)) != 0) {
        LOGEXIT("ERROR - Parse error for >%.*s<\n", data_len, data);
    }

    info->expression = g_strndup((char*)expression, expression_len);
    for (i = 0; out[i]; i += 4) {
        if (strncmp("group", (char*)data + out[i], 5) == 0) {
            g_free(info->group);
            info->group = g_strndup((char*)data + out[i + 2], out[i + 3]);
        } else if (strncmp("dbField2", (char*)data + out[i], 7) == 0) {
            g_free(info->dbFieldFull);
            info->dbFieldFull = info->dbField = g_strndup((char*)data + out[i + 2], out[i + 3]);
            info->dbFieldLen  = out[i + 3];
        } else if (strncmp("fieldECS", (char*)data + out[i], 7) == 0) {
            g_free(info->dbFieldFull);
            info->dbFieldFull = info->dbField = g_strndup((char*)data + out[i + 2], out[i + 3]);
            info->dbFieldLen  = out[i + 3];
        } else if (strncmp("type", (char*)data + out[i], 4) == 0) {
            g_free(info->kind);
            info->kind = g_strndup((char*)data + out[i + 2], out[i + 3]);
        } else if (strncmp("category", (char*)data + out[i], 8) == 0) {
            g_free(info->category);
            info->category = g_strndup((char*)data + out[i + 2], out[i + 3]);
        } else if (strncmp("transform", (char*)data + out[i], 8) == 0) {
            g_free(info->transform);
            info->transform = g_strndup((char*)data + out[i + 2], out[i + 3]);
        } else if (strncmp("aliases", (char*)data + out[i], 7) == 0) {
            g_free(info->aliases);
            info->aliases = g_strndup((char*)data + out[i + 2], out[i + 3]);
        } else if (strncmp("disabled", (char*)data + out[i], 8) == 0) {
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
int arkime_field_define_text_full(char *field, char *text, int *shortcut)
{
    int count = 0;
    int nolinked = 0;
    int noutf8 = 0;
    int fake = 0;
    char *kind = 0;
    char *help = 0;
    char *db = 0;
    char *group = 0;
    char *friendly = 0;
    char *category = 0;
    char *transform = 0;
    char *aliases = 0;

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
        char *dot = strchr(field, '.');
        if (dot) {
            if (dot-field >= (int)sizeof(groupbuf) - 1)
                LOGEXIT("ERROR - field '%s' too long", field);
            memcpy(groupbuf, field, dot-field);
            groupbuf[dot-field] = 0;
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
int arkime_field_define_text(char *text, int *shortcut)
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
int arkime_field_define(char *group, char *kind, char *expression, char *friendlyName, char *dbField, char *help, ArkimeFieldType type, int flags, ...)
{
    char dbField2[100];
    char expression2[1000];
    char friendlyName2[1000];
    char help2[1000];

    ArkimeFieldInfo_t *minfo = 0;
    HASH_FIND(d_, fieldsByDb, dbField, minfo);

    if (!minfo) {
        minfo = ARKIME_TYPE_ALLOC0(ArkimeFieldInfo_t);
        minfo->dbFieldFull = g_strdup(dbField);
        minfo->dbField     = minfo->dbFieldFull;
        minfo->dbFieldLen  = strlen(minfo->dbField);
        minfo->pos         = -1;
        minfo->expression  = g_strdup(expression);
        minfo->group       = g_strdup(group);
        minfo->kind        = g_strdup(kind);
        HASH_ADD(d_, fieldsByDb, minfo->dbField, minfo);
        HASH_ADD(e_, fieldsByExp, minfo->expression, minfo);

        if ((flags & ARKIME_FIELD_FLAG_NODB) == 0) {
            va_list args;
            va_start(args, flags);
            arkime_db_add_field(group, kind, expression, friendlyName, dbField, help, TRUE, args);
            va_end(args);
        }
    } else {
        flags |= (minfo->flags & ARKIME_FIELD_FLAG_DISABLED);

        // If we already have a pos can't be a fake field later
        if (minfo->pos != -1 && (flags & ARKIME_FIELD_FLAG_FAKE)) {
            return minfo->pos;
        }

        char *category = NULL;
        char *transform = NULL;
        char *aliases = NULL;
        if (strcmp(kind, minfo->kind) != 0) {
            LOG("WARNING - Field kind in db %s doesn't match field kind %s in capture for field %s", minfo->kind, kind, expression);
        }
        va_list args;
        va_start(args, flags);
        while(1) {
            char *field = va_arg(args, char *);
            if (!field) break;
            char *value = va_arg(args, char *);
            if (strcmp(field, "category") == 0 && value) {
                category = value;
            } else if (strcmp(field, "transform") == 0 && value) {
                transform = value;
            } else if (strcmp(field, "aliases") == 0 && value) {
                aliases = value;
            }
        }
        va_end(args);
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
            minfo->pos = ARKIME_THREAD_INCROLD(config.maxField);
            if (config.maxField >= ARKIME_FIELDS_DB_MAX) {
                LOGEXIT("ERROR - Max Fields is too large %d", config.maxField);
            }
        }

        config.fields[minfo->pos] = minfo;

        // Change leading part to dbGroup
        char *firstdot = strchr(minfo->dbField, '.');
        if (firstdot) {
            static char lastGroup[100] = "";
            static int groupNum = 0;

            if (firstdot-minfo->dbField + 1 >= (int)sizeof(lastGroup) - 1)
                LOGEXIT("ERROR - field '%s' too long", minfo->dbField);

            if (memcmp(minfo->dbField, lastGroup, (firstdot-minfo->dbField) + 1) == 0) {
                minfo->dbGroupNum = groupNum;
            } else {
                minfo->dbGroupNum = ARKIME_THREAD_INCRNEW(groupNum);
                memcpy(lastGroup, minfo->dbField, (firstdot-minfo->dbField) + 1);
            }
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
            arkime_field_by_exp_add_special_type(expression2, minfo->pos + ARKIME_FIELDS_CNT_MIN, ARKIME_FIELD_TYPE_INT);
        }
    } else if (flags & ARKIME_FIELD_FLAG_ECS_CNT) {
        snprintf(dbField2, sizeof(dbField2), "%s-cnt", dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            snprintf(expression2, sizeof(expression2), "%s.cnt", expression);
            snprintf(friendlyName2, sizeof(friendlyName2), "%s Cnt", friendlyName);
            snprintf(help2, sizeof(help2), "Unique number of %s", help);
            arkime_db_add_field(group, "integer", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
            arkime_field_by_exp_add_special_type(expression2, minfo->pos + ARKIME_FIELDS_CNT_MIN, ARKIME_FIELD_TYPE_INT);
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
            snprintf(expression2, sizeof(expression2), "country.%s", expression+ 3);
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
        info->pos = ARKIME_THREAD_INCROLD(config.maxField);
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

    if (info->flags & ARKIME_FIELD_FLAG_DISABLED || pos >= session->maxFields)
        return NULL;

    if (!session->fields[pos]) {
        field = ARKIME_TYPE_ALLOC(ArkimeField_t);
        session->fields[pos] = field;
        if (len == -1)
            len = strlen(string);

        if (len > ARKIME_FIELD_MAX_ELEMENT_SIZE) {
            len = ARKIME_FIELD_MAX_ELEMENT_SIZE;
            arkime_field_truncated(session, info);
        }

        field->jsonSize = 6 + info->dbFieldLen + 2 * len;
        if (copy)
            string = g_strndup(string, len);
        switch (info->type) {
        case ARKIME_FIELD_TYPE_STR:
            field->str = (char*)string;
            goto added;
        case ARKIME_FIELD_TYPE_STR_ARRAY:
            field->sarray = g_ptr_array_new_with_free_func(g_free);
            g_ptr_array_add(field->sarray, (char*)string);
            goto added;
        case ARKIME_FIELD_TYPE_STR_HASH:
            hash = ARKIME_TYPE_ALLOC(ArkimeStringHashStd_t);
            HASH_INIT(s_, *hash, arkime_string_hash, arkime_string_ncmp);
            field->shash = hash;
            hstring = ARKIME_TYPE_ALLOC(ArkimeString_t);
            hstring->str = (char*)string;
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

    if (len == -1)
        len = strlen(string);

    if (len > ARKIME_FIELD_MAX_ELEMENT_SIZE) {
        len = ARKIME_FIELD_MAX_ELEMENT_SIZE;
        arkime_field_truncated(session, info);
    }

    field = session->fields[pos];
    field->jsonSize += (6 + 2 * len);

    if (field->jsonSize > 20000)
        session->midSave = 1;

    switch (info->type) {
    case ARKIME_FIELD_TYPE_STR:
        if (copy)
            string = g_strndup(string, len);
        g_free(field->str);
        field->str = (char*)string;
        goto added;
    case ARKIME_FIELD_TYPE_STR_ARRAY:
        if (copy)
            string = g_strndup(string, len);
        g_ptr_array_add(field->sarray, (char*)string);
        goto added;
    case ARKIME_FIELD_TYPE_STR_HASH:
        HASH_FIND_HASH(s_, *(field->shash), arkime_string_hash_len(string, len), string, hstring);

        if (hstring) {
            field->jsonSize -= (6 + 2 * len);
            return NULL;
        }
        hstring = ARKIME_TYPE_ALLOC(ArkimeString_t);
        if (copy)
            string = g_strndup(string, len);
        hstring->str = (char*)string;
        hstring->len = len;
        hstring->utf8 = 0;
        hstring->uw = 0;
        HASH_ADD(s_, *(field->shash), hstring->str, hstring);
        goto added;
    case ARKIME_FIELD_TYPE_STR_GHASH:
        if (copy)
            string = g_strndup(string, len);
        if (g_hash_table_lookup(field->ghash, string)) {
            field->jsonSize -= (6 + 2 * len);
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
    if (info->ruleEnabled)
      arkime_rules_run_field_set(session, pos, (const gpointer) string);

    return string;
}
/******************************************************************************/
gboolean arkime_field_string_add_lower(int pos, ArkimeSession_t *session, const char *string, int len)
{
    if (len == -1)
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

    if (len == -1 ) {
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

    if (info->flags & ARKIME_FIELD_FLAG_DISABLED || pos >= session->maxFields)
        return NULL;

    if (!session->fields[pos]) {
        field = ARKIME_TYPE_ALLOC(ArkimeField_t);
        session->fields[pos] = field;
        if (len == -1)
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
            hstring->str = (char*)string;
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

    if (len == -1)
        len = strlen(string);

    field = session->fields[pos];
    field->jsonSize += (6 + 2 * len);

    if (len > ARKIME_FIELD_MAX_ELEMENT_SIZE) {
        len = ARKIME_FIELD_MAX_ELEMENT_SIZE;
        arkime_field_truncated(session, info);
    }

    if (field->jsonSize > 20000)
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
        hstring->str = (char*)string;
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

    if (info->flags & ARKIME_FIELD_FLAG_DISABLED || pos >= session->maxFields)
        return FALSE;

    if (!session->fields[pos]) {
        field = ARKIME_TYPE_ALLOC(ArkimeField_t);
        session->fields[pos] = field;
        field->jsonSize = 13 + info->dbFieldLen;
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
    field->jsonSize += 13;
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
            field->jsonSize -= 13;
            return FALSE;
        }
        hint = ARKIME_TYPE_ALLOC(ArkimeInt_t);
        HASH_ADD(i_, *(field->ihash), (void *)(long)i, hint);
        goto added;
    case ARKIME_FIELD_TYPE_INT_GHASH:
        if (!g_hash_table_add(field->ghash, (void *)(long)i)) {
            field->jsonSize -= 13;
            return FALSE;
        }
        goto added;
    default:
        LOGEXIT("ERROR - Not an integer, expression: %s field: %s, tried to set '%d'", info->expression, info->dbFieldFull, i);
    }

added:
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

    if (info->flags & ARKIME_FIELD_FLAG_DISABLED || pos >= session->maxFields)
        return FALSE;

    if (!session->fields[pos]) {
        field = ARKIME_TYPE_ALLOC(ArkimeField_t);
        session->fields[pos] = field;
        field->jsonSize = 15 + info->dbFieldLen;
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
    field->jsonSize += 15;
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
            field->jsonSize -= 15;
            return FALSE;
        }
        goto added;
    default:
        LOGEXIT("ERROR - Not a float, expression: %s field: %s, tried to set '%f'", info->expression, info->dbFieldFull, f);
    }

added:
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
void *arkime_field_parse_ip(const char *str) {

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
gboolean arkime_field_ip_add_str(int pos, ArkimeSession_t *session, char *str)
{
    ArkimeField_t                    *field;
    const ArkimeFieldInfo_t          *info = config.fields[pos];

    if (info->flags & ARKIME_FIELD_FLAG_DISABLED || pos >= session->maxFields)
        return FALSE;

    int len = strlen(str);
    struct in6_addr *v = arkime_field_parse_ip(str);

    if (!v) {
        return FALSE;
    }

    if (!session->fields[pos]) {
        field = ARKIME_TYPE_ALLOC(ArkimeField_t);
        session->fields[pos] = field;
        field->jsonSize = 3 + info->dbFieldLen + len + 100;
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
    field->jsonSize += (3 + len + 100);
    switch (info->type) {
    case ARKIME_FIELD_TYPE_IP:
        g_free(field->ip);
        field->ip = v;
        goto added;
    case ARKIME_FIELD_TYPE_IP_GHASH:
        if (!g_hash_table_add(field->ghash, v)) {
            field->jsonSize -= 3 + len + 100;
            return FALSE;
        } else {
            goto added;
        }
    default:
        LOGEXIT("ERROR - Not an ip, expression: %s field: %s, tried to set '%s'", info->expression, info->dbFieldFull, str);
    }

added:
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

    if (info->flags & ARKIME_FIELD_FLAG_DISABLED || pos >= session->maxFields)
        return FALSE;

    struct in6_addr *v = g_malloc(sizeof(struct in6_addr));

    memset(v->s6_addr, 0, 8);
    ((uint32_t *)v->s6_addr)[2] = htonl(0xffff);
    ((uint32_t *)v->s6_addr)[3] = i;

    if (!session->fields[pos]) {
        field = ARKIME_TYPE_ALLOC(ArkimeField_t);
        session->fields[pos] = field;
        field->jsonSize = 3 + info->dbFieldLen + 15 + 100;
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
    field->jsonSize += (3 + 15 + 100);
    switch (info->type) {
    case ARKIME_FIELD_TYPE_IP:
        g_free(field->ip);
        field->ip = v;
        goto added;
    case ARKIME_FIELD_TYPE_IP_GHASH:
        if (!g_hash_table_add(field->ghash, v)) {
            field->jsonSize -= 3 + 15 + 100;
            return FALSE;
        } else {
            goto added;
        }
    default:
        snprintf(ipbuf, sizeof(ipbuf), "%u.%u.%u.%u", i & 0xff, (i >> 8) & 0xff, (i >> 16) & 0xff, (i >> 24) & 0xff);
        LOGEXIT("ERROR - Not an ip, expression: %s field: %s, tried to set '%s'", info->expression, info->dbFieldFull, ipbuf);
    }

added:
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

    if (info->flags & ARKIME_FIELD_FLAG_DISABLED || pos >= session->maxFields)
        return FALSE;

    struct in6_addr *v = g_memdup(val, sizeof(struct in6_addr));

    if (!session->fields[pos]) {
        field = ARKIME_TYPE_ALLOC(ArkimeField_t);
        session->fields[pos] = field;
        field->jsonSize = 3 + info->dbFieldLen + 30 + 100;
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
    field->jsonSize += (3 + 30 + 100);
    switch (info->type) {
    case ARKIME_FIELD_TYPE_IP:
        g_free(field->ip);
        field->ip = v;
        goto added;
    case ARKIME_FIELD_TYPE_IP_GHASH:
        if (!g_hash_table_add(field->ghash, v)) {
            field->jsonSize -= 3 + 30 + 100;
            return FALSE;
        } else {
            goto added;
        }
    default:
        inet_ntop(AF_INET6, v, ipbuf, sizeof(ipbuf));
        LOGEXIT("ERROR - Not an ip, expression: %s field: %s, tried to set '%s'", info->expression, info->dbFieldFull, ipbuf);
    }

added:
    if (info->ruleEnabled)
      arkime_rules_run_field_set(session, pos, v);

    return TRUE;
}
/******************************************************************************/
SUPPRESS_UNSIGNED_INTEGER_OVERFLOW
SUPPRESS_SHIFT
SUPPRESS_INT_CONVERSION
uint32_t arkime_field_certsinfo_hash(const void *key)
{
    ArkimeCertsInfo_t *ci = (ArkimeCertsInfo_t *)key;

    if (ci->serialNumberLen == 0) {
        return ((ci->issuer.commonName.s_count << 18) |
                (ci->issuer.orgName.s_count << 12) |
                (ci->subject.commonName.s_count << 6) |
                (ci->subject.orgName.s_count));
    }
    return ((ci->serialNumber[0] << 28) |
            (ci->serialNumber[ci->serialNumberLen-1] << 24) |
            (ci->issuer.commonName.s_count << 18) |
            (ci->issuer.orgName.s_count << 12) |
            (ci->subject.commonName.s_count << 6) |
            (ci->subject.orgName.s_count));
}

/******************************************************************************/
int arkime_field_certsinfo_cmp(const void *keyv, const void *elementv)
{
    ArkimeCertsInfo_t *key = (ArkimeCertsInfo_t *)keyv;
    ArkimeCertsInfo_t *element = (ArkimeCertsInfo_t *)elementv;

    // Make sure all the easy things to check are the same
    if ( !((key->serialNumberLen == element->serialNumberLen) &&
           (memcmp(key->serialNumber, element->serialNumber, element->serialNumberLen) == 0) &&
           (key->issuer.commonName.s_count == element->issuer.commonName.s_count) &&
           (key->issuer.orgName.s_count == element->issuer.orgName.s_count) &&
           (key->issuer.orgUnit.s_count == element->issuer.orgUnit.s_count) &&
           (key->subject.commonName.s_count == element->subject.commonName.s_count) &&
           (key->subject.orgName.s_count == element->subject.orgName.s_count) &&
           (key->subject.orgUnit.s_count == element->subject.orgUnit.s_count)
          )
       ) {

        return 0;
    }

    // Now see if all the other items are the same

    ArkimeString_t *kstr, *estr;
    for (kstr = key->issuer.commonName.s_next, estr = element->issuer.commonName.s_next;
         kstr != (void *)&(key->issuer.commonName);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    for (kstr = key->issuer.orgName.s_next, estr = element->issuer.orgName.s_next;
         kstr != (void *)&(key->issuer.orgName);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    for (kstr = key->issuer.orgUnit.s_next, estr = element->issuer.orgUnit.s_next;
         kstr != (void *)&(key->issuer.orgUnit);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    for (kstr = key->subject.commonName.s_next, estr = element->subject.commonName.s_next;
         kstr != (void *)&(key->subject.commonName);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    for (kstr = key->subject.orgName.s_next, estr = element->subject.orgName.s_next;
         kstr != (void *)&(key->subject.orgName);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    for (kstr = key->subject.orgUnit.s_next, estr = element->subject.orgUnit.s_next;
         kstr != (void *)&(key->subject.orgUnit);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    return 1;
}
/******************************************************************************/
gboolean arkime_field_certsinfo_add(int pos, ArkimeSession_t *session, ArkimeCertsInfo_t *certs, int len)
{
    ArkimeField_t             *field;
    ArkimeCertsInfoHashStd_t   *hash;
    ArkimeCertsInfo_t          *hci;

    if (!session->fields[pos]) {
        field = ARKIME_TYPE_ALLOC(ArkimeField_t);
        session->fields[pos] = field;
        field->jsonSize = 3 + config.fields[pos]->dbFieldLen + 120 + len;
        switch (config.fields[pos]->type) {
        case ARKIME_FIELD_TYPE_CERTSINFO:
            hash = ARKIME_TYPE_ALLOC(ArkimeCertsInfoHashStd_t);
            HASH_INIT(t_, *hash, arkime_field_certsinfo_hash, arkime_field_certsinfo_cmp);
            field->cihash = hash;
            HASH_ADD(t_, *hash, certs, certs);
            return TRUE;
        default:
            LOGEXIT("ERROR - Not a certsinfo %s field", config.fields[pos]->dbField);
        }
    }

    field = session->fields[pos];
    switch (config.fields[pos]->type) {
    case ARKIME_FIELD_TYPE_CERTSINFO:
        HASH_FIND(t_, *(field->cihash), certs, hci);
        if (hci)
            return FALSE;
        field->jsonSize += 3 + 120 + len;
        HASH_ADD(t_, *(field->cihash), certs, certs);
        return TRUE;
    default:
        LOGEXIT("ERROR - Not a certsinfo %s field", config.fields[pos]->dbField);
    }
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
    int                       pos;
    ArkimeString_t           *hstring;
    ArkimeStringHashStd_t    *shash;
    ArkimeInt_t              *hint;
    ArkimeIntHashStd_t       *ihash;
    ArkimeCertsInfo_t        *hci;
    ArkimeCertsInfoHashStd_t *cihash;

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
        case ARKIME_FIELD_TYPE_CERTSINFO:
            cihash = session->fields[pos]->cihash;
            HASH_FORALL_POP_HEAD2(t_, *cihash, hci) {
                arkime_field_certsinfo_free(hci);
            }
            ARKIME_TYPE_FREE(ArkimeCertsInfoHashStd_t, cihash);
            break;
        } // switch
        ARKIME_TYPE_FREE(ArkimeField_t, session->fields[pos]);
    }
    ARKIME_SIZE_FREE(fields, session->fields);
    session->fields = 0;
}
/******************************************************************************/
void arkime_field_certsinfo_free (ArkimeCertsInfo_t *certs)
{
    ArkimeString_t *string;

    while (DLL_POP_HEAD(s_, &certs->alt, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    while (DLL_POP_HEAD(s_, &certs->issuer.commonName, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    while (DLL_POP_HEAD(s_, &certs->issuer.orgName, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    while (DLL_POP_HEAD(s_, &certs->issuer.orgUnit, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    while (DLL_POP_HEAD(s_, &certs->subject.commonName, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    while (DLL_POP_HEAD(s_, &certs->subject.orgName, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    while (DLL_POP_HEAD(s_, &certs->subject.orgUnit, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    if (certs->serialNumber)
        free(certs->serialNumber);

    ARKIME_TYPE_FREE(ArkimeCertsInfo_t, certs);
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
    case ARKIME_FIELD_TYPE_CERTSINFO:
        return HASH_COUNT(s_, *(field->cihash));
    default:
        LOGEXIT("ERROR - Unknown field type for counting %s %d", config.fields[pos]->dbField, config.fields[pos]->type);
    }
}
/******************************************************************************/
int arkime_field_ops_should_run_int_op(ArkimeFieldOp_t *op, int value)
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
void arkime_field_ops_run(ArkimeSession_t *session, ArkimeFieldOps_t *ops)
{
    int i;

    for (i = 0; i < ops->num; i++) {
        ArkimeFieldOp_t *op = &(ops->ops[i]);

        // Special field pos that really are setting a field in sessions
        if (op->fieldPos < 0) {
            switch (op->fieldPos) {
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
        // Exspecial Fields
        if (op->fieldPos >= ARKIME_FIELD_EXSPECIAL_START) {
            switch (op->fieldPos) {
            case ARKIME_FIELD_EXSPECIAL_SRC_IP:
            case ARKIME_FIELD_EXSPECIAL_SRC_PORT:
            case ARKIME_FIELD_EXSPECIAL_DST_IP:
            case ARKIME_FIELD_EXSPECIAL_DST_PORT:
            case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_SYN:
            case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_SYN_ACK:
            case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_ACK:
            case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_PSH:
            case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_RST:
            case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_FIN:
            case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_URG:
            case ARKIME_FIELD_EXSPECIAL_PACKETS_SRC:
            case ARKIME_FIELD_EXSPECIAL_PACKETS_DST:
            case ARKIME_FIELD_EXSPECIAL_DATABYTES_SRC:
            case ARKIME_FIELD_EXSPECIAL_DATABYTES_DST:
            case ARKIME_FIELD_EXSPECIAL_COMMUNITYID:
                break;
            }
            continue;
        }

        switch (config.fields[op->fieldPos]->type) {
        case ARKIME_FIELD_TYPE_INT:
            if (arkime_field_count(op->fieldPos, session) == 0 ||
                arkime_field_ops_should_run_int_op(op, session->fields[op->fieldPos]->i)) {

                arkime_field_int_add(op->fieldPos, session, op->strLenOrInt);
            }
            break;
        case ARKIME_FIELD_TYPE_INT_HASH:
        case ARKIME_FIELD_TYPE_INT_GHASH:
        case ARKIME_FIELD_TYPE_INT_ARRAY:
            arkime_field_int_add(op->fieldPos, session, op->strLenOrInt);
            break;
        case ARKIME_FIELD_TYPE_FLOAT_GHASH:
        case ARKIME_FIELD_TYPE_FLOAT:
        case ARKIME_FIELD_TYPE_FLOAT_ARRAY:
            arkime_field_float_add(op->fieldPos, session, op->f);
            break;
        case ARKIME_FIELD_TYPE_IP:
        case ARKIME_FIELD_TYPE_IP_GHASH:
            arkime_field_ip_add_str(op->fieldPos, session, op->str);
            break;
        case ARKIME_FIELD_TYPE_STR:
        case ARKIME_FIELD_TYPE_STR_ARRAY:
        case ARKIME_FIELD_TYPE_STR_HASH:
        case ARKIME_FIELD_TYPE_STR_GHASH:
            arkime_field_string_add(op->fieldPos, session, op->str, op->strLenOrInt, TRUE);
            break;
        case ARKIME_FIELD_TYPE_CERTSINFO:
            // Unsupported
            break;
        }
    }
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
void arkime_field_ops_int_parse(ArkimeFieldOp_t *op, char *value)
{
    int len;
    switch(value[0]) {
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
void arkime_field_ops_add(ArkimeFieldOps_t *ops, int fieldPos, char *value, int valuelen)
{
    if (ops->num >= ops->size) {
        ops->size = ceil (ops->size * 1.6);
        ops->ops = realloc(ops->ops, ops->size * sizeof(ArkimeFieldOp_t));
    }

    if (fieldPos == -1 || fieldPos > config.maxField) {
        LOG("WARNING - Not adding %d %s %d", fieldPos, value, valuelen);
        return;
    }

    ArkimeFieldOp_t *op = &(ops->ops[ops->num]);

    op->fieldPos = fieldPos;

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
    } else if (fieldPos >= ARKIME_FIELD_EXSPECIAL_START) {
        switch (op->fieldPos) {
        case ARKIME_FIELD_EXSPECIAL_SRC_IP:
        case ARKIME_FIELD_EXSPECIAL_SRC_PORT:
        case ARKIME_FIELD_EXSPECIAL_DST_IP:
        case ARKIME_FIELD_EXSPECIAL_DST_PORT:
            LOG("Warning - not allow to set src/dst ip/port: %s", op->str);
            break;
        case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_SYN:
        case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_SYN_ACK:
        case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_ACK:
        case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_PSH:
        case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_RST:
        case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_FIN:
        case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_URG:
            LOG("Warning - not allow to set tcpflags: %s", op->str);
            break;
        case ARKIME_FIELD_EXSPECIAL_PACKETS_SRC:
        case ARKIME_FIELD_EXSPECIAL_PACKETS_DST:
            LOG("Warning - not allow to set num packets: %s", op->str);
            break;
        case ARKIME_FIELD_EXSPECIAL_DATABYTES_SRC:
        case ARKIME_FIELD_EXSPECIAL_DATABYTES_DST:
            LOG("Warning - not allow to set databytes: %s", op->str);
            break;
        case ARKIME_FIELD_EXSPECIAL_COMMUNITYID:
            LOG("Warning - not allow to set communityId: %s", op->str);
            break;
        }
    } else {
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
void arkime_field_init()
{
    config.maxField = 0;
    HASH_INIT(d_, fieldsByDb, arkime_string_hash, arkime_string_cmp);
    HASH_INIT(e_, fieldsByExp, arkime_string_hash, (HASH_CMP_FUNC)arkime_field_exp_cmp);

    arkime_field_by_exp_add_special("dontSaveSPI", ARKIME_FIELD_SPECIAL_STOP_SPI);
    arkime_field_by_exp_add_special("_dontSaveSPI", ARKIME_FIELD_SPECIAL_STOP_SPI);
    arkime_field_by_exp_add_special("_maxPacketsToSave", ARKIME_FIELD_SPECIAL_STOP_PCAP);
    arkime_field_by_exp_add_special("_minPacketsBeforeSavingSPI", ARKIME_FIELD_SPECIAL_MIN_SAVE);
    arkime_field_by_exp_add_special("_dropBySrc", ARKIME_FIELD_SPECIAL_DROP_SRC);
    arkime_field_by_exp_add_special("_dropByDst", ARKIME_FIELD_SPECIAL_DROP_DST);
    arkime_field_by_exp_add_special("_dropBySession", ARKIME_FIELD_SPECIAL_DROP_SESSION);
    arkime_field_by_exp_add_special("_dontCheckYara", ARKIME_FIELD_SPECIAL_STOP_YARA);

    arkime_field_by_exp_add_special_type("ip.src", ARKIME_FIELD_EXSPECIAL_SRC_IP, ARKIME_FIELD_TYPE_IP);
    arkime_field_by_exp_add_special_type("port.src", ARKIME_FIELD_EXSPECIAL_SRC_PORT, ARKIME_FIELD_TYPE_INT);
    arkime_field_by_exp_add_special_type("ip.dst", ARKIME_FIELD_EXSPECIAL_DST_IP, ARKIME_FIELD_TYPE_IP);
    arkime_field_by_exp_add_special_type("port.dst", ARKIME_FIELD_EXSPECIAL_DST_PORT, ARKIME_FIELD_TYPE_INT);
    arkime_field_by_exp_add_special_type("tcpflags.syn", ARKIME_FIELD_EXSPECIAL_TCPFLAGS_SYN, ARKIME_FIELD_TYPE_INT);
    arkime_field_by_exp_add_special_type("tcpflags.syn-ack", ARKIME_FIELD_EXSPECIAL_TCPFLAGS_SYN_ACK, ARKIME_FIELD_TYPE_INT);
    arkime_field_by_exp_add_special_type("tcpflags.ack", ARKIME_FIELD_EXSPECIAL_TCPFLAGS_ACK, ARKIME_FIELD_TYPE_INT);
    arkime_field_by_exp_add_special_type("tcpflags.psh", ARKIME_FIELD_EXSPECIAL_TCPFLAGS_PSH, ARKIME_FIELD_TYPE_INT);
    arkime_field_by_exp_add_special_type("tcpflags.rst", ARKIME_FIELD_EXSPECIAL_TCPFLAGS_RST, ARKIME_FIELD_TYPE_INT);
    arkime_field_by_exp_add_special_type("tcpflags.fin", ARKIME_FIELD_EXSPECIAL_TCPFLAGS_FIN, ARKIME_FIELD_TYPE_INT);
    arkime_field_by_exp_add_special_type("tcpflags.urg", ARKIME_FIELD_EXSPECIAL_TCPFLAGS_URG, ARKIME_FIELD_TYPE_INT);
    arkime_field_by_exp_add_special_type("packets.src", ARKIME_FIELD_EXSPECIAL_PACKETS_SRC, ARKIME_FIELD_TYPE_INT);
    arkime_field_by_exp_add_special_type("packets.dst", ARKIME_FIELD_EXSPECIAL_PACKETS_DST, ARKIME_FIELD_TYPE_INT);
    arkime_field_by_exp_add_special_type("databytes.src", ARKIME_FIELD_EXSPECIAL_DATABYTES_SRC, ARKIME_FIELD_TYPE_INT);
    arkime_field_by_exp_add_special_type("databytes.dst", ARKIME_FIELD_EXSPECIAL_DATABYTES_DST, ARKIME_FIELD_TYPE_INT);
    arkime_field_by_exp_add_special_type("communityId", ARKIME_FIELD_EXSPECIAL_COMMUNITYID, ARKIME_FIELD_TYPE_STR);
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
/******************************************************************************/
