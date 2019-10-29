/* main.c  -- Initialization of components
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
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <sys/resource.h>
#ifdef _POSIX_MEMLOCK
#include <sys/mman.h>
#endif
#ifdef _POSIX_PRIORITY_SCHEDULING
#include <sched.h>
#endif
#include "pcap.h"
#include "molochconfig.h"

#ifndef BUILD_VERSION
#define BUILD_VERSION "unkn"
#endif

/******************************************************************************/
MolochConfig_t         config;
extern void           *esServer;
GMainLoop             *mainLoop;
char                  *moloch_char_to_hex = "0123456789abcdef"; /* don't change case */
unsigned char          moloch_char_to_hexstr[256][3];
unsigned char          moloch_hex_to_char[256][256];

extern MolochWriterQueueLength moloch_writer_queue_length;
extern MolochPcapFileHdr_t     pcapFileHeader;

MOLOCH_LOCK_DEFINE(LOG);

/******************************************************************************/
LOCAL  gboolean showVersion    = FALSE;

#define FREE_LATER_SIZE 32768
LOCAL int freeLaterFront;
LOCAL int freeLaterBack;
typedef struct {
    void              *ptr;
    GDestroyNotify     cb;
    uint32_t           sec;
} MolochFreeLater_t;
MolochFreeLater_t  freeLaterList[FREE_LATER_SIZE];
MOLOCH_LOCK_DEFINE(freeLaterList);

/******************************************************************************/
gboolean moloch_debug_flag()
{
    config.debug++;
    config.quiet = 0;
    return TRUE;
}

/******************************************************************************/
gboolean moloch_cmdline_option(const gchar *option_name, const gchar *input, gpointer UNUSED(data), GError **UNUSED(error))
{
    char *equal = strchr(input, '=');
    if (!equal)
        LOGEXIT("%s requires a '=' in value %s", option_name, input);

    char *key = g_strndup(input, equal - input);
    char *value = g_strdup(equal + 1);

    if (!config.override) {
        config.override = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    }
    g_hash_table_insert(config.override, key, value);

    return TRUE;
}
/******************************************************************************/

LOCAL  GOptionEntry entries[] =
{
    { "config",    'c',                    0, G_OPTION_ARG_FILENAME,       &config.configFile,    "Config file name, default '/data/moloch/etc/config.ini'", NULL },
    { "pcapfile",  'r',                    0, G_OPTION_ARG_FILENAME_ARRAY, &config.pcapReadFiles, "Offline pcap file", NULL },
    { "pcapdir",   'R',                    0, G_OPTION_ARG_FILENAME_ARRAY, &config.pcapReadDirs,  "Offline pcap directory, all *.pcap files will be processed", NULL },
    { "monitor",   'm',                    0, G_OPTION_ARG_NONE,           &config.pcapMonitor,   "Used with -R option monitors the directory for closed files", NULL },
    { "packetcnt",   0,                    0, G_OPTION_ARG_INT,            &config.pktsToRead,    "Number of packets to read from each offline file", NULL},
    { "delete",      0,                    0, G_OPTION_ARG_NONE,           &config.pcapDelete,    "In offline mode delete files once processed, requires --copy", NULL },
    { "skip",      's',                    0, G_OPTION_ARG_NONE,           &config.pcapSkip,      "Used with -R option and without --copy, skip files already processed", NULL },
    { "reprocess",   0,                    0, G_OPTION_ARG_NONE,           &config.pcapReprocess, "In offline mode reprocess files, use the same files table entry", NULL },
    { "recursive",   0,                    0, G_OPTION_ARG_NONE,           &config.pcapRecursive, "When in offline pcap directory mode, recurse sub directories", NULL },
    { "node",      'n',                    0, G_OPTION_ARG_STRING,         &config.nodeName,      "Our node name, defaults to hostname.  Multiple nodes can run on same host", NULL },
    { "host",        0,                    0, G_OPTION_ARG_STRING,         &config.hostName,      "Override hostname, this is what remote viewers will use to connect", NULL },
    { "tag",       't',                    0, G_OPTION_ARG_STRING_ARRAY,   &config.extraTags,     "Extra tag to add to all packets, can be used multiple times", NULL },
    { "filelist",  'F',                    0, G_OPTION_ARG_STRING_ARRAY,   &config.pcapFileLists, "File that has a list of pcap file names, 1 per line", NULL },
    { "op",          0,                    0, G_OPTION_ARG_STRING_ARRAY,   &config.extraOps,      "FieldExpr=Value to set on all session, can be used multiple times", NULL},
    { "option",    'o',                    0, G_OPTION_ARG_CALLBACK,       moloch_cmdline_option, "Key=Value to override config.ini", NULL},
    { "version",   'v',                    0, G_OPTION_ARG_NONE,           &showVersion,          "Show version number", NULL },
    { "debug",     'd', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,       moloch_debug_flag,     "Turn on all debugging", NULL },
    { "quiet",     'q',                    0, G_OPTION_ARG_NONE,           &config.quiet,         "Turn off regular logging", NULL },
    { "copy",        0,                    0, G_OPTION_ARG_NONE,           &config.copyPcap,      "When in offline mode copy the pcap files into the pcapDir from the config file", NULL },
    { "dryrun",      0,                    0, G_OPTION_ARG_NONE,           &config.dryRun,        "dry run, nothing written to databases or filesystem", NULL },
    { "flush",       0,                    0, G_OPTION_ARG_NONE,           &config.flushBetween,  "In offline mode flush streams between files", NULL },
    { "nospi",       0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,           &config.noSPI,         "no SPI data written to ES", NULL },
    { "tests",       0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,           &config.tests,         "Output test suite information", NULL },
    { "nostats",     0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,           &config.noStats,       "Don't send node stats", NULL },
    { "insecure",    0,                    0, G_OPTION_ARG_NONE,           &config.insecure,      "insecure https calls", NULL },
    { "nolockpcap",  0,                    0, G_OPTION_ARG_NONE,           &config.noLockPcap,    "Don't lock offline pcap files (ie., allow deletion)", NULL },
    { "ignoreerrors",0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,           &config.ignoreErrors,  "Ignore most errors and continue", NULL },
    { NULL,          0, 0,                                    0,           NULL, NULL, NULL }
};


