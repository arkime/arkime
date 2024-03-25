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

LOCAL  ArkimeStringHashStd_t  schemesHash;
LOCAL  ArkimeScheme_t        *fileScheme;

LOCAL uint64_t totalPackets;
LOCAL uint64_t dropped;

LOCAL uint64_t lastBytes;
LOCAL uint64_t lastPackets;

LOCAL int state = 0;
LOCAL uint8_t tmpBuffer[0xffff];
LOCAL uint32_t tmpBufferLen;

LOCAL int                   offlineDispatchAfter;
extern void                *esServer;

extern ArkimeOfflineInfo_t  offlineInfo[256];

extern ArkimeFieldOps_t     readerFieldOps[256];
extern ArkimeFilenameOps_t  readerFilenameOps[256];
extern int                  readerFilenameOpsNum;

#define SWAP32(x) ((((x)&0xff000000) >> 24) | (((x)&0x00ff0000) >> 8) | (((x)&0x0000ff00) << 8) | (((x)&0x000000ff) << 24))
#define SWAP16(x) ((((x)&0xff00) >> 8) | (((x)&0x00ff) << 8))

LOCAL uint64_t             startPos;
LOCAL uint8_t              readerPos;
LOCAL int                  needSwap;
LOCAL int                  nanosecond;

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
void arkime_reader_scheme_load(const char *uri)
{
    LOG ("Processing %s", uri);
    ArkimeScheme_t *readerScheme = uri2scheme(uri);
    if (!readerScheme) {
        LOG("ERROR - Unknown scheme for %s", uri);
        return;
    }

    if (config.flushBetween) {
        arkime_session_flush();
        int rc[4];

        // Pause until all packets and commands are done
        while ((rc[0] = arkime_session_cmd_outstanding()) + (rc[1] = arkime_session_close_outstanding()) + (rc[2] = arkime_packet_outstanding()) + (rc[3] = arkime_session_monitoring()) > 0) {
            if (config.debug) {
                LOG("Waiting next file %d %d %d %d", rc[0], rc[1], rc[2], rc[3]);
            }
            usleep(5000);
        }
    }

    startPos = 0;
    state = 0;
    lastBytes = 0;
    lastPackets = 0;

    int rc = readerScheme->load(uri);

    if (rc == 0 && !config.dryRun && !config.copyPcap) {
        // Wait for the first packet to be processed so we have an outputId
        while (offlineInfo[readerPos].outputId == 0 || arkime_http_queue_length_best(esServer) > 0) {
            usleep(5000);
        }
        arkime_db_update_filesize(offlineInfo[readerPos].outputId, lastBytes, lastBytes, lastPackets);
    }
}
/******************************************************************************/
LOCAL int reader_scheme_header(const char *uri, const uint8_t *header, const char *extraInfo)
{
    ArkimePcapFileHdr_t *h = (ArkimePcapFileHdr_t *)header;
    if (h->magic != 0xa1b2c3d4 && h->magic != 0xd4c3b2a1 &&
        h->magic != 0xa1b23c4d && h->magic != 0x4d3cb2a1) {

        if (config.ignoreErrors) {
            LOG("ERROR - Unknown magic %x in %s", h->magic, uri);
            return 1;
        } else {
            LOGEXIT("ERROR - Unknown magic %x in %s", h->magic, uri);
        }
    }

    needSwap = (h->magic == 0xd4c3b2a1 || h->magic == 0x4d3cb2a1);
    nanosecond = (h->magic == 0xa1b23c4d || h->magic == 0x4d3cb2a1);

    if (needSwap) {
        h->snaplen = SWAP32(h->snaplen);
        h->dlt = SWAP32(h->dlt);
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

    arkime_packet_set_dltsnap(h->dlt, h->snaplen);

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

    // Load files
    for (int i = 0; config.pcapReadFiles && config.pcapReadFiles[i]; i++) {
        arkime_reader_scheme_load(config.pcapReadFiles[i]);
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
                break;
            }

            int lineLen = strlen(line);
            if (line[lineLen - 1] == '\n') {
                line[lineLen - 1] = 0;
            }

            g_strstrip(line);
            if (!line[0] || line[0] == '#')
                continue;
            arkime_reader_scheme_load(line);
        }
        fclose(file);
    }

    for (int i = 0; config.pcapReadDirs && config.pcapReadDirs[i]; i++) {
        arkime_reader_scheme_load(config.pcapReadDirs[i]);
    }

    arkime_quit();
    return NULL;
}

/******************************************************************************/
LOCAL void reader_scheme_start()
{
    g_thread_unref(g_thread_new("arkime-scheme", &reader_scheme_thread, NULL));
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
int arkime_reader_scheme_process(const char *uri, uint8_t *data, int len, char *extraInfo)
{
    ArkimePacketBatch_t   batch;
    arkime_packet_batch_init(&batch);

    reader_scheme_pause();
    lastBytes += len;

    while (len > 0) {
        if (state == 0) {
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
            if (reader_scheme_header(uri, header, extraInfo)) {
                tmpBufferLen = 0;
                return 1;
            }
            startPos = 24;
            state = 1;
            continue;
        }
        if (state == 1) {
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
            state = 2;
            packet = ARKIME_TYPE_ALLOC0(ArkimePacket_t);
            struct arkime_pcap_sf_pkthdr *h = (struct arkime_pcap_sf_pkthdr *)pheader;
            if (unlikely(h->caplen != h->pktlen) && !config.readTruncatedPackets && !config.ignoreErrors) {
                LOGEXIT("ERROR - Arkime requires full packet captures caplen: %u pktlen: %u. "
                        "If using tcpdump use the \"-s0\" option, or set readTruncatedPackets in ini file",
                        needSwap ? SWAP32(h->caplen) : h->caplen,
                        needSwap ? SWAP32(h->pktlen) : h->pktlen);
            }
            if (needSwap) {
                packet->pktlen = SWAP32(h->caplen);
                packet->ts.tv_sec = SWAP32(h->ts.tv_sec);
                packet->ts.tv_usec = SWAP32(h->ts.tv_usec);
            } else {
                packet->pktlen = h->caplen;
                packet->ts.tv_sec = h->ts.tv_sec;
                packet->ts.tv_usec = h->ts.tv_usec;
            }
            if (nanosecond)
                packet->ts.tv_usec = packet->ts.tv_usec / 1000;

            packet->readerFilePos = startPos;
            packet->readerPos = readerPos;
            startPos += packet->pktlen + 16;
        }
        if (state == 2) {
            if (tmpBufferLen == 0) {
                if (len < packet->pktlen) {
                    memcpy(tmpBuffer, data, len);
                    tmpBufferLen = len;
                    goto process;
                }
                packet->pkt = data;
                data += packet->pktlen;
                len -= packet->pktlen;
            } else {
                int need = packet->pktlen - tmpBufferLen;
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
            lastPackets++;
            if (deadPcap && bpf_filter(bpf.bf_insns, packet->pkt, packet->pktlen, packet->pktlen)) {
                ARKIME_TYPE_FREE(ArkimePacket_t, packet);
            } else {
                arkime_packet_batch(&batch, packet);
            }
            packet = 0;
            state = 1;
        }
    }
process:
    arkime_packet_batch_flush(&batch);
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
}
