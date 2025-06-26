/******************************************************************************/
/* reader-scheme.c
 *
 * Copyright 2023 All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"
#include "pcap.h"

extern ArkimePcapFileHdr_t   pcapFileHeader;

LOCAL struct bpf_program     bpf;
LOCAL pcap_t                *deadPcap;

extern ArkimeConfig_t        config;

typedef struct {
    char              *name;
    ArkimeReaderExit   exit;
    ArkimeSchemeLoad   load;
} ArkimeScheme_t;

typedef struct ArkimeSchemeLater {
    struct ArkimeSchemeLater *next;
    char                     *uri;
    ArkimeSchemeFlags         flags;
    ArkimeSchemeAction_t    *actions;
} ArkimeSchemeLater_t;

LOCAL ArkimeSchemeLater_t *laterHead;
LOCAL ArkimeSchemeLater_t *laterTail;
ARKIME_COND_DEFINE(laterLock);
ARKIME_LOCK_DEFINE(laterLock);
LOCAL GThread *schemeThread;

LOCAL  ArkimeStringHashStd_t  schemesHash;
LOCAL  ArkimeScheme_t        *fileScheme;

LOCAL uint64_t totalPackets;
LOCAL uint64_t dropped;

enum ArkimeSchemeMode { ARKIME_SCHEME_FILEHEADER, ARKIME_SCHEME_PACKET_HEADER, ARKIME_SCHEME_PACKET, ARKIME_SCHEME_PACKET_SKIP};
LOCAL enum ArkimeSchemeMode state;
LOCAL int32_t pktlen;

enum ArkimeSchemeMode { ARKIME_SCHEME_FILEHEADER, ARKIME_SCHEME_PACKET_HEADER, ARKIME_SCHEME_PACKET, ARKIME_SCHEME_PACKET_SKIP};
LOCAL enum ArkimeSchemeMode state;

LOCAL int32_t pktlen;
LOCAL uint8_t tmpBuffer[0xffff];
LOCAL uint32_t tmpBufferLen;

LOCAL int                    offlineDispatchAfter;
extern void                 *esServer;

extern ArkimeOfflineInfo_t   offlineInfo[256];

extern ArkimeFieldOps_t     readerFieldOps[256];
ArkimeSchemeAction_t       *schemeActions[256];
extern ArkimeFilenameOps_t  readerFilenameOps[256];
extern int                  readerFilenameOpsNum;

#define SWAP32(x) ((((x)&0xff000000) >> 24) | (((x)&0x00ff0000) >> 8) | (((x)&0x0000ff00) << 8) | (((x)&0x000000ff) << 24))
#define SWAP16(x) ((((x)&0xff00) >> 8) | (((x)&0x00ff) << 8))

LOCAL uint64_t             startPos;
LOCAL uint8_t              readerPos;
LOCAL int                  needSwap;
LOCAL int                  nanosecond;

/******************************************************************************/
LOCAL void reader_scheme_actions_ref(ArkimeSchemeAction_t *actions)
{
    if (!actions)
        return;

    actions->refs++;
}
/******************************************************************************/
LOCAL void reader_scheme_actions_deref(ArkimeSchemeAction_t *actions)
{
    if (!actions)
        return;

    actions->refs--;

    if (actions->refs)
        return;

    arkime_field_ops_free(&actions->ops);
    ARKIME_TYPE_FREE(ArkimeSchemeAction_t, actions);
}
/******************************************************************************/
LOCAL ArkimeScheme_t *uri2scheme(const char *uri)
{
    ArkimeString_t *str;

    const char *colonslashslash = strstr(uri, "://");
    if (colonslashslash) {
        char scheme[30];
        if (colonslashslash - uri > 29) {
            LOGEXIT("ERROR - Scheme too long for %s", uri);
        }
        memcpy(scheme, uri, colonslashslash - uri);
        scheme[colonslashslash - uri] = 0;
        HASH_FIND(s_, schemesHash, scheme, str);
    } else {
        return fileScheme;
    }
    return str ? str->uw : NULL;
}
/******************************************************************************/
LOCAL void arkime_reader_scheme_load_thread(const char *uri, ArkimeSchemeFlags flags, ArkimeSchemeAction_t *actions)
{
    LOG ("Processing %s", uri);
    ArkimeScheme_t *readerScheme = uri2scheme(uri);
    if (!readerScheme) {
        LOG("ERROR - Unknown scheme for %s", uri);
        return;
    }

    startPos = 0;
    state = ARKIME_SCHEME_FILEHEADER;
    lastBytes = 0;
    lastPackets = 0;
    tmpBufferLen = 0;

    int rcl = readerScheme->load(uri, flags, actions);

    if (rcl == 0 && !config.dryRun && !config.copyPcap && offlineInfo[readerPos].didBatch) {
        arkime_packet_batch_end_of_file(readerPos);
    }

    if (config.flushBetween) {
        arkime_session_flush();
        int rc[4];

        // Pause until all packets and commands are done
        while ((rc[0] = arkime_session_cmd_outstanding()) + (rc[1] = arkime_session_close_outstanding()) + (rc[2] = arkime_packet_outstanding()) + (rc[3] = arkime_session_monitoring()) > 0) {
            if (config.debug) {
                LOG("Waiting before next %d %d %d %d", rc[0], rc[1], rc[2], rc[3]);
            }
            usleep(5000);
        }
    }
}
/******************************************************************************/
void arkime_reader_scheme_load(const char *uri, ArkimeSchemeFlags flags, ArkimeSchemeAction_t *actions)
{
    static int depth;
    // if on the scheme thread and stack isn't too deep just process right away
    if (g_thread_self() == schemeThread && depth < 20) {
        depth++;
        arkime_reader_scheme_load_thread(uri, flags, actions);
        depth--;
        return;
    }

    // Enqueue for later
    ArkimeSchemeLater_t *item = ARKIME_TYPE_ALLOC(ArkimeSchemeLater_t);
    item->next = 0;
    item->uri = g_strdup(uri);
    item->flags = flags;
    item->actions = actions;
    reader_scheme_actions_ref(actions);
    ARKIME_LOCK(laterLock);
    if (laterHead) {
        laterTail->next = item;
    } else {
        laterHead = laterTail = item;
    }
    ARKIME_COND_SIGNAL(laterLock);
    ARKIME_UNLOCK(laterLock);
}
/******************************************************************************/
LOCAL int reader_scheme_header(const char *uri, const uint8_t *header, const char *extraInfo, ArkimeSchemeAction_t *actions)
{
    const ArkimePcapFileHdr_t *h = (const ArkimePcapFileHdr_t *)header;
    if (h->magic != 0xa1b2c3d4 && h->magic != 0xd4c3b2a1 &&
        h->magic != 0xa1b23c4d && h->magic != 0x4d3cb2a1) {

        if (config.ignoreErrors) {
#ifndef SFUZZLOCH
            LOG("ERROR - Unknown magic %x in %s", h->magic, uri);
#endif
            return 1;
        } else {
            LOGEXIT("ERROR - Unknown magic %x in %s", h->magic, uri);
        }
    }

    needSwap = (h->magic == 0xd4c3b2a1 || h->magic == 0x4d3cb2a1);
    nanosecond = (h->magic == 0xa1b23c4d || h->magic == 0x4d3cb2a1);

    uint32_t snaplen;
    uint32_t dlt;
    if (needSwap) {
        snaplen = SWAP32(h->snaplen);
        dlt = SWAP32(h->dlt);
    } else {
        snaplen = h->snaplen;
        dlt = h->dlt;
    }

    readerPos++;
    // We've wrapped around all 256 reader items, clear the previous file information
    if (offlineInfo[readerPos].filename) {
        g_free(offlineInfo[readerPos].filename);
        g_free(offlineInfo[readerPos].extra);
        memset(&offlineInfo[readerPos], 0, sizeof(ArkimeOfflineInfo_t));
    }
    offlineInfo[readerPos].filename = g_strdup(uri);

    ArkimeScheme_t *readerScheme = uri2scheme(uri);
    offlineInfo[readerPos].scheme = readerScheme->name;
    offlineInfo[readerPos].extra = g_strdup(extraInfo);

    if (schemeActions[readerPos])
        reader_scheme_actions_deref(schemeActions[readerPos]);

    schemeActions[readerPos] = actions;
    reader_scheme_actions_ref(actions);

    if (readerFilenameOpsNum > 0) {
        // Free any previously allocated
        if (readerFieldOps[readerPos].size > 0)
            arkime_field_ops_free(&readerFieldOps[readerPos]);

        arkime_field_ops_init(&readerFieldOps[readerPos], readerFilenameOpsNum, ARKIME_FIELD_OPS_FLAGS_COPY);

        // Go thru all the filename ops looking for matches and then expand the value string
        int i;
        for (i = 0; i < readerFilenameOpsNum; i++) {
            GMatchInfo *match_info = 0;
            g_regex_match(readerFilenameOps[i].regex, uri, 0, &match_info);
            if (g_match_info_matches(match_info)) {
                GError *error = 0;
                char *expand = g_match_info_expand_references(match_info, readerFilenameOps[i].expand, &error);
                if (error) {
                    LOG("Error expanding '%s' with '%s' - %s", uri, readerFilenameOps[i].expand, error->message);
                    g_error_free(error);
                }
                if (expand) {
                    arkime_field_ops_add(&readerFieldOps[readerPos], readerFilenameOps[i].field, expand, -1);
                    g_free(expand);
                }
            }
            g_match_info_free(match_info);
        }
    }

    arkime_packet_set_dltsnap(dlt, snaplen);

    if (config.bpf && pcapFileHeader.dlt != DLT_NFLOG) {
        if (deadPcap) {
            pcap_freecode(&bpf);
            pcap_close(deadPcap);
        }
        deadPcap = pcap_open_dead(pcapFileHeader.dlt, pcapFileHeader.snaplen);
        if (pcap_compile(deadPcap, &bpf, config.bpf, 1, PCAP_NETMASK_UNKNOWN) == -1) {
            CONFIGEXIT("Couldn't compile bpf filter: '%s' with %s", config.bpf, pcap_geterr(deadPcap));
        }
    }

    return 0;
}
/******************************************************************************/
LOCAL void *reader_scheme_thread(void *UNUSED(arg))
{
    ArkimeSchemeFlags flags = ARKIME_SCHEME_FLAG_NONE;
    if (config.pcapMonitor) {
        flags |= ARKIME_SCHEME_FLAG_MONITOR;
    }
    if (config.pcapRecursive) {
        flags |= ARKIME_SCHEME_FLAG_RECURSIVE;
    }
    if (config.pcapSkip) {
        flags |= ARKIME_SCHEME_FLAG_SKIP;
    }
    if (config.pcapDelete) {
        flags |= ARKIME_SCHEME_FLAG_DELETE;
    }

    // Load files
    for (int i = 0; config.pcapReadFiles && config.pcapReadFiles[i]; i++) {
        arkime_reader_scheme_load_thread(config.pcapReadFiles[i], flags, NULL);
    }

    // Load list of files
    for (int i = 0; config.pcapFileLists && config.pcapFileLists[i]; i++) {
        FILE *file;
        char line[PATH_MAX];

        if (strcmp(config.pcapFileLists[i], "-") == 0)
            file = stdin;
        else
            file = fopen(config.pcapFileLists[i], "r");
        if (!file) {
            LOG("ERROR - Couldn't open %s", config.pcapFileLists[i]);
            continue;
        }

        while (!feof(file)) {
            if (!fgets(line, sizeof(line), file)) {
                fclose(file);
                file = NULL;
                break;
            }

            int lineLen = strlen(line);
            if (line[lineLen - 1] == '\n') {
                line[lineLen - 1] = 0;
            }

            g_strstrip(line);
            if (!line[0] || line[0] == '#')
                continue;
            arkime_reader_scheme_load_thread(line, flags, NULL);
        }
        if (file) {
            fclose(file);
        }
    }

    for (int i = 0; config.pcapReadDirs && config.pcapReadDirs[i]; i++) {
        arkime_reader_scheme_load_thread(config.pcapReadDirs[i], flags | ARKIME_SCHEME_FLAG_DIRHINT, NULL);
    }

    while (config.commandWait || config.pcapMonitor || laterHead) {
        ARKIME_LOCK(laterLock);
        while (!laterHead) {
            ARKIME_COND_WAIT(laterLock);
        }
        ArkimeSchemeLater_t *item;
        item = laterHead;
        laterHead = item->next;
        ARKIME_UNLOCK(laterLock);
        arkime_reader_scheme_load_thread(item->uri, item->flags, item->actions);
        g_free(item->uri);
        reader_scheme_actions_deref(item->actions);
        ARKIME_TYPE_FREE(ArkimeSchemeLater_t, item);
    }

    arkime_quit();
    return NULL;
}