/******************************************************************************/
void free_args()
{
    g_free(config.nodeName);
    g_free(config.hostName);
    g_free(config.configFile);
    if (config.pcapReadFiles)
        g_strfreev(config.pcapReadFiles);
    if (config.pcapFileLists)
        g_strfreev(config.pcapFileLists);
    if (config.pcapReadDirs)
        g_strfreev(config.pcapReadDirs);
    if (config.extraTags)
        g_strfreev(config.extraTags);
    if (config.extraOps)
        g_strfreev(config.extraOps);
}
/******************************************************************************/
void parse_args(int argc, char **argv)
{
    GError *error = NULL;
    GOptionContext *context;

    extern char *curl_version(void);
    extern char *pcre_version(void);
    extern const char *MMDB_lib_version(void);

    context = g_option_context_new ("- capture");
    g_option_context_add_main_entries (context, entries, NULL);
    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
        g_print ("option parsing failed: %s\n", error->message);
        exit (1);
    }

    g_option_context_free(context);

    config.pcapReadOffline = (config.pcapReadFiles || config.pcapReadDirs || config.pcapFileLists);

    if (!config.configFile)
        config.configFile = g_strdup("/data/moloch/etc/config.ini");

    if (showVersion || config.debug) {
        printf("moloch-capture %s/%s session size=%d packet size=%d api=%d\n", PACKAGE_VERSION, BUILD_VERSION, (int)sizeof(MolochSession_t), (int)sizeof(MolochPacket_t), MOLOCH_API_VERSION);
    }

    if (showVersion) {
        printf("glib2: %u.%u.%u\n", glib_major_version, glib_minor_version, glib_micro_version);
        printf("libpcap: %s\n", pcap_lib_version());
        printf("curl: %s\n", curl_version());
        printf("pcre: %s\n", pcre_version());
        //printf("magic: %d\n", magic_version());
        printf("yara: %s\n", moloch_yara_version());
        printf("maxminddb: %s\n", MMDB_lib_version());

        exit(0);
    }

    if (glib_major_version !=  GLIB_MAJOR_VERSION ||
        glib_minor_version !=  GLIB_MINOR_VERSION ||
        glib_micro_version !=  GLIB_MICRO_VERSION) {

        LOG("WARNING - glib compiled %d.%d.%d vs linked %u.%u.%u",
                GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION,
                glib_major_version, glib_minor_version, glib_micro_version);
    }


    if (!config.hostName) {
        config.hostName = malloc(256);
        gethostname(config.hostName, 256);
        char *dot = strchr(config.hostName, '.');
        if (!dot) {
            char domainname[256];
            if (getdomainname(domainname, 255) == 0 && strlen(domainname) > 0 && strcmp(domainname, "(none)") != 0) {
                g_strlcat(config.hostName, ".", 255);
                g_strlcat(config.hostName, domainname, 255);
            } else {
                LOG("WARNING: gethostname doesn't return a fully qualified name and getdomainname failed, this may cause issues when viewing pcaps, use the --host option - %s", config.hostName);
            }
        }
        config.hostName[255] = 0;
    }

    if (!config.nodeName) {
        config.nodeName = g_strdup(config.hostName);
        char *dot = strchr(config.nodeName, '.');
        if (dot) {
            *dot = 0;
        }
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
        printf("Must specify directories to monitor with -R\n");
        exit(1);
    }
}
/******************************************************************************/
void moloch_free_later(void *ptr, GDestroyNotify cb)
{
    struct timespec currentTime;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &currentTime);

    MOLOCH_LOCK(freeLaterList);
    if ((freeLaterBack + 1 ) % FREE_LATER_SIZE == freeLaterFront) {
        freeLaterList[freeLaterFront].cb(freeLaterList[freeLaterFront].ptr);
        freeLaterFront = (freeLaterFront + 1 ) % FREE_LATER_SIZE;
    }

    freeLaterList[freeLaterBack].sec = currentTime.tv_sec + 7;
    freeLaterList[freeLaterBack].ptr = ptr;
    freeLaterList[freeLaterBack].cb  = cb;
    freeLaterBack = (freeLaterBack + 1 ) % FREE_LATER_SIZE;
    MOLOCH_UNLOCK(freeLaterList);
}
/******************************************************************************/
LOCAL gboolean moloch_free_later_check (gpointer UNUSED(user_data))
{
    if (freeLaterFront == freeLaterBack)
        return TRUE;

    struct timespec currentTime;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &currentTime);
    MOLOCH_LOCK(freeLaterList);
    while (freeLaterFront != freeLaterBack &&
           freeLaterList[freeLaterFront].sec < currentTime.tv_sec) {
        freeLaterList[freeLaterFront].cb(freeLaterList[freeLaterFront].ptr);
        freeLaterFront = (freeLaterFront + 1 ) % FREE_LATER_SIZE;
    }
    MOLOCH_UNLOCK(freeLaterList);
    return TRUE;
}
/******************************************************************************/
LOCAL void moloch_free_later_init()
{
    g_timeout_add_seconds(1, moloch_free_later_check, 0);
}

