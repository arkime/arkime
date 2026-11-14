/******************************************************************************/
/* readers.c  -- Functions dealing with pcap readers
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"
#include "pcap.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>

extern ArkimeConfig_t        config;

LOCAL  ArkimeStringHashStd_t readersHash;

void reader_libpcapfile_init(const char *);
void reader_libpcap_init(const char *);
void reader_tpacketv3_init(const char *);
void reader_bpf_init(const char *);
void reader_netmap_init(const char *);
void reader_xdp_init(const char *);
void reader_null_init(const char *);
void reader_pcapoverip_init(const char *);
void reader_tzsp_init(const char *);
void arkime_reader_scheme_init(const char *);

ArkimeReaderStart  arkime_reader_start;
ArkimeReaderStats  arkime_reader_stats;
ArkimeReaderStop   arkime_reader_stop;
ArkimeReaderExit   arkime_reader_exit;

char              *readerFileName[256];
ArkimeFieldOps_t   readerFieldOps[256];

ArkimeFileInfo_t  fileInfo[256];

ArkimeFilenameOps_t  readerFilenameOps[256];
int                  readerFilenameOpsNum;


/******************************************************************************/
void arkime_readers_set(char *name)
{
    ArkimeString_t *str;
    char *freeIt = NULL;
    if (!name)
        name = freeIt = arkime_config_str(NULL, "pcapReadMethod", "libpcap");


    HASH_FIND(s_, readersHash, name, str);
    if (!str) {
        CONFIGEXIT("Couldn't find pcapReadMethod '%s' implementation", name);
    }
    ArkimeReaderInit func = str->uw;
    func(name);
    g_free(freeIt);
}
/******************************************************************************/
void arkime_readers_add(char *name, ArkimeReaderInit func)
{
    arkime_string_add(&readersHash, name, func, TRUE);
}
/******************************************************************************/
void arkime_readers_init()
{
    HASH_INIT(s_, readersHash, arkime_string_hash, arkime_string_cmp);
    arkime_readers_add("libpcap-file", reader_libpcapfile_init);
    arkime_readers_add("libpcap", reader_libpcap_init);
    arkime_readers_add("tpacketv3", reader_tpacketv3_init);
    arkime_readers_add("afpacketv3", reader_tpacketv3_init);
    arkime_readers_add("null", reader_null_init);
    arkime_readers_add("pcapoveripclient", reader_pcapoverip_init);
    arkime_readers_add("pcap-over-ip-client", reader_pcapoverip_init);
    arkime_readers_add("pcapoveripserver", reader_pcapoverip_init);
    arkime_readers_add("pcap-over-ip-server", reader_pcapoverip_init);
    arkime_readers_add("tzsp", reader_tzsp_init);
    arkime_readers_add("scheme", arkime_reader_scheme_init);
    arkime_readers_add("bpf", reader_bpf_init);
    arkime_readers_add("netmap", reader_netmap_init);
    arkime_readers_add("xdp", reader_xdp_init);
}