/******************************************************************************/
LOCAL void reader_scheme_start()
{
    g_thread_unref((schemeThread = g_thread_new("arkime-scheme", &reader_scheme_thread, NULL)));
}

/******************************************************************************/
LOCAL int reader_scheme_stats(ArkimeReaderStats_t *stats)
{
    stats->dropped = dropped;
    stats->total = totalPackets;
    return 0;
}
/******************************************************************************/
// Pause the reading thread if we are getting too far ahead of the processing
LOCAL void reader_scheme_pause()
{
    while (1) {
        // pause reading if too many waiting disk operations
        if (arkime_writer_queue_length() > 10) {
            if (config.debug) {
                static uint8_t msgcnt;
                if (msgcnt++ % 10 == 0)
                    LOG("Waiting to process more packets, write q: %u", arkime_writer_queue_length());
            }
            while (arkime_writer_queue_length() > 10) {
                usleep(5000);
            }
            continue;
        }

        // pause reading if too many waiting ES operations
        if (arkime_http_queue_length(esServer) > 30) {
            if (config.debug) {
                static uint8_t msgcnt;
                if (msgcnt++ % 10 == 0)
                    LOG("Waiting to process more packets, es q: %d", arkime_http_queue_length(esServer));
            }
            while (arkime_http_queue_length(esServer) > 30) {
                usleep(5000);
            }
            continue;
        }

        // pause reading if too many packets are waiting to be processed
        int m = config.maxPacketsInQueue - offlineDispatchAfter;
        if (arkime_packet_outstanding() > m) {
            while (arkime_packet_outstanding() > m) {
                usleep(5000);
            }
            continue;
        }
        break;
    }
}

