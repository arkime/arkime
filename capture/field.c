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
#include "moloch.h"
#include <stdarg.h>
#include <arpa/inet.h>
#include "patricia.h"

extern MolochConfig_t        config;

HASH_VAR(d_, fieldsByDb, MolochFieldInfo_t, 307);
HASH_VAR(e_, fieldsByExp, MolochFieldInfo_t, 307);

#define MOLOCH_FIELD_SPECIAL_STOP_SPI   -2
#define MOLOCH_FIELD_SPECIAL_STOP_PCAP  -3
#define MOLOCH_FIELD_SPECIAL_MIN_SAVE   -4
#define MOLOCH_FIELD_SPECIAL_DROP_SRC   -5
#define MOLOCH_FIELD_SPECIAL_DROP_DST   -6
#define MOLOCH_FIELD_SPECIAL_STOP_YARA  -7
#define MOLOCH_FIELD_SPECIAL_MIN        -7

LOCAL va_list empty_va_list;

#define MOLOCH_FIELD_MAX_ELEMENT_SIZE 16384

/******************************************************************************/
void moloch_field_by_exp_add_special(char *exp, int pos)
{
    MolochFieldInfo_t *info = MOLOCH_TYPE_ALLOC0(MolochFieldInfo_t);
    info->expression = exp;
    info->pos = pos;
    HASH_ADD(e_, fieldsByExp, info->expression, info);
}
/******************************************************************************/
void moloch_field_by_exp_add_special_type(char *exp, int pos, int type)
{
    MolochFieldInfo_t *info = MOLOCH_TYPE_ALLOC0(MolochFieldInfo_t);
    info->expression   = exp;
    info->pos          = pos;
    info->type         = type;
    config.fields[pos] = info;
    HASH_ADD(e_, fieldsByExp, info->expression, info);
}
/******************************************************************************/
LOCAL int moloch_field_exp_cmp(const char *key, const MolochFieldInfo_t *element)
{
    return strcmp(key, element->expression) == 0;
}
/******************************************************************************/
void moloch_field_define_json(unsigned char *expression, int expression_len, unsigned char *data, int data_len)
{
    MolochFieldInfo_t *info = MOLOCH_TYPE_ALLOC0(MolochFieldInfo_t);
    int                i;
    uint32_t           out[4*100]; // Can have up to 100 elements at any level
    int                disabled = 0;

    memset(out, 0, sizeof(out));
    if (js0n(data, data_len, out) != 0) {
        LOGEXIT("ERROR: Parse error for >%.*s<\n", data_len, data);
    }

    info->expression = g_strndup((char*)expression, expression_len);
    for (i = 0; out[i]; i += 4) {
        if (strncmp("group", (char*)data + out[i], 5) == 0) {
            g_free(info->group);
            info->group = g_strndup((char*)data + out[i+2], out[i+3]);
        } else if (strncmp("dbField2", (char*)data + out[i], 7) == 0) {
            g_free(info->dbFieldFull);
            info->dbFieldFull = info->dbField = g_strndup((char*)data + out[i+2], out[i+3]);
            info->dbFieldLen  = out[i+3];
        } else if (strncmp("type", (char*)data + out[i], 4) == 0) {
            g_free(info->kind);
            info->kind = g_strndup((char*)data + out[i+2], out[i+3]);
        } else if (strncmp("category", (char*)data + out[i], 8) == 0) {
            g_free(info->category);
            info->category = g_strndup((char*)data + out[i+2], out[i+3]);
        } else if (strncmp("transform", (char*)data + out[i], 8) == 0) {
            g_free(info->transform);
            info->transform = g_strndup((char*)data + out[i+2], out[i+3]);
        } else if (strncmp("disabled", (char*)data + out[i], 8) == 0) {
            if (strncmp((char *)data + out[i+2], "true", 4) == 0) {
                disabled = 1;
            }
        }
    }

    if (disabled)
        info->flags    |= MOLOCH_FIELD_FLAG_DISABLED;
    else
        info->flags    &= ~MOLOCH_FIELD_FLAG_DISABLED;

    info->pos = -1;
    HASH_ADD(d_, fieldsByDb, info->dbField, info);
    HASH_ADD(e_, fieldsByExp, info->expression, info);

    return;
}
/******************************************************************************/
int moloch_field_define_text_full(char *field, char *text, int *shortcut)
{
    int count = 0;
    char *kind = 0;
    char *help = 0;
    char *db = 0;
    char *group = 0;
    char *friendly = 0;
    char *category = 0;
    char *transform = 0;

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
        int pos = moloch_field_by_exp(field);
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
        LOGEXIT("ERROR - db field %s for %s should NOT end with -term in '%s' with Moloch 1.0", kind, db, text);
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

    int type, flags = 0;
    if (strcmp(kind, "integer") == 0 ||
        strcmp(kind, "seconds") == 0)
        type = MOLOCH_FIELD_TYPE_INT_GHASH;
    else if (strcmp(kind, "ip") == 0) {
        type = MOLOCH_FIELD_TYPE_IP_GHASH;
        if (!category)
            category = "ip";
    } else
        type = MOLOCH_FIELD_TYPE_STR_HASH;

    if (count)
        flags |= MOLOCH_FIELD_FLAG_CNT;

    int pos =  moloch_field_define(group, kind, field, friendly, db, help, type, flags, "category", category, "transform", transform, (char *)NULL);
    g_strfreev(elements);
    return pos;
}
/******************************************************************************/
int moloch_field_define_text(char *text, int *shortcut)
{
    return moloch_field_define_text_full(NULL, text, shortcut);
}
/******************************************************************************/
/* Changes ... to va_list */
/*static void moloch_session_add_field_proxy(char *group, char *kind, char *expression, char *friendlyName, char *dbField, char *help, ...)
{
    va_list args;
    va_start(args, help);
    moloch_db_add_field(group, kind, expression, friendlyName, dbField, help, TRUE, args);
    va_end(args);
}*/
/******************************************************************************/
int moloch_field_define(char *group, char *kind, char *expression, char *friendlyName, char *dbField, char *help, int type, int flags, ...)
{
    char dbField2[100];
    char expression2[1000];
    char friendlyName2[1000];
    char help2[1000];

    MolochFieldInfo_t *minfo = 0;
    HASH_FIND(d_, fieldsByDb, dbField, minfo);

    if (!minfo) {
        minfo = MOLOCH_TYPE_ALLOC0(MolochFieldInfo_t);
        minfo->dbFieldFull = g_strdup(dbField);
        minfo->dbField     = minfo->dbFieldFull;
        minfo->dbFieldLen  = strlen(minfo->dbField);
        minfo->pos         = -1;
        minfo->expression  = g_strdup(expression);
        minfo->group       = g_strdup(group);
        minfo->kind        = g_strdup(kind);
        HASH_ADD(d_, fieldsByDb, minfo->dbField, minfo);
        HASH_ADD(e_, fieldsByExp, minfo->expression, minfo);

        if ((flags & MOLOCH_FIELD_FLAG_NODB) == 0) {
            va_list args;
            va_start(args, flags);
            moloch_db_add_field(group, kind, expression, friendlyName, dbField, help, TRUE, args);
            va_end(args);
        }
    } else {
        flags |= (minfo->flags & MOLOCH_FIELD_FLAG_DISABLED);

        char *category = NULL;
        char *transform = NULL;
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
            }
        }
        va_end(args);
        if (category && (!minfo->category || strcmp(category, minfo->category) != 0)) {
            LOG("UPDATING - Field category in db %s doesn't match field category %s in capture for field %s", minfo->category, category, expression);
            moloch_db_update_field(expression, "category", category);
        }
        if (transform && (!minfo->transform || strcmp(transform, minfo->transform) != 0)) {
            LOG("UPDATING - Field transform in db %s doesn't match field transform %s in capture for field %s", minfo->transform, transform, expression);
            moloch_db_update_field(expression, "transform", transform);
        }
    }

    minfo->type     = type;
    minfo->flags    = flags;

    if ((flags & MOLOCH_FIELD_FLAG_FAKE) == 0) {
        if (minfo->pos == -1) {
            minfo->pos = MOLOCH_THREAD_INCROLD(config.maxField);
            if (config.maxField >= MOLOCH_FIELDS_DB_MAX) {
                LOGEXIT("ERROR - Max Fields is too large %d", config.maxField);
            }
        }

        config.fields[minfo->pos] = minfo;

        // Change leading part to dbGroup
        char *firstdot = strchr(minfo->dbField, '.');
        if (firstdot) {
            static char lastGroup[100] = "";
            static int groupNum = 0;

            if (firstdot-minfo->dbField+1 >= (int)sizeof(lastGroup) - 1)
                LOGEXIT("ERROR - field '%s' too long", minfo->dbField);

            if (memcmp(minfo->dbField, lastGroup, (firstdot-minfo->dbField)+1) == 0) {
                minfo->dbGroupNum = groupNum;
            } else {
                minfo->dbGroupNum = MOLOCH_THREAD_INCRNEW(groupNum);
                memcpy(lastGroup, minfo->dbField, (firstdot-minfo->dbField)+1);
            }
            minfo->dbGroup = minfo->dbField;
            minfo->dbGroupLen = firstdot - minfo->dbField;
            minfo->dbField += (firstdot - minfo->dbField) + 1;
            minfo->dbFieldLen = strlen(minfo->dbField);
        }
    }

    if (flags & MOLOCH_FIELD_FLAG_NODB)
        return minfo->pos;

    MolochFieldInfo_t *info;
    if (flags & MOLOCH_FIELD_FLAG_CNT) {
        snprintf(dbField2, sizeof(dbField2), "%sCnt", dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            snprintf(expression2, sizeof(expression2), "%s.cnt", expression);
            snprintf(friendlyName2, sizeof(friendlyName2), "%s Cnt", friendlyName);
            snprintf(help2, sizeof(help2), "Unique number of %s", help);
            moloch_db_add_field(group, "integer", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
            moloch_field_by_exp_add_special_type(g_strdup(expression2), minfo->pos + MOLOCH_FIELDS_CNT_MIN, MOLOCH_FIELD_TYPE_INT);
        }
    }

    if (flags & MOLOCH_FIELD_FLAG_FAKE) {
        g_free(minfo->expression);
        g_free(minfo->dbField);
        g_free(minfo->group);
        g_free(minfo->kind);
        g_free(minfo->transform);
        HASH_REMOVE(d_, fieldsByDb, minfo);
        HASH_REMOVE(e_, fieldsByExp, minfo);
        MOLOCH_TYPE_FREE(MolochFieldInfo_t, minfo);
        return -1;
    }

    if (flags & MOLOCH_FIELD_FLAG_IPPRE) {
        int l = strlen(dbField)-2;
        int fnlen = strlen(friendlyName);
        snprintf(dbField2, sizeof(dbField2), "%.*sGEO", l, dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            snprintf(expression2, sizeof(expression2), "country.%s", expression+3);
            snprintf(friendlyName2, sizeof(friendlyName2), "%.*s GEO", fnlen-2, friendlyName);
            snprintf(help2, sizeof(help2), "GeoIP country string calculated from the %s", help);
            moloch_db_add_field(group, "uptermfield", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
        }

        snprintf(dbField2, sizeof(dbField2), "%.*sASN", l, dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            snprintf(expression2, sizeof(expression2), "asn.%s", expression+3);
            snprintf(friendlyName2, sizeof(friendlyName2), "%.*s ASN", fnlen-2, friendlyName);
            snprintf(help2, sizeof(help2), "GeoIP ASN string calculated from the %s", help);
            moloch_db_add_field(group, "termfield", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
        }

        snprintf(dbField2, sizeof(dbField2), "%.*sRIR", l, dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            snprintf(expression2, sizeof(expression2), "rir.%s", expression+3);
            snprintf(friendlyName2, sizeof(friendlyName2), "%.*s RIR", fnlen-2, friendlyName);
            snprintf(help2, sizeof(help2), "Regional Internet Registry string calculated from %s", help);
            moloch_db_add_field(group, "uptermfield", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
        }
    } else if (type == MOLOCH_FIELD_TYPE_IP || type == MOLOCH_FIELD_TYPE_IP_GHASH) {
        int l = strlen(dbField)-2;
        snprintf(dbField2, sizeof(dbField2), "%.*sGEO", l, dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            snprintf(expression2, sizeof(expression2), "%s.country", expression);
            snprintf(friendlyName2, sizeof(friendlyName2), "%s GEO", friendlyName);
            snprintf(help2, sizeof(help2), "GeoIP country string calculated from the %s", help);
            moloch_db_add_field(group, "uptermfield", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
        }

        snprintf(dbField2, sizeof(dbField2), "%.*sASN", l, dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            snprintf(expression2, sizeof(expression2), "%s.asn", expression);
            snprintf(friendlyName2, sizeof(friendlyName2), "%s ASN", friendlyName);
            snprintf(help2, sizeof(help2), "GeoIP ASN string calculated from the %s", help);
            moloch_db_add_field(group, "termfield", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
        }

        snprintf(dbField2, sizeof(dbField2), "%.*sRIR", l, dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            snprintf(expression2, sizeof(expression2), "%s.rir", expression);
            snprintf(friendlyName2, sizeof(friendlyName2), "%s RIR", friendlyName);
            snprintf(help2, sizeof(help2), "Regional Internet Registry string calculated from %s", help);
            moloch_db_add_field(group, "uptermfield", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
        }
    }
    return minfo->pos;
}
/******************************************************************************/
int moloch_field_by_db(const char *dbField)
{
    MolochFieldInfo_t *info = 0;
    HASH_FIND(d_, fieldsByDb, dbField, info);
    if (!info || info->pos == -1)
        LOGEXIT("dbField %s wasn't defined", dbField);

    return info->pos;
}
/******************************************************************************/
int moloch_field_by_exp(const char *exp)
{
    MolochFieldInfo_t *info = 0;
    HASH_FIND(e_, fieldsByExp, exp, info);
    if (info) {
        if (info->pos != -1)
            return info->pos;

        // Need to change from field we just know about to real field
        if (strcmp(info->kind, "integer") == 0 || strcmp(info->kind, "seconds") == 0) {
            info->type = MOLOCH_FIELD_TYPE_INT_HASH;
        } else if (strcmp(info->kind, "ip") == 0) {
            info->type = MOLOCH_FIELD_TYPE_IP_GHASH;
        } else {
            info->type = MOLOCH_FIELD_TYPE_STR_HASH;
        }
        info->pos = MOLOCH_THREAD_INCROLD(config.maxField);
        config.fields[info->pos] = info;
        return info->pos;
    }

    LOGEXIT("expr %s wasn't defined", exp);
}
/******************************************************************************/
void moloch_field_truncated(MolochSession_t *session, const MolochFieldInfo_t *info)
{
    char str[1024];
    snprintf(str, sizeof(str), "truncated-field-%s", info->dbField);
    moloch_session_add_tag(session, str);
}
/******************************************************************************/
const char *moloch_field_string_add(int pos, MolochSession_t *session, const char *string, int len, gboolean copy)
{
    MolochField_t                    *field;
    MolochStringHashStd_t            *hash;
    MolochString_t                   *hstring;
    const MolochFieldInfo_t          *info = config.fields[pos];

    if (info->flags & MOLOCH_FIELD_FLAG_DISABLED || pos >= session->maxFields)
        return NULL;

    if (!session->fields[pos]) {
        field = MOLOCH_TYPE_ALLOC(MolochField_t);
        session->fields[pos] = field;
        if (len == -1)
            len = strlen(string);

        if (len > MOLOCH_FIELD_MAX_ELEMENT_SIZE) {
            len = MOLOCH_FIELD_MAX_ELEMENT_SIZE;
            moloch_field_truncated(session, info);
        }

        field->jsonSize = 6 + info->dbFieldLen + 2*len;
        if (copy)
            string = g_strndup(string, len);
        switch (info->type) {
        case MOLOCH_FIELD_TYPE_STR:
            field->str = (char*)string;
            goto added;
        case MOLOCH_FIELD_TYPE_STR_ARRAY:
            field->sarray = g_ptr_array_new_with_free_func(g_free);
            g_ptr_array_add(field->sarray, (char*)string);
            goto added;
        case MOLOCH_FIELD_TYPE_STR_HASH:
            hash = MOLOCH_TYPE_ALLOC(MolochStringHashStd_t);
            HASH_INIT(s_, *hash, moloch_string_hash, moloch_string_ncmp);
            field->shash = hash;
            hstring = MOLOCH_TYPE_ALLOC(MolochString_t);
            hstring->str = (char*)string;
            hstring->len = len;
            hstring->utf8 = 0;
            hstring->uw = 0;
            HASH_ADD(s_, *hash, hstring->str, hstring);
            goto added;
        case MOLOCH_FIELD_TYPE_STR_GHASH:
            field->ghash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
            g_hash_table_add(field->ghash, (gpointer)string);
            goto added;
        default:
            LOGEXIT("Not a string %s", info->dbField);
        }
    }

    if (len == -1)
        len = strlen(string);

    if (len > MOLOCH_FIELD_MAX_ELEMENT_SIZE) {
        len = MOLOCH_FIELD_MAX_ELEMENT_SIZE;
        moloch_field_truncated(session, info);
    }

    field = session->fields[pos];
    field->jsonSize += (6 + 2*len);

    if (field->jsonSize > 20000)
        session->midSave = 1;

    switch (info->type) {
    case MOLOCH_FIELD_TYPE_STR:
        if (copy)
            string = g_strndup(string, len);
        g_free(field->str);
        field->str = (char*)string;
        goto added;
    case MOLOCH_FIELD_TYPE_STR_ARRAY:
        if (copy)
            string = g_strndup(string, len);
        g_ptr_array_add(field->sarray, (char*)string);
        goto added;
    case MOLOCH_FIELD_TYPE_STR_HASH:
        HASH_FIND_HASH(s_, *(field->shash), moloch_string_hash_len(string, len), string, hstring);

        if (hstring) {
            field->jsonSize -= (6 + 2*len);
            return NULL;
        }
        hstring = MOLOCH_TYPE_ALLOC(MolochString_t);
        if (copy)
            string = g_strndup(string, len);
        hstring->str = (char*)string;
        hstring->len = len;
        hstring->utf8 = 0;
        hstring->uw = 0;
        HASH_ADD(s_, *(field->shash), hstring->str, hstring);
        goto added;
    case MOLOCH_FIELD_TYPE_STR_GHASH:
        if (copy)
            string = g_strndup(string, len);
        if (g_hash_table_lookup(field->ghash, string)) {
            field->jsonSize -= (6 + 2*len);
            if (copy)
                g_free((gpointer)string);
            return NULL;
        }
        g_hash_table_add(field->ghash, (gpointer)string);
        goto added;
    default:
        LOGEXIT("Not a string %s", info->dbField);
    }

added:
    if (info->ruleEnabled)
      moloch_rules_run_field_set(session, pos, (const gpointer) string);

    return string;
}
/******************************************************************************/
gboolean moloch_field_string_add_lower(int pos, MolochSession_t *session, const char *string, int len)
{
    if (len == -1)
        len = strlen(string);

    if (len > MOLOCH_FIELD_MAX_ELEMENT_SIZE) {
        len = MOLOCH_FIELD_MAX_ELEMENT_SIZE;
        moloch_field_truncated(session, config.fields[pos]);
    }

    char *lower = g_ascii_strdown(string, len);
    if (!moloch_field_string_add(pos, session, lower, len, FALSE)) {
        g_free(lower);
        return FALSE;
    }
    return TRUE;
}
/******************************************************************************/
gboolean moloch_field_string_add_host(int pos, MolochSession_t *session, char *string, int len)
{
    char *host;

    if (len == -1 ) {
        len = strlen(string);
    }

    if (len > MOLOCH_FIELD_MAX_ELEMENT_SIZE) {
        len = MOLOCH_FIELD_MAX_ELEMENT_SIZE;
        moloch_field_truncated(session, config.fields[pos]);
    }

    if (string[len] == 0)
        host = g_hostname_to_unicode(string);
    else {
        char ch = string[len];
        string[len] = 0;
        host = g_hostname_to_unicode(string);
        string[len] = ch;
    }

    // If g_hostname_to_unicode fails, just use the input
    if (!host) {
        host = g_strndup(string, len);
        moloch_session_add_tag(session, "bad-punycode");
    }

    if (!moloch_field_string_add(pos, session, host, -1, FALSE)) {
        g_free(host);
        return FALSE;
    }
    return TRUE;
}
/******************************************************************************/
const char *moloch_field_string_uw_add(int pos, MolochSession_t *session, const char *string, int len, gpointer uw, gboolean copy)
{
    MolochField_t                    *field;
    MolochStringHashStd_t            *hash;
    MolochString_t                   *hstring;
    const MolochFieldInfo_t          *info = config.fields[pos];

    if (info->flags & MOLOCH_FIELD_FLAG_DISABLED || pos >= session->maxFields)
        return NULL;

    if (!session->fields[pos]) {
        field = MOLOCH_TYPE_ALLOC(MolochField_t);
        session->fields[pos] = field;
        if (len == -1)
            len = strlen(string);
        if (len > MOLOCH_FIELD_MAX_ELEMENT_SIZE) {
            len = MOLOCH_FIELD_MAX_ELEMENT_SIZE;
            moloch_field_truncated(session, info);
        }
        field->jsonSize = 6 + info->dbFieldLen + 2*len;
        if (copy)
            string = g_strndup(string, len);
        switch (info->type) {
        case MOLOCH_FIELD_TYPE_STR_HASH:
            hash = MOLOCH_TYPE_ALLOC(MolochStringHashStd_t);
            HASH_INIT(s_, *hash, moloch_string_hash, moloch_string_ncmp);
            field->shash = hash;
            hstring = MOLOCH_TYPE_ALLOC(MolochString_t);
            hstring->str = (char*)string;
            hstring->len = len;
            hstring->utf8 = 0;
            hstring->uw = uw;
            HASH_ADD(s_, *hash, hstring->str, hstring);
            if (info->ruleEnabled)
                moloch_rules_run_field_set(session, pos, (const gpointer) string);
            return string;
        default:
            LOGEXIT("Not a string hash %s", info->dbField);
        }
    }

    if (len == -1)
        len = strlen(string);

    field = session->fields[pos];
    field->jsonSize += (6 + 2*len);

    if (len > MOLOCH_FIELD_MAX_ELEMENT_SIZE) {
        len = MOLOCH_FIELD_MAX_ELEMENT_SIZE;
        moloch_field_truncated(session, info);
    }

    if (field->jsonSize > 20000)
        session->midSave = 1;

    switch (info->type) {
    case MOLOCH_FIELD_TYPE_STR_HASH:
        HASH_FIND_HASH(s_, *(field->shash), moloch_string_hash_len(string, len), string, hstring);

        if (hstring) {
            field->jsonSize -= (6 + 2*len);
            return NULL;
        }
        hstring = MOLOCH_TYPE_ALLOC(MolochString_t);
        if (copy)
            string = g_strndup(string, len);
        hstring->str = (char*)string;
        hstring->len = len;
        hstring->utf8 = 0;
        hstring->uw = uw;
        HASH_ADD(s_, *(field->shash), hstring->str, hstring);
        if (info->ruleEnabled)
            moloch_rules_run_field_set(session, pos, (const gpointer) string);
        return string;
    default:
        LOGEXIT("Not a string hash %s", info->dbField);
    }
}
/******************************************************************************/
gboolean moloch_field_int_add(int pos, MolochSession_t *session, int i)
{
    MolochField_t        *field;
    MolochIntHashStd_t   *hash;
    MolochInt_t          *hint;

    if (config.fields[pos]->flags & MOLOCH_FIELD_FLAG_DISABLED || pos >= session->maxFields)
        return FALSE;

    if (!session->fields[pos]) {
        field = MOLOCH_TYPE_ALLOC(MolochField_t);
        session->fields[pos] = field;
        field->jsonSize = 3 + config.fields[pos]->dbFieldLen + 10;
        switch (config.fields[pos]->type) {
        case MOLOCH_FIELD_TYPE_INT:
            field->i = i;
            goto added;
        case MOLOCH_FIELD_TYPE_INT_ARRAY:
            field->iarray = g_array_new(FALSE, FALSE, 4);
            g_array_append_val(field->iarray, i);
            goto added;
        case MOLOCH_FIELD_TYPE_INT_HASH:
            hash = MOLOCH_TYPE_ALLOC(MolochIntHashStd_t);
            HASH_INIT(i_, *hash, moloch_int_hash, moloch_int_cmp);
            field->ihash = hash;
            hint = MOLOCH_TYPE_ALLOC(MolochInt_t);
            HASH_ADD(i_, *hash, (void *)(long)i, hint);
            goto added;
        case MOLOCH_FIELD_TYPE_INT_GHASH:
            field->ghash = g_hash_table_new(NULL, NULL);
            g_hash_table_add(field->ghash, (void *)(long)i);
            goto added;
        default:
            LOGEXIT("Not a int %s", config.fields[pos]->dbField);
        }
    }

    field = session->fields[pos];
    field->jsonSize += (3 + 10);
    switch (config.fields[pos]->type) {
    case MOLOCH_FIELD_TYPE_INT:
        field->i = i;
        goto added;
    case MOLOCH_FIELD_TYPE_INT_ARRAY:
        g_array_append_val(field->iarray, i);
        goto added;
    case MOLOCH_FIELD_TYPE_INT_HASH:
        HASH_FIND_INT(i_, *(field->ihash), i, hint);
        if (hint) {
            field->jsonSize -= (3 + 10);
            return FALSE;
        }
        hint = MOLOCH_TYPE_ALLOC(MolochInt_t);
        HASH_ADD(i_, *(field->ihash), (void *)(long)i, hint);
        goto added;
    case MOLOCH_FIELD_TYPE_INT_GHASH:
        if (!g_hash_table_add(field->ghash, (void *)(long)i)) {
            field->jsonSize -= 13;
            return FALSE;
        }
        goto added;
    default:
        LOGEXIT("Not a int %s", config.fields[pos]->dbField);
    }

added:
    if (config.fields[pos]->ruleEnabled)
      moloch_rules_run_field_set(session, pos, (gpointer)(long)i);

    return TRUE;
}
/******************************************************************************/
gboolean moloch_field_ip_equal (gconstpointer v1, gconstpointer v2)
{
  return memcmp (v1, v2, 16) == 0;
}
/******************************************************************************/
SUPPRESS_UNSIGNED_INTEGER_OVERFLOW
guint moloch_field_ip_hash (gconstpointer v)
{
  const signed char *p;
  guint32 h = 5381;
  int i;

  for (i = 0, p = v; i < 16; i++, p++) {
    h = (h << 5) + h + *p;
  }

  return h;
}

/******************************************************************************/
void *moloch_field_parse_ip(const char *str) {

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
gboolean moloch_field_ip_add_str(int pos, MolochSession_t *session, char *str)
{
    MolochField_t        *field;

    if (config.fields[pos]->flags & MOLOCH_FIELD_FLAG_DISABLED || pos >= session->maxFields)
        return FALSE;

    int len = strlen(str);
    struct in6_addr *v = moloch_field_parse_ip(str);

    if (!v) {
        return FALSE;
    }

    if (!session->fields[pos]) {
        field = MOLOCH_TYPE_ALLOC(MolochField_t);
        session->fields[pos] = field;
        field->jsonSize = 3 + config.fields[pos]->dbFieldLen + len + 100;
        switch (config.fields[pos]->type) {
        case MOLOCH_FIELD_TYPE_IP:
            field->ip = v;
            goto added;
        case MOLOCH_FIELD_TYPE_IP_GHASH:
            field->ghash = g_hash_table_new_full(moloch_field_ip_hash, moloch_field_ip_equal, g_free, NULL);
            g_hash_table_add(field->ghash, v);
            goto added;
        default:
            LOGEXIT("Not a ip %s", config.fields[pos]->dbField);
        }
    }

    field = session->fields[pos];
    field->jsonSize += (3 + len + 100);
    switch (config.fields[pos]->type) {
    case MOLOCH_FIELD_TYPE_IP:
        g_free(field->ip);
        field->ip = v;
        goto added;
    case MOLOCH_FIELD_TYPE_IP_GHASH:
        if (!g_hash_table_add(field->ghash, v)) {
            field->jsonSize -= 3 + len + 100;
            return FALSE;
        } else {
            goto added;
        }
    default:
        LOGEXIT("Not a ip %s", config.fields[pos]->dbField);
    }

added:
    if (config.fields[pos]->ruleEnabled)
      moloch_rules_run_field_set(session, pos, v);

    return TRUE;
}
/******************************************************************************/
gboolean moloch_field_ip4_add(int pos, MolochSession_t *session, int i)
{
    MolochField_t        *field;

    if (config.fields[pos]->flags & MOLOCH_FIELD_FLAG_DISABLED || pos >= session->maxFields)
        return FALSE;

    struct in6_addr *v = g_malloc(sizeof(struct in6_addr));

    memset(v->s6_addr, 0, 8);
    ((uint32_t *)v->s6_addr)[2] = htonl(0xffff);
    ((uint32_t *)v->s6_addr)[3] = i;

    if (!session->fields[pos]) {
        field = MOLOCH_TYPE_ALLOC(MolochField_t);
        session->fields[pos] = field;
        field->jsonSize = 3 + config.fields[pos]->dbFieldLen + 15 + 100;
        switch (config.fields[pos]->type) {
        case MOLOCH_FIELD_TYPE_IP:
            field->ip = v;
            goto added;
        case MOLOCH_FIELD_TYPE_IP_GHASH:
            field->ghash = g_hash_table_new_full(moloch_field_ip_hash, moloch_field_ip_equal, g_free, NULL);
            g_hash_table_add(field->ghash, v);
            goto added;
        default:
            LOGEXIT("Not a ip %s", config.fields[pos]->dbField);
        }
    }

    field = session->fields[pos];
    field->jsonSize += (3 + 15 + 100);
    switch (config.fields[pos]->type) {
    case MOLOCH_FIELD_TYPE_IP:
        g_free(field->ip);
        field->ip = v;
        goto added;
    case MOLOCH_FIELD_TYPE_IP_GHASH:
        if (!g_hash_table_add(field->ghash, v)) {
            field->jsonSize -= 3 + 15 + 100;
            return FALSE;
        } else {
            goto added;
        }
    default:
        LOGEXIT("Not a ip %s", config.fields[pos]->dbField);
    }

added:
    if (config.fields[pos]->ruleEnabled)
      moloch_rules_run_field_set(session, pos, v);

    return TRUE;
}
/******************************************************************************/
gboolean moloch_field_ip6_add(int pos, MolochSession_t *session, const uint8_t *val)
{
    MolochField_t        *field;

    if (config.fields[pos]->flags & MOLOCH_FIELD_FLAG_DISABLED || pos >= session->maxFields)
        return FALSE;

    struct in6_addr *v = g_memdup(val, sizeof(struct in6_addr));

    if (!session->fields[pos]) {
        field = MOLOCH_TYPE_ALLOC(MolochField_t);
        session->fields[pos] = field;
        field->jsonSize = 3 + config.fields[pos]->dbFieldLen + 30 + 100;
        switch (config.fields[pos]->type) {
        case MOLOCH_FIELD_TYPE_IP:
            field->ip = v;
            goto added;
        case MOLOCH_FIELD_TYPE_IP_GHASH:
            field->ghash = g_hash_table_new_full(moloch_field_ip_hash, moloch_field_ip_equal, g_free, NULL);
            g_hash_table_add(field->ghash, v);
            goto added;
        default:
            LOGEXIT("Not a ip %s", config.fields[pos]->dbField);
        }
    }

    field = session->fields[pos];
    field->jsonSize += (3 + 30 + 100);
    switch (config.fields[pos]->type) {
    case MOLOCH_FIELD_TYPE_IP:
        g_free(field->ip);
        field->ip = v;
        goto added;
    case MOLOCH_FIELD_TYPE_IP_GHASH:
        if (!g_hash_table_add(field->ghash, v)) {
            field->jsonSize -= 3 + 30 + 100;
            return FALSE;
        } else {
            goto added;
        }
    default:
        LOGEXIT("Not a ip %s", config.fields[pos]->dbField);
    }

added:
    if (config.fields[pos]->ruleEnabled)
      moloch_rules_run_field_set(session, pos, v);

    return TRUE;
}
/******************************************************************************/
SUPPRESS_SHIFT
uint32_t moloch_field_certsinfo_hash(const void *key)
{
    MolochCertsInfo_t *ci = (MolochCertsInfo_t *)key;

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
int moloch_field_certsinfo_cmp(const void *keyv, const void *elementv)
{
    MolochCertsInfo_t *key = (MolochCertsInfo_t *)keyv;
    MolochCertsInfo_t *element = (MolochCertsInfo_t *)elementv;

    // Make sure all the easy things to check are the same
    if ( !((key->serialNumberLen == element->serialNumberLen) &&
           (memcmp(key->serialNumber, element->serialNumber, element->serialNumberLen) == 0) &&
           (key->issuer.commonName.s_count == element->issuer.commonName.s_count) &&
           (key->issuer.orgName.s_count == element->issuer.orgName.s_count) &&
           (key->subject.commonName.s_count == element->subject.commonName.s_count) &&
           (key->subject.orgName.s_count == element->subject.orgName.s_count)
          )
       ) {

        return 0;
    }

    // Now see if all the other items are the same

    MolochString_t *kstr, *estr;
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

    return 1;
}
/******************************************************************************/
gboolean moloch_field_certsinfo_add(int pos, MolochSession_t *session, MolochCertsInfo_t *certs, int len)
{
    MolochField_t             *field;
    MolochCertsInfoHashStd_t   *hash;
    MolochCertsInfo_t          *hci;

    if (!session->fields[pos]) {
        field = MOLOCH_TYPE_ALLOC(MolochField_t);
        session->fields[pos] = field;
        field->jsonSize = 3 + config.fields[pos]->dbFieldLen + 100 + len;
        switch (config.fields[pos]->type) {
        case MOLOCH_FIELD_TYPE_CERTSINFO:
            hash = MOLOCH_TYPE_ALLOC(MolochCertsInfoHashStd_t);
            HASH_INIT(t_, *hash, moloch_field_certsinfo_hash, moloch_field_certsinfo_cmp);
            field->cihash = hash;
            HASH_ADD(t_, *hash, certs, certs);
            return TRUE;
        default:
            LOGEXIT("Not a certsinfo %s", config.fields[pos]->dbField);
        }
    }

    field = session->fields[pos];
    switch (config.fields[pos]->type) {
    case MOLOCH_FIELD_TYPE_CERTSINFO:
        HASH_FIND(t_, *(field->cihash), certs, hci);
        if (hci)
            return FALSE;
        field->jsonSize += 3 + 100 + len;
        HASH_ADD(t_, *(field->cihash), certs, certs);
        return TRUE;
    default:
        LOGEXIT("Not a certsinfo %s", config.fields[pos]->dbField);
    }
}
/******************************************************************************/
void moloch_field_macoui_add(MolochSession_t *session, int macField, int ouiField, const uint8_t *mac)
{
    char str[20];

    snprintf(str, sizeof(str), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0],
            mac[1],
            mac[2],
            mac[3],
            mac[4],
            mac[5]);

    if (moloch_field_string_add(macField, session, str, 17, TRUE))
        moloch_db_oui_lookup(ouiField, session, mac);
}
/******************************************************************************/
void moloch_field_free(MolochSession_t *session)
{
    int                       pos;
    MolochString_t           *hstring;
    MolochStringHashStd_t    *shash;
    MolochInt_t              *hint;
    MolochIntHashStd_t       *ihash;
    MolochCertsInfo_t        *hci;
    MolochCertsInfoHashStd_t *cihash;

    for (pos = 0; pos < session->maxFields; pos++) {
        MolochField_t        *field;

        if (!(field = session->fields[pos]))
            continue;

        switch (config.fields[pos]->type) {
        case MOLOCH_FIELD_TYPE_STR:
            g_free(field->str);
            break;
        case MOLOCH_FIELD_TYPE_STR_ARRAY:
            g_ptr_array_free(field->sarray, TRUE);
            break;
        case MOLOCH_FIELD_TYPE_STR_HASH:
            shash = session->fields[pos]->shash;
            HASH_FORALL_POP_HEAD(s_, *shash, hstring,
                g_free(hstring->str);
                MOLOCH_TYPE_FREE(MolochString_t, hstring);
            );
            MOLOCH_TYPE_FREE(MolochStringHashStd_t, shash);
            break;
        case MOLOCH_FIELD_TYPE_INT:
            break;
        case MOLOCH_FIELD_TYPE_INT_ARRAY:
            g_array_free(field->iarray, TRUE);
            break;
        case MOLOCH_FIELD_TYPE_INT_HASH:
            ihash = session->fields[pos]->ihash;
            HASH_FORALL_POP_HEAD(i_, *ihash, hint,
                MOLOCH_TYPE_FREE(MolochInt_t, hint);
            );
            MOLOCH_TYPE_FREE(MolochIntHashStd_t, ihash);
            break;
        case MOLOCH_FIELD_TYPE_IP:
            g_free(session->fields[pos]->ip);
            break;
        case MOLOCH_FIELD_TYPE_IP_GHASH:
        case MOLOCH_FIELD_TYPE_INT_GHASH:
        case MOLOCH_FIELD_TYPE_STR_GHASH:
            g_hash_table_destroy(session->fields[pos]->ghash);
            break;
        case MOLOCH_FIELD_TYPE_CERTSINFO:
            cihash = session->fields[pos]->cihash;
            HASH_FORALL_POP_HEAD(t_, *cihash, hci,
                moloch_field_certsinfo_free(hci);
            );
            MOLOCH_TYPE_FREE(MolochCertsInfoHashStd_t, cihash);
            break;
        } // switch
        MOLOCH_TYPE_FREE(MolochField_t, session->fields[pos]);
    }
    MOLOCH_SIZE_FREE(fields, session->fields);
    session->fields = 0;
}
/******************************************************************************/
void moloch_field_certsinfo_free (MolochCertsInfo_t *certs)
{
    MolochString_t *string;

    while (DLL_POP_HEAD(s_, &certs->alt, string)) {
        g_free(string->str);
        MOLOCH_TYPE_FREE(MolochString_t, string);
    }

    while (DLL_POP_HEAD(s_, &certs->issuer.commonName, string)) {
        g_free(string->str);
        MOLOCH_TYPE_FREE(MolochString_t, string);
    }

    while (DLL_POP_HEAD(s_, &certs->issuer.orgName, string)) {
        g_free(string->str);
        MOLOCH_TYPE_FREE(MolochString_t, string);
    }

    while (DLL_POP_HEAD(s_, &certs->subject.commonName, string)) {
        g_free(string->str);
        MOLOCH_TYPE_FREE(MolochString_t, string);
    }

    while (DLL_POP_HEAD(s_, &certs->subject.orgName, string)) {
        g_free(string->str);
        MOLOCH_TYPE_FREE(MolochString_t, string);
    }

    if (certs->serialNumber)
        free(certs->serialNumber);

    MOLOCH_TYPE_FREE(MolochCertsInfo_t, certs);
}
/******************************************************************************/
int moloch_field_count(int pos, MolochSession_t *session)
{
    MolochField_t         *field;

    if (!session->fields[pos])
        return 0;

    field = session->fields[pos];

    switch (config.fields[pos]->type) {
    case MOLOCH_FIELD_TYPE_INT:
    case MOLOCH_FIELD_TYPE_STR:
    case MOLOCH_FIELD_TYPE_IP:
        return 1;
    case MOLOCH_FIELD_TYPE_STR_ARRAY:
        return field->sarray->len;
    case MOLOCH_FIELD_TYPE_INT_ARRAY:
        return field->iarray->len;
    case MOLOCH_FIELD_TYPE_STR_HASH:
        return HASH_COUNT(s_, *(field->shash));
    case MOLOCH_FIELD_TYPE_INT_HASH:
        return HASH_COUNT(s_, *(field->ihash));
    case MOLOCH_FIELD_TYPE_IP_GHASH:
    case MOLOCH_FIELD_TYPE_INT_GHASH:
    case MOLOCH_FIELD_TYPE_STR_GHASH:
        return g_hash_table_size(field->ghash);
    case MOLOCH_FIELD_TYPE_CERTSINFO:
        return HASH_COUNT(s_, *(field->cihash));
    default:
        LOGEXIT("ERROR - Unknown field type for counting %s %d", config.fields[pos]->dbField, config.fields[pos]->type);
    }
}
/******************************************************************************/
void moloch_field_ops_run(MolochSession_t *session, MolochFieldOps_t *ops)
{
    int i;

    for (i = 0; i < ops->num; i++) {
        MolochFieldOp_t *op = &(ops->ops[i]);

        // Special field pos that really are setting a field in sessions
        if (op->fieldPos < 0) {
            switch (op->fieldPos) {
            case MOLOCH_FIELD_SPECIAL_STOP_SPI:
                session->stopSPI = op->strLenOrInt;
                break;
            case MOLOCH_FIELD_SPECIAL_STOP_PCAP:
                session->stopSaving = op->strLenOrInt;
                if (session->packets[0] + session->packets[1] >=session->stopSaving)
                    moloch_session_add_tag(session, "truncated-pcap");
                break;
            case MOLOCH_FIELD_SPECIAL_MIN_SAVE:
                session->minSaving = op->strLenOrInt;
                break;
            case MOLOCH_FIELD_SPECIAL_DROP_SRC:
                moloch_packet_drophash_add(session, 0, op->strLenOrInt);
                break;
            case MOLOCH_FIELD_SPECIAL_DROP_DST:
                moloch_packet_drophash_add(session, 1, op->strLenOrInt);
                break;
            case MOLOCH_FIELD_SPECIAL_STOP_YARA:
                session->stopYara = 1;
                break;
            }
            continue;
        }
        // Exspecial Fields
        if (op->fieldPos >= MOLOCH_FIELD_EXSPECIAL_START) {
            switch (op->fieldPos) {
            case MOLOCH_FIELD_EXSPECIAL_SRC_IP:
            case MOLOCH_FIELD_EXSPECIAL_SRC_PORT:
            case MOLOCH_FIELD_EXSPECIAL_DST_IP:
            case MOLOCH_FIELD_EXSPECIAL_DST_PORT:
            case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_SYN:
            case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_SYN_ACK:
            case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_ACK:
            case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_PSH:
            case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_RST:
            case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_FIN:
            case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_URG:
            case MOLOCH_FIELD_EXSPECIAL_PACKETS_SRC:
            case MOLOCH_FIELD_EXSPECIAL_PACKETS_DST:
            case MOLOCH_FIELD_EXSPECIAL_DATABYTES_SRC:
            case MOLOCH_FIELD_EXSPECIAL_DATABYTES_DST:
                break;
            }
            continue;
        }

        switch (config.fields[op->fieldPos]->type) {
        case  MOLOCH_FIELD_TYPE_INT_HASH:
        case  MOLOCH_FIELD_TYPE_INT_GHASH:
        case  MOLOCH_FIELD_TYPE_INT:
        case  MOLOCH_FIELD_TYPE_INT_ARRAY:
            moloch_field_int_add(op->fieldPos, session, op->strLenOrInt);
            break;
        case  MOLOCH_FIELD_TYPE_IP:
        case  MOLOCH_FIELD_TYPE_IP_GHASH:
            moloch_field_ip_add_str(op->fieldPos, session, op->str);
            break;
        case  MOLOCH_FIELD_TYPE_STR:
        case  MOLOCH_FIELD_TYPE_STR_ARRAY:
        case  MOLOCH_FIELD_TYPE_STR_HASH:
        case  MOLOCH_FIELD_TYPE_STR_GHASH:
            moloch_field_string_add(op->fieldPos, session, op->str, op->strLenOrInt, TRUE);
            break;
        }
    }
}
/******************************************************************************/
void moloch_field_ops_free(MolochFieldOps_t *ops)
{
    if (ops->flags & MOLOCH_FIELD_OPS_FLAGS_COPY) {
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
void moloch_field_ops_init(MolochFieldOps_t *ops, int numOps, uint16_t flags)
{
    ops->num   = 0;
    ops->size  = numOps;
    ops->flags = flags;

    if (numOps > 0)
        ops->ops = malloc(numOps * sizeof(MolochFieldOp_t));
    else
        ops->ops = NULL;
}


/******************************************************************************/
void moloch_field_ops_add(MolochFieldOps_t *ops, int fieldPos, char *value, int valuelen)
{
    if (ops->num >= ops->size || fieldPos == -1 || fieldPos > config.maxField) {
        LOG("WARNING - Not adding %d %s %d", fieldPos, value, valuelen);
        return;
    }

    MolochFieldOp_t *op = &(ops->ops[ops->num]);

    op->fieldPos = fieldPos;

    if (fieldPos < 0) {
        switch (op->fieldPos) {
        case MOLOCH_FIELD_SPECIAL_STOP_SPI:
        case MOLOCH_FIELD_SPECIAL_STOP_PCAP:
        case MOLOCH_FIELD_SPECIAL_MIN_SAVE:
        case MOLOCH_FIELD_SPECIAL_DROP_SRC:
        case MOLOCH_FIELD_SPECIAL_DROP_DST:
        case MOLOCH_FIELD_SPECIAL_STOP_YARA:
            op->strLenOrInt = atoi(value);
            op->str = 0;
            break;
        default:
            LOG("WARNING - Unknown special field pos %d", fieldPos);
            break;
        }
    } else if (fieldPos >= MOLOCH_FIELD_EXSPECIAL_START) {
        switch (op->fieldPos) {
        case MOLOCH_FIELD_EXSPECIAL_SRC_IP:
        case MOLOCH_FIELD_EXSPECIAL_SRC_PORT:
        case MOLOCH_FIELD_EXSPECIAL_DST_IP:
        case MOLOCH_FIELD_EXSPECIAL_DST_PORT:
            LOG("Warning - not allow to set src/dst ip/port: %s", op->str);
            break;
        case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_SYN:
        case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_SYN_ACK:
        case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_ACK:
        case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_PSH:
        case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_RST:
        case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_FIN:
        case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_URG:
            LOG("Warning - not allow to set tcpflags: %s", op->str);
            break;
        case MOLOCH_FIELD_EXSPECIAL_PACKETS_SRC:
        case MOLOCH_FIELD_EXSPECIAL_PACKETS_DST:
            LOG("Warning - not allow to set num packets: %s", op->str);
            break;
        case MOLOCH_FIELD_EXSPECIAL_DATABYTES_SRC:
        case MOLOCH_FIELD_EXSPECIAL_DATABYTES_DST:
            LOG("Warning - not allow to set databytes: %s", op->str);
            break;
        }
    } else {
        switch (config.fields[fieldPos]->type) {
        case  MOLOCH_FIELD_TYPE_INT_HASH:
        case  MOLOCH_FIELD_TYPE_INT_GHASH:
        case  MOLOCH_FIELD_TYPE_INT:
        case  MOLOCH_FIELD_TYPE_INT_ARRAY:
            op->str = 0;
            op->strLenOrInt = atoi(value);
            break;
        case  MOLOCH_FIELD_TYPE_STR:
        case  MOLOCH_FIELD_TYPE_STR_ARRAY:
        case  MOLOCH_FIELD_TYPE_STR_HASH:
        case  MOLOCH_FIELD_TYPE_STR_GHASH:
        case  MOLOCH_FIELD_TYPE_IP:
        case  MOLOCH_FIELD_TYPE_IP_GHASH:
            if (valuelen == -1)
                valuelen = strlen(value);
            if (ops->flags & MOLOCH_FIELD_OPS_FLAGS_COPY)
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
void moloch_field_init()
{
    config.maxField = 0;
    HASH_INIT(d_, fieldsByDb, moloch_string_hash, moloch_string_cmp);
    HASH_INIT(e_, fieldsByExp, moloch_string_hash, (HASH_CMP_FUNC)moloch_field_exp_cmp);

    moloch_field_by_exp_add_special("dontSaveSPI", MOLOCH_FIELD_SPECIAL_STOP_SPI);
    moloch_field_by_exp_add_special("_dontSaveSPI", MOLOCH_FIELD_SPECIAL_STOP_SPI);
    moloch_field_by_exp_add_special("_maxPacketsToSave", MOLOCH_FIELD_SPECIAL_STOP_PCAP);
    moloch_field_by_exp_add_special("_minPacketsBeforeSavingSPI", MOLOCH_FIELD_SPECIAL_MIN_SAVE);
    moloch_field_by_exp_add_special("_dropBySrc", MOLOCH_FIELD_SPECIAL_DROP_SRC);
    moloch_field_by_exp_add_special("_dropByDst", MOLOCH_FIELD_SPECIAL_DROP_DST);
    moloch_field_by_exp_add_special("_dontCheckYara", MOLOCH_FIELD_SPECIAL_STOP_YARA);

    moloch_field_by_exp_add_special_type("ip.src", MOLOCH_FIELD_EXSPECIAL_SRC_IP, MOLOCH_FIELD_TYPE_IP);
    moloch_field_by_exp_add_special_type("port.src", MOLOCH_FIELD_EXSPECIAL_SRC_PORT, MOLOCH_FIELD_TYPE_INT);
    moloch_field_by_exp_add_special_type("ip.dst", MOLOCH_FIELD_EXSPECIAL_DST_IP, MOLOCH_FIELD_TYPE_IP);
    moloch_field_by_exp_add_special_type("port.dst", MOLOCH_FIELD_EXSPECIAL_DST_PORT, MOLOCH_FIELD_TYPE_INT);
    moloch_field_by_exp_add_special_type("tcpflags.syn", MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_SYN, MOLOCH_FIELD_TYPE_INT);
    moloch_field_by_exp_add_special_type("tcpflags.syn-ack", MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_SYN_ACK, MOLOCH_FIELD_TYPE_INT);
    moloch_field_by_exp_add_special_type("tcpflags.ack", MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_ACK, MOLOCH_FIELD_TYPE_INT);
    moloch_field_by_exp_add_special_type("tcpflags.psh", MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_PSH, MOLOCH_FIELD_TYPE_INT);
    moloch_field_by_exp_add_special_type("tcpflags.rst", MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_RST, MOLOCH_FIELD_TYPE_INT);
    moloch_field_by_exp_add_special_type("tcpflags.fin", MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_FIN, MOLOCH_FIELD_TYPE_INT);
    moloch_field_by_exp_add_special_type("tcpflags.urg", MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_URG, MOLOCH_FIELD_TYPE_INT);
    moloch_field_by_exp_add_special_type("packets.src", MOLOCH_FIELD_EXSPECIAL_PACKETS_SRC, MOLOCH_FIELD_TYPE_INT);
    moloch_field_by_exp_add_special_type("packets.dst", MOLOCH_FIELD_EXSPECIAL_PACKETS_DST, MOLOCH_FIELD_TYPE_INT);
    moloch_field_by_exp_add_special_type("databytes.src", MOLOCH_FIELD_EXSPECIAL_DATABYTES_SRC, MOLOCH_FIELD_TYPE_INT);
    moloch_field_by_exp_add_special_type("databytes.dst", MOLOCH_FIELD_EXSPECIAL_DATABYTES_DST, MOLOCH_FIELD_TYPE_INT);
}
/******************************************************************************/
void moloch_field_exit()
{
    MolochFieldInfo_t *info;

    HASH_FORALL_POP_HEAD(d_, fieldsByDb, info,
        g_free(info->dbFieldFull);
        g_free(info->expression);
        g_free(info->group);
        g_free(info->kind);
        g_free(info->category);
        g_free(info->transform);
        MOLOCH_TYPE_FREE(MolochFieldInfo_t, info);
    );
}
/******************************************************************************/
