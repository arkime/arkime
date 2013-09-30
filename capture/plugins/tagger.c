/* tagger.c  -- Simple plugin that tags sessions by using ip and hosts 
 *              lists fetched from the ES database.  taggerUpdate.pl is
 *              used to upload files to the database.  tagger checks
 *              once a minute to see if the files in the database have 
 *              changed.
 *     
 * Copyright 2012 AOL Inc. All rights reserved.
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


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "moloch.h"
#include "nids.h"


/******************************************************************************/

extern MolochConfig_t        config;

extern void                 *esServer;

/******************************************************************************/

typedef struct tagger_string {
    struct tagger_string *s_next, *s_prev;
    char                 *str;
    uint32_t              s_hash;
    short                 s_bucket;
    GPtrArray            *files;
} TaggerString_t;

typedef struct {
    struct tagger_string *s_next, *s_prev;
    int                   s_count;
} TaggerStringHead_t;

/******************************************************************************/

typedef struct tagger_int {
    struct tagger_int    *i_next, *i_prev;
    uint32_t              i_hash;
    short                 i_bucket;
    GPtrArray            *files;
} TaggerInt_t;

typedef struct {
    struct tagger_int *i_next, *i_prev;
    int i_count;
} TaggerIntHead_t;

/******************************************************************************/

typedef struct tagger_file {
    struct tagger_file   *s_next, *s_prev;
    char                 *str;
    uint32_t              s_hash;
    short                 s_bucket;
    char                 *md5;
    char                 *type;
    char                **tags;
    char                **elements;
} TaggerFile_t;

typedef struct {
    struct tagger_file   *s_next, *s_prev;
    int                   s_count;
} TaggerFileHead_t;

/******************************************************************************/

HASH_VAR(s_, allFiles, TaggerFileHead_t, 101);
HASH_VAR(s_, allDomains, TaggerStringHead_t, 101);
HASH_VAR(i_, allIps, TaggerIntHead_t, 101);

/******************************************************************************/
void tagger_add_tags(MolochSession_t *session, GPtrArray *files)
{
    uint32_t f, t;
    for (f = 0; f < files->len; f++) {
        TaggerFile_t *file = files->pdata[f];
        for (t = 0; file->tags[t]; t++) {
            moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, file->tags[t]);
        }
    }
}
/******************************************************************************/
/* 
 * Called by moloch when a session is about to be saved
 */
void tagger_plugin_save(MolochSession_t *session, int UNUSED(final))
{
    TaggerInt_t    *ti;
    TaggerString_t *tstring;

    HASH_FIND_INT(i_, allIps, session->addr1, ti);
    if (ti)
        tagger_add_tags(session, ti->files);

    HASH_FIND_INT(i_, allIps, session->addr2, ti);
    if (ti)
        tagger_add_tags(session, ti->files);

    if (session->fields[MOLOCH_FIELD_HTTP_XFF]) {
        MolochIntHashStd_t *ihash = session->fields[MOLOCH_FIELD_HTTP_XFF]->ihash;
        MolochInt_t        *xff;

        HASH_FORALL(i_, *ihash, xff, 
            HASH_FIND_INT(i_, allIps, xff->i_hash, ti);
            if (ti)
                tagger_add_tags(session, ti->files);
        );
    }

    MolochString_t *hstring;
    if (session->fields[MOLOCH_FIELD_HTTP_HOST]) {
        MolochStringHashStd_t *shash = session->fields[MOLOCH_FIELD_HTTP_HOST]->shash;
        HASH_FORALL(s_, *shash, hstring, 
            HASH_FIND_HASH(s_, allDomains, hstring->s_hash, hstring->str, tstring);
            if (tstring)
                tagger_add_tags(session, tstring->files);
        );
    }
    if (session->fields[MOLOCH_FIELD_DNS_HOST]) {
        MolochStringHashStd_t *shash = session->fields[MOLOCH_FIELD_DNS_HOST]->shash;
        HASH_FORALL(s_, *shash, hstring, 
            HASH_FIND_HASH(s_, allDomains, hstring->s_hash, hstring->str, tstring);
            if (tstring)
                tagger_add_tags(session, tstring->files);
        );
    }
}

