/******************************************************************************/
/* reader-libpcap-file.c  -- Reader using libpcap to a file
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
#define _FILE_OFFSET_BITS 64
#include "moloch.h"
#include <errno.h>
#include <sys/stat.h>
#include "pcap.h"
#include "molochconfig.h"
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>

extern MolochPcapFileHdr_t   pcapFileHeader;

extern MolochConfig_t        config;

LOCAL  pcap_t               *pcap;
LOCAL  FILE                 *offlineFile = 0;

extern void                 *esServer;
LOCAL  MolochStringHead_t    monitorQ;

LOCAL  char                  offlinePcapFilename[PATH_MAX+1];
LOCAL  int                   pktsToRead;

LOCAL void reader_libpcapfile_opened();

LOCAL MolochPacketBatch_t   batch;
LOCAL uint8_t               readerPos;
extern char                *readerFileName[256];
extern MolochFieldOps_t     readerFieldOps[256];
extern uint32_t             readerOutputIds[256];

LOCAL struct {
    GRegex    *regex;
    int        field;
    char      *expand;
} filenameOps[100];
LOCAL int                   filenameOpsNum;

#ifdef HAVE_SYS_INOTIFY_H
#include <sys/inotify.h>
LOCAL int         monitorFd;
LOCAL GHashTable *wdHashTable;

LOCAL void reader_libpcapfile_monitor_dir(char *dirname);

LOCAL void reader_libpcapfile_monitor_do(struct inotify_event *event)
{
    gchar *dirname = g_hash_table_lookup(wdHashTable, (void *)(long)event->wd);
    gchar *fullfilename = g_build_filename (dirname, event->name, NULL);

    if (config.pcapRecursive &&
        (event->mask & IN_CREATE) &&
        g_file_test(fullfilename, G_FILE_TEST_IS_DIR)) {

        reader_libpcapfile_monitor_dir(fullfilename);
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

    MolochString_t *string = MOLOCH_TYPE_ALLOC0(MolochString_t);
    string->str = fullfilename;

    if (config.debug)
        LOG("Monitor enqueing %s", string->str);
    DLL_PUSH_TAIL(s_, &monitorQ, string);
    return;
}
/******************************************************************************/
LOCAL gboolean reader_libpcapfile_monitor_read()
{
    char buf[20 * (sizeof(struct inotify_event) + NAME_MAX + 1)] __attribute__ ((aligned(8)));

    int rc = read (monitorFd, buf, sizeof(buf));
    if (rc == 0)
        return TRUE;
    if (rc == -1)
        LOGEXIT("Monitor read failed - %s", strerror(errno));
    buf[rc] = 0;

    char *p;
    for (p = buf; p < buf + rc; ) {
        struct inotify_event *event = (struct inotify_event *) p;
        reader_libpcapfile_monitor_do(event);
        p += sizeof(struct inotify_event) + event->len;
     }
    return TRUE;
}
/******************************************************************************/
LOCAL void reader_libpcapfile_monitor_dir(char *dirname)
{
    if (config.debug)
        LOG("Monitoring %s", dirname);

    int rc = inotify_add_watch(monitorFd, dirname, IN_CLOSE_WRITE | IN_CREATE);
    if (rc == -1) {
        LOG ("WARNING - Couldn't watch %s %s", dirname, strerror(errno));
        return;
    } else {
        g_hash_table_insert(wdHashTable, (void*)(long)rc, g_strdup(dirname));
    }

    if (!config.pcapRecursive)
        return;

    GError   *error = NULL;
    GDir     *dir = g_dir_open(dirname, 0, &error);

    if (error)
        LOGEXIT("ERROR: Couldn't open pcap directory %s: Receive Error: %s", dirname, error->message);

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
            reader_libpcapfile_monitor_dir(fullfilename);
        }
        g_free(fullfilename);
    }
    g_dir_close(dir);
}
/******************************************************************************/
LOCAL void reader_libpcapfile_init_monitor()
{
    int          dir;
    monitorFd = inotify_init1(IN_NONBLOCK);

    if (monitorFd < 0)
        LOGEXIT("Couldn't init inotify %s", strerror(errno));

    wdHashTable = g_hash_table_new (g_direct_hash, g_direct_equal);
    moloch_watch_fd(monitorFd, MOLOCH_GIO_READ_COND, reader_libpcapfile_monitor_read, NULL);

    for (dir = 0; config.pcapReadDirs[dir] && config.pcapReadDirs[dir][0]; dir++) {
        reader_libpcapfile_monitor_dir(config.pcapReadDirs[dir]);
    }
}
#else
LOCAL void reader_libpcapfile_init_monitor()
{
    LOGEXIT("Monitoring not supporting on this OS");
}
#endif
/******************************************************************************/
LOCAL int reader_libpcapfile_process(char *filename)
{
    char         errbuf[1024];
    char         path[PATH_MAX];
    struct       stat stats;
    char         *token;
    char         *save_ptr;
    char         tmpFilename[PATH_MAX];
    struct group *gr;
    struct passwd *pw;


    if (!realpath(filename, offlinePcapFilename)) {
        LOG("ERROR - pcap open failed - Couldn't realpath file: '%s' with %d", filename, errno);
        return 1;
    }

    if (config.pcapSkip && moloch_db_file_exists(offlinePcapFilename, NULL)) {
        if (config.debug)
            LOG("Skipping %s", filename);
        return 1;
    }

    if (config.pcapReprocess && !moloch_db_file_exists(offlinePcapFilename, NULL)) {
        LOG("Can't reprocess %s", filename);
        return 1;
    }

    // check to see if viewer might have access issues to non-copied pcap file
    if (config.copyPcap == 0) {

      if (strlen (filename) > PATH_MAX) {
        // filename bigger than path buffer, skip check
      } else if ((config.dropUser == NULL) && (config.dropGroup == NULL)) {
        // drop.User,Group not defined -- skip check
      } else if (strncmp (filename, "/", 1) != 0) {
        LOG("WARNING using a relative path may make pcap inaccessible to viewer");
      } else {

    	  path[0] = 0;

        // process copy of filename given strtok_r changes arg
        strncpy (tmpFilename, filename, PATH_MAX);

        token = strtok_r (tmpFilename, "/", &save_ptr);

        while (token != NULL) {
          strcat (path, "/");
          strcat (path, token);

          if (stat(path, &stats) != -1) {
            gr = getgrgid (stats.st_gid);
            pw = getpwuid (stats.st_uid);

            if (stats.st_mode & S_IROTH)  {
              // world readable
            } else if ((stats.st_mode & S_IRGRP) && config.dropGroup && (strcmp (config.dropGroup, gr->gr_name) == 0)) {
              // group readable and dropGroup matches file group
              // TODO compare group id values as opposed to group name
            } else if ((stats.st_mode & S_IRUSR) && config.dropUser && (strcmp (config.dropUser, pw->pw_name) == 0)) {
              // user readable and dropUser matches file user
              // TODO compare user id values as opposed to user name
            } else
              LOG("WARNING -- permission issues with %s might make pcap inaccessible to viewer", path);
          } else
            LOG("WARNING -- Can't stat %s.  Pcap might not be accessible to viewer", path);

          token = strtok_r (NULL, "/", &save_ptr);
        }
      }
    }


    errbuf[0] = 0;
    LOG ("Processing %s", filename);
    pktsToRead = config.pktsToRead;
    pcap = pcap_open_offline(filename, errbuf);

    if (!pcap) {
        LOG("Couldn't process '%s' error '%s'", filename, errbuf);
        return 1;
    }

    reader_libpcapfile_opened();
    return 0;
}
/******************************************************************************/
LOCAL int reader_libpcapfile_next()
{
    gchar       *fullfilename;

    pcap = 0;

    if (config.pcapReadFiles) {
        static int pcapFilePos = 0;

        fullfilename = config.pcapReadFiles[pcapFilePos];

        if (!fullfilename) {
            goto filesDone;
        }
        pcapFilePos++;

        if (reader_libpcapfile_process(fullfilename)) {
            return reader_libpcapfile_next();
        }

        return 1;
    }

filesDone:
    if (config.pcapFileLists) {
        static int pcapFileListsPos;
        static FILE *file;
        char line[PATH_MAX];

        if (!file && !config.pcapFileLists[pcapFileListsPos]) {
            goto fileListsDone;
        }

        if (!file) {
            if (strcmp(config.pcapFileLists[pcapFileListsPos], "-") == 0)
                file = stdin;
            else
                file = fopen(config.pcapFileLists[pcapFileListsPos], "r");
            pcapFileListsPos++;
            if (!file) {
                LOG("ERROR - Couldn't open %s", config.pcapFileLists[pcapFileListsPos - 1]);
                return reader_libpcapfile_next();
            }
        }

        if (feof(file)) {
            fclose(file);
            file = NULL;
            return reader_libpcapfile_next();
        }

        if (!fgets(line, sizeof(line), file)) {
            fclose(file);
            file = NULL;
            return reader_libpcapfile_next();
        }

        int lineLen = strlen(line);
        if (line[lineLen-1] == '\n') {
            line[lineLen-1] = 0;
        }

        g_strstrip(line);
        if (!line[0] || line[0] == '#')
            return reader_libpcapfile_next();

        if (reader_libpcapfile_process(line)) {
            return reader_libpcapfile_next();
        }

        return 1;
    }


fileListsDone:
    if (config.pcapReadDirs) {
        static int   pcapDirPos = 0;
        static GDir *pcapGDir[21];
        static char *pcapBase[21];
        static int   pcapGDirLevel = -1;
        GError      *error = 0;

        if (pcapGDirLevel == -2) {
            goto dirsDone;
        }

        if (pcapGDirLevel == -1) {
            pcapGDirLevel = 0;
            pcapBase[0] = config.pcapReadDirs[pcapDirPos];
            if (!pcapBase[0]) {
                pcapGDirLevel = -2;
                goto dirsDone;
            }
        }

        if (!pcapGDir[pcapGDirLevel]) {
            pcapGDir[pcapGDirLevel] = g_dir_open(pcapBase[pcapGDirLevel], 0, &error);
            if (error) {
                LOGEXIT("ERROR: Couldn't open pcap directory: Receive Error: %s", error->message);
            }
        }
        while (1) {
            const gchar *filename = g_dir_read_name(pcapGDir[pcapGDirLevel]);

            // No more files, stop processing this directory
            if (!filename) {
                break;
            }

            // Skip hidden files/directories
            if (filename[0] == '.')
                continue;

            fullfilename = g_build_filename (pcapBase[pcapGDirLevel], filename, NULL);

            // If recursive option and a directory then process all the files in that dir
            if (config.pcapRecursive && g_file_test(fullfilename, G_FILE_TEST_IS_DIR)) {
                if (pcapGDirLevel >= 20)
                    continue;
                pcapBase[pcapGDirLevel+1] = fullfilename;
                pcapGDirLevel++;
                return reader_libpcapfile_next();
            }

            if (!g_regex_match(config.offlineRegex, filename, 0, NULL)) {
                g_free(fullfilename);
                continue;
            }

            if (reader_libpcapfile_process(fullfilename)) {
                g_free(fullfilename);
                continue;
            }

            g_free(fullfilename);
            return 1;
        }
        g_dir_close(pcapGDir[pcapGDirLevel]);
        pcapGDir[pcapGDirLevel] = 0;

        if (pcapGDirLevel > 0) {
            g_free(pcapBase[pcapGDirLevel]);
            pcapBase[pcapGDirLevel] = 0;
            pcapGDirLevel--;
            return reader_libpcapfile_next();
        } else {
            pcapDirPos++;
            pcapGDirLevel = -1;
            return reader_libpcapfile_next();
        }

    }

dirsDone:
    while (DLL_COUNT(s_, &monitorQ) > 0) {
        MolochString_t *string;
        DLL_POP_HEAD(s_, &monitorQ, string);
        fullfilename = string->str;
        MOLOCH_TYPE_FREE(MolochString_t, string);

        if (reader_libpcapfile_process(fullfilename)) {
            g_free(fullfilename);
            continue;
        }

        g_free(fullfilename);
        return 1;
    }
    return 0;
}
/******************************************************************************/
LOCAL gboolean reader_libpcapfile_monitor_gfunc (gpointer UNUSED(user_data))
{
    if (DLL_COUNT(s_, &monitorQ) == 0)
        return TRUE;

    if (reader_libpcapfile_next()) {
        return FALSE;
    }

    return TRUE;
}
/******************************************************************************/
LOCAL int reader_libpcapfile_stats(MolochReaderStats_t *stats)
{
    struct pcap_stat ps;
    if (!pcap) {
        stats->dropped = 0;
        stats->total = 0;
        return 1;
    }

    int rc = pcap_stats (pcap, &ps);
    if (rc)
        return rc;
    stats->dropped = ps.ps_drop;
    stats->total = ps.ps_recv;
    return 0;
}
/******************************************************************************/
LOCAL void reader_libpcapfile_pcap_cb(u_char *UNUSED(user), const struct pcap_pkthdr *h, const u_char *bytes)
{
    MolochPacket_t *packet = MOLOCH_TYPE_ALLOC0(MolochPacket_t);

    if (unlikely(h->caplen != h->len)) {
        if (!config.readTruncatedPackets && !config.ignoreErrors) {
            LOGEXIT("ERROR - Moloch requires full packet captures caplen: %d pktlen: %d. "
                "If using tcpdump use the \"-s0\" option, or set readTruncatedPackets in ini file",
                h->caplen, h->len);
        }
        packet->pktlen     = h->caplen;
    } else {
        packet->pktlen     = h->len;
    }

    packet->pkt           = (u_char *)bytes;
    packet->ts            = h->ts;
    packet->readerFilePos = ftell(offlineFile) - 16 - h->len;
    packet->readerPos     = readerPos;
    moloch_packet_batch(&batch, packet);
}
/******************************************************************************/
LOCAL gboolean reader_libpcapfile_read()
{
    // pause reading if too many waiting disk operations
    if (moloch_writer_queue_length() > 10) {
        if (config.debug)
            LOG("Waiting to start next file, write q: %u", moloch_writer_queue_length());
        return G_SOURCE_CONTINUE;
    }

    // pause reading if too many waiting ES operations
    if (moloch_http_queue_length(esServer) > 40) {
        if (config.debug)
            LOG("Waiting to start next file, es q: %d", moloch_http_queue_length(esServer));
        return G_SOURCE_CONTINUE;
    }

    // pause reading if too many packets are waiting to be processed
    if (moloch_packet_outstanding() > 2048) {
        if (config.debug)
            LOG("Waiting to start next file, packet q: %d", moloch_packet_outstanding());
        return G_SOURCE_CONTINUE;
    }

    int r;
    if (pktsToRead > 0) {
        r = pcap_dispatch(pcap, MIN(pktsToRead, 5000), reader_libpcapfile_pcap_cb, NULL);

        if (r > 0)
            pktsToRead -= r;

        if (pktsToRead == 0)
            r = 0;
    } else {
        r = pcap_dispatch(pcap, 5000, reader_libpcapfile_pcap_cb, NULL);
    }
    moloch_packet_batch_flush(&batch);

    // Some kind of failure, move to the next file or quit
    if (r <= 0) {
        if (config.pcapDelete && r == 0) {
            if (config.debug)
                LOG("Deleting %s", offlinePcapFilename);
            int rc = unlink(offlinePcapFilename);
            if (rc != 0)
                LOG("Failed to delete file %s %s (%d)", offlinePcapFilename, strerror(errno), errno);
        }
        pcap_close(pcap);
        if (reader_libpcapfile_next()) {
            return G_SOURCE_REMOVE;
        }

        if (config.pcapMonitor)
            g_timeout_add(100, reader_libpcapfile_monitor_gfunc, 0);
        else {
            moloch_quit();
        }
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}
/******************************************************************************/
LOCAL void reader_libpcapfile_opened()
{
    int dlt_to_linktype(int dlt);

    if (config.flushBetween)
        moloch_session_flush();

    moloch_packet_set_linksnap(dlt_to_linktype(pcap_datalink(pcap)) | pcap_datalink_ext(pcap), pcap_snapshot(pcap));

    offlineFile = pcap_file(pcap);

    if (config.bpf && pcapFileHeader.linktype != 239) {
        struct bpf_program   bpf;

        if (pcap_compile(pcap, &bpf, config.bpf, 1, PCAP_NETMASK_UNKNOWN) == -1) {
            LOGEXIT("ERROR - Couldn't compile filter: '%s' with %s", config.bpf, pcap_geterr(pcap));
        }

	if (pcap_setfilter(pcap, &bpf) == -1) {
            LOGEXIT("ERROR - Couldn't set filter: '%s' with %s", config.bpf, pcap_geterr(pcap));
        }
        pcap_freecode(&bpf);
    }

    readerPos++;
    // We've wrapped around all 256 reader items, clear the previous file information
    if (readerFileName[readerPos]) {
        g_free(readerFileName[readerPos]);
        readerOutputIds[readerPos] = 0;
    }
    readerFileName[readerPos] = g_strdup(offlinePcapFilename);

    int fd = pcap_fileno(pcap);
    if (fd == -1) {
        g_timeout_add(100, reader_libpcapfile_read, NULL);
    } else {
        moloch_watch_fd(fd, MOLOCH_GIO_READ_COND, reader_libpcapfile_read, NULL);
    }

    if (filenameOpsNum > 0) {

        // Free any previously allocated
        if (readerFieldOps[readerPos].size > 0)
            moloch_field_ops_free(&readerFieldOps[readerPos]);

        moloch_field_ops_init(&readerFieldOps[readerPos], filenameOpsNum, MOLOCH_FIELD_OPS_FLAGS_COPY);

        // Go thru all the filename ops looking for matches and then expand the value string
        int i;
        for (i = 0; i < filenameOpsNum; i++) {
            GMatchInfo *match_info = 0;
            g_regex_match(filenameOps[i].regex, offlinePcapFilename, 0, &match_info);
            if (g_match_info_matches(match_info)) {
                GError *error = 0;
                char *expand = g_match_info_expand_references(match_info, filenameOps[i].expand, &error);
                if (error) {
                    LOG("Error expanding '%s' with '%s' - %s", offlinePcapFilename, filenameOps[i].expand, error->message);
                    g_error_free(error);
                }
                if (expand) {
                    moloch_field_ops_add(&readerFieldOps[readerPos], filenameOps[i].field, expand, -1);
                    g_free(expand);
                }
            }
            g_match_info_free(match_info);
        }
    }
}

/******************************************************************************/
LOCAL void reader_libpcapfile_start() {


    // Compile all the filename ops.  The formation is fieldexpr=value%value
    // value is expanded using the g_regex_replace rules (\1 being the first capture group)
    // https://developer.gnome.org/glib/stable/glib-Perl-compatible-regular-expressions.html#g-regex-replace
    char **filenameOpsStr;
    filenameOpsStr = moloch_config_str_list(NULL, "filenameOps", "");

    int i;
    for (i = 0; filenameOpsStr && filenameOpsStr[i] && i < 100; i++) {
        if (!filenameOpsStr[i][0])
            continue;

        char *equal = strchr(filenameOpsStr[i], '=');
        if (!equal) {
            LOGEXIT("Must be FieldExpr=regex%%value, missing equal '%s'", filenameOpsStr[i]);
        }

        char *percent = strchr(equal+1, '%');
        if (!percent) {
            LOGEXIT("Must be FieldExpr=regex%%value, missing percent '%s'", filenameOpsStr[i]);
        }

        *equal = 0;
        *percent = 0;

        int elen = strlen(equal+1);
        if (!elen) {
            LOGEXIT("Must be FieldExpr=regex%%value, empty regex for '%s'", filenameOpsStr[i]);
        }

        int vlen = strlen(percent+1);
        if (!vlen) {
            LOGEXIT("Must be FieldExpr=regex%%value, empty value for '%s'", filenameOpsStr[i]);
        }

        int fieldPos = moloch_field_by_exp(filenameOpsStr[i]);
        if (fieldPos == -1) {
            LOGEXIT("Must be FieldExpr=regex?value, Unknown field expression '%s'", filenameOpsStr[i]);
        }

        filenameOps[filenameOpsNum].regex = g_regex_new(equal+1, 0, 0, 0);
        filenameOps[filenameOpsNum].expand = g_strdup(percent+1);
        if (!filenameOps[filenameOpsNum].regex)
            LOGEXIT("Couldn't compile regex '%s'", equal+1);
        filenameOps[filenameOpsNum].field = fieldPos;
        filenameOpsNum++;
    }
    g_strfreev(filenameOpsStr);

    // Now actually start
    reader_libpcapfile_next();
    if (!pcap) {
        if (config.pcapMonitor) {
            g_timeout_add(100, reader_libpcapfile_monitor_gfunc, 0);
        } else {
            moloch_quit();
        }
    }
}
/******************************************************************************/
void reader_libpcapfile_init(char *UNUSED(name))
{
    moloch_reader_start         = reader_libpcapfile_start;
    moloch_reader_stats         = reader_libpcapfile_stats;

    if (config.pcapMonitor)
        reader_libpcapfile_init_monitor();

    DLL_INIT(s_, &monitorQ);
    moloch_packet_batch_init(&batch);
}
