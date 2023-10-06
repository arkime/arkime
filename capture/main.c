/* main.c  -- Initialization of components
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"
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
#include "arkimeconfig.h"

#ifndef BUILD_VERSION
#define BUILD_VERSION "unkn"
#endif
#include "nghttp2/nghttp2.h"

/******************************************************************************/
ArkimeConfig_t         config;
extern void           *esServer;
GMainLoop             *mainLoop;
char                  *arkime_char_to_hex = "0123456789abcdef"; /* don't change case */
uint8_t                arkime_char_to_hexstr[256][3];
uint8_t                arkime_hex_to_char[256][256];
uint32_t               hashSalt;

extern ArkimeWriterQueueLength arkime_writer_queue_length;
extern ArkimePcapFileHdr_t     pcapFileHeader;

ARKIME_LOCK_DEFINE(LOG);

/******************************************************************************/
LOCAL  gboolean showVersion    = FALSE;

#define FREE_LATER_SIZE 32768
LOCAL int freeLaterFront;
LOCAL int freeLaterBack;
typedef struct {
    void              *ptr;
    GDestroyNotify     cb;
    uint32_t           sec;
} ArkimeFreeLater_t;
ArkimeFreeLater_t  freeLaterList[FREE_LATER_SIZE];
ARKIME_LOCK_DEFINE(freeLaterList);

/******************************************************************************/
gboolean arkime_debug_flag()
{
    config.debug++;
    config.quiet = 0;
    return TRUE;
}

