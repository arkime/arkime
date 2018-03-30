/* tagger.c  -- Simple plugin that tags sessions by using ip, hosts, md5s
 *              lists fetched from the ES database.  taggerUpdate.pl is
 *              used to upload files to the database.  tagger checks
 *              once a minute to see if the files in the database have
 *              changed.
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


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "patricia.h"
#include "moloch.h"


/******************************************************************************/

extern MolochConfig_t        config;

extern void                 *esServer;

LOCAL  int                   tagsField;
LOCAL  int                   httpHostField;
LOCAL  int                   httpXffField;
LOCAL  int                   httpMd5Field;
LOCAL  int                   httpPathField;
LOCAL  int                   emailMd5Field;
LOCAL  int                   emailSrcField;
LOCAL  int                   emailDstField;
LOCAL  int                   dnsHostField;

/******************************************************************************/

typedef struct tagger_string {
    struct tagger_string *s_next, *s_prev;
    char                 *str;
    GPtrArray            *infos;
    uint32_t              s_hash;
    uint16_t              s_bucket;
} TaggerString_t;

typedef struct {
    struct tagger_string *s_next, *s_prev;
    int                   s_count;
} TaggerStringHead_t;

/******************************************************************************/

typedef struct tagger_ip {
    GPtrArray            *infos;
} TaggerIP_t;

/******************************************************************************/

typedef struct tagger_file {
    struct tagger_file   *s_next, *s_prev;
    char                 *str;
    char                 *md5;
    char                 *type;
    char                **tags;
    char                **elements;
    uint32_t              s_hash;
    uint16_t              s_bucket;
} TaggerFile_t;

typedef struct {
    struct tagger_file   *s_next, *s_prev;
    int                   s_count;
} TaggerFileHead_t;

/******************************************************************************/

typedef struct tagger_info {
    MolochFieldOps_t  ops;
    TaggerFile_t     *file;
} TaggerInfo_t;

/******************************************************************************/
typedef HASH_VAR(s_, TaggerStringHash_t, TaggerStringHead_t, 37277);

TaggerStringHash_t allDomains;
TaggerStringHash_t allMD5s;
TaggerStringHash_t allEmails;
TaggerStringHash_t allURIs;

HASH_VAR(s_, allFiles, TaggerFileHead_t, 101);

LOCAL  patricia_tree_t *allIps;

/******************************************************************************/
LOCAL void tagger_process_match(MolochSession_t *session, GPtrArray *infos)
{
    uint32_t f, t;
    for (f = 0; f < infos->len; f++) {
        TaggerInfo_t *info = g_ptr_array_index(infos, f);
        TaggerFile_t *file = info->file;
        for (t = 0; file->tags[t]; t++) {
            moloch_session_add_tag(session, file->tags[t]);
        }
        moloch_field_ops_run(session, &info->ops);
    }
}
/******************************************************************************/
/*
 * Called by moloch when a session is about to be saved
 */