/******************************************************************************/
/* 
 * Called by moloch when moloch is quiting
 */
void tagger_plugin_exit()
{
    TaggerString_t *tstring;
    HASH_FORALL_POP_HEAD(s_, allDomains, tstring, 
        free(tstring->str);
        g_ptr_array_free(tstring->files, TRUE);
        MOLOCH_TYPE_FREE(TaggerString_t, tstring);
    );

    TaggerInt_t *ti;
    HASH_FORALL_POP_HEAD(i_, allIps, ti, 
        g_ptr_array_free(ti->files, TRUE);
        MOLOCH_TYPE_FREE(TaggerInt_t, ti);
    );

    TaggerFile_t *file;
    HASH_FORALL_POP_HEAD(s_, allFiles, file, 
        free(file->str);
        g_free(file->md5);
        g_free(file->type);
        g_strfreev(file->tags);
        g_strfreev(file->elements);
        MOLOCH_TYPE_FREE(TaggerFile_t, file);
    );
}

/******************************************************************************/
/*
 * Free most of the memory used by a file
 */
void tagger_unload_file(TaggerFile_t *file) {
    int i;
    for (i = 0; file->elements[i]; i++) {
        if (file->type[0] == 'i') {
            uint32_t ip = inet_addr(file->elements[i]);
            if (ip == 0xffffffff)
                continue;
            TaggerInt_t *ti;
            HASH_FIND_INT(i_, allIps, ip, ti);
            if (ti) {
                g_ptr_array_remove(ti->files, file);
            }
        } else {
            TaggerString_t *tstring;
            HASH_FIND(s_, allDomains, file->elements[i], tstring);
            if (tstring) {
                g_ptr_array_remove(tstring->files, file);
            }
        } 
    }

    g_free(file->md5);
    g_free(file->type);
    g_strfreev(file->tags);
    g_strfreev(file->elements);
    file->md5 = NULL;
}
/******************************************************************************/
/*
 * File data from ES
 */
void tagger_load_file_cb(unsigned char *data, int data_len, gpointer uw)
{
    TaggerFile_t *file = uw;

    if (file->md5)
        tagger_unload_file(file);

    uint32_t           source_len;
    unsigned char     *source = 0;

    source = moloch_js0n_get(data, data_len, "_source", &source_len);

    if (!source_len || !source) {
        HASH_REMOVE(s_, allFiles, file);
        free(file->str);
        MOLOCH_TYPE_FREE(TaggerFile_t, file);
        return;
    }

    file->md5 = moloch_js0n_get_str(source, source_len, "md5");

    char *tags = moloch_js0n_get_str(source, source_len, "tags");
    file->tags = g_strsplit(tags, ",", 0);
    g_free(tags);

    int tag = 0;
    for (tag = 0; file->tags[tag]; tag++) {
        moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, file->tags[tag], NULL);
    }

    file->type = moloch_js0n_get_str(source, source_len, "type");

    char *jdata = moloch_js0n_get_str(source, source_len, "data");
    file->elements = g_strsplit(jdata, ",", 0);
    g_free(jdata);

    int i;
    for (i = 0; file->elements[i]; i++) {
        if (file->type[0] == 'i') {
            uint32_t ip = inet_addr(file->elements[i]);
            if (ip == 0xffffffff)
                continue;

            TaggerInt_t *ti;
            HASH_FIND_INT(i_, allIps, ip, ti);
            if (!ti) {
                ti = MOLOCH_TYPE_ALLOC(TaggerInt_t);
                ti->files = g_ptr_array_new();
                HASH_ADD(i_, allIps, (void *)(long)ip, ti);
            }
            g_ptr_array_add(ti->files, file);
        } else {
            TaggerString_t *tstring;

            HASH_FIND(s_, allDomains, file->elements[i], tstring);
            if (!tstring) {
                tstring = MOLOCH_TYPE_ALLOC(TaggerString_t);
                tstring->str = strdup(file->elements[i]);
                tstring->files = g_ptr_array_new();
                HASH_ADD(s_, allDomains, tstring->str, tstring);
            }
            g_ptr_array_add(tstring->files, file);
        }
    }
}
/******************************************************************************/
/*
 * Start loading a file from database
 */