/******************************************************************************/
gboolean arkime_cmdline_option(const gchar *option_name, const gchar *input, gpointer UNUSED(data), GError **UNUSED(error))
{
    char *equal = strchr(input, '=');
    if (!equal)
        CONFIGEXIT("The option %s requires a '=' in value '%s'", option_name, input);

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
    { "config",    'c',                    0, G_OPTION_ARG_FILENAME,       &config.configFile,    "Config file name, default '" CONFIG_PREFIX "/etc/config.ini'", NULL },
    { "pcapfile",  'r',                    0, G_OPTION_ARG_FILENAME_ARRAY, &config.pcapReadFiles, "Offline pcap file", NULL },
    { "pcapdir",   'R',                    0, G_OPTION_ARG_FILENAME_ARRAY, &config.pcapReadDirs,  "Offline pcap directory, all *.pcap files will be processed", NULL },
    { "monitor",   'm',                    0, G_OPTION_ARG_NONE,           &config.pcapMonitor,   "Used with -R option monitors the directory for closed files", NULL },
    { "packetcnt",   0,                    0, G_OPTION_ARG_INT,            &config.pktsToRead,    "Number of packets to read from each offline file", NULL },
    { "delete",      0,                    0, G_OPTION_ARG_NONE,           &config.pcapDelete,    "In offline mode delete files once processed, requires --copy", NULL },
    { "skip",      's',                    0, G_OPTION_ARG_NONE,           &config.pcapSkip,      "Used with -R option and without --copy, skip files already processed", NULL },
    { "reprocess",   0,                    0, G_OPTION_ARG_NONE,           &config.pcapReprocess, "In offline mode reprocess files, use the same files table entry", NULL },
    { "recursive",   0,                    0, G_OPTION_ARG_NONE,           &config.pcapRecursive, "When in offline pcap directory mode, recurse sub directories", NULL },
    { "node",      'n',                    0, G_OPTION_ARG_STRING,         &config.nodeName,      "Our node name, defaults to hostname.  Multiple nodes can run on same host", NULL },
    { "host",        0,                    0, G_OPTION_ARG_STRING,         &config.hostName,      "Override hostname, this is what remote viewers will use to connect", NULL },
    { "tag",       't',                    0, G_OPTION_ARG_STRING_ARRAY,   &config.extraTags,     "Extra tag to add to all packets, can be used multiple times", NULL },
    { "tags",        0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING_ARRAY,   &config.extraTags,     "Extra tag to add to all packets, can be used multiple times", NULL },
    { "filelist",  'F',                    0, G_OPTION_ARG_STRING_ARRAY,   &config.pcapFileLists, "File that has a list of pcap file names, 1 per line", NULL },
    { "op",          0,                    0, G_OPTION_ARG_STRING_ARRAY,   &config.extraOps,      "FieldExpr=Value to set on all session, can be used multiple times", NULL},
    { "option",    'o',                    0, G_OPTION_ARG_CALLBACK,       arkime_cmdline_option, "Key=Value to override config.ini", NULL },
    { "version",   'v',                    0, G_OPTION_ARG_NONE,           &showVersion,          "Show version number", NULL },
    { "debug",     'd', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,       arkime_debug_flag,     "Turn on all debugging", NULL },
    { "quiet",     'q',                    0, G_OPTION_ARG_NONE,           &config.quiet,         "Turn off regular logging", NULL },
    { "copy",        0,                    0, G_OPTION_ARG_NONE,           &config.copyPcap,      "When in offline mode copy the pcap files into the pcapDir from the config file", NULL },
    { "dryrun",      0,                    0, G_OPTION_ARG_NONE,           &config.dryRun,        "dry run, nothing written to databases or filesystem", NULL },
    { "flush",       0,                    0, G_OPTION_ARG_NONE,           &config.flushBetween,  "In offline mode flush streams between files", NULL },
    { "nospi",       0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,           &config.noSPI,         "no SPI data written to ES", NULL },
    { "tests",       0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,           &config.tests,         "Output test suite information", NULL },
    { "nostats",     0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,           &config.noStats,       "Don't send node stats", NULL },
    { "insecure",    0,                    0, G_OPTION_ARG_NONE,           &config.insecure,      "Disable certificate verification for https calls", NULL },
    { "nolockpcap",  0,                    0, G_OPTION_ARG_NONE,           &config.noLockPcap,    "Don't lock offline pcap files (ie., allow deletion)", NULL },
    { "ignoreerrors",0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,           &config.ignoreErrors,  "Ignore most errors and continue", NULL },
    { "dumpConfig",  0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,           &config.dumpConfig,    "Display the config.", NULL },
    { "regressionTests",  0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,      &config.regressionTests, "Regression Tests", NULL },
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
    extern const char *zlibVersion(void);
    extern const char *yaml_get_version_string(void);
    //extern int magic_version(void);

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
        config.configFile = g_strdup(CONFIG_PREFIX "/etc/config.ini");

    if (showVersion || config.debug) {
        printf("arkime-capture %s/%s session size=%d packet size=%d api=%d\n", PACKAGE_VERSION, BUILD_VERSION, (int)sizeof(ArkimeSession_t), (int)sizeof(ArkimePacket_t), ARKIME_API_VERSION);
    }

    if (showVersion) {
        printf("curl: %s\n", curl_version());
        printf("glib2: %u.%u.%u\n", glib_major_version, glib_minor_version, glib_micro_version);
        printf("libpcap: %s\n", pcap_lib_version());
        //printf("magic: %d\n", magic_version());
        printf("maxminddb: %s\n", MMDB_lib_version());
        //printf("openssl: %s\n", OpenSSL_version(0));
        printf("pcre: %s\n", pcre_version());
        printf("yaml: %s\n", yaml_get_version_string());
        printf("yara: %s\n", arkime_yara_version());
        printf("zlib: %s\n", zlibVersion());
#ifdef HAVE_ZSTD
        extern unsigned ZSTD_versionNumber(void);
        unsigned zver = ZSTD_versionNumber();
        printf("zstd: %u.%u.%u\n", zver/(100 * 100), (zver / 100) % 100, zver % 100);
#endif
        nghttp2_info *ngver = nghttp2_version(0);
        printf("nghttp2: %s\n", ngver->version_str);

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

    if (config.pcapSkip && config.copyPcap) {
        printf("Can't skip and copy pcap files\n");
        exit(1);
    }

    if (config.pcapDelete && !config.copyPcap) {
        printf("--delete requires --copy\n");
        exit(1);
    }

    if (config.copyPcap && !config.pcapReadOffline) {
        printf("--copy requires -r or -R\n");
        exit(1);
    }

    if (config.pcapMonitor && !config.pcapReadDirs) {
        printf("Must specify directories to monitor with -R\n");
        exit(1);
    }

    if (config.pcapReadFiles) {
        for (int i = 0; config.pcapReadFiles[i]; i++) {
            if (strcmp("-", config.pcapReadFiles[i]) == 0 && !config.copyPcap) {
                printf("-r - requires --copy be used\n");
                exit(1);
            }
        }
    }
}
/******************************************************************************/
void arkime_free_later(void *ptr, GDestroyNotify cb)
{
    if (!ptr)
        return;

    struct timespec currentTime;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &currentTime);

    ARKIME_LOCK(freeLaterList);
    if ((freeLaterBack + 1) % FREE_LATER_SIZE == freeLaterFront) {
        freeLaterList[freeLaterFront].cb(freeLaterList[freeLaterFront].ptr);
        freeLaterFront = (freeLaterFront + 1) % FREE_LATER_SIZE;
    }

    freeLaterList[freeLaterBack].sec = currentTime.tv_sec + 7;
    freeLaterList[freeLaterBack].ptr = ptr;
    freeLaterList[freeLaterBack].cb  = cb;
    freeLaterBack = (freeLaterBack + 1) % FREE_LATER_SIZE;
    ARKIME_UNLOCK(freeLaterList);
}
/******************************************************************************/
LOCAL gboolean arkime_free_later_check (gpointer UNUSED(user_data))
{
    if (freeLaterFront == freeLaterBack)
        return TRUE;

    struct timespec currentTime;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &currentTime);
    ARKIME_LOCK(freeLaterList);
    while (freeLaterFront != freeLaterBack &&
           freeLaterList[freeLaterFront].sec < currentTime.tv_sec) {
        freeLaterList[freeLaterFront].cb(freeLaterList[freeLaterFront].ptr);
        freeLaterFront = (freeLaterFront + 1) % FREE_LATER_SIZE;
    }
    ARKIME_UNLOCK(freeLaterList);
    return G_SOURCE_CONTINUE;
}
/******************************************************************************/
LOCAL void arkime_free_later_init()
{
    g_timeout_add_seconds(1, arkime_free_later_check, 0);
}