LOCAL void tagger_plugin_save(MolochSession_t *session, int UNUSED(final))
{
    TaggerString_t *tstring;

    patricia_node_t *nodes[PATRICIA_MAXBITS+1];
    int cnt;
    prefix_t prefix;

    int i;

    if (IN6_IS_ADDR_V4MAPPED(&session->addr1)) {
        prefix.family = AF_INET;
        prefix.bitlen = 32;
        prefix.add.sin.s_addr = MOLOCH_V6_TO_V4(session->addr1);
    } else {
        prefix.family = AF_INET;
        prefix.bitlen = 128;
        memcpy(&prefix.add.sin6.s6_addr, &session->addr1, 16);
    }

    cnt = patricia_search_all(allIps, &prefix, 1, nodes);
    for (i = 0; i < cnt; i++) {
        tagger_process_match(session, ((TaggerIP_t *)(nodes[i]->data))->infos);
    }

    if (IN6_IS_ADDR_V4MAPPED(&session->addr2)) {
        prefix.family = AF_INET;
        prefix.bitlen = 32;
        prefix.add.sin.s_addr = MOLOCH_V6_TO_V4(session->addr2);
    } else {
        prefix.family = AF_INET;
        prefix.bitlen = 128;
        memcpy(&prefix.add.sin6.s6_addr, &session->addr2, 16);
    }

    cnt = patricia_search_all(allIps, &prefix, 1, nodes);
    for (i = 0; i < cnt; i++) {
        tagger_process_match(session, ((TaggerIP_t *)(nodes[i]->data))->infos);
    }

    if (httpXffField != -1 && session->fields[httpXffField]) {
        GHashTable            *ghash;
        GHashTableIter         iter;
        gpointer               ikey;

        ghash = session->fields[httpXffField]->ghash;
        g_hash_table_iter_init (&iter, ghash);
        while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
            if (IN6_IS_ADDR_V4MAPPED((struct in6_addr*)ikey)) {
                prefix.family = AF_INET;
                prefix.bitlen = 32;
                prefix.add.sin.s_addr = MOLOCH_V6_TO_V4(*(struct in6_addr*)ikey);
            } else {
                prefix.family = AF_INET6;
                prefix.bitlen = 128;
                memcpy(&prefix.add.sin6.s6_addr, ikey, 16);
            }

            cnt = patricia_search_all(allIps, &prefix, 1, nodes);
            for (i = 0; i < cnt; i++) {
                tagger_process_match(session, ((TaggerIP_t *)(nodes[i]->data))->infos);
            }
        }
    }

    MolochString_t *hstring;
    if (httpHostField != -1 && session->fields[httpHostField]) {
        MolochStringHashStd_t *shash = session->fields[httpHostField]->shash;
        HASH_FORALL(s_, *shash, hstring,
            HASH_FIND_HASH(s_, allDomains, hstring->s_hash, hstring->str, tstring);
            if (tstring)
                tagger_process_match(session, tstring->infos);
            char *dot = strchr(hstring->str, '.');
            if (dot && *(dot+1)) {
                HASH_FIND(s_, allDomains, dot+1, tstring);
                if (tstring)
                    tagger_process_match(session, tstring->infos);
            }
        );
    }
    if (dnsHostField != -1 && session->fields[dnsHostField]) {
        MolochStringHashStd_t *shash = session->fields[dnsHostField]->shash;
        HASH_FORALL(s_, *shash, hstring,
            HASH_FIND_HASH(s_, allDomains, hstring->s_hash, hstring->str, tstring);
            if (tstring)
                tagger_process_match(session, tstring->infos);
            char *dot = strchr(hstring->str, '.');
            if (dot && *(dot+1)) {
                HASH_FIND(s_, allDomains, dot+1, tstring);
                if (tstring)
                    tagger_process_match(session, tstring->infos);
            }
        );
    }

    if (httpMd5Field != -1 && session->fields[httpMd5Field]) {
        MolochStringHashStd_t *shash = session->fields[httpMd5Field]->shash;
        HASH_FORALL(s_, *shash, hstring,
            HASH_FIND_HASH(s_, allMD5s, hstring->s_hash, hstring->str, tstring);
            if (tstring)
                tagger_process_match(session, tstring->infos);
        );
    }

    if (httpPathField != -1 && session->fields[httpPathField]) {
        MolochStringHashStd_t *shash = session->fields[httpPathField]->shash;
        HASH_FORALL(s_, *shash, hstring,
            HASH_FIND_HASH(s_, allURIs, hstring->s_hash, hstring->str, tstring);
            if (tstring) {
                tagger_process_match(session, tstring->infos);
            }
        );
    }

    if (emailMd5Field != -1 && session->fields[emailMd5Field]) {
        MolochStringHashStd_t *shash = session->fields[emailMd5Field]->shash;
        HASH_FORALL(s_, *shash, hstring,
            HASH_FIND_HASH(s_, allMD5s, hstring->s_hash, hstring->str, tstring);
            if (tstring)
                tagger_process_match(session, tstring->infos);
        );
    }

    if (emailSrcField != -1 && session->fields[emailSrcField]) {
        MolochStringHashStd_t *shash = session->fields[emailSrcField]->shash;
        HASH_FORALL(s_, *shash, hstring,
            HASH_FIND_HASH(s_, allEmails, hstring->s_hash, hstring->str, tstring);
            if (tstring)
                tagger_process_match(session, tstring->infos);
        );
    }

    if (emailDstField != -1 && session->fields[emailDstField]) {
        MolochStringHashStd_t *shash = session->fields[emailDstField]->shash;
        HASH_FORALL(s_, *shash, hstring,
            HASH_FIND_HASH(s_, allEmails, hstring->s_hash, hstring->str, tstring);
            if (tstring)
                tagger_process_match(session, tstring->infos);
        );
    }
}

