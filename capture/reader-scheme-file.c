/******************************************************************************/
/* reader-scheme-file.c
 *
 * Copyright 2023 All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include "arkime.h"
#include "arkimeconfig.h"

extern ArkimeConfig_t        config;

#ifdef HAVE_SYS_INOTIFY_H
#include <sys/inotify.h>
LOCAL int         monitorFd;
LOCAL GHashTable *wdHashTable;

LOCAL void scheme_file_monitor_dir(const char *dirname);

LOCAL void scheme_file_monitor_do(struct inotify_event *event)
{
    gchar *dirname = g_hash_table_lookup(wdHashTable, (void *)(long)event->wd);
    gchar *fullfilename = g_build_filename (dirname, event->name, NULL);

    if (config.pcapRecursive &&
        (event->mask & IN_CREATE) &&
        g_file_test(fullfilename, G_FILE_TEST_IS_DIR)) {

        scheme_file_monitor_dir(fullfilename);
        g_free(fullfilename);
        return;
    }

    if ((event->mask & IN_CLOSE_WRITE) == 0) {
        g_free(fullfilename);
        return;
    }

    if (!g_regex_match(config.offlineRegex, fullfilename, 0, NULL)) {
        g_free(fullfilename);
        return;
    }

    if (config.debug)
        LOG("Monitor enqueing %s", fullfilename);
    arkime_reader_scheme_load(fullfilename, FALSE);
}
/******************************************************************************/
LOCAL gboolean scheme_file_monitor_read()
{
    char buf[20 * (sizeof(struct inotify_event) + NAME_MAX + 1)] __attribute__ ((aligned(8)));

    int rc = read (monitorFd, buf, sizeof(buf));
    if (rc == 0)
        return TRUE;
    if (rc == -1)
        LOGEXIT("ERROR - Monitor read failed - %s", strerror(errno));
    buf[rc] = 0;

    char *p;
    for (p = buf; p < buf + rc; ) {
        struct inotify_event *event = (struct inotify_event *) p;
        scheme_file_monitor_do(event);
        p += sizeof(struct inotify_event) + event->len;
    }
    return TRUE;
}
/******************************************************************************/
LOCAL void scheme_file_init_monitor()
{
    monitorFd = inotify_init1(IN_NONBLOCK);

    if (monitorFd < 0)
        LOGEXIT("ERROR - Couldn't init inotify %s", strerror(errno));

    wdHashTable = g_hash_table_new (g_direct_hash, g_direct_equal);
    arkime_watch_fd(monitorFd, ARKIME_GIO_READ_COND, scheme_file_monitor_read, NULL);
}
/******************************************************************************/
LOCAL void scheme_file_monitor_dir(const char *dirname)
{
    static char inited = 0;
    if (!inited) {
        inited = 1;
        scheme_file_init_monitor();
    }

    if (config.debug)
        LOG("Monitoring %s", dirname);

    int rc = inotify_add_watch(monitorFd, dirname, IN_CLOSE_WRITE | IN_CREATE);
    if (rc == -1) {
        LOG ("WARNING - Couldn't watch %s %s", dirname, strerror(errno));
        return;
    } else {
        g_hash_table_insert(wdHashTable, (void *)(long)rc, g_strdup(dirname));
    }

    if (!config.pcapRecursive)
        return;

    GError   *error = NULL;
    GDir     *dir = g_dir_open(dirname, 0, &error);

    if (error)
        LOGEXIT("ERROR - Couldn't open pcap directory %s: Receive Error: %s", dirname, error->message);

    while (1) {
        const gchar *filename = g_dir_read_name(dir);

        // No more files, stop processing this directory
        if (!filename) {
            break;
        }

        // Skip hidden files/directories
        if (filename[0] == '.')
            continue;

        gchar *fullfilename = g_build_filename (dirname, filename, NULL);

        if (g_file_test(fullfilename, G_FILE_TEST_IS_DIR)) {
            scheme_file_monitor_dir(fullfilename);
        }
        g_free(fullfilename);
    }
    g_dir_close(dir);
}
#else
LOCAL void scheme_file_monitor_dir(const char UNUSED(*dirname))
{
    LOGEXIT("ERROR - Monitoring not supporting on this OS");
}
#endif
/******************************************************************************/
int scheme_file_dir(const char *dirname)
{
    GDir   *pcapGDir;
    GError *error = 0;

    if (config.pcapMonitor) {
        scheme_file_monitor_dir(dirname);
    }

    pcapGDir = g_dir_open(dirname, 0, &error);
    while (1) {
        const gchar *filename = g_dir_read_name(pcapGDir);

        // No more files, stop processing this directory
        if (!filename) {
            break;
        }

        // Skip hidden files/directories
        if (filename[0] == '.')
            continue;

        gchar *fullfilename = g_build_filename (dirname, filename, NULL);

        // If recursive option and a directory then process all the files in that dir
        if (config.pcapRecursive && g_file_test(fullfilename, G_FILE_TEST_IS_DIR)) {
            scheme_file_dir(fullfilename);
            g_free(fullfilename);
            continue;
        }

        if (!g_regex_match(config.offlineRegex, filename, 0, NULL)) {
            g_free(fullfilename);
            continue;
        }

        arkime_reader_scheme_load(fullfilename, FALSE);
        g_free(fullfilename);
    }
    g_dir_close(pcapGDir);
    return 1;
}
/******************************************************************************/
LOCAL uint8_t buffer[0xfffff];
int scheme_file_load(const char *uri, gboolean UNUSED(dirHint))
{
    if (strncmp("file://", uri, 7) == 0) {
        uri += 7;
    }

    if (g_file_test(uri, G_FILE_TEST_IS_DIR)) {
        return scheme_file_dir(uri);
    }

    int fd;
    if (strcmp(uri, "-") == 0) {
        fd = fileno(stdin);
    } else {
        LOCAL  char  filename[PATH_MAX + 1];
        if (!realpath(uri, filename)) {
            LOG("ERROR - pcap open failed - Couldn't realpath file: '%s' with %s (%d)", uri, strerror(errno), errno);
            return 1;
        }

        if (config.pcapSkip && arkime_db_file_exists(filename, NULL)) {
            if (config.debug)
                LOG("Skipping %s", filename);
            return 1;
        }

        if (config.pcapReprocess && !arkime_db_file_exists(filename, NULL)) {
            LOG("Can't reprocess %s", filename);
            return 1;
        }


        fd = open(filename, O_RDONLY);
        if (!fd) {
            LOG("ERROR - pcap open failed - Couldn't realpath file: '%s' with %s (%d)", filename, strerror(errno), errno);
            return 1;
        }

        // check to see if viewer might have access issues to non-copied pcap file
        if (config.copyPcap == 0) {
            arkime_check_file_permissions(filename);
        }
        uri = filename;
    }

    ssize_t bytesRead = 0;

    do {
        bytesRead = read(fd, buffer, sizeof(buffer));
        if (bytesRead > 0) {
            if (arkime_reader_scheme_process(uri, buffer, bytesRead, NULL)) {
                close(fd);
                return 1;
            }
        } else if (bytesRead == 0) {
            break;
        } else if (bytesRead < 0) {
        }
    } while (bytesRead > 0);

    close(fd);
    return 0;
}
/******************************************************************************/
LOCAL void scheme_file_exit()
{
}
/******************************************************************************/
void arkime_reader_scheme_file_init()
{
    arkime_reader_scheme_register("file", scheme_file_load, scheme_file_exit);
}