/******************************************************************************/
void *arkime_size_alloc(int size, int zero)
{
    size += 8;
    void *mem = (zero?g_slice_alloc0(size):g_slice_alloc(size));
    memcpy(mem, &size, 4);
    return (char *)mem + 8;
}
/******************************************************************************/
int arkime_size_free(void *mem)
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
    arkime_quit();
}
/******************************************************************************/
void terminate(int UNUSED(sig))
{
    LOG("Terminate");
    arkime_quit();
}
/******************************************************************************/
void reload(int UNUSED(sig))
{
    arkime_plugins_reload();
}
/******************************************************************************/
uint32_t arkime_get_next_prime(uint32_t v)
{
    static uint32_t primes[] = {1009, 10007, 49999, 99991, 199799, 400009, 500009, 732209,
                                1092757, 1299827, 1500007, 1987411, 2999999, 4000037,
                                5000011, 6000011, 7000003, 8000009, 9000011, 10000019,
                                11000027, 12000017, 13000027, 14000029, 15000017, 16000057,
                                17000023, 18000041, 19000013, 20000003, 21000037, 22000001,
                                0};

    int p;
    for (p = 0; primes[p]; p++) {
        if (primes[p] > v)
            return primes[p];
    }
    return primes[p-1];
}
/******************************************************************************/
//https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
uint32_t arkime_get_next_powerof2(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}
/******************************************************************************/
uint8_t *arkime_js0n_get(uint8_t *data, uint32_t len, char *key, uint32_t *olen)
{
    uint32_t key_len = strlen(key);
    int      i;
    uint32_t out[4 * 100]; // Can have up to 100 elements at any level

    *olen = 0;
    int rc;
    if ((rc = js0n(data, len, out, sizeof(out))) != 0) {
        LOG("ERROR - Parse error %d for >%s< in >%.*s<\n", rc, key, len, data);
        fflush(stdout);
        return 0;
    }

    for (i = 0; out[i]; i+= 4) {
        if (out[i + 1] == key_len && memcmp(key, data + out[i], key_len) == 0) {
            *olen = out[i + 3];
            return data + out[i + 2];
        }
    }
    return 0;
}
/******************************************************************************/
char *arkime_js0n_get_str(uint8_t *data, uint32_t len, char *key)
{
    uint32_t           value_len;
    uint8_t           *value = 0;

    value = arkime_js0n_get(data, len, key, &value_len);
    if (!value)
        return NULL;
    return g_strndup((gchar*)value, value_len);
}
/******************************************************************************/
const char *arkime_memstr(const char *haystack, int haysize, const char *needle, int needlesize)
{
    const char *p;
    while (haysize >= needlesize && (p = memchr(haystack, *needle, haysize - needlesize + 1))) {
        if (memcmp(p, needle, needlesize) == 0)
            return p;
        haysize -= (p - haystack + 1);
        haystack = p + 1;
    }
    return NULL;
}
/******************************************************************************/
const char *arkime_memcasestr(const char *haystack, int haysize, const char *needle, int needlesize)
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
gboolean arkime_string_add(void *hashv, char *string, gpointer uw, gboolean copy)
{
    ArkimeStringHash_t *hash = hashv;
    ArkimeString_t *hstring;

    HASH_FIND(s_, *hash, string, hstring);
    if (hstring) {
        hstring->uw = uw;
        return FALSE;
    }

    hstring = ARKIME_TYPE_ALLOC0(ArkimeString_t);
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
uint32_t arkime_string_hash(const void *key)
{
    uint8_t *p = (uint8_t *)key;
    uint32_t n = 0;
    while (*p) {
        n = (n << 5) - n + *p;
        p++;
    }

    n ^= hashSalt;

    return n;
}
/******************************************************************************/
SUPPRESS_UNSIGNED_INTEGER_OVERFLOW
uint32_t arkime_string_hash_len(const void *key, int len)
{
    uint8_t *p = (uint8_t *)key;
    uint32_t n = 0;
    while (len) {
        n = (n << 5) - n + *p;
        p++;
        len--;
    }

    n ^= hashSalt;

    return n;
}

/******************************************************************************/
int arkime_string_cmp(const void *keyv, const void *elementv)
{
    char *key = (char*)keyv;
    ArkimeString_t *element = (ArkimeString_t *)elementv;

    return strcmp(key, element->str) == 0;
}
/******************************************************************************/
int arkime_string_ncmp(const void *keyv, const void *elementv)
{
    char *key = (char*)keyv;
    ArkimeString_t *element = (ArkimeString_t *)elementv;

    return strncmp(key, element->str, element->len) == 0;
}
/******************************************************************************/
uint32_t arkime_int_hash(const void *key)
{
    return (uint32_t)((long)key);
}

/******************************************************************************/
int arkime_int_cmp(const void *keyv, const void *elementv)
{
    uint32_t key = (uint32_t)((long)keyv);
    ArkimeInt_t *element = (ArkimeInt_t *)elementv;

    return key == element->i_hash;
}
/******************************************************************************/
typedef struct {
    ArkimeWatchFd_func  func;
    gpointer            data;
} ArkimeWatchFd_t;

/******************************************************************************/
LOCAL void arkime_gio_destroy(gpointer data)
{
    g_free(data);
}
/******************************************************************************/
LOCAL gboolean arkime_gio_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
{
    ArkimeWatchFd_t *watch = data;

    return watch->func(g_io_channel_unix_get_fd(source), condition, watch->data);
}

/******************************************************************************/
gint arkime_watch_fd(gint fd, GIOCondition cond, ArkimeWatchFd_func func, gpointer data)
{

    ArkimeWatchFd_t *watch = g_new0(ArkimeWatchFd_t, 1);
    watch->func = func;
    watch->data = data;

    GIOChannel *channel = g_io_channel_unix_new(fd);

    gint id =  g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond, arkime_gio_invoke, watch, arkime_gio_destroy);

    g_io_channel_unref(channel);
    return id;
}