void tagger_load_file(TaggerFile_t *file, gpointer sync)
{
    char                key[500];
    int                 key_len;

    key_len = snprintf(key, sizeof(key), "/tagger/file/%s", file->str);

    if (sync) {
        size_t         data_len;
        unsigned char *data = moloch_http_send_sync(esServer, "GET", key, key_len, NULL, 0, &data_len);;
        tagger_load_file_cb(data, data_len, file);
    } else {
        moloch_http_send(esServer, "GET", key, key_len, NULL, 0, FALSE, tagger_load_file_cb, file);
    }
}
/******************************************************************************/
/*
 * Process the list of files from ES
 */
void tagger_fetch_files_cb(unsigned char *data, int data_len, gpointer sync)
{
    uint32_t           hits_len;
    unsigned char     *hits = moloch_js0n_get(data, data_len, "hits", &hits_len);

    if (!hits_len || !hits)
        return;

    hits = moloch_js0n_get(hits, hits_len, "hits", &hits_len);

    if (!hits_len || !hits)
        return;

    uint32_t out[2*8000];
    memset(out, 0, sizeof(out));
    js0n(hits, hits_len, out);
    int i;
    for (i = 0; out[i]; i+= 2) {
        uint32_t           fields_len;
        unsigned char     *fields = 0;
        fields = moloch_js0n_get(hits+out[i], out[i+1], "fields", &fields_len);
        if (!fields) {
            continue;
        }

        char     *id = moloch_js0n_get_str(hits+out[i], out[i+1], "_id");

        uint32_t           md5_len;
        unsigned char     *md5 = 0;
        md5 = moloch_js0n_get(fields, fields_len, "md5", &md5_len);

        TaggerFile_t *file;
        HASH_FIND(s_, allFiles, id, file);
        if (!file) {
            file = MOLOCH_TYPE_ALLOC0(TaggerFile_t);
            file->str = id;
            HASH_ADD(s_, allFiles, file->str, file);
            tagger_load_file(file, sync);
            continue;
        }
        g_free(id);
        if (!file->md5 || strncmp(file->md5, (char*)md5, md5_len) != 0) {
            tagger_load_file(file, sync);
        }
    }
}

/******************************************************************************/
/* 
 * Get the list of files from ES, when called at start up it will be a sync call
 */
gboolean tagger_fetch_files (gpointer sync)
{
    char                key[500];
    int                 key_len;

    key_len = snprintf(key, sizeof(key), "/tagger/_search?fields=md5");

    /* Need to copy the data since sync uses a static buffer, should fix that */
    if (sync) {
        size_t         data_len;
        unsigned char *data = moloch_http_send_sync(esServer, "GET", key, key_len, NULL, 0, &data_len);;
        unsigned char *datacopy = (unsigned char*)g_strndup((char*)data, data_len);
        tagger_fetch_files_cb(datacopy, data_len, sync);
        g_free(datacopy);
    } else {
        moloch_http_send(esServer, "GET", key, key_len, NULL, 0, FALSE, tagger_fetch_files_cb, sync);
    }

    return TRUE;
}
/******************************************************************************/
/*
 * Called by moloch when the plugin is loaded
 */
void moloch_plugin_init()
{
    HASH_INIT(s_, allFiles, moloch_string_hash, moloch_string_cmp);
    HASH_INIT(s_, allDomains, moloch_string_hash, moloch_string_cmp);
    HASH_INIT(i_, allIps, moloch_int_hash, moloch_int_cmp);

    moloch_plugins_register("tagger", FALSE);

    moloch_plugins_set_cb("tagger",
      NULL,
      NULL,
      NULL,
      NULL,
      tagger_plugin_save,
      NULL,
      tagger_plugin_exit,
      NULL
    );

    /* Call right away sync, and schedule every 60 seconds async */
    tagger_fetch_files((gpointer)1);
    g_timeout_add_seconds(60, tagger_fetch_files, 0);
}