/******************************************************************************/
void *moloch_size_alloc(int size, int zero)
{
    size += 8;
    void *mem = (zero?g_slice_alloc0(size):g_slice_alloc(size));
    memcpy(mem, &size, 4);
    return (char *)mem + 8;
}
/******************************************************************************/
int moloch_size_free(void *mem)
{
    int size;
    mem = (char *)mem - 8;

    memcpy(&size, mem, 4);
    g_slice_free1(size, mem);
    return size - 8;
}
/******************************************************************************/
void controlc(int UNUSED(sig))
{
    LOG("Control-C");
    signal(SIGINT, exit); // Double Control-C quits right away
    moloch_quit();
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
    while (haysize >= needlesize && (p = memchr(haystack, *needle, haysize - needlesize + 1))) {
        if (memcmp(p, needle, needlesize) == 0)
            return p;
        haysize -= (p - haystack + 1);
        haystack = p+1;
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
SUPPRESS_UNSIGNED_INTEGER_OVERFLOW
uint32_t moloch_string_hash(const void *key)
{
    unsigned char *p = (unsigned char *)key;
    uint32_t n = 0;
    while (*p) {
        n = (n << 5) - n + *p;
        p++;
    }
    return n;
}
/******************************************************************************/
SUPPRESS_UNSIGNED_INTEGER_OVERFLOW
uint32_t moloch_string_hash_len(const void *key, int len)
{
    unsigned char *p = (unsigned char *)key;
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
typedef struct {
    MolochWatchFd_func  func;
    gpointer            data;
} MolochWatchFd_t;

/******************************************************************************/
LOCAL void moloch_gio_destroy(gpointer data)
{
    g_free(data);
}
/******************************************************************************/
LOCAL gboolean moloch_gio_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
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
            LOGEXIT("ERROR: Group '%s' not found", config.dropGroup);
        }

        if (setgid(grp->gr_gid) != 0) {
            LOGEXIT("ERROR: Couldn't change group - %s", strerror(errno));
        }
    }

    if (config.dropUser) {
        struct passwd   *usr;
        usr = getpwnam(config.dropUser);
        if (!usr) {
            LOGEXIT("ERROR: User '%s' not found", config.dropUser);
        }

        if (setuid(usr->pw_uid) != 0) {
            LOGEXIT("ERROR: Couldn't change user - %s", strerror(errno));
        }
    }


}
/******************************************************************************/
LOCAL  MolochCanQuitFunc  canQuitFuncs[20];
LOCAL  const char        *canQuitNames[20];
LOCAL  int                canQuitFuncsNum;