/******************************************************************************/
void arkime_drop_privileges()
{
    if (getuid() != 0)
        return;

    if (config.dropGroup) {
        struct group   *grp;
        grp = getgrnam(config.dropGroup);
        if (!grp) {
            CONFIGEXIT("Group '%s' not found", config.dropGroup);
        }

        if (setgid(grp->gr_gid) != 0) {
            CONFIGEXIT("Couldn't change group - %s", strerror(errno));
        }
    }

    if (config.dropUser) {
        struct passwd   *usr;
        usr = getpwnam(config.dropUser);
        if (!usr) {
            CONFIGEXIT("User '%s' not found", config.dropUser);
        }

        if (setuid(usr->pw_uid) != 0) {
            CONFIGEXIT("Couldn't change user - %s", strerror(errno));
        }
    }


}
/******************************************************************************/
LOCAL  ArkimeCanQuitFunc  canQuitFuncs[20];
LOCAL  const char        *canQuitNames[20];
LOCAL  int                canQuitFuncsNum;

void arkime_add_can_quit (ArkimeCanQuitFunc func, const char *name)
{
    if (canQuitFuncsNum >= 20) {
        LOGEXIT("ERROR - Can't add canQuitFunc");
    }
    canQuitFuncs[canQuitFuncsNum] = func;
    canQuitNames[canQuitFuncsNum] = name;
    canQuitFuncsNum++;
}
/******************************************************************************/
/*
 * Don't actually end main loop until all the various pieces are done
 */
