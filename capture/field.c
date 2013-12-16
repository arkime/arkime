/******************************************************************************/
/* field.c  -- Functions dealing with declaring fields
 *
 * Copyright 2012-2013 AOL Inc. All rights reserved.
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
#include "glib.h"
#include "moloch.h"

extern MolochConfig_t        config;

/******************************************************************************/
int moloch_field_define(char *name, int type, int flags)
{
    MolochFieldInfo_t *info = MOLOCH_TYPE_ALLOC0(MolochFieldInfo_t);
    info->name  = name;
    info->type  = type;
    info->flags = flags;
    info->pos   = config.maxField;
    config.fields[config.maxField] = info;
    config.maxField++;

    return info->pos;
}
/******************************************************************************/
void moloch_field_define_internal(int pos, char *name, int type, int flags)
{
    MolochFieldInfo_t *info = MOLOCH_TYPE_ALLOC0(MolochFieldInfo_t);
    info->name  = name;
    info->len   = strlen(name);
    info->type  = type;
    info->flags = flags;
    info->pos   = pos;
    config.fields[pos] = info;
    if (config.maxField <= pos)
        config.maxField = pos+1;
}
/******************************************************************************/
int moloch_field_get(char *name)
{
    int i;
    for (i = 0; i < config.maxField; i++) {
        if (config.fields[i] && strcmp(config.fields[i]->name, name) == 0)
            return i;
    }
    return -1;
}
/******************************************************************************/
void moloch_field_init()
{
    config.maxField = 0;
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
gboolean moloch_field_string_add(int pos, MolochSession_t *session, char *string, int len, gboolean copy)
{
    MolochField_t         *field;
    MolochStringHashStd_t *hash;
    MolochString_t        *hstring;

    if (!session->fields[pos]) {
        field = MOLOCH_TYPE_ALLOC(MolochField_t);
        session->fields[pos] = field;
        if (len == -1)
            len = strlen(string);
        field->jsonSize = 6 + config.fields[pos]->len + 2*len;
        if (copy)
            string = g_strndup(string, len);
        switch (config.fields[pos]->type) {
        case MOLOCH_FIELD_TYPE_STR:
            field->str = string;
            return TRUE;
        case MOLOCH_FIELD_TYPE_STR_ARRAY:
            field->sarray = g_ptr_array_new_with_free_func(g_free);
            g_ptr_array_add(field->sarray, string);
            return TRUE;
        case MOLOCH_FIELD_TYPE_STR_HASH:
            hash = MOLOCH_TYPE_ALLOC(MolochStringHashStd_t);
            HASH_INIT(s_, *hash, moloch_string_hash, moloch_string_cmp);
            field->shash = hash;
            hstring = MOLOCH_TYPE_ALLOC(MolochString_t);
            hstring->str = string;
            hstring->len = len;
            hstring->utf8 = 0;
            HASH_ADD(s_, *hash, hstring->str, hstring);
            return TRUE;
        default:
            LOG("Not a string %s", config.fields[pos]->name);
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
        field->str = string;
        return TRUE;
    case MOLOCH_FIELD_TYPE_STR_ARRAY:
        if (copy)
            string = g_strndup(string, len);
        g_ptr_array_add(field->sarray, string);
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
            hstring->str = string;
            hstring->len = len;
            hstring->utf8 = 0;
        }
        HASH_ADD(s_, *(field->shash), hstring->str, hstring);
        return TRUE;
    default:
        LOG("Not a string %s", config.fields[pos]->name);
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
        field->jsonSize = 3 + config.fields[pos]->len + 10;
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
            LOG("Not a int %s", config.fields[pos]->name);
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
        LOG("Not a int %s", config.fields[pos]->name);
        exit (1);
    }
}
/******************************************************************************/
void moloch_field_free(MolochSession_t *session)
{
    int                    pos;
    MolochField_t         *field;
    MolochString_t        *hstring;
    MolochStringHashStd_t *shash;
    MolochInt_t           *hint;
    MolochIntHashStd_t    *ihash;

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
        }
        MOLOCH_TYPE_FREE(MolochField_t, session->fields[pos]);
    }
    MOLOCH_SIZE_FREE(fields, session->fields);
    session->fields = 0;
}
