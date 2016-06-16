/******************************************************************************/
/* reader-libpcap-file.c  -- Reader using libpcap to a file
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
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
#include <gio/gio.h>
#include "pcap.h"

extern MolochPcapFileHdr_t   pcapFileHeader;

extern MolochConfig_t        config;

static pcap_t               *pcap;
static FILE                 *offlineFile = 0;

extern void                 *esServer;
LOCAL  MolochStringHead_t    monitorQ;

LOCAL  char                  offlinePcapFilename[PATH_MAX+1];
LOCAL  char                 *offlinePcapName;

void reader_libpcapfile_opened();

static struct bpf_program   *bpf_programs = 0;

/******************************************************************************/
void reader_libpcapfile_monitor_dir(char *dirname);
static void
reader_libpcapfile_monitor_changed (GFileMonitor      *UNUSED(monitor),
                                    GFile             *file,
                                    GFile             *UNUSED(other_file),
                                    GFileMonitorEvent  event_type,
                                    gpointer           UNUSED(user_data))
{
    // Monitor new directories?
    if (config.pcapRecursive &&
        event_type == G_FILE_MONITOR_EVENT_CREATED &&
        g_file_query_file_type(file, G_FILE_QUERY_INFO_NONE, NULL) == G_FILE_TYPE_DIRECTORY) {

        gchar *path = g_file_get_path(file);
        reader_libpcapfile_monitor_dir(path);
        g_free(path);

        return;
    }

    if (event_type != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
        return;

    gchar *basename = g_file_get_path(file);
    if (!g_regex_match(config.offlineRegex, basename, 0, NULL)) {
        g_free(basename);
        return;
    }
    g_free(basename);

    gchar *path = g_file_get_path(file);
    MolochString_t *string = MOLOCH_TYPE_ALLOC0(MolochString_t);
    string->str = path;

    if (config.debug) 
        LOG("Monitor enqueing %s", string->str);
    DLL_PUSH_TAIL(s_, &monitorQ, string);
}
/******************************************************************************/
void reader_libpcapfile_monitor_dir(char *dirname)
{
    GError      *error = 0;
    if (config.debug)
        LOG("Monitoring %s", dirname);
    if (error) {
        LOG("ERROR: Couldn't open pcap directory %s: Receive Error: %s", dirname, error->message);
        exit(0);
    }

    GFile *filedir = g_file_new_for_path(dirname);
    GFileMonitor *monitor = g_file_monitor_directory (filedir, 0, NULL, &error);
    g_file_monitor_set_rate_limit(monitor, 0);
    g_signal_connect (monitor, "changed", G_CALLBACK (reader_libpcapfile_monitor_changed), 0);

    if (!config.pcapRecursive)
        return;
    GDir *dir = g_dir_open(dirname, 0, &error);
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
void reader_libpcapfile_init_monitor()
{
    int          dir;

    for (dir = 0; config.pcapReadDirs[dir] && config.pcapReadDirs[dir][0]; dir++) {
        reader_libpcapfile_monitor_dir(config.pcapReadDirs[dir]);
    }
}
/******************************************************************************/
int reader_libpcapfile_next()
{
    char         errbuf[1024];
    gchar       *fullfilename;

    pcap = 0;

    if (config.pcapReadFiles) {
        static int pcapFilePos = 0;

        fullfilename = config.pcapReadFiles[pcapFilePos];

        errbuf[0] = 0;
        if (!fullfilename) {
            goto filesDone;
        }
        pcapFilePos++;

        LOG ("Processing %s", fullfilename);
        pcap = pcap_open_offline(fullfilename, errbuf);

        if (!pcap) {
            LOG("Couldn't process '%s' error '%s'", fullfilename, errbuf);
            return reader_libpcapfile_next();
        }
        if (!realpath(fullfilename, offlinePcapFilename)) {
            LOG("ERROR - pcap open failed - Couldn't realpath file: '%s' with %d", fullfilename, errno);
            exit(1);
        }

        reader_libpcapfile_opened();
        return 1;
    }

filesDone:

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
                LOG("ERROR: Couldn't open pcap directory: Receive Error: %s", error->message);
                exit(0);
            }
        }
        const gchar *filename;
        while (1) {
            filename = g_dir_read_name(pcapGDir[pcapGDirLevel]);

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

            if (!realpath(fullfilename, offlinePcapFilename)) {
                g_free(fullfilename);
                continue;
            }

            if (config.pcapSkip && moloch_db_file_exists(offlinePcapFilename)) {
                if (config.debug)
                    LOG("Skipping %s", fullfilename);
                g_free(fullfilename);
                continue;
            }

            LOG ("Processing %s", fullfilename);
            errbuf[0] = 0;
            pcap = pcap_open_offline(fullfilename, errbuf);
            if (!pcap) {
                LOG("Couldn't process '%s' error '%s'", fullfilename, errbuf);
                g_free(fullfilename);
                continue;
            }
            reader_libpcapfile_opened();
            g_free(fullfilename);
            return 1;
        }
        g_dir_close(pcapGDir[pcapGDirLevel]);
        pcapGDir[pcapGDirLevel] = 0;

        if (pcapGDirLevel > 0) {
            g_free(pcapBase[pcapGDirLevel]);
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

        if (!realpath(fullfilename, offlinePcapFilename)) {
            g_free(fullfilename);
            continue;
        }

        if (config.pcapSkip && moloch_db_file_exists(offlinePcapFilename)) {
            if (config.debug)
                LOG("Skipping %s", fullfilename);
            g_free(fullfilename);
            continue;
        }

        LOG ("Processing %s", fullfilename);
        errbuf[0] = 0;
        pcap = pcap_open_offline(fullfilename, errbuf);
        if (!pcap) {
            LOG("Couldn't process '%s' error '%s'", fullfilename, errbuf);
            g_free(fullfilename);
            continue;
        }
        reader_libpcapfile_opened();
        g_free(fullfilename);
        return 1;
    }
    return 0;
}
/******************************************************************************/
gboolean reader_libpcapfile_monitor_gfunc (gpointer UNUSED(user_data))
{
    if (DLL_COUNT(s_, &monitorQ) == 0)
        return TRUE;

    if (reader_libpcapfile_next()) {
        return FALSE;
    }

    return TRUE;
}
/******************************************************************************/
int reader_libpcapfile_stats(MolochReaderStats_t *stats)
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
void reader_libpcapfile_pcap_cb(u_char *UNUSED(user), const struct pcap_pkthdr *h, const u_char *bytes)
{
    MolochPacket_t *packet = MOLOCH_TYPE_ALLOC0(MolochPacket_t);

    if (unlikely(h->caplen != h->len)) {
        if (!config.readTruncatedPackets) {
            LOG("ERROR - Moloch requires full packet captures caplen: %d pktlen: %d. "
                "If using tcpdump use the \"-s0\" option, or set readTruncatedPackets in ini file",
                h->caplen, h->len);
            exit (0);
        }
        packet->pktlen     = h->caplen;
    } else {
        packet->pktlen     = h->len;
    }

    packet->pkt           = (u_char *)bytes;
    packet->ts            = h->ts;
    packet->readerFilePos = ftell(offlineFile) - 16 - h->len;
    packet->readerName    = offlinePcapName;
    moloch_packet(packet);
}
/******************************************************************************/
gboolean reader_libpcapfile_read()
{
    // pause reading if too many waiting disk operations
    if (moloch_writer_queue_length() > 10) {
        return TRUE;
    }

    // pause reading if too many waiting ES operations
    if (moloch_http_queue_length(esServer) > 100) {
        return TRUE;
    }

    // pause reading if too many packets are waiting to be processed
    if (moloch_packet_outstanding() > (int32_t)(config.maxPacketsInQueue/2)) {
        return TRUE;
    }

    int r = pcap_dispatch(pcap, 10000, reader_libpcapfile_pcap_cb, NULL);

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
            return FALSE;
        }

        if (config.pcapMonitor)
            g_timeout_add(100, reader_libpcapfile_monitor_gfunc, 0);
        else
            moloch_quit();
        return FALSE;
    }

    return TRUE;
}
/******************************************************************************/
int reader_libpcapfile_should_filter(const MolochPacket_t *packet)
{
    int i;
    for (i = 0; i < config.dontSaveBPFsNum; i++) {
        if (bpf_filter(bpf_programs[i].bf_insns, packet->pkt, packet->pktlen, packet->pktlen)) {
            return i;
            break;
        }
    }
    return -1;
}
/******************************************************************************/
void reader_libpcapfile_opened()
{
    int dlt_to_linktype(int dlt);

    pcapFileHeader.linktype = dlt_to_linktype(pcap_datalink(pcap)) | pcap_datalink_ext(pcap);
    pcapFileHeader.snaplen = pcap_snapshot(pcap);

    offlineFile = pcap_file(pcap);

    if (config.bpf) {
        struct bpf_program   bpf;

        if (pcap_compile(pcap, &bpf, config.bpf, 1, PCAP_NETMASK_UNKNOWN) == -1) {
            LOG("ERROR - Couldn't compile filter: '%s' with %s", config.bpf, pcap_geterr(pcap));
            exit(1);
        }

	if (pcap_setfilter(pcap, &bpf) == -1) {
            LOG("ERROR - Couldn't set filter: '%s' with %s", config.bpf, pcap_geterr(pcap));
            exit(1);
        }
    }

    if (config.dontSaveBPFs) {
        int i;
        if (bpf_programs) {
            for (i = 0; i < config.dontSaveBPFsNum; i++) {
                pcap_freecode(&bpf_programs[i]);
            }
        } else {
            bpf_programs = malloc(config.dontSaveBPFsNum*sizeof(struct bpf_program));
        }
        for (i = 0; i < config.dontSaveBPFsNum; i++) {
            if (pcap_compile(pcap, &bpf_programs[i], config.dontSaveBPFs[i], 1, PCAP_NETMASK_UNKNOWN) == -1) {
                LOG("ERROR - Couldn't compile filter: '%s' with %s", config.dontSaveBPFs[i], pcap_geterr(pcap));
                exit(1);
            }
        }
        moloch_reader_should_filter = reader_libpcapfile_should_filter;
    }

    if (config.flushBetween)
        moloch_session_flush();

    offlinePcapName = strdup(offlinePcapFilename);

    int fd = pcap_fileno(pcap);
    if (fd == -1) {
        g_timeout_add(0, reader_libpcapfile_read, NULL);
    } else {
        moloch_watch_fd(fd, MOLOCH_GIO_READ_COND, reader_libpcapfile_read, NULL);
    }
}

/******************************************************************************/
void reader_libpcapfile_start() {
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
}