gboolean arkime_quit_gfunc (gpointer UNUSED(user_data))
{
LOCAL gboolean readerExit   = TRUE;
LOCAL gboolean writerExit   = TRUE;

// On the first run shutdown reader and sessions
    if (readerExit) {
        readerExit = FALSE;
        if (arkime_reader_stop)
            arkime_reader_stop();
        arkime_packet_exit();
        arkime_session_exit();
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
            arkime_writer_exit();
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
void arkime_quit()
{
    if (config.quitting)
        return;

    if (config.debug)
        LOG("Quitting");

    config.quitting = TRUE;
    g_timeout_add(100, arkime_quit_gfunc, 0);
}
/******************************************************************************/
/*
 * Don't actually init nids/pcap until all the pre tags are loaded.
 * TRUE - call again in 1ms
 * FALSE - don't call again
 */
gboolean arkime_ready_gfunc (gpointer UNUSED(user_data))
{
    if (arkime_http_queue_length(esServer))
        return G_SOURCE_CONTINUE;

    if (config.debug)
        LOG("maxField = %d", config.maxField);

    if (config.pcapReadOffline) {
        if (config.dryRun || !config.copyPcap) {
            arkime_writers_start("inplace");
        } else {
            arkime_writers_start(NULL);
        }

    } else {
        if (config.dryRun) {
            arkime_writers_start("null");
        } else {
            arkime_writers_start(NULL);
        }
    }
    arkime_readers_start();
    if (!config.pcapReadOffline && (pcapFileHeader.dlt == DLT_NULL || pcapFileHeader.snaplen == 0))
        LOGEXIT("ERROR - Reader didn't call arkime_packet_set_dltsnap");
    return G_SOURCE_REMOVE;
}
/******************************************************************************/
void arkime_hex_init()
{
    int i, j;
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 16; j++) {
            arkime_hex_to_char[(uint8_t)arkime_char_to_hex[i]][(uint8_t)arkime_char_to_hex[j]] = i << 4 | j;
            arkime_hex_to_char[toupper(arkime_char_to_hex[i])][(uint8_t)arkime_char_to_hex[j]] = i << 4 | j;
            arkime_hex_to_char[(uint8_t)arkime_char_to_hex[i]][toupper(arkime_char_to_hex[j])] = i << 4 | j;
            arkime_hex_to_char[toupper(arkime_char_to_hex[i])][toupper(arkime_char_to_hex[j])] = i << 4 | j;
        }
    }

    for (i = 0; i < 256; i++) {
        arkime_char_to_hexstr[i][0] = arkime_char_to_hex[(i >> 4) & 0xf];
        arkime_char_to_hexstr[i][1] = arkime_char_to_hex[i & 0xf];
    }
}

