/* main.c  -- Initialization of components
 *
 * Copyright 2012 The AOL Moloch Authors.  All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/udp.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include "glib.h"
#include "pcap.h"
#include "moloch.h"
#include "molochconfig.h"

/******************************************************************************/
MolochConfig_t         config;
GMainLoop             *mainLoop;

/******************************************************************************/
gchar   *pcapFile       = NULL;
gboolean fakePcap       = FALSE;
gchar   *nodeName       = NULL;
gchar   *hostName       = NULL;
gchar   *configFile     = NULL;
gboolean showVersion    = FALSE;
gboolean debug          = FALSE;
gboolean dryRun         = FALSE;

static GOptionEntry entries[] =
{
    { "config",    'c',                    0, G_OPTION_ARG_FILENAME, &configFile,  "Config file name, default './config.ini'", NULL },
    { "pcapfile",  'r',                    0, G_OPTION_ARG_FILENAME, &pcapFile,    "Offline pcap file", NULL },
    { "node",      'n',                    0, G_OPTION_ARG_STRING,   &nodeName,    "Our node name, defaults to hostname.  Multiple nodes can run on same host.", NULL },
    { "version",   'v',                    0, G_OPTION_ARG_NONE,     &showVersion, "Show version number", NULL },
    { "debug",     'd',                    0, G_OPTION_ARG_NONE,     &debug,       "Turn on all debugging", NULL },
    { "fakepcap",    0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,     &fakePcap,    "fake pcap", NULL },
    { "dryrun",      0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,     &dryRun,      "dry run", NULL },
    { NULL,          0, 0,                                    0, NULL, NULL, NULL }
};


/******************************************************************************/
void parse_args(int argc, char **argv)
{
    GError *error = NULL;
    GOptionContext *context;

    context = g_option_context_new ("- capture");
    g_option_context_add_main_entries (context, entries, NULL);
    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
        g_print ("option parsing failed: %s\n", error->message);
        exit (1);
    }

    g_option_context_free(context);

    if (!configFile)
        configFile = g_strdup("config.ini");

    if (showVersion) {
        printf("moloch-capture %s\n", PACKAGE_VERSION);
        exit(0);
    }

    if (!nodeName) {
        nodeName = malloc(101);
        hostName = malloc(101);
        gethostname(nodeName, 101);
        gethostname(hostName, 101);
        nodeName[100] = 0;
        hostName[100] = 0;
        char *dot = strchr(nodeName, '.');
        if (dot)
            *dot = 0;
    }
    if (!hostName) {
        hostName = malloc(101);
        gethostname(hostName, 101);
        hostName[100] = 0;
    }
    if (debug) {
        LOG("nodeName = %s", nodeName);
        LOG("hostName = %s", hostName);
    }
}

/******************************************************************************/
void cleanup(int UNUSED(sig))
{

    moloch_nids_exit();
    moloch_yara_exit();
    moloch_db_exit();
    moloch_es_exit();
    moloch_config_exit();


    if (pcapFile)
        g_free(pcapFile);
    exit(0);
}
/******************************************************************************/
unsigned char *moloch_js0n_get(unsigned char *data, uint32_t len, char *key, uint32_t *olen)
{
    uint32_t key_len = strlen(key);
    int      i;
    uint32_t out[4*100]; // Can have up to 100 elements at any level

    memset(out, 0, sizeof(out));
    *olen = 0;
    if (js0n(data, len, out) != 0) {
        LOG("ERROR: Parse error for >%s< in >%.*s<\n", key, len, data);
        fflush(stdout);
        return 0;
    }

    for (i = 0; out[i]; i+= 4) {
        if (out[i+1] == key_len && memcmp(key, data + out[i], key_len) == 0) { 
            *olen = out[i+3];
            return data + out[i+2];
        }
    }
    return 0;
}
/******************************************************************************/
uint32_t moloch_string_hash(const void *key)
{
    char *p = (char *)key;
    uint32_t n = 0;
    while (*p) {
        n = (n << 5) - n + *p;
        p++;
    }
    return n;
}

/******************************************************************************/
int moloch_string_cmp(const void *keyv, const void *elementv)
{
    char *key = (char*)keyv;
    MolochString_t *element = (MolochString_t *)elementv;

    return strcmp(key, element->str) == 0;
}
/******************************************************************************/
uint32_t moloch_int_hash(const void *key)
{
    return (uint32_t)((long)key);
}

/******************************************************************************/
int moloch_int_cmp(const void *keyv, const void *elementv)
{
    int key = (int)((long)keyv);
    MolochInt_t *element = (MolochInt_t *)elementv;

    return key == element->i;
}
/******************************************************************************/
typedef struct {
    MolochWatchFd_func  func;
    gpointer            data;
} MolochWatchFd_t;

/******************************************************************************/
static void moloch_gio_destroy(gpointer data)
{
    g_free(data);
}
/******************************************************************************/
static gboolean moloch_gio_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
{
    MolochWatchFd_t *watch = data;

    return watch->func(g_io_channel_unix_get_fd(source), condition, watch->data);
}

/******************************************************************************/
gint moloch_watch_fd(gint fd, GIOCondition cond, MolochWatchFd_func func, gpointer data)
{

    MolochWatchFd_t *watch = g_new0(MolochWatchFd_t, 1);
    watch->func = func;
    watch->data = data;

    GIOChannel *channel = g_io_channel_unix_new(fd);

    gint id =  g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond, moloch_gio_invoke, watch, moloch_gio_destroy);

    g_io_channel_unref(channel);
    return id;
}

/******************************************************************************/
void moloch_drop_privileges()
{
    if (getuid() != 0)
        return;

    if (config.dropGroup) {
        struct group   *grp;
        grp = getgrnam(config.dropGroup);
        if (!grp) {
            LOG("ERROR: Group '%s' not found", config.dropGroup);
            exit(1);
        }

        if (setgid(grp->gr_gid) != 0) {
            LOG("ERROR: Couldn't change group - %s", strerror(errno));
            exit(1);
        }
    }

    if (config.dropUser) {
        struct passwd   *usr;
        usr = getpwnam(config.dropUser);
        if (!usr) {
            LOG("ERROR: User '%s' not found", config.dropUser);
            exit(1);
        }

        if (setuid(usr->pw_uid) != 0) {
            LOG("ERROR: Couldn't change user - %s", strerror(errno));
            exit(1);
        }
    }


}
/******************************************************************************/
int main(int argc, char **argv)
{
    signal(SIGINT, cleanup);
    signal(SIGUSR1, exit);
    signal(SIGCHLD, SIG_IGN);

    mainLoop = g_main_loop_new(NULL, FALSE);

    parse_args(argc, argv);
    moloch_config_init();
    moloch_nids_root_init();
    moloch_drop_privileges();
    moloch_es_init();
    moloch_db_init();
    moloch_yara_init();
    moloch_nids_init();

    if (!config.pcapDir) {
        printf("Must set a pcapDir\n");
        exit(1);
    }


    g_main_loop_run(mainLoop);
    cleanup(0);
    exit(0);
}
