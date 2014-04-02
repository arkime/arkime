/******************************************************************************/
/* field.c  -- Functions dealing with declaring fields
 *
 * Copyright 2012-2014 AOL Inc. All rights reserved.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include "glib.h"
#include "moloch.h"
#include "patricia.h"

extern patricia_tree_t *ipTree;
extern MolochConfig_t        config;
HASH_VAR(f_, fields, MolochFieldInfo_t, 13);

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
            info->dbField = g_strndup((char*)data + out[i+2], out[i+3]);
            info->dbFieldLen = out[i+3];
        }
    }

    info->pos = -1;
    HASH_ADD(f_, fields, info->dbField, info);

    return;
}
/******************************************************************************/
/* Changes ... to va_list */
static void moloch_nids_add_field_proxy(char *group, char *kind, char *expression, char *friendlyName, char *dbField, char *help, ...)
{
    va_list args;
    va_start(args, help);
    moloch_db_add_field(group, kind, expression, friendlyName, dbField, help, args);
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
    int  pos = -1;

    MolochFieldInfo_t *minfo = 0;
    HASH_FIND(f_, fields, dbField, minfo);

    if (!minfo) {
        minfo = MOLOCH_TYPE_ALLOC0(MolochFieldInfo_t);
        minfo->dbField    = g_strdup(dbField);
        minfo->dbFieldLen = strlen(minfo->dbField);
        minfo->pos        = -1;
        minfo->expression = g_strdup(expression);
        minfo->group      = g_strdup(group);
        HASH_ADD(f_, fields, minfo->dbField, minfo);

        if ((flags & MOLOCH_FIELD_FLAG_NODB) == 0) {
            va_list args;
            va_start(args, flags);
            moloch_db_add_field(group, kind, expression, friendlyName, dbField, help, args);
            va_end(args);
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
            pos = config.maxField++;
            minfo->pos      = pos;
        }

        config.fields[pos] = minfo;

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
        return pos;

    MolochFieldInfo_t *info = 0;
    if (flags & MOLOCH_FIELD_FLAG_CNT) {
        sprintf(dbField2, "%scnt", dbField);
        HASH_FIND(f_, fields, dbField2, info);
        if (!info) {
            sprintf(expression2, "%s.cnt", expression);
            sprintf(friendlyName2, "%s Cnt", friendlyName);
            sprintf(help2, "Unique number of %s", help);
            moloch_db_add_field(group, "integer", expression2, friendlyName2, dbField2, help2, NULL);
        }
    }

    if (flags & MOLOCH_FIELD_FLAG_SCNT) {
        sprintf(dbField2, "%sscnt", dbField);
        HASH_FIND(f_, fields, dbField2, info);
        if (!info) {
            sprintf(expression2, "%s.cnt", expression);
            sprintf(friendlyName2, "%s Cnt", friendlyName);
            sprintf(help2, "Unique number of %s", help);
            moloch_db_add_field(group, "integer", expression2, friendlyName2, dbField2, help2, NULL);
        }
    }

    if (flags & MOLOCH_FIELD_FLAG_COUNT) {
        sprintf(dbField2, "%s-cnt", dbField);
        HASH_FIND(f_, fields, dbField2, info);
        if (!info) {
            sprintf(expression2, "%s.cnt", expression);
            sprintf(friendlyName2, "%s Cnt", friendlyName);
            sprintf(help2, "Unique number of %s", help);
            moloch_db_add_field(group, "integer", expression2, friendlyName2, dbField2, help2, NULL);
        }
    }

    if (flags & MOLOCH_FIELD_FLAG_FAKE) {
        g_free(minfo->expression);
        g_free(minfo->dbField);
        HASH_REMOVE(f_, fields, minfo);
        MOLOCH_TYPE_FREE(MolochFieldInfo_t, minfo);
        return -1;
    }

    if (flags & MOLOCH_FIELD_FLAG_IPPRE) {
        int fnlen = strlen(friendlyName);
        sprintf(dbField2, "g%s", dbField);
        HASH_FIND(f_, fields, dbField2, info);
        if (!info) {
            sprintf(expression2, "country.%s", expression+3);
            sprintf(friendlyName2, "%.*s GEO", fnlen-2, friendlyName);
            sprintf(help2, "GeoIP country string calculated from the %s", help);
            moloch_db_add_field(group, "uptermfield", expression2, friendlyName2, dbField2, help2, NULL);
        }

        sprintf(dbField2, "as%s", dbField);
        HASH_FIND(f_, fields, dbField2, info);
        if (!info) {
            sprintf(expression2, "asn.%s", expression+3);
            sprintf(friendlyName2, "%.*s ASN", fnlen-2, friendlyName);
            sprintf(help2, "GeoIP ASN string calculated from the %s", help);
            sprintf(rawField, "raw%s", dbField2);
            moloch_nids_add_field_proxy(group, "textfield", expression2, friendlyName2, dbField2, help2, "rawField", rawField, NULL);
        }

        sprintf(dbField2, "rir%s", dbField);
        HASH_FIND(f_, fields, dbField2, info);
        if (!info) {
            sprintf(expression2, "rir.%s", expression+3);
            sprintf(friendlyName2, "%.*s RIR", fnlen-2, friendlyName);
            sprintf(help2, "Regional Internet Registry string calculated from %s", help);
            moloch_db_add_field(group, "uptermfield", expression2, friendlyName2, dbField2, help2, NULL);
        }
    } else if (type == MOLOCH_FIELD_TYPE_IP || type == MOLOCH_FIELD_TYPE_IP_HASH) {
        sprintf(dbField2, "%s-geo", dbField);
        HASH_FIND(f_, fields, dbField2, info);
        if (!info) {
            sprintf(expression2, "%s.country", expression);
            sprintf(friendlyName2, "%s GEO", friendlyName);
            sprintf(help2, "GeoIP country string calculated from the %s", help);
            moloch_db_add_field(group, "uptermfield", expression2, friendlyName2, dbField2, help2, NULL);
        }

        sprintf(dbField2, "%s-asn.snow", dbField);
        HASH_FIND(f_, fields, dbField2, info);
        if (!info) {
            sprintf(dbField2, "%s-asn.snow", dbField);
            sprintf(expression2, "%s.asn", expression);
            sprintf(friendlyName2, "%s ASN", friendlyName);
            sprintf(rawField, "%s.raw", dbField);
            sprintf(help2, "GeoIP ASN string calculated from the %s", help);
            moloch_nids_add_field_proxy(group, "textfield", expression2, friendlyName2, dbField2, help2, "rawField", rawField, NULL);
        }

        sprintf(dbField2, "%s-rir", dbField);
        HASH_FIND(f_, fields, dbField2, info);
        if (!info) {
            sprintf(expression2, "%s.rir", expression);
            sprintf(friendlyName2, "%s RIR", friendlyName);
            sprintf(help2, "Regional Internet Registry string calculated from %s", help);
            moloch_db_add_field(group, "uptermfield", expression2, friendlyName2, dbField2, help2, NULL);
        }
    }
    return pos;
}
/******************************************************************************/
int moloch_field_by_db(char *dbField)
{
    MolochFieldInfo_t *info = 0;
    HASH_FIND(f_, fields, dbField, info);
    if (info)
        return info->pos;
    return -1;
}
/******************************************************************************/
int moloch_field_by_exp(char *exp)
{
    MolochFieldInfo_t *info = 0;
    HASH_FORALL(f_, fields, info,
        if (strcmp(exp, info->expression) == 0)
            return info->pos;
    );
    return -1;
}
/******************************************************************************/
void moloch_field_init()
{
    config.maxField = 0;
    HASH_INIT(f_, fields, moloch_string_hash, moloch_string_cmp);
}
/******************************************************************************/
void moloch_field_exit()
{
    int i;

    for (i = 0; i < config.maxField; i++) {
        MOLOCH_TYPE_FREE(MolochFieldInfo_t, config.fields[i]);
    }
}
/******************************************************************************/
gboolean moloch_field_string_add(int pos, MolochSession_t *session, const char *string, int len, gboolean copy)
{
    MolochField_t         *field;
    MolochStringHashStd_t *hash;
    MolochString_t        *hstring;

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
            HASH_INIT(s_, *hash, moloch_string_hash, moloch_string_cmp);
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
    field->jsonSize += 6 + 2*len;

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

        if (hstring)
            return FALSE;
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
        default:
            LOG("Not a int %s", config.fields[pos]->dbField);
            exit (1);
        }
    }

    field = session->fields[pos];
    field->jsonSize += 3 + 10;
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
        if (hint)
            return FALSE;
        hint = MOLOCH_TYPE_ALLOC(MolochInt_t);
        HASH_ADD(i_, *(field->ihash), (void *)(long)i, hint);
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
    field->jsonSize += 3 + len;
    switch (config.fields[pos]->type) {
    case MOLOCH_FIELD_TYPE_CERTSINFO:
        HASH_FIND(t_, *(field->cihash), certs, hci);
        if (hci)
            return FALSE;
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

    for (pos = 0; pos < config.maxField; pos++) {
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