/*
void arkime_sched_init()
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
void arkime_mlockall_init()
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

ArkimePacketBatch_t   batch;
uint64_t              fuzzloch_sessionid = 0;

int
LLVMFuzzerInitialize(int *UNUSED(argc), char ***UNUSED(argv))
{
    config.configFile = g_strdup("config.test.ini");
    config.dryRun = 1;
    config.pcapReadOffline = 1;
    config.hostName = strdup("fuzz.example.com");
    config.nodeName = strdup("fuzz");

    hashSalt = 0;
    pcapFileHeader.dlt = DLT_EN10MB;

    arkime_free_later_init();
    arkime_hex_init();
    arkime_http_init();
    arkime_config_init();
    arkime_writers_init();
    arkime_writers_start("null");
    arkime_readers_init();
    arkime_readers_set("null");
    arkime_plugins_init();
    arkime_field_init();
    arkime_db_init();
    arkime_packet_init();
    arkime_config_load_packet_ips();
    arkime_yara_init();
    arkime_parsers_init();
    arkime_session_init();
    arkime_plugins_load(config.plugins);
    arkime_config_load_override_ips();
    arkime_rules_init();
    arkime_packet_batch_init(&batch);
    return 0;
}

/******************************************************************************/
/* In libFuzzer mode this is called for each packet.
 * There are no packet threads in fuzz mode, and the batch call will actually
 * process the packet.  The current time just increases for each packet.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static uint64_t       ts = 10000;
    BSB                   bsb;

    BSB_INIT(bsb, data, size);

    fuzzloch_sessionid++;

    while (BSB_REMAINING(bsb) > 3 && !BSB_IS_ERROR(bsb)) {
        uint16_t len = 0;
        BSB_IMPORT_u16(bsb, len);

        if (len == 0 || len > BSB_REMAINING(bsb))
            break;

        u_char *ptr = 0;
        BSB_IMPORT_ptr(bsb, ptr, len);

        if (!ptr || BSB_IS_ERROR(bsb))
            break;

        // LOG("Packet %llu %d", fuzzloch_sessionid, len);

        ArkimePacket_t *packet = ARKIME_TYPE_ALLOC0(ArkimePacket_t);
        packet->pktlen         = len;
        packet->pkt            = ptr;
        packet->ts.tv_sec      = ts >> 4;
        packet->ts.tv_usec     = ts & 0x8;
        ts++;
        packet->readerFilePos  = 0;
        packet->readerPos      = 0;

        // In FUZZ mode batch will actually process it
        arkime_packet_batch(&batch, packet);
    }

    return 0;
}

#else
int main(int argc, char **argv)
{
    signal(SIGHUP, reload);
    signal(SIGINT, controlc);
    signal(SIGTERM, terminate);
    signal(SIGUSR1, exit);
    signal(SIGCHLD, SIG_IGN);

    mainLoop = g_main_loop_new(NULL, FALSE);

    hashSalt = (uint32_t)time(NULL);

    parse_args(argc, argv);
    if (config.debug)
        LOG("THREAD %p", (gpointer)pthread_self());

    if (config.insecure)
        LOG("\n\nDON'T DO IT!!!! `--insecure` is a bad idea\n\n");

    arkime_free_later_init();
    arkime_hex_init();
    arkime_http_init();
    arkime_config_init();
    arkime_dedup_init();
    arkime_writers_init();
    arkime_readers_init();
    arkime_plugins_init();
    arkime_plugins_load(config.rootPlugins);
    if (config.pcapReadOffline)
        arkime_readers_set("libpcap-file");
    else
        arkime_readers_set(NULL);
    if (!config.pcapReadOffline) {
        arkime_drop_privileges();
        config.copyPcap = 1;
        arkime_mlockall_init();
    }
    arkime_field_init();
    arkime_db_init();
    arkime_packet_init();
    arkime_config_load_packet_ips();
    arkime_yara_init();
    arkime_parsers_init();
    arkime_session_init();
    arkime_plugins_load(config.plugins);
    arkime_config_load_override_ips();
    arkime_rules_init();
    g_timeout_add(1, arkime_ready_gfunc, 0);

    g_main_loop_run(mainLoop);

    if (!config.pcapReadOffline || config.debug)
        LOG("Final cleanup");
    arkime_plugins_exit();
    arkime_parsers_exit();
    arkime_db_exit();
    arkime_http_exit();
    arkime_field_exit();
    arkime_readers_exit();
    arkime_dedup_exit();
    arkime_config_exit();
    arkime_rules_exit();
    arkime_yara_exit();

    g_main_loop_unref(mainLoop);

    free_args();
    exit(0);
}
#endif
