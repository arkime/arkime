#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "moloch.h"
#include "nids.h"


#define FILE_INFO_MAX 100

/******************************************************************************/


extern MolochConfig_t        config;

static gchar   **ipFiles;
static gchar   **domainFiles;
static gchar   **fileInfo[FILE_INFO_MAX];
static int     fileInfoMax = 0;

/******************************************************************************/


typedef struct tagger_string {
    struct tagger_string *s_next, *s_prev;
    char                 *str;
    short                 s_bucket;
    int                   filenum;
} TaggerString_t;

typedef struct {
    struct tagger_string *s_next, *s_prev;
    int                   s_count;
} TaggerStringHead_t;

typedef struct tagger_int {
    struct tagger_int    *i_next, *i_prev;
    int                   i;
    short                 i_bucket;
    int                   filenum;
} TaggerInt_t;

typedef struct {
    struct tagger_int *i_next, *i_prev;
    int i_count;
} TaggerIntHead_t;

HASH_VAR(s_, domains, TaggerStringHead_t, 101);
HASH_VAR(i_, ips, TaggerIntHead_t, 101);

/******************************************************************************/
void tagger_add_tags(MolochSession_t *session, int filenum)
{
    int p;

    for (p = 1; fileInfo[filenum][p]; p++) {
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, fileInfo[filenum][p]);
    }

}
/******************************************************************************/
void tagger_plugin_save(MolochSession_t *session, int UNUSED(final))
{
    TaggerInt_t    *ti;
    TaggerString_t *tstring;

    HASH_FIND(i_, ips, (void*)(long)session->addr1, ti);
    if (ti)
        tagger_add_tags(session, ti->filenum);

    HASH_FIND(i_, ips, (void*)(long)session->addr2, ti);
    if (ti)
        tagger_add_tags(session, ti->filenum);

    MolochInt_t *xff;
    HASH_FORALL(i_, session->xffs, xff, 
        HASH_FIND(i_, ips, (void*)(long)xff->i, ti);
        if (ti)
            tagger_add_tags(session, ti->filenum);
    );

    MolochString_t *hstring;
    HASH_FORALL(s_, session->hosts, hstring, 
        HASH_FIND(s_, domains, hstring->str, tstring);
        if (tstring)
            tagger_add_tags(session, tstring->filenum);
    );
}

/******************************************************************************/
void tagger_plugin_exit()
{
    TaggerString_t *string;
    HASH_FORALL_POP_HEAD(s_, domains, string, 
        free(string->str);
        free(string);
    );

    TaggerInt_t *ti;
    HASH_FORALL_POP_HEAD(i_, ips, ti, 
        free(ti);
    );

    while (fileInfoMax > 0) {
        g_strfreev(fileInfo[--fileInfoMax]);
    }
}
/******************************************************************************/
void tagger_load_files()
{

    tagger_plugin_exit();

    int i, p;

    /* Load ip files */
    for (i = 0; fileInfoMax < FILE_INFO_MAX && ipFiles[i]; i++) {
        gchar **parts = g_strsplit(ipFiles[i], ",", 0);
        if (!parts[0] || !parts[1]) {
            LOG("Need to have at least 1 tag with file '%s'", ipFiles[i]);
            exit (1);
        }
        for (p = 0; parts[p]; p++) {
            while (isspace(*parts[p]))
                parts[p]++;
            g_strchomp(parts[p]);

            if (p > 0)
                moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, parts[p], NULL);
        }

        FILE *file = fopen(parts[0], "r");
        if (!file) {
            LOG("Couldn't open file '%s'", parts[0]);
            continue;
        }

        fileInfo[fileInfoMax++] = parts;

        char line[1024];
        while (fgets(line, sizeof(line), file)) {
            if (line[0] == '#' || line[0] == '\n') {
                continue;
            }

            uint32_t ip = inet_addr(line);
            if (ip == 0xffffffff)
                continue;

            TaggerInt_t *ti;

            HASH_FIND(i_, ips, (void*)(long)ip, ti);
            if (!ti) {
                ti = malloc(sizeof(*ti));
                ti->i = ip;
                ti->filenum = fileInfoMax - 1;
                HASH_ADD(i_, ips, (void *)(long)ti->i, ti);
            }
        }
        fclose(file);
        if (config.debug)
            LOG("Loaded ip file %s", parts[0]);
    }

    /* Load domain files */
    for (i = 0; fileInfoMax < FILE_INFO_MAX && domainFiles[i]; i++) {
        gchar **parts = g_strsplit(domainFiles[i], ",", 0);
        if (!parts[0] || !parts[1]) {
            LOG("Need to have at least 1 tag with file '%s'", domainFiles[i]);
            exit (1);
        }
        for (p = 0; parts[p]; p++) {
            while (isspace(*parts[p]))
                parts[p]++;
            g_strchomp(parts[p]);

            if (p > 0)
                moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, parts[p], NULL);
        }

        FILE *file = fopen(parts[0], "r");
        if (!file) {
            LOG("Couldn't open file '%s'", parts[0]);
            continue;
        }

        fileInfo[fileInfoMax++] = parts;

        char line[1024];
        while (fgets(line, sizeof(line), file)) {
            if (line[0] == '#' || line[0] == '\n') {
                continue;
            }
            g_strchomp(line);
            if (strlen(line) < 3)
                continue;

            TaggerString_t *string;

            HASH_FIND(s_, domains, line, string);
            if (!string) {
                string = malloc(sizeof(*string));
                string->str = strdup(line);
                string->filenum = fileInfoMax - 1;
                HASH_ADD(s_, domains, string->str, string);
            }
        }
        fclose(file);
        if (config.debug)
            LOG("Loaded domain file %s", parts[0]);
    }

}

/******************************************************************************/
void tagger_plugin_reload()
{
    tagger_load_files();
}

/******************************************************************************/
void moloch_plugin_init()
{
    HASH_INIT(s_, domains, moloch_string_hash, moloch_string_cmp);
    HASH_INIT(i_, ips, moloch_int_hash, moloch_int_cmp);

    ipFiles = moloch_config_str_list(NULL, "taggerIpFiles", NULL);
    domainFiles = moloch_config_str_list(NULL, "taggerDomainFiles", NULL);

    moloch_plugins_register("tagger", FALSE);

    moloch_plugins_set_cb("tagger",
      NULL,
      NULL,
      NULL,
      tagger_plugin_save,
      NULL,
      tagger_plugin_exit,
      tagger_plugin_reload
    );

    tagger_load_files();
}