void moloch_add_can_quit (MolochCanQuitFunc func, const char *name)
{
    if (canQuitFuncsNum >= 20) {
        LOGEXIT("Can't add canQuitFunc");
        return;
    }
    canQuitFuncs[canQuitFuncsNum] = func;
    canQuitNames[canQuitFuncsNum] = name;
    canQuitFuncsNum++;
}
/******************************************************************************/
/*
 * Don't actually end main loop until all the various pieces are done
 */
gboolean moloch_quit_gfunc (gpointer UNUSED(user_data))
{
LOCAL gboolean readerExit   = TRUE;
LOCAL gboolean writerExit   = TRUE;

// On the first run shutdown reader and sessions
    if (readerExit) {
        readerExit = FALSE;
        if (moloch_reader_stop)
            moloch_reader_stop();
        moloch_readers_exit();
        moloch_packet_exit();
        moloch_session_exit();
        if (config.debug)
            LOG("Read exit finished");
        return G_SOURCE_CONTINUE;
    }

// Wait for all the can quits to signal all clear
    int i;
    for (i = 0; i < canQuitFuncsNum; i++) {
        int val = canQuitFuncs[i]();
        if (val != 0) {
            if (config.debug && canQuitNames[i]) {
                LOG ("Can't quit, %s is %d", canQuitNames[i], val);
            }
            return G_SOURCE_CONTINUE;
        }
    }

// Once all clear stop the writer and wait for all clears again
    if (writerExit) {
        writerExit = FALSE;
        if (!config.dryRun && config.copyPcap) {
            moloch_writer_exit();
            if (config.debug)
                LOG("Write exit finished");
            return G_SOURCE_CONTINUE;
        }
    }

// Can quit the main loop now
    g_main_loop_quit(mainLoop);
    return G_SOURCE_REMOVE;
}
/******************************************************************************/
void moloch_quit()
{
    if (config.debug)
        LOG("Quitting");
    config.quitting = TRUE;
    g_timeout_add(100, moloch_quit_gfunc, 0);
}
/******************************************************************************/
/*
 * Don't actually init nids/pcap until all the pre tags are loaded.
 * TRUE - call again in 1ms
 * FALSE - don't call again
 */
gboolean moloch_ready_gfunc (gpointer UNUSED(user_data))
{
    if (moloch_http_queue_length(esServer))
        return TRUE;

    if (config.debug)
        LOG("maxField = %d", config.maxField);

    if (config.pcapReadOffline) {
        if (config.dryRun || !config.copyPcap) {
            moloch_writers_start("inplace");
        } else {
            moloch_writers_start(NULL);
        }

    } else {
        if (config.dryRun) {
            moloch_writers_start("null");
        } else {
            moloch_writers_start(NULL);
        }
    }
    moloch_reader_start();
    if (!config.pcapReadOffline && (pcapFileHeader.linktype == 0 || pcapFileHeader.snaplen == 0))
        LOGEXIT("Reader didn't call moloch_packet_set_linksnap");
    return FALSE;
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
        LOG("WARNING: memlock in limits.conf must be unlimited or at least 4000000, currently %lu", (unsigned long)l.rlim_max/1024);
        return;
    }

    if (l.rlim_cur != l.rlim_max) {
        if (config.debug)
            LOG("Setting memlock soft to %lu", (unsigned long)l.rlim_max);
        l.rlim_cur = l.rlim_max;
        setrlimit(RLIMIT_MEMLOCK, &l);
    }

    int result = mlockall(MCL_FUTURE | MCL_CURRENT);
    if (result != 0) {
        LOG("WARNING: Failed to mlockall - %s", strerror(errno));
    } else if (config.debug) {
        LOG("mlockall with max of %lu", (unsigned long)l.rlim_max);
    }
