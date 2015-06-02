/* main.c  -- Initialization of components
 *
 * Copyright 2012-2015 AOL Inc. All rights reserved.
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
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <ctype.h>
#include <sys/resource.h>
#ifdef _POSIX_MEMLOCK
#include <sys/mman.h>
#endif
#ifdef _POSIX_PRIORITY_SCHEDULING
#include <sched.h>
#endif
#include "pcap.h"
#include "moloch.h"
#include "molochconfig.h"

/******************************************************************************/
MolochConfig_t         config;
extern void           *esServer;
GMainLoop             *mainLoop;
char                  *moloch_char_to_hex = "0123456789abcdef"; /* don't change case */
unsigned char          moloch_char_to_hexstr[256][3];
unsigned char          moloch_hex_to_char[256][256];

extern MolochWriterQueueLength moloch_writer_queue_length;

/******************************************************************************/
static gboolean showVersion    = FALSE;
static gboolean needNidsExit   = TRUE;

/******************************************************************************/
gboolean moloch_debug_flag()
{
    config.debug++;
    return TRUE;
}

static GOptionEntry entries[] =
{
    { "config",    'c',                    0, G_OPTION_ARG_FILENAME,       &config.configFile,    "Config file name, default '/data/moloch/etc/config.ini'", NULL },
    { "pcapfile",  'r',                    0, G_OPTION_ARG_FILENAME_ARRAY, &config.pcapReadFiles, "Offline pcap file", NULL },
    { "pcapdir",   'R',                    0, G_OPTION_ARG_FILENAME_ARRAY, &config.pcapReadDirs,  "Offline pcap directory, all *.pcap files will be processed", NULL },
    { "monitor",   'm',                    0, G_OPTION_ARG_NONE,           &config.pcapMonitor,   "Used with -R option monitors the directory for closed files", NULL },
    { "delete",      0,                    0, G_OPTION_ARG_NONE,           &config.pcapDelete,    "In offline mode delete files once processed, requires --copy", NULL },
    { "skip",      's',                    0, G_OPTION_ARG_NONE,           &config.pcapSkip,      "Used with -R option and without --copy, skip files already processed", NULL },
    { "recursive",   0,                    0, G_OPTION_ARG_NONE,           &config.pcapRecursive, "When in offline pcap directory mode, recurse sub directories", NULL },
    { "node",      'n',                    0, G_OPTION_ARG_STRING,         &config.nodeName,      "Our node name, defaults to hostname.  Multiple nodes can run on same host", NULL },
    { "tag",       't',                    0, G_OPTION_ARG_STRING_ARRAY,   &config.extraTags,     "Extra tag to add to all packets, can be used multiple times", NULL },
    { "version",   'v',                    0, G_OPTION_ARG_NONE,           &showVersion,          "Show version number", NULL },
    { "debug",     'd', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,       moloch_debug_flag,     "Turn on all debugging", NULL },
    { "copy",        0,                    0, G_OPTION_ARG_NONE,           &config.copyPcap,      "When in offline mode copy the pcap files into the pcapDir from the config file", NULL },
    { "dryrun",      0,                    0, G_OPTION_ARG_NONE,           &config.dryRun,        "dry run, noting written to databases or filesystem", NULL },
    { "nospi",       0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,           &config.noSPI,         "no SPI data written to ES", NULL },
    { "tests",       0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,           &config.tests,         "Output test suite information", NULL },
    { NULL,          0, 0,                                    0,           NULL, NULL, NULL }
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

    config.pcapReadOffline = (config.pcapReadFiles || config.pcapReadDirs);

    if (!config.configFile)
        config.configFile = g_strdup("/data/moloch/etc/config.ini");

    if (showVersion) {
        printf("moloch-capture %s session size=%zd\n", PACKAGE_VERSION, sizeof(MolochSession_t));
        exit(0);
    }

    if (!config.nodeName) {
        config.nodeName = malloc(101);
        config.hostName = malloc(101);
        gethostname(config.nodeName, 101);
        gethostname(config.hostName, 101);
        config.nodeName[100] = 0;
        config.hostName[100] = 0;
        char *dot = strchr(config.nodeName, '.');
        if (dot)
            *dot = 0;
    }
    if (!config.hostName) {
        config.hostName = malloc(101);
        gethostname(config.hostName, 101);
        config.hostName[100] = 0;
    }
    if (config.debug) {
        LOG("debug = %d", config.debug);
        LOG("nodeName = %s", config.nodeName);
        LOG("hostName = %s", config.hostName);
    }

    if (config.tests) {
        config.dryRun = 1;
    }

    if (config.pcapSkip && config.copyPcap)  {
        printf("Can't skip and copy pcap files\n");
        exit(1);
    }

    if (config.pcapDelete && !config.copyPcap)  {
        printf("--delete requires --copy\n");
        exit(1);
    }

    if (config.copyPcap && !config.pcapReadOffline)  {
        printf("--copy requires -r or -R\n");
        exit(1);
    }

    if (config.pcapMonitor && !config.pcapReadDirs)  {
        printf("Must specify directories to monitor\n");
        exit(1);
    }
}
/******************************************************************************/
void *moloch_size_alloc(int size, int zero)
{
    size += 8;
    void *mem = (zero?g_slice_alloc0(size):g_slice_alloc(size));
    memcpy(mem, &size, 4);
    return mem + 8;
}
/******************************************************************************/
int moloch_size_free(void *mem)
{
    int size;
    mem -= 8;

    memcpy(&size, mem, 4);
    g_slice_free1(size, mem);
    return size - 8;
}
/******************************************************************************/
void cleanup(int UNUSED(sig))
{
    LOG("exiting");
    if (needNidsExit)
        moloch_nids_exit();
    moloch_plugins_exit();
    moloch_parsers_exit();
    moloch_yara_exit();
    moloch_db_exit();
    moloch_http_exit();
    moloch_config_exit();
    moloch_field_exit();


    if (config.pcapReadFiles)
        g_strfreev(config.pcapReadFiles);
    if (config.pcapReadDirs)
        g_strfreev(config.pcapReadDirs);
    if (config.extraTags)
        g_strfreev(config.extraTags);
    exit(0);
}
/******************************************************************************/
void reload(int UNUSED(sig))
{
    moloch_plugins_reload();
}
/******************************************************************************/
unsigned char *moloch_js0n_get(unsigned char *data, uint32_t len, char *key, uint32_t *olen)
{
    uint32_t key_len = strlen(key);
    int      i;
    uint32_t out[4*100]; // Can have up to 100 elements at any level

    memset(out, 0, sizeof(out));
    *olen = 0;
    int rc;
    if ((rc = js0n(data, len, out)) != 0) {
        LOG("ERROR: Parse error %d for >%s< in >%.*s<\n", rc, key, len, data);
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
char *moloch_js0n_get_str(unsigned char *data, uint32_t len, char *key)
{
    uint32_t           value_len;
    unsigned char     *value = 0;

    value = moloch_js0n_get(data, len, key, &value_len);
    if (!value)
        return NULL;
    return g_strndup((gchar*)value, value_len);
}
/******************************************************************************/
const char *moloch_memstr(const char *haystack, int haysize, const char *needle, int needlesize)
{
    const char *p;
    const char *end = haystack + haysize - needlesize;

    for (p = haystack; p <= end; p++)
    {
        if (p[0] == needle[0] && memcmp(p+1, needle+1, needlesize-1) == 0)
            return p;
    }
    return NULL;
}
/******************************************************************************/
const char *moloch_memcasestr(const char *haystack, int haysize, const char *needle, int needlesize)
{
    const char *p;
    const char *end = haystack + haysize - needlesize;
    int i;

    for (p = haystack; p <= end; p++)
    {
        for (i = 0; i < needlesize; i++) {
            if (tolower(p[i]) != needle[i]) {
                goto memcasestr_outer;
            }
        }
        return p;

        memcasestr_outer: ;
    }
    return NULL;
}
/******************************************************************************/
gboolean moloch_string_add(void *hashv, char *string, gpointer uw, gboolean copy)
{
    MolochStringHash_t *hash = hashv;
    MolochString_t *hstring;

    HASH_FIND(s_, *hash, string, hstring);
    if (hstring) {
        hstring->uw = uw;
        return FALSE;
    }

    hstring = MOLOCH_TYPE_ALLOC0(MolochString_t);
    if (copy) {
        hstring->str = g_strdup(string);
    } else {
        hstring->str = string;
    }
    hstring->len = strlen(string);
    hstring->uw = uw;
    HASH_ADD(s_, *hash, hstring->str, hstring);
    return TRUE;
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
uint32_t moloch_string_hash_len(const void *key, int len)
{
    char *p = (char *)key;
    uint32_t n = 0;
    while (len) {
        n = (n << 5) - n + *p;
        p++;
        len--;
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
int moloch_string_ncmp(const void *keyv, const void *elementv)
{
    char *key = (char*)keyv;
    MolochString_t *element = (MolochString_t *)elementv;

    return strncmp(key, element->str, element->len) == 0;
}
/******************************************************************************/
uint32_t moloch_int_hash(const void *key)
{
    return (uint32_t)((long)key);
}

/******************************************************************************/
int moloch_int_cmp(const void *keyv, const void *elementv)
{
    uint32_t key = (uint32_t)((long)keyv);
    MolochInt_t *element = (MolochInt_t *)elementv;

    return key == element->i_hash;
}
/******************************************************************************/
void moloch_session_id (char *buf, uint32_t addr1, uint16_t port1, uint32_t addr2, uint16_t port2)
{
    if (addr1 < addr2) {
        *(uint32_t *)buf = addr1;
        *(uint16_t *)(buf+4) = port1;
        *(uint32_t *)(buf+6) = addr2;
        *(uint16_t *)(buf+10) = port2;
    } else if (addr1 > addr2) {
        *(uint32_t *)buf = addr2;
        *(uint16_t *)(buf+4) = port2;
        *(uint32_t *)(buf+6) = addr1;
        *(uint16_t *)(buf+10) = port1;
    } else if (ntohs(port1) < ntohs(port2)) {
        *(uint32_t *)buf = addr1;
        *(uint16_t *)(buf+4) = port1;
        *(uint32_t *)(buf+6) = addr2;
        *(uint16_t *)(buf+10) = port2;
    } else {
        *(uint32_t *)buf = addr2;
        *(uint16_t *)(buf+4) = port2;
        *(uint32_t *)(buf+6) = addr1;
        *(uint16_t *)(buf+10) = port1;
    }
}
/******************************************************************************/
/* Must match moloch_session_cmp and moloch_session_id
 * a1 0-3
 * p1 4-5
 * a2 6-9
 * p2 10-11
 */
uint32_t moloch_session_hash(const void *key)
{
    unsigned char *p = (unsigned char *)key;
    //return ((p[2] << 16 | p[3] << 8 | p[4]) * 59) ^ (p[8] << 16 | p[9] << 8 |  p[10]);
    return (((p[1]<<24) ^ (p[2]<<18) ^ (p[3]<<12) ^ (p[4]<<6) ^ p[5]) * 13) ^ (p[8]<<24|p[9]<<16 | p[10]<<8 | p[11]);
}

/******************************************************************************/
int moloch_session_cmp(const void *keyv, const void *elementv)
{
    MolochSession_t *session = (MolochSession_t *)elementv;

    return (*(uint64_t *)keyv     == session->sessionIda && 
            *(uint32_t *)(keyv+8) == session->sessionIdb);
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
/*
 * Don't actually end main loop until all tags are loaded
 */
gboolean moloch_quit_gfunc (gpointer UNUSED(user_data))
{
    if (needNidsExit) {
        needNidsExit = FALSE;
        moloch_nids_exit();
        return TRUE;
    }
    if (moloch_db_tags_loading() == 0 && moloch_plugins_outstanding() == 0 && moloch_writer_queue_length() == 0 && moloch_http_queue_length(esServer) == 0) {
        g_main_loop_quit(mainLoop);
        return FALSE;
    }
    return TRUE;
}
/******************************************************************************/
void moloch_quit()
{
    g_timeout_add(100, moloch_quit_gfunc, 0);
}
/******************************************************************************/
/*
 * Don't actually init nids/pcap until all the pre tags are loaded
 */
gboolean moloch_nids_init_gfunc (gpointer UNUSED(user_data))
{
    if (moloch_db_tags_loading() == 0 && moloch_http_queue_length(esServer) == 0) {
        if (config.debug)
            LOG("maxField = %d", config.maxField);
        moloch_nids_init();
        return FALSE;
    }
    return TRUE;
}
/******************************************************************************/
void moloch_hex_init()
{
    int i, j;
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 16; j++) {
            moloch_hex_to_char[(unsigned char)moloch_char_to_hex[i]][(unsigned char)moloch_char_to_hex[j]] = i << 4 | j;
            moloch_hex_to_char[toupper(moloch_char_to_hex[i])][(unsigned char)moloch_char_to_hex[j]] = i << 4 | j;
            moloch_hex_to_char[(unsigned char)moloch_char_to_hex[i]][toupper(moloch_char_to_hex[j])] = i << 4 | j;
            moloch_hex_to_char[toupper(moloch_char_to_hex[i])][toupper(moloch_char_to_hex[j])] = i << 4 | j;
        }
    }

    for (i = 0; i < 256; i++) {
        moloch_char_to_hexstr[i][0] = moloch_char_to_hex[(i >> 4) & 0xf];
        moloch_char_to_hexstr[i][1] = moloch_char_to_hex[i & 0xf];
    }
}

/*
void moloch_sched_init()
{
#ifdef _POSIX_PRIORITY_SCHEDULING
    struct sched_param sp;
    sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
    int result = sched_setscheduler(0, SCHED_FIFO, &sp);
    if (result != 0) {
        LOG("WARNING: Failed to change to FIFO scheduler - %s", strerror(errno));
    } else if (config.debug) {
        LOG("Changed to FIFO scheduler with priority %d", sp.sched_priority);
    }
#endif
}
*/
/******************************************************************************/
void moloch_mlockall_init()
{
#ifdef _POSIX_MEMLOCK
    struct rlimit l;
    getrlimit(RLIMIT_MEMLOCK, &l);
    if (l.rlim_max != RLIM_INFINITY && l.rlim_max < 4000000000LL) {
        LOG("WARNING: memlock in limits.conf must be unlimited or at least 4000000, currently %lu", (long)l.rlim_max/1024);
        return;
    }

    if (l.rlim_cur != l.rlim_max) {
        if (config.debug)
            LOG("Setting memlock soft to %lu", (long)l.rlim_max);
        l.rlim_cur = l.rlim_max;
        setrlimit(RLIMIT_MEMLOCK, &l);
    }

    int result = mlockall(MCL_FUTURE | MCL_CURRENT);
    if (result != 0) {
        LOG("WARNING: Failed to mlockall - %s", strerror(errno));
    } else if (config.debug) {
        LOG("mlockall with max of %lu", (long)l.rlim_max);
    }
#endif
}
/******************************************************************************/
int main(int argc, char **argv)
{
    signal(SIGHUP, reload);
    signal(SIGINT, cleanup);
    signal(SIGUSR1, exit);
    signal(SIGCHLD, SIG_IGN);

    mainLoop = g_main_loop_new(NULL, FALSE);

    parse_args(argc, argv);
    moloch_config_init();
    moloch_writers_init();
    moloch_hex_init();
    moloch_nids_root_init();
    if (!config.pcapReadOffline) {
        moloch_drop_privileges();
        config.copyPcap = 1;
        moloch_mlockall_init();
    }
    moloch_field_init();
    moloch_http_init();
    moloch_db_init();
    moloch_config_load_local_ips();
    moloch_yara_init();
    moloch_parsers_init();
    moloch_plugins_init();
    g_timeout_add(1, moloch_nids_init_gfunc, 0);

    g_main_loop_run(mainLoop);
    cleanup(0);
    exit(0);
}
