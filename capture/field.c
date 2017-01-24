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

extern patricia_tree_t *ipTree;
extern MolochConfig_t        config;
HASH_VAR(d_, fieldsByDb, MolochFieldInfo_t, 13);
HASH_VAR(e_, fieldsByExp, MolochFieldInfo_t, 13);

#define MOLOCH_FIELD_SPECIAL_NOT_FOUND -1
#define MOLOCH_FIELD_SPECIAL_STOP_SPI  -2

LOCAL va_list empty_va_list;

/******************************************************************************/
int moloch_field_exp_cmp(const void *keyv, const void *elementv)
{
    char *key = (char*)keyv;
    MolochFieldInfo_t *element = (MolochFieldInfo_t *)elementv;

    return strcmp(key, element->expression) == 0;
}
/******************************************************************************/
void moloch_field_define_json(unsigned char *expression, int expression_len, unsigned char *data, int data_len)
{
    MolochFieldInfo_t *info = MOLOCH_TYPE_ALLOC0(MolochFieldInfo_t);
    int                i;
    uint32_t           out[4*100]; // Can have up to 100 elements at any level

    memset(out, 0, sizeof(out));
    if (js0n(data, data_len, out) != 0) {
        LOG("ERROR: Parse error for >%.*s<\n", data_len, data);
        fflush(stdout);
        exit(0);
    }

    info->expression = g_strndup((char*)expression, expression_len);
    for (i = 0; out[i]; i += 4) {
        if (strncmp("group", (char*)data + out[i], 5) == 0) {
            info->group = g_strndup((char*)data + out[i+2], out[i+3]);
        } else if (strncmp("dbField", (char*)data + out[i], 7) == 0) {
            info->dbFieldFull = info->dbField = g_strndup((char*)data + out[i+2], out[i+3]);
            info->dbFieldLen  = out[i+3];
        } else if (strncmp("type", (char*)data + out[i], 4) == 0) {
            info->kind = g_strndup((char*)data + out[i+2], out[i+3]);
        } else if (strncmp("category", (char*)data + out[i], 8) == 0) {
            info->category = g_strndup((char*)data + out[i+2], out[i+3]);
        } else if (strncmp("disabled", (char*)data + out[i], 8) == 0) {
            if (strncmp((char *)data + out[i+2], "true", 4) == 0) {
                info->flags    |= MOLOCH_FIELD_FLAG_DISABLED;
            }
        }
    }

    info->pos = -1;
    HASH_ADD(d_, fieldsByDb, info->dbField, info);
    HASH_ADD(e_, fieldsByExp, info->expression, info);

    return;
}
/******************************************************************************/
int moloch_field_define_text(char *text, int *shortcut)
{
    int count = 0;
    char *field = 0;
    char *kind = 0;
    char *help = 0;
    char *db = 0;
    char *group = 0;
    char *friendly = 0;
    char *category = 0;

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

    if (strstr(kind, "termfield") != 0 && strstr(db, "-term") == 0) {
        LOG("ERROR - db field %s for %s should end with -term in '%s'", kind, db, text);
        exit(1);
    }

    char groupbuf[100];
    if (!group) {
        char *dot = strchr(field, '.');
        if (dot) {
            memcpy(groupbuf, field, MIN(100, dot-field));
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
        flags |= MOLOCH_FIELD_FLAG_COUNT;

    int pos =  moloch_field_define(group, kind, field, friendly, db, help, type, flags, "category", category, NULL);
    g_strfreev(elements);
    return pos;
}
/******************************************************************************/
/* Changes ... to va_list */
static void moloch_session_add_field_proxy(char *group, char *kind, char *expression, char *friendlyName, char *dbField, char *help, ...)
{
    va_list args;
    va_start(args, help);
    moloch_db_add_field(group, kind, expression, friendlyName, dbField, help, TRUE, args);
    va_end(args);
}
/******************************************************************************/
int moloch_field_define(char *group, char *kind, char *expression, char *friendlyName, char *dbField, char *help, int type, int flags, ...)
{
    char dbField2[100];
    char expression2[1000];
    char friendlyName2[1000];
    char help2[1000];
    char rawField[100];

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
        char *category = NULL;
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
            }
        }
        va_end(args);
        if (category && (!minfo->category || strcmp(category, minfo->category) != 0)) {
            LOG("UPDATING - Field category in db %s doesn't match field category %s in capture for field %s", minfo->category, category, expression);
            moloch_db_update_field(expression, "category", category);
        }
    }

    // Hack to remove trailing .snow on capture side
    int dbLen = strlen(minfo->dbField);
    if (dbLen > 5 && memcmp(".snow", minfo->dbField+dbLen-5, 5) == 0) {
        minfo->dbField[dbLen-5] = 0;
        minfo->dbFieldLen -= 5;
    }

    minfo->type     = type;
    minfo->flags    = flags;

    if ((flags & MOLOCH_FIELD_FLAG_FAKE) == 0) {
        if (minfo->pos == -1) {
            minfo->pos = config.maxField++;
            if (config.maxField > 255) {
                LOG("ERROR - Max Fields is too large %d", config.maxField);
                exit(0);
            }
        }

        config.fields[minfo->pos] = minfo;

        // Change leading part to dbGroup
        char *firstdot = strchr(minfo->dbField, '.');
        if (firstdot) {
            static char lastGroup[100] = "";
            static int groupNum = 0;
            if (memcmp(minfo->dbField, lastGroup, (firstdot-minfo->dbField)+1) == 0) {
                minfo->dbGroupNum = groupNum;
            } else {
                groupNum++;
                minfo->dbGroupNum = groupNum;
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

    MolochFieldInfo_t *info = 0;
    if (flags & MOLOCH_FIELD_FLAG_CNT) {
        sprintf(dbField2, "%scnt", dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            sprintf(expression2, "%s.cnt", expression);
            sprintf(friendlyName2, "%s Cnt", friendlyName);
            sprintf(help2, "Unique number of %s", help);
            moloch_db_add_field(group, "integer", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
        }
    }

    if (flags & MOLOCH_FIELD_FLAG_SCNT) {
        sprintf(dbField2, "%sscnt", dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            sprintf(expression2, "%s.cnt", expression);
            sprintf(friendlyName2, "%s Cnt", friendlyName);
            sprintf(help2, "Unique number of %s", help);
            moloch_db_add_field(group, "integer", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
        }
    }

    if (flags & MOLOCH_FIELD_FLAG_COUNT) {
        sprintf(dbField2, "%s-cnt", dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            sprintf(expression2, "%s.cnt", expression);
            sprintf(friendlyName2, "%s Cnt", friendlyName);
            sprintf(help2, "Unique number of %s", help);
            moloch_db_add_field(group, "integer", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
        }
    }

    if (flags & MOLOCH_FIELD_FLAG_FAKE) {
        g_free(minfo->expression);
        g_free(minfo->dbField);
        g_free(minfo->group);
        g_free(minfo->kind);
        HASH_REMOVE(d_, fieldsByDb, minfo);
        HASH_REMOVE(e_, fieldsByExp, minfo);
        MOLOCH_TYPE_FREE(MolochFieldInfo_t, minfo);
        return -1;
    }

    if (flags & MOLOCH_FIELD_FLAG_IPPRE) {
        int fnlen = strlen(friendlyName);
        sprintf(dbField2, "g%s", dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            sprintf(expression2, "country.%s", expression+3);
            sprintf(friendlyName2, "%.*s GEO", fnlen-2, friendlyName);
            sprintf(help2, "GeoIP country string calculated from the %s", help);
            moloch_db_add_field(group, "uptermfield", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
        }

        sprintf(dbField2, "as%s", dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            sprintf(expression2, "asn.%s", expression+3);
            sprintf(friendlyName2, "%.*s ASN", fnlen-2, friendlyName);
            sprintf(help2, "GeoIP ASN string calculated from the %s", help);
            sprintf(rawField, "raw%s", dbField2);
            moloch_session_add_field_proxy(group, "textfield", expression2, friendlyName2, dbField2, help2, "rawField", rawField, NULL);
        }

        sprintf(dbField2, "rir%s", dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            sprintf(expression2, "rir.%s", expression+3);
            sprintf(friendlyName2, "%.*s RIR", fnlen-2, friendlyName);
            sprintf(help2, "Regional Internet Registry string calculated from %s", help);
            moloch_db_add_field(group, "uptermfield", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
        }
    } else if (type == MOLOCH_FIELD_TYPE_IP || type == MOLOCH_FIELD_TYPE_IP_HASH || type == MOLOCH_FIELD_TYPE_IP_GHASH) {
        sprintf(dbField2, "%s-geo", dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            sprintf(expression2, "%s.country", expression);
            sprintf(friendlyName2, "%s GEO", friendlyName);
            sprintf(help2, "GeoIP country string calculated from the %s", help);
            moloch_db_add_field(group, "uptermfield", expression2, friendlyName2, dbField2, help2, FALSE, empty_va_list);
        }

        sprintf(dbField2, "%s-asn.snow", dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            sprintf(dbField2, "%s-asn.snow", dbField);
            sprintf(expression2, "%s.asn", expression);
            sprintf(friendlyName2, "%s ASN", friendlyName);
            sprintf(rawField, "%s-asn.raw", dbField);
            sprintf(help2, "GeoIP ASN string calculated from the %s", help);
            moloch_session_add_field_proxy(group, "textfield", expression2, friendlyName2, dbField2, help2, "rawField", rawField, NULL);
        }

        sprintf(dbField2, "%s-rir", dbField);
        HASH_FIND(d_, fieldsByDb, dbField2, info);
        if (!info) {
            sprintf(expression2, "%s.rir", expression);
            sprintf(friendlyName2, "%s RIR", friendlyName);
            sprintf(help2, "Regional Internet Registry string calculated from %s", help);
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
    if (info)
        return info->pos;
    return -1;
}
/******************************************************************************/
int moloch_field_by_exp(const char *exp)
{
    MolochFieldInfo_t *info = 0;
    HASH_FIND(e_, fieldsByExp, exp, info);
    if (info)
        return info->pos;
    return -1;
}
/******************************************************************************/
void moloch_field_by_exp_add_special(char *exp, int pos)
{
    MolochFieldInfo_t *info = MOLOCH_TYPE_ALLOC0(MolochFieldInfo_t);
    info->expression = exp;
    info->pos = pos;
    HASH_ADD(e_, fieldsByExp, info->expression, info);
}
/******************************************************************************/
void moloch_field_init()
{
    config.maxField = 0;
    HASH_INIT(d_, fieldsByDb, moloch_string_hash, moloch_string_cmp);
    HASH_INIT(e_, fieldsByExp, moloch_string_hash, moloch_field_exp_cmp);

    moloch_field_by_exp_add_special("dontSaveSPI", MOLOCH_FIELD_SPECIAL_STOP_SPI);
}
/******************************************************************************/
void moloch_field_exit()
{
    MolochFieldInfo_t *info = 0;

    HASH_FORALL_POP_HEAD(d_, fieldsByDb, info,
        if (info->dbFieldFull)
            g_free(info->dbFieldFull);
        if (info->expression)
            g_free(info->expression);
        if (info->group)
            g_free(info->group);
        if (info->kind)
            g_free(info->kind);
        if (info->category)
            g_free(info->category);
        MOLOCH_TYPE_FREE(MolochFieldInfo_t, info);
    );
}
/******************************************************************************/
gboolean moloch_field_string_add(int pos, MolochSession_t *session, const char *string, int len, gboolean copy)
{
    MolochField_t         *field;
    MolochStringHashStd_t *hash;
    MolochString_t        *hstring;

    if (config.fields[pos]->flags & MOLOCH_FIELD_FLAG_DISABLED || pos >= session->maxFields)
        return FALSE;

    if (!session->fields[pos]) {
        field = MOLOCH_TYPE_ALLOC(MolochField_t);
        session->fields[pos] = field;
        if (len == -1)
            len = strlen(string);
        field->jsonSize = 6 + config.fields[pos]->dbFieldLen + 2*len;
        if (copy)
            string = g_strndup(string, len);
        switch (config.fields[pos]->type) {
        case MOLOCH_FIELD_TYPE_STR:
            field->str = (char*)string;
            return TRUE;
        case MOLOCH_FIELD_TYPE_STR_ARRAY:
            field->sarray = g_ptr_array_new_with_free_func(g_free);
            g_ptr_array_add(field->sarray, (char*)string);
            return TRUE;
        case MOLOCH_FIELD_TYPE_STR_HASH:
            hash = MOLOCH_TYPE_ALLOC(MolochStringHashStd_t);
            HASH_INIT(s_, *hash, moloch_string_hash, moloch_string_ncmp);
            field->shash = hash;
            hstring = MOLOCH_TYPE_ALLOC(MolochString_t);
            hstring->str = (char*)string;
            hstring->len = len;
            hstring->utf8 = 0;
            HASH_ADD(s_, *hash, hstring->str, hstring);
            return TRUE;
        default:
            LOG("Not a string %s", config.fields[pos]->dbField);
            exit (1);
        }
    }

    if (len == -1)
        len = strlen(string);

    field = session->fields[pos];
    field->jsonSize += (6 + 2*len);

    if (field->jsonSize > 20000)
        session->midSave = 1;

    switch (config.fields[pos]->type) {
    case MOLOCH_FIELD_TYPE_STR:
        if (copy)
            string = g_strndup(string, len);
        g_free(field->str);
        field->str = (char*)string;
        return TRUE;
    case MOLOCH_FIELD_TYPE_STR_ARRAY:
        if (copy)
            string = g_strndup(string, len);
        g_ptr_array_add(field->sarray, (char*)string);
        return TRUE;
    case MOLOCH_FIELD_TYPE_STR_HASH:
        HASH_FIND_HASH(s_, *(field->shash), moloch_string_hash_len(string, len), string, hstring);

        if (hstring) {
            field->jsonSize -= (6 + 2*len);
            return FALSE;
        }
        hstring = MOLOCH_TYPE_ALLOC(MolochString_t);
        if (copy) {
            hstring->str = g_strndup(string, len);
            hstring->len = len;
            hstring->utf8 = 0;
        } else {
            hstring->str = (char*)string;
            hstring->len = len;
            hstring->utf8 = 0;
        }
        HASH_ADD(s_, *(field->shash), hstring->str, hstring);
        return TRUE;
    default:
        LOG("Not a string %s", config.fields[pos]->dbField);
        exit (1);
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
        case MOLOCH_FIELD_TYPE_IP:
            field->jsonSize += 100;
        case MOLOCH_FIELD_TYPE_INT:
            field->i = i;
            return TRUE;
        case MOLOCH_FIELD_TYPE_INT_ARRAY:
            field->iarray = g_array_new(FALSE, FALSE, 4);
            g_array_append_val(field->iarray, i);
            return TRUE;
        case MOLOCH_FIELD_TYPE_IP_HASH:
            field->jsonSize += 100;
        case MOLOCH_FIELD_TYPE_INT_HASH:
            hash = MOLOCH_TYPE_ALLOC(MolochIntHashStd_t);
            HASH_INIT(i_, *hash, moloch_int_hash, moloch_int_cmp);
            field->ihash = hash;
            hint = MOLOCH_TYPE_ALLOC(MolochInt_t);
            HASH_ADD(i_, *hash, (void *)(long)i, hint);
            return TRUE;
        case MOLOCH_FIELD_TYPE_IP_GHASH:
            field->jsonSize += 100;
        case MOLOCH_FIELD_TYPE_INT_GHASH:
            field->ghash = g_hash_table_new(NULL, NULL);
            g_hash_table_insert(field->ghash, (void *)(long)i, NULL);
            return TRUE;
        default:
            LOG("Not a int %s", config.fields[pos]->dbField);
            exit (1);
        }
    }

    field = session->fields[pos];
    field->jsonSize += (3 + 10);
    switch (config.fields[pos]->type) {
    case MOLOCH_FIELD_TYPE_IP:
        field->jsonSize += 100;
    case MOLOCH_FIELD_TYPE_INT:
        field->i = i;
        return TRUE;
    case MOLOCH_FIELD_TYPE_INT_ARRAY:
        g_array_append_val(field->iarray, i);
        return TRUE;
    case MOLOCH_FIELD_TYPE_IP_HASH:
        field->jsonSize += 100;
    case MOLOCH_FIELD_TYPE_INT_HASH:
        HASH_FIND_INT(i_, *(field->ihash), i, hint);
        if (hint) {
            field->jsonSize -= (3 + 10);
            return FALSE;
        }
        hint = MOLOCH_TYPE_ALLOC(MolochInt_t);
        HASH_ADD(i_, *(field->ihash), (void *)(long)i, hint);
        return TRUE;
    case MOLOCH_FIELD_TYPE_IP_GHASH:
        if (!g_hash_table_insert(field->ghash, (void *)(long)i, NULL)) {
            field->jsonSize -= 13;
            return FALSE;
        } else {
            field->jsonSize += 100;
            return TRUE;
        }
    case MOLOCH_FIELD_TYPE_INT_GHASH:
        if (!g_hash_table_insert(field->ghash, (void *)(long)i, NULL)) {
            field->jsonSize -= 13;
            return FALSE;
        }
        return TRUE;
    default:
        LOG("Not a int %s", config.fields[pos]->dbField);
        exit (1);
    }
}
/******************************************************************************/
uint32_t moloch_field_certsinfo_hash(const void *key)
{
    MolochCertsInfo_t *ci = (MolochCertsInfo_t *)key;

    return ((ci->serialNumber[0] << 28) |
            (ci->serialNumber[ci->serialNumberLen-1] << 24) |
            (ci->issuer.commonName.s_count << 18) |
            (ci->issuer.orgName?ci->issuer.orgName[0] << 12:0) |
            (ci->subject.commonName.s_count << 6) |
            (ci->subject.orgName?ci->subject.orgName[0]:0));
}

/******************************************************************************/
int moloch_field_certsinfo_cmp(const void *keyv, const void *elementv)
{
    MolochCertsInfo_t *key = (MolochCertsInfo_t *)keyv;
    MolochCertsInfo_t *element = (MolochCertsInfo_t *)elementv;

    if ( !((key->serialNumberLen == element->serialNumberLen) &&
           (memcmp(key->serialNumber, element->serialNumber, element->serialNumberLen) == 0) &&
           (key->issuer.commonName.s_count == element->issuer.commonName.s_count) &&
           (key->issuer.orgName == element->issuer.orgName || strcmp(key->issuer.orgName, element->issuer.orgName) == 0) &&
           (key->subject.commonName.s_count == element->subject.commonName.s_count) &&
           (key->subject.orgName == element->subject.orgName || strcmp(key->subject.orgName, element->subject.orgName) == 0)
          )
       ) {

        return 0;
    }

    MolochString_t *kstr, *estr;
    for (kstr = key->issuer.commonName.s_next, estr = element->issuer.commonName.s_next;
         kstr != (void *)&(key->issuer.commonName);
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
        field->jsonSize = 3 + config.fields[pos]->dbFieldLen + len;
        switch (config.fields[pos]->type) {
        case MOLOCH_FIELD_TYPE_CERTSINFO:
            hash = MOLOCH_TYPE_ALLOC(MolochCertsInfoHashStd_t);
            HASH_INIT(t_, *hash, moloch_field_certsinfo_hash, moloch_field_certsinfo_cmp);
            field->cihash = hash;
            HASH_ADD(t_, *hash, certs, certs);
            return TRUE;
        default:
            LOG("Not a certsinfo %s", config.fields[pos]->dbField);
            exit (1);
        }
    }

    field = session->fields[pos];
    switch (config.fields[pos]->type) {
    case MOLOCH_FIELD_TYPE_CERTSINFO:
        HASH_FIND(t_, *(field->cihash), certs, hci);
        if (hci)
            return FALSE;
        field->jsonSize += 3 + len;
        HASH_ADD(t_, *(field->cihash), certs, certs);
        return TRUE;
    default:
        LOG("Not a certsinfo %s", config.fields[pos]->dbField);
        exit (1);
    }
}
/******************************************************************************/
void moloch_field_free(MolochSession_t *session)
{
    int                       pos;
    MolochField_t            *field;
    MolochString_t           *hstring;
    MolochStringHashStd_t    *shash;
    MolochInt_t              *hint;
    MolochIntHashStd_t       *ihash;
    MolochCertsInfo_t        *hci;
    MolochCertsInfoHashStd_t *cihash;

    for (pos = 0; pos < session->maxFields; pos++) {
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
        case MOLOCH_FIELD_TYPE_IP_HASH:
        case MOLOCH_FIELD_TYPE_INT_HASH:
            ihash = session->fields[pos]->ihash;
            HASH_FORALL_POP_HEAD(i_, *ihash, hint,
                MOLOCH_TYPE_FREE(MolochInt_t, hint);
            );
            MOLOCH_TYPE_FREE(MolochIntHashStd_t, ihash);
            break;
        case MOLOCH_FIELD_TYPE_IP_GHASH:
        case MOLOCH_FIELD_TYPE_INT_GHASH:
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

    while (DLL_POP_HEAD(s_, &certs->subject.commonName, string)) {
        g_free(string->str);
        MOLOCH_TYPE_FREE(MolochString_t, string);
    }

    if (certs->issuer.orgName)
        g_free(certs->issuer.orgName);
    if (certs->subject.orgName)
        g_free(certs->subject.orgName);
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
    case MOLOCH_FIELD_TYPE_IP_HASH:
        return HASH_COUNT(s_, *(field->ihash));
    case MOLOCH_FIELD_TYPE_INT_GHASH:
    case MOLOCH_FIELD_TYPE_IP_GHASH:
        return g_hash_table_size(field->ghash);
    case MOLOCH_FIELD_TYPE_CERTSINFO:
        return HASH_COUNT(s_, *(field->cihash));
    default:
        LOG("ERROR - Unknown field type for counting %s %d", config.fields[pos]->dbField, config.fields[pos]->type);
        exit (1);
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
            }
            continue;
        }

        switch (config.fields[op->fieldPos]->type) {
        case  MOLOCH_FIELD_TYPE_INT_HASH:
        case  MOLOCH_FIELD_TYPE_INT_GHASH:
            if (op->fieldPos == config.tagsField) {
                moloch_session_add_tag(session, op->str);
                continue;
            }
            // Fall Thru
        case  MOLOCH_FIELD_TYPE_INT:
        case  MOLOCH_FIELD_TYPE_INT_ARRAY:
        case  MOLOCH_FIELD_TYPE_IP:
        case  MOLOCH_FIELD_TYPE_IP_HASH:
        case  MOLOCH_FIELD_TYPE_IP_GHASH:
            moloch_field_int_add(op->fieldPos, session, op->strLenOrInt);
            break;
        case  MOLOCH_FIELD_TYPE_STR:
        case  MOLOCH_FIELD_TYPE_STR_ARRAY:
        case  MOLOCH_FIELD_TYPE_STR_HASH:
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
            if (ops->ops[i].str)
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
            op->strLenOrInt = atoi(value);
            op->str = 0;
            break;
        default:
            LOG("WARNING - Unknown special field pos %d", fieldPos);
            break;
        }
    } else {
        switch (config.fields[fieldPos]->type) {
        case  MOLOCH_FIELD_TYPE_INT_HASH:
        case  MOLOCH_FIELD_TYPE_INT_GHASH:
            if (fieldPos == config.tagsField) {
                moloch_db_get_tag(NULL, config.tagsField, value, NULL); // Preload the tagname -> tag mapping
                if (ops->flags & MOLOCH_FIELD_OPS_FLAGS_COPY)
                    op->str = g_strndup(value, valuelen);
                else
                    op->str = value;
                op->strLenOrInt = valuelen;
                break;
            }
            // Fall thru
        case  MOLOCH_FIELD_TYPE_INT:
        case  MOLOCH_FIELD_TYPE_INT_ARRAY:
            op->str = 0;
            op->strLenOrInt = atoi(value);
            break;
        case  MOLOCH_FIELD_TYPE_STR:
        case  MOLOCH_FIELD_TYPE_STR_ARRAY:
        case  MOLOCH_FIELD_TYPE_STR_HASH:
            if (ops->flags & MOLOCH_FIELD_OPS_FLAGS_COPY)
                op->str = g_strndup(value, valuelen);
            else
                op->str = value;
            op->strLenOrInt = valuelen;
            break;
        case  MOLOCH_FIELD_TYPE_IP:
        case  MOLOCH_FIELD_TYPE_IP_HASH:
        case  MOLOCH_FIELD_TYPE_IP_GHASH:
            op->str = 0;
            op->strLenOrInt = inet_addr(value);
            break;
        default:
            LOG("WARNING - Unsupported expression type %d for %s", config.fields[fieldPos]->type, value);
            return;
        }
    }
    ops->num++;
}
/******************************************************************************/