/******************************************************************************/
LOCAL void tagger_free_ip (TaggerIP_t *tip) 
{
    g_ptr_array_free(tip->infos, TRUE);
    MOLOCH_TYPE_FREE(TaggerIP_t, tip);
}
/******************************************************************************/
/*
 * Called by moloch when moloch is quiting
 */
LOCAL void tagger_plugin_exit()
{
    TaggerString_t *tstring;
    HASH_FORALL_POP_HEAD(s_, allDomains, tstring,
        free(tstring->str);
        g_ptr_array_free(tstring->infos, TRUE);
        MOLOCH_TYPE_FREE(TaggerString_t, tstring);
    );

    HASH_FORALL_POP_HEAD(s_, allMD5s, tstring,
        free(tstring->str);
        g_ptr_array_free(tstring->infos, TRUE);
        MOLOCH_TYPE_FREE(TaggerString_t, tstring);
    );

    HASH_FORALL_POP_HEAD(s_, allEmails, tstring,
        free(tstring->str);
        g_ptr_array_free(tstring->infos, TRUE);
        MOLOCH_TYPE_FREE(TaggerString_t, tstring);
    );

    HASH_FORALL_POP_HEAD(s_, allURIs, tstring,
        free(tstring->str);
        g_ptr_array_free(tstring->infos, TRUE);
        MOLOCH_TYPE_FREE(TaggerString_t, tstring);
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

    Destroy_Patricia(allIps, tagger_free_ip);
}

/******************************************************************************/
LOCAL void tagger_remove_file(GPtrArray *infos, TaggerFile_t *file)
{
    int f;
    for (f = 0; f < (int)infos->len; f++) {
        if (file == ((TaggerInfo_t *)g_ptr_array_index(infos, f))->file) {
            g_ptr_array_remove_index_fast(infos, f);
            return;
        }
    }
}
/******************************************************************************/
/*
 * Free most of the memory used by a file
 */
LOCAL void tagger_unload_file(TaggerFile_t *file) {
    int i;
    if (file->type[0] == 'i') {
        prefix_t prefix;

        for (i = 0; file->elements[i]; i++) {
            if (!ascii2prefix2(AF_INET, file->elements[i], &prefix)) {
                LOG("Couldn't unload %s", file->elements[i]);
                continue;
            }

            patricia_node_t *node = patricia_search_exact(allIps, &prefix);
            if (!node || !(node->data)) {
                LOG("Couldn't unload %s", file->elements[i]);
                continue;
            }

            tagger_remove_file(((TaggerIP_t *)(node->data))->infos, file);
        }
        return;
    }

    TaggerStringHash_t *hash = 0;
    switch (file->type[0]) {
    case 'h':
        hash = (TaggerStringHash_t *)&allDomains;
        break;
    case 'm':
        hash = (TaggerStringHash_t *)&allMD5s;
        break;
    case 'e':
        hash = (TaggerStringHash_t *)&allEmails;
        break;
    case 'u':
        hash = (TaggerStringHash_t *)&allURIs;
        break;
    default:
        LOG("ERROR - Unknown tagger type %s for %s", file->type, file->str);
    }

    TaggerString_t *tstring;
    if (hash) {
        for (i = 0; file->elements[i]; i++) {
            HASH_FIND(s_, *hash, file->elements[i], tstring);
            if (tstring) {
                tagger_remove_file(tstring->infos, file);
                // We could check if files is now empty and remove the node, but the
                // theory is most of the time it will be just readded in the load_file
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
LOCAL void tagger_info_free(gpointer data)
{
    TaggerInfo_t *info = data;

    moloch_field_ops_free(&info->ops);
    MOLOCH_TYPE_FREE(TaggerInfo_t, info);
}
/******************************************************************************/
/*
 * File data from ES
 */
LOCAL void tagger_load_file_cb(int UNUSED(code), unsigned char *data, int data_len, gpointer uw)
{
    TaggerFile_t *file = uw;
    uint32_t out[4*100];

    if (file->md5)
        tagger_unload_file(file);

    memset(out, 0, sizeof(out));
    if (!data_len || !data) {
        HASH_REMOVE(s_, allFiles, file);
        free(file->str);
        MOLOCH_TYPE_FREE(TaggerFile_t, file);
        return;
    }

    int rc;
    if ((rc = js0n(data, data_len, out)) != 0) {
        LOG("ERROR: Parse error %d in >%.*s<\n", rc, data_len, data);
        HASH_REMOVE(s_, allFiles, file);
        free(file->str);
        MOLOCH_TYPE_FREE(TaggerFile_t, file);
        return;
    }

    unsigned int fieldShortHand[20];
    memset(fieldShortHand, 0xff, sizeof(fieldShortHand));

    int i;
    for (i = 0; out[i]; i+= 4) {
        if (out[i+1] == 3 && memcmp("md5", data + out[i], sizeof("md5")-1) == 0) {
            file->md5 = g_strndup((char*)data + out[i+2], out[i+3]);
        } else if (out[i+1] == 4 && memcmp("tags", data + out[i], sizeof("tags")-1) == 0) {
            data[out[i+2] + out[i+3]] = 0;
            file->tags = g_strsplit((char*)data + out[i+2], ",", 0);
        } else if (out[i+1] == sizeof("type")-1 && memcmp("type", data + out[i], sizeof("type")-1) == 0) {
            file->type = g_strndup((char*)data + out[i+2], out[i+3]);
        } else if (out[i+1] == sizeof("data")-1 && memcmp("data", data + out[i], sizeof("data")-1) == 0) {
            data[out[i+2] + out[i+3]] = 0;
            file->elements = g_strsplit((char*)data + out[i+2], ",", 0);
        } else if (out[i+1] == sizeof("fields") - 1 && memcmp("fields", data + out[i], sizeof("fields")-1) == 0) {
            data[out[i+2] + out[i+3]] = 0;
            char **fields = g_strsplit((char*)data + out[i+2], ",", 0);
            int f;
            for (f = 0; fields[f] && f < 100; f++) {
                int shortcut = -1;
                int pos = moloch_field_define_text(fields[f], &shortcut);
                if (shortcut != -1 && shortcut >= 0 && shortcut < 20)
                    fieldShortHand[shortcut] = pos;
            }
            g_strfreev(fields);
        }
    }

    patricia_node_t *node;
    TaggerIP_t *tip;

    for (i = 0; file->elements[i]; i++) {

        int p = 2;
        char *parts[100];
        char *str = file->elements[i];

        parts[0] = file->elements[i];
        parts[1] = 0;
        while (*str) {
            if (*str == ';' || *str == '=') {
                if (!str[1])
                    break;
                parts[p] = str+1;
                p++;
                *str = 0;
            }
            str++;
        }

        TaggerInfo_t *info = MOLOCH_TYPE_ALLOC(TaggerInfo_t);
        info->file = file;
        moloch_field_ops_init(&info->ops, p-2, 0);

        int j;
        for(j = 2; j < p; j+=2) {
            int pos = -1;
            if (isdigit(parts[j][0])) {
                unsigned int f = atoi(parts[j]);
                if (f < 20 && fieldShortHand[f] != 0xffffffff)
                    pos = fieldShortHand[f];
            } else {
                pos = moloch_field_by_exp(parts[j]);
            }
            if (pos == -1) {
                LOG("WARNING - Unknown expression field %s", parts[j]);
                continue;
            }

            moloch_field_ops_add(&info->ops, pos, parts[j+1], strlen(parts[j+1]));
        }

        TaggerStringHash_t *hash = 0;
        switch (file->type[0]) {
        case 'i':
            node = make_and_lookup(allIps, parts[0]);
            if (!node) {
                LOG("Couldn't create node for %s", parts[0]);
                continue;
            }
            if (!node->data) {
                tip = MOLOCH_TYPE_ALLOC(TaggerIP_t);
                tip->infos = g_ptr_array_new_with_free_func(tagger_info_free);
                node->data = tip;
            } else {
                tip = node->data;
            }
            g_ptr_array_add(tip->infos, info);
            continue;
        case 'h':
            hash = (TaggerStringHash_t *)&allDomains;
            break;
        case 'm':
            hash = (TaggerStringHash_t *)&allMD5s;
            break;
        case 'e':
            hash = (TaggerStringHash_t *)&allEmails;
            break;
        case 'u':
            hash = (TaggerStringHash_t *)&allURIs;
            break;
        default:
            LOG("ERROR - Unknown tagger type %s for %s", file->type, file->str);
            continue;
        } 

        TaggerString_t *tstring;

        HASH_FIND(s_, *hash, parts[0], tstring);
        if (!tstring) {
            tstring = MOLOCH_TYPE_ALLOC(TaggerString_t);
            tstring->str = strdup(parts[0]); // Need to strdup since file might be unloaded
            tstring->infos = g_ptr_array_new_with_free_func(tagger_info_free);
            HASH_ADD(s_, *hash, tstring->str, tstring);
        }
        g_ptr_array_add(tstring->infos, info);
    } /* for elements */
}
/******************************************************************************/
/*
 * Start loading a file from database
 */
LOCAL void tagger_load_file(TaggerFile_t *file)
{
    char                key[500];
    int                 key_len;

    key_len = snprintf(key, sizeof(key), "/tagger/file/%s/_source", file->str);

    moloch_http_send(esServer, "GET", key, key_len, NULL, 0, NULL, FALSE, tagger_load_file_cb, file);
}
/******************************************************************************/
/*
 * Process the list of files from ES
 */
LOCAL void tagger_fetch_files_cb(int UNUSED(code), unsigned char *data, int data_len, gpointer UNUSED(uw))
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
        uint32_t           source_len;
        unsigned char     *source = 0;
        source = moloch_js0n_get(hits+out[i], out[i+1], "_source", &source_len);
        if (!source) {
            continue;
        }

        char     *id = moloch_js0n_get_str(hits+out[i], out[i+1], "_id");

        uint32_t           md5_len;
        unsigned char     *md5 = 0;
        md5 = moloch_js0n_get(source, source_len, "md5", &md5_len);

        if (*md5 == '[') {
            md5+=2;
            md5_len -= 4;
        }


        TaggerFile_t *file;
        HASH_FIND(s_, allFiles, id, file);
        if (!file) {
            file = MOLOCH_TYPE_ALLOC0(TaggerFile_t);
            file->str = id;
            HASH_ADD(s_, allFiles, file->str, file);
            tagger_load_file(file);
            continue;
        }
        g_free(id);
        if (!file->md5 || strncmp(file->md5, (char*)md5, md5_len) != 0) {
            tagger_load_file(file);
        }
    }
}

/******************************************************************************/
/*
 * Get the list of files from ES, when called at start up it will be a sync call
 */
LOCAL gboolean tagger_fetch_files (gpointer sync)
{
    char                key[500];
    int                 key_len;

    key_len = snprintf(key, sizeof(key), "/tagger/_search?_source=md5&size=999");

    /* Need to copy the data since sync uses a static buffer, should fix that */
    if (sync) {
        size_t         data_len;
        unsigned char *data = moloch_http_send_sync(esServer, "GET", key, key_len, NULL, 0, NULL, &data_len);;
        tagger_fetch_files_cb(200, data, data_len, NULL);
        free(data);
    } else {
        moloch_http_send(esServer, "GET", key, key_len, NULL, 0, NULL, FALSE, tagger_fetch_files_cb, NULL);
    }

    return TRUE;
}
/******************************************************************************/
/*
 * Called by moloch when the plugin is loaded
 */
void moloch_plugin_init()
{
    if (config.dryRun) {
        LOG("Not enabling in dryRun mode");
        return;
    }

    HASH_INIT(s_, allFiles, moloch_string_hash, moloch_string_cmp);
    HASH_INIT(s_, allDomains, moloch_string_hash, moloch_string_cmp);
    HASH_INIT(s_, allMD5s, moloch_string_hash, moloch_string_cmp);
    HASH_INIT(s_, allEmails, moloch_string_hash, moloch_string_cmp);
    HASH_INIT(s_, allURIs, moloch_string_hash, moloch_string_cmp);
    allIps = New_Patricia(128);

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

    tagsField      = moloch_field_by_db("tags");
    httpHostField  = moloch_field_by_db("http.host");
    httpXffField   = moloch_field_by_db("http.xffIp");
    httpMd5Field   = moloch_field_by_db("http.md5");
    httpPathField  = moloch_field_by_db("http.path");
    emailMd5Field  = moloch_field_by_db("email.md5");
    emailSrcField  = moloch_field_by_db("email.src");
    emailDstField  = moloch_field_by_db("email.dst");
    dnsHostField   = moloch_field_by_db("dns.host");

    /* Call right away sync, and schedule every 60 seconds async */
    tagger_fetch_files((gpointer)1);
    g_timeout_add_seconds(60, tagger_fetch_files, 0);

}
