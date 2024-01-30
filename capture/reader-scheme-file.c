/******************************************************************************/
/* reader-scheme-file.c
 *
 * Copyright 2023 All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include "arkime.h"

extern ArkimeConfig_t        config;

/******************************************************************************/
int scheme_file_dir(const char *dirname)
{
    GDir   *pcapGDir;
    GError *error = 0;

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

        arkime_reader_scheme_load(fullfilename);
        g_free(fullfilename);
    }
    g_dir_close(pcapGDir);
    return 0;
}
/******************************************************************************/
LOCAL uint8_t buffer[0xfffff];
int scheme_file_load(const char *uri)
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