/******************************************************************************/
ArkimePacket_t *packet;

SUPPRESS_ALIGNMENT
int arkime_reader_scheme_process(const char *uri, uint8_t *data, int len, const char *extraInfo, ArkimeSchemeAction_t *actions)
{
    ArkimePacketBatch_t   batch;
    arkime_packet_batch_init(&batch);

    reader_scheme_pause();

    // HACK: If state is 0 we haven't actually incremented readerPos yet, so do here
    if (state == 0) {
        offlineInfo[(readerPos + 1) & 0xff].lastBytes += len;
    } else {
        offlineInfo[readerPos].lastBytes += len;
    }

    while (len > 0) {
        if (state == ARKIME_SCHEME_FILEHEADER) {
            const uint8_t *header;
            if (tmpBufferLen == 0) {
                if (len < 24) {
                    memcpy(tmpBuffer, data, len);
                    tmpBufferLen = len;
                    return 0;
                }
                header = data;
                data += 24;
                len -= 24;
            } else {
                int need = 24 - tmpBufferLen;
                if (len < need) {
                    memcpy(tmpBuffer + tmpBufferLen, data, len);
                    tmpBufferLen += len;
                    return 0;
                }
                memcpy(tmpBuffer + tmpBufferLen, data, need);
                header = tmpBuffer;
                data += need;
                len -= need;
                tmpBufferLen = 0;
            }
            if (reader_scheme_header(uri, header, extraInfo, actions)) {
                tmpBufferLen = 0;
                return 1;
            }
            startPos = 24;
            state = ARKIME_SCHEME_PACKET_HEADER;
            continue;
        }
        if (state == ARKIME_SCHEME_PACKET_HEADER) {
            uint8_t *pheader;
            if (tmpBufferLen == 0) {
                if (len < 16) {
                    memcpy(tmpBuffer, data, len);
                    tmpBufferLen = len;
                    goto process;
                }
                pheader = data;
                data += 16;
                len -= 16;
            } else {
                int need = 16 - tmpBufferLen;
                if (len < need) {
                    memcpy(tmpBuffer + tmpBufferLen, data, len);
                    tmpBufferLen += len;
                    goto process;
                }
                memcpy(tmpBuffer + tmpBufferLen, data, need);
                pheader = tmpBuffer;
                data += need;
                len -= need;
                tmpBufferLen = 0;
            }
            state = ARKIME_SCHEME_PACKET;
            packet = ARKIME_TYPE_ALLOC0(ArkimePacket_t);
            struct arkime_pcap_sf_pkthdr *h = (struct arkime_pcap_sf_pkthdr *)pheader;
            if (unlikely(h->caplen != h->pktlen) && !config.readTruncatedPackets && !config.ignoreErrors) {
                LOGEXIT("ERROR - Arkime requires full packet captures caplen: %u pktlen: %u. "
                        "If using tcpdump use the \"-s0\" option, or set readTruncatedPackets in ini file",
                        needSwap ? SWAP32(h->caplen) : h->caplen,
                        needSwap ? SWAP32(h->pktlen) : h->pktlen);
            }
            if (needSwap) {
                pktlen = SWAP32(h->caplen);
                packet->ts.tv_sec = SWAP32(h->ts.tv_sec);
                packet->ts.tv_usec = SWAP32(h->ts.tv_usec);
            } else {
                pktlen = h->caplen;
                packet->ts.tv_sec = h->ts.tv_sec;
                packet->ts.tv_usec = h->ts.tv_usec;
            }

            if (nanosecond)
                packet->ts.tv_usec = packet->ts.tv_usec / 1000;

            packet->readerFilePos = startPos;
            packet->readerPos = readerPos;
            startPos += pktlen + 16;

            if (unlikely(pktlen > 0xffff)) {
                state = ARKIME_SCHEME_PACKET_SKIP;
            } else {
                packet->pktlen = pktlen;
            }
        }
        if (state == ARKIME_SCHEME_PACKET) {
            if (tmpBufferLen == 0) {
                if (len < pktlen) {
                    memcpy(tmpBuffer, data, len);
                    tmpBufferLen = len;
                    goto process;
                }
                packet->pkt = data;
                data += pktlen;
                len -= pktlen;
            } else {
                int need = pktlen - tmpBufferLen;
                if (len < need) {
                    memcpy(tmpBuffer + tmpBufferLen, data, len);
                    tmpBufferLen += len;
                    goto process;
                }
                memcpy(tmpBuffer + tmpBufferLen, data, need);
                packet->pkt = tmpBuffer;
                data += need;
                len -= need;
                tmpBufferLen = 0;
            }
            totalPackets++;
            offlineInfo[readerPos].lastPackets++;
            offlineInfo[readerPos].lastPacketTime = packet->ts;
            if (deadPcap && bpf_filter(bpf.bf_insns, packet->pkt, pktlen, pktlen)) {
                ARKIME_TYPE_FREE(ArkimePacket_t, packet);
            } else {
                arkime_packet_batch(&batch, packet);
            }
            packet = 0;
            state = ARKIME_SCHEME_PACKET_HEADER;
        }
        if (state == ARKIME_SCHEME_PACKET_SKIP) {
            ARKIME_TYPE_FREE(ArkimePacket_t, packet);
            packet = 0;
            if (len < pktlen) {
                data += len;
                pktlen -= len;
                len = 0;
                goto process;
            } else {
                data += pktlen;
                len -= pktlen;
                pktlen = 0;
                state = ARKIME_SCHEME_PACKET_HEADER;
            }
        }
    }
process:
    // Record if any packets were batched
    if (batch.count > 0) {
        offlineInfo[readerPos].didBatch = 1;
        arkime_packet_batch_flush(&batch);
    }
    return 0;
}
/******************************************************************************/
void arkime_reader_scheme_register(char *name, ArkimeSchemeLoad load, ArkimeSchemeExit exit)
{
    ArkimeScheme_t *readerScheme = ARKIME_TYPE_ALLOC0(ArkimeScheme_t);
    readerScheme->name = name;
    readerScheme->load = load;
    readerScheme->exit = exit;
    arkime_string_add(&schemesHash, name, readerScheme, TRUE);
    if (strcmp(name, "file") == 0) {
        fileScheme = readerScheme;
    }
}
/******************************************************************************/
LOCAL int arkime_scheme_cmd_add(int argc, char **argv, gpointer cc, ArkimeSchemeFlags flags)
{
    int opsNum = 0;
    char *ops[11];
    if (config.pcapMonitor) {
        flags |= ARKIME_SCHEME_FLAG_MONITOR;
    }
    if (config.pcapRecursive) {
        flags |= ARKIME_SCHEME_FLAG_RECURSIVE;
    }
    if (config.pcapSkip) {
        flags |= ARKIME_SCHEME_FLAG_SKIP;
    }
    if (config.pcapDelete) {
        flags |= ARKIME_SCHEME_FLAG_DELETE;
    }

    for (int i = 1; i < argc - 1; i++) {
        const char *cmp = argv[i];
        if (*cmp == '-' && cmp[1] == '-') {
            cmp++;
        }

        if (strcmp(cmp, "-monitor") == 0) {
            flags |= ARKIME_SCHEME_FLAG_MONITOR;
        } else if (strcmp(cmp, "-nomonitor") == 0) {
            flags &= (ArkimeSchemeFlags)(~ARKIME_SCHEME_FLAG_MONITOR);
        } else if (strcmp(cmp, "-recursive") == 0) {
            flags |= ARKIME_SCHEME_FLAG_RECURSIVE;
        } else if (strcmp(cmp, "-norecursive") == 0) {
            flags &= (ArkimeSchemeFlags)(~ARKIME_SCHEME_FLAG_RECURSIVE);
        } else if (strcmp(cmp, "-skip") == 0) {
            flags |= ARKIME_SCHEME_FLAG_SKIP;
        } else if (strcmp(cmp, "-noskip") == 0) {
            flags &= (ArkimeSchemeFlags)(~ARKIME_SCHEME_FLAG_SKIP);
        } else if (strcmp(cmp, "-delete") == 0) {
            flags |= ARKIME_SCHEME_FLAG_DELETE;
        } else if (strcmp(cmp, "-nodelete") == 0) {
            flags &= (ArkimeSchemeFlags)(~ARKIME_SCHEME_FLAG_DELETE);
        } else if (strcmp(cmp, "-op") == 0) {
            if (opsNum >= 10) {
                arkime_command_respond(cc, "Too many ops\n", -1);
                return 1;
            }
            if (i == argc - 1) {
                arkime_command_respond(cc, "Missing argument to -op\n", -1);
                return 1;
            }
            ops[opsNum++] = argv[++i];
        } else if (cmp[0] == '-') {
            char err[1000];
            snprintf(err, sizeof(err), "Unknown option %s\n", argv[i]);
            arkime_command_respond(cc, err, -1);
            return 1;
        }
    }

    ArkimeSchemeAction_t *actions = NULL;
    if (opsNum > 0) {
        actions = ARKIME_TYPE_ALLOC0(ArkimeSchemeAction_t);
        ops[opsNum] = 0;
        const char *error = arkime_field_ops_parse(&actions->ops, ARKIME_FIELD_OPS_FLAGS_COPY, ops);
        if (error) {
            ARKIME_TYPE_FREE(ArkimeSchemeAction_t, actions);
            arkime_command_respond(cc, error, -1);
            arkime_command_respond(cc, "\n", -1);
            return 1;
        }
    }

    arkime_reader_scheme_load(argv[argc - 1], flags, actions);
    return 0;
}