#endif
}
/******************************************************************************/
#ifdef FUZZLOCH

/* This replaces main for libFuzzer.  Basically initialized everything like main
 * would for starting up and set some important settings.  Must be run from tests
 * directory, and config.test.ini will be loaded for fuzz node.
 */

MolochPacketBatch_t   batch;

int
LLVMFuzzerInitialize(int *UNUSED(argc), char ***UNUSED(argv))
{
    config.configFile = g_strdup("config.test.ini");
    config.dryRun = 1;
    config.pcapReadOffline = 1;
    config.hostName = strdup("fuzz.example.com");
    config.nodeName = strdup("fuzz");
    moloch_free_later_init();
    moloch_hex_init();
    moloch_config_init();
    moloch_writers_init();
    moloch_writers_start("null");
    moloch_readers_init();
    moloch_readers_set("null");
    moloch_plugins_init();
    moloch_field_init();
    moloch_http_init();
    moloch_db_init();
    moloch_packet_init();
    moloch_config_load_local_ips();
    moloch_config_load_packet_ips();
    moloch_yara_init();
    moloch_parsers_init();
    moloch_session_init();
    moloch_plugins_load(config.plugins);
    moloch_rules_init();
    moloch_packet_batch_init(&batch);
    return 0;
}

/******************************************************************************/
/* In libFuzzer mode this is called for each packet.
 * There are no packet threads in fuzz mode, and the batch call will actually
 * process the packet.  The current time just increases for each packet.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    MolochPacket_t       *packet = MOLOCH_TYPE_ALLOC0(MolochPacket_t);
    static uint64_t       ts = 10000;

    packet->pktlen        = size;
    packet->pkt           = (u_char *)data;
    packet->ts.tv_sec     = ts >> 4;
    packet->ts.tv_usec    = ts & 0x8;
    ts++;
    packet->readerFilePos = 0;
    packet->readerPos     = 0;

    // In FUZZ mode batch will actually process it
    moloch_packet_batch(&batch, packet);

    return 0;
}

#else
int main(int argc, char **argv)
{
    signal(SIGHUP, reload);
    signal(SIGINT, controlc);
    signal(SIGUSR1, exit);
    signal(SIGCHLD, SIG_IGN);

    mainLoop = g_main_loop_new(NULL, FALSE);

    parse_args(argc, argv);
    if (config.debug)
        LOG("THREAD %p", (gpointer)pthread_self());

    if (config.insecure)
        LOG("\n\nDON'T DO IT!!!! `--insecure` is a bad idea\n\n");

    moloch_free_later_init();
    moloch_hex_init();
    moloch_config_init();
    moloch_writers_init();
    moloch_readers_init();
    moloch_plugins_init();
    moloch_plugins_load(config.rootPlugins);
    if (config.pcapReadOffline)
        moloch_readers_set("libpcap-file");
    else
        moloch_readers_set(NULL);
    if (!config.pcapReadOffline) {
        moloch_drop_privileges();
        config.copyPcap = 1;
        moloch_mlockall_init();
    }
    moloch_field_init();
    moloch_http_init();
    moloch_db_init();
    moloch_packet_init();
    moloch_config_load_local_ips();
    moloch_config_load_packet_ips();
    moloch_yara_init();
    moloch_parsers_init();
    moloch_session_init();
    moloch_plugins_load(config.plugins);
    moloch_rules_init();
    g_timeout_add(1, moloch_ready_gfunc, 0);

    g_main_loop_run(mainLoop);

    if (!config.pcapReadOffline || config.debug)
        LOG("Final cleanup");
    moloch_plugins_exit();
    moloch_parsers_exit();
    moloch_db_exit();
    moloch_http_exit();
    moloch_field_exit();
    moloch_config_exit();
    moloch_rules_exit();
    moloch_yara_exit();

    g_main_loop_unref(mainLoop);

    free_args();
    exit(0);
}
#endif