/******************************************************************************/
void arkime_readers_start()
{
    char **interfaceOps;
    interfaceOps = arkime_config_raw_str_list(NULL, "interfaceOps", "");

    for (int i = 0; interfaceOps[i]; i++) {
        if (i >= 256) {
            CONFIGEXIT("Too many interfaceOps, max is 256");
        }
        if (!interfaceOps[i][0])
            continue;

        char **opsstr = g_strsplit(interfaceOps[i], ",", 0);
        const char *error = arkime_field_ops_parse(&readerFieldOps[i], ARKIME_FIELD_OPS_FLAGS_COPY, opsstr);
        g_strfreev(opsstr);

        if (error) {
            CONFIGEXIT("%s", error);
        }
    }
    g_strfreev(interfaceOps);

    // Compile all the filename ops.  The format is fieldexpr=value%value
    // value is expanded using the g_regex_replace rules (\1 being the first capture group)
    // https://developer.gnome.org/glib/stable/glib-Perl-compatible-regular-expressions.html#g-regex-replace
    char **filenameOpsStr;
    filenameOpsStr = arkime_config_str_list(NULL, "filenameOps", "");

    for (int i = 0; filenameOpsStr && filenameOpsStr[i]; i++) {
        if (i >= 256) {
            CONFIGEXIT("Too many filenameOps, max is 256");
        }
        if (!filenameOpsStr[i][0])
            continue;

        char *equal = strchr(filenameOpsStr[i], '=');
        if (!equal) {
            CONFIGEXIT("Must be FieldExpr=regex%%value, missing equal '%s'", filenameOpsStr[i]);
        }

        char *percent = strchr(equal + 1, '%');
        if (!percent) {
            CONFIGEXIT("Must be FieldExpr=regex%%value, missing percent '%s'", filenameOpsStr[i]);
        }

        *equal = 0;
        *percent = 0;

        int elen = strlen(equal + 1);
        if (!elen) {
            CONFIGEXIT("Must be FieldExpr=regex%%value, empty regex for '%s'", filenameOpsStr[i]);
        }

        int vlen = strlen(percent + 1);
        if (!vlen) {
            CONFIGEXIT("Must be FieldExpr=regex%%value, empty value for '%s'", filenameOpsStr[i]);
        }

        int fieldPos = arkime_field_by_exp(filenameOpsStr[i]);
        if (fieldPos == -1) {
            CONFIGEXIT("Must be FieldExpr=regex%%value, Unknown field expression '%s'", filenameOpsStr[i]);
        }

        readerFilenameOps[readerFilenameOpsNum].regex = g_regex_new(equal + 1, 0, 0, 0);
        readerFilenameOps[readerFilenameOpsNum].expand = g_strdup(percent + 1);
        if (!readerFilenameOps[readerFilenameOpsNum].regex)
            CONFIGEXIT("Couldn't compile regex '%s'", equal + 1);
        readerFilenameOps[readerFilenameOpsNum].field = fieldPos;
        readerFilenameOpsNum++;
    }
    g_strfreev(filenameOpsStr);

    arkime_reader_start();
}
/******************************************************************************/
// Compile a bpf filter for readers that filter in software, exits on error
void arkime_readers_compile_bpf(const char *name, int dlt, int snaplen, const char *filter, struct bpf_program *bpf)
{
    pcap_t *dead = pcap_open_dead(dlt, snaplen);
    if (!dead) {
        CONFIGEXIT("%s: Couldn't create pcap handle to compile bpf filter '%s'", name, filter);
    }
    if (pcap_compile(dead, bpf, filter, 1, PCAP_NETMASK_UNKNOWN) == -1) {
        CONFIGEXIT("%s: Couldn't compile bpf filter '%s': %s", name, filter, pcap_geterr(dead));
    }
    pcap_close(dead);
}
/******************************************************************************/
// Offline directory walks don't follow links unless asked, a dropped link could point anywhere
gboolean arkime_readers_is_dir(const char *path, gboolean followLinks)
{
    if (followLinks)
        return g_file_test(path, G_FILE_TEST_IS_DIR);

    struct stat st;
    return lstat(path, &st) == 0 && S_ISDIR(st.st_mode);
}
/******************************************************************************/
// The kernel's path for an open fd, so a link anywhere in the opened path shows up
LOCAL gboolean arkime_readers_fd_path(int fd, char *buf, int buflen)
{
#if defined(__linux__)
    char proc[64];
    snprintf(proc, sizeof(proc), "/proc/self/fd/%d", fd);
    const ssize_t len = readlink(proc, buf, buflen - 1);
    if (len <= 0 || len >= buflen - 1)
        return FALSE;
    buf[len] = 0;
    return TRUE;
#elif defined(F_GETPATH)
    if (buflen < MAXPATHLEN)
        return FALSE;
    return fcntl(fd, F_GETPATH, buf) != -1;
#else
    return FALSE;
#endif
}
/******************************************************************************/
// Open a file found by a directory walk. It must be a regular file and no part of path can be
// a link, so path must be built from a realpath'd directory. Returns the fd or -1, fills in st.
int arkime_readers_open_nofollow(const char *path, struct stat *st)
{
    const int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        LOG("ERROR - pcap open failed - Couldn't open file: '%s' with %s (%d)%s", path, strerror(errno), errno,
            errno == ELOOP ? ", use --followlinks to allow links" : "");
        return -1;
    }

    if (fstat(fd, st) != 0 || !S_ISREG(st->st_mode)) {
        LOG("ERROR - pcap open failed - '%s' is not a regular file", path);
        close(fd);
        return -1;
    }

    // O_NOFOLLOW only covers the last part, a directory above it could have been swapped for a link
    char real[MAX(PATH_MAX, MAXPATHLEN) + 1];
    if (!arkime_readers_fd_path(fd, real, sizeof(real)) && !realpath(path, real)) {
        LOG("ERROR - pcap open failed - Couldn't find the real path of '%s' with %s (%d)", path, strerror(errno), errno);
        close(fd);
        return -1;
    }

    if (strcmp(real, path) != 0) {
        LOG("ERROR - pcap open failed - '%s' is reached through a link to '%s', use --followlinks to allow links", path, real);
        close(fd);
        return -1;
    }

    return fd;
}
/******************************************************************************/
// When opened is set, only delete path if it is still the file that was opened
void arkime_readers_delete(const char *path, const struct stat *opened)
{
    if (config.debug)
        LOG("Deleting %s", path);

    struct stat st;
    if (opened && opened->st_ino &&
        (lstat(path, &st) != 0 || st.st_dev != opened->st_dev || st.st_ino != opened->st_ino)) {
        LOG("ERROR - Not deleting %s, it is no longer the file that was read", path);
        return;
    }

    if (unlink(path) != 0)
        LOG("Failed to delete file %s %s (%d)", path, strerror(errno), errno);
}
/******************************************************************************/
void arkime_readers_exit()
{
    if (arkime_reader_exit)
        arkime_reader_exit();
}