/******************************************************************************/
LOCAL void arkime_scheme_cmd_add_file(int argc, char **argv, gpointer cc)
{
    if (argc < 2) {
        arkime_command_respond(cc, "Usage: add-file [<file options>] <file>\n", -1);
        return;
    }

    if (!arkime_scheme_cmd_add(argc, argv, cc, ARKIME_SCHEME_FLAG_NONE))
        arkime_command_respond(cc, "Added file\n", -1);
}
/******************************************************************************/
LOCAL void arkime_scheme_cmd_add_dir(int argc, char **argv, gpointer cc)
{
    if (argc < 2) {
        arkime_command_respond(cc, "Usage: add-dir [<dir options>] [<file options>] <dir>\n", -1);
        return;
    }

    if (!arkime_scheme_cmd_add(argc, argv, cc, ARKIME_SCHEME_FLAG_DIRHINT))
        arkime_command_respond(cc, "Added directory\n", -1);
}
/******************************************************************************/
void arkime_reader_scheme_init()
{
    HASH_INIT(s_, schemesHash, arkime_string_hash, arkime_string_cmp);

    arkime_reader_start         = reader_scheme_start;
    arkime_reader_stats         = reader_scheme_stats;

    offlineDispatchAfter        = arkime_config_int(NULL, "offlineDispatchAfter", 2500, 1, 0x7fff);

    if (offlineDispatchAfter > (int)(config.maxPacketsInQueue + 1000)) {
        CONFIGEXIT("offlineDispatchAfter (%d) must be less than maxPacketsInQueue (%u) + 1000", offlineDispatchAfter, config.maxPacketsInQueue);
    }

    void arkime_reader_scheme_file_init();
    arkime_reader_scheme_file_init();

    void arkime_reader_scheme_http_init();
    arkime_reader_scheme_http_init();

    void arkime_reader_scheme_s3_init();
    arkime_reader_scheme_s3_init();

    void arkime_reader_scheme_sqs_init();
    arkime_reader_scheme_sqs_init();

    arkime_command_register_opts("add-file", arkime_scheme_cmd_add_file, "Add a file to process",
                                 "[--delete|--nodelete]", "Override command line delete files after processing",
                                 "[--op <field>=<value>]", "Can be multiple, override command line op option",
                                 "[--skip|--noskip]", "Override command line skip files already processed",
                                 "<file>", "File to process",
                                 NULL);
    arkime_command_register_opts("add-dir", arkime_scheme_cmd_add_dir, "Add a directory to process",
                                 "[--delete|--nodelete]", "Override command line delete files after processing",
                                 "[--op <field>=<value>]", "Can be multiple, override command line op option",
                                 "[--skip|--noskip]", "Override command line skip files already processed",
                                 "[--monitor|--nomonitor]", "Override command line monitor the directory for new files option",
                                 "[--recursive|--norecursive]", "Override command line Recurse sub directories option",
                                 "<dir>", "Directory to process",
                                 NULL);
}
