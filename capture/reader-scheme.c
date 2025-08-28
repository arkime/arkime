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
    ArkimeSchemeAction_t     *actions;
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

enum ArkimeSchemeMode {
    ARKIME_SCHEME_FILEHEADER,

    ARKIME_SCHEME_PACKET_HEADER,
    ARKIME_SCHEME_PACKET,
    ARKIME_SCHEME_PACKET_SKIP,

    ARKIME_SCHEME_NG_HEADER,
    ARKIME_SCHEME_NG_INTERFACE,
    ARKIME_SCHEME_NG_PACKET_HEADER,
    ARKIME_SCHEME_NG_PACKET,
    ARKIME_SCHEME_NG_SKIP
};

LOCAL int                    offlineDispatchAfter;
extern void                 *esServer;

extern ArkimeOfflineInfo_t   offlineInfo[256];

extern ArkimeFieldOps_t     readerFieldOps[256];
ArkimeSchemeAction_t       *schemeActions[256];
extern ArkimeFilenameOps_t  readerFilenameOps[256];
extern int                  readerFilenameOpsNum;

#define SWAP32(x) ((((x)&0xff000000) >> 24) | (((x)&0x00ff0000) >> 8) | (((x)&0x0000ff00) << 8) | (((x)&0x000000ff) << 24))
#define SWAP16(x) ((((x)&0xff00) >> 8) | (((x)&0x00ff) << 8))


LOCAL struct {
    int                    needSwap;
    int                    isNanosecond;
    int                    isPcapNG;
    int                    fileHeaderLen;
    uint64_t               startPos;
    uint64_t               nextStartPos;
    uint8_t                readerPos;
    enum ArkimeSchemeMode  state;
    ArkimePacket_t        *packet;
    int32_t                pktlen;
    uint8_t                tmpBuffer[0xffff];
    int                    tmpBufferLen;
    int                    blockSize;
    int                    haveInterface;
    uint64_t               tsresol;
} readerState;

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

    readerState.startPos = 0;
    readerState.state = ARKIME_SCHEME_FILEHEADER;
    readerState.tmpBufferLen = 0;
    readerState.fileHeaderLen = 24;
    readerState.isPcapNG = 0;
    readerState.haveInterface = -1;

    int rcl = readerScheme->load(uri, flags, actions);

    if (rcl == 0 && !config.dryRun && !config.copyPcap && offlineInfo[readerState.readerPos].didBatch) {
        arkime_packet_batch_end_of_file(readerState.readerPos);
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
LOCAL int reader_scheme_header_common(const char *uri, int dlt, int snaplen, const char *extraInfo, ArkimeSchemeAction_t *actions)
{
    readerState.readerPos++;
    // We've wrapped around all 256 reader items, clear the previous file information
    if (offlineInfo[readerState.readerPos].filename) {
        g_free(offlineInfo[readerState.readerPos].filename);
        g_free(offlineInfo[readerState.readerPos].extra);
        memset(&offlineInfo[readerState.readerPos], 0, sizeof(ArkimeOfflineInfo_t));
    }
    offlineInfo[readerState.readerPos].filename = g_strdup(uri);

    ArkimeScheme_t *readerScheme = uri2scheme(uri);
    offlineInfo[readerState.readerPos].scheme = readerScheme->name;
    offlineInfo[readerState.readerPos].extra = g_strdup(extraInfo);

    if (schemeActions[readerState.readerPos])
        reader_scheme_actions_deref(schemeActions[readerState.readerPos]);

    schemeActions[readerState.readerPos] = actions;
    reader_scheme_actions_ref(actions);

    if (readerFilenameOpsNum > 0) {
        // Free any previously allocated
        if (readerFieldOps[readerState.readerPos].size > 0)
            arkime_field_ops_free(&readerFieldOps[readerState.readerPos]);

        arkime_field_ops_init(&readerFieldOps[readerState.readerPos], readerFilenameOpsNum, ARKIME_FIELD_OPS_FLAGS_COPY);

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
                    arkime_field_ops_add(&readerFieldOps[readerState.readerPos], readerFilenameOps[i].field, expand, -1);
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

    readerState.needSwap = (h->magic == 0xd4c3b2a1 || h->magic == 0x4d3cb2a1);
    readerState.isNanosecond = (h->magic == 0xa1b23c4d || h->magic == 0x4d3cb2a1);

    uint32_t snaplen;
    uint32_t dlt;
    if (readerState.needSwap) {
        snaplen = SWAP32(h->snaplen);
        dlt = SWAP32(h->dlt);
    } else {
        snaplen = h->snaplen;
        dlt = h->dlt;
    }

    return reader_scheme_header_common(uri, dlt, snaplen, extraInfo, actions);
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
typedef struct {
    uint32_t block_type;
    uint32_t block_total_length;
} ArkimePcapNGBlockHeader_t;

SUPPRESS_ALIGNMENT
int arkime_reader_scheme_processNG(const char *uri, uint8_t *data, int len, const char *extraInfo, ArkimeSchemeAction_t *actions)
{
    ArkimePacketBatch_t   batch;
    arkime_packet_batch_init(&batch);

    reader_scheme_pause();

    // HACK: If state is 0 we haven't actually incremented readerPos yet, so do here
    if (readerState.state == 0) {
        offlineInfo[(readerState.readerPos + 1) & 0xff].lastBytes += len;
    } else {
        offlineInfo[readerState.readerPos].lastBytes += len;
    }

    int need;

    while (len > 0) {
        switch (readerState.state) {
        case ARKIME_SCHEME_FILEHEADER: {

            // Always copy header into tmpBuffer
            need = readerState.fileHeaderLen - readerState.tmpBufferLen;
            if (len < need) {
                memcpy(readerState.tmpBuffer + readerState.tmpBufferLen, data, len);
                readerState.tmpBufferLen += len;
                return 0;
            }
            memcpy(readerState.tmpBuffer + readerState.tmpBufferLen, data, need);
            readerState.tmpBufferLen += need;
            data += need;
            len -= need;

            ArkimePcapNGFileHdr_t *h = (ArkimePcapNGFileHdr_t *)readerState.tmpBuffer;

            readerState.needSwap = h->byte_order_magic != 0x1A2B3C4D;
            readerState.tsresol = 1000000; // default to microsecond resolution

            if (readerState.needSwap) {
                readerState.fileHeaderLen  = SWAP32(h->block_total_length);
            } else {
                readerState.fileHeaderLen = h->block_total_length;
            }

            if (readerState.tmpBufferLen < readerState.fileHeaderLen) {
                continue;
            }

            readerState.nextStartPos = readerState.fileHeaderLen;
            readerState.tmpBufferLen = 0;
            readerState.state = ARKIME_SCHEME_NG_HEADER;
            continue;
        }
        case ARKIME_SCHEME_NG_HEADER: {
            readerState.startPos = readerState.nextStartPos;
            need = 8 - readerState.tmpBufferLen;
            if (len < need) {
                memcpy(readerState.tmpBuffer + readerState.tmpBufferLen, data, len);
                readerState.tmpBufferLen += len;
                goto processNG;
            }

            memcpy(readerState.tmpBuffer + readerState.tmpBufferLen, data, need);
            data += need;
            len -= need;

            ArkimePcapNGBlockHeader_t *blockHeader = (ArkimePcapNGBlockHeader_t *)readerState.tmpBuffer;
            if (readerState.needSwap) {
                blockHeader->block_type = SWAP32(blockHeader->block_type);
                blockHeader->block_total_length = SWAP32(blockHeader->block_total_length);
            }
            readerState.tmpBufferLen = 0;
            readerState.blockSize = blockHeader->block_total_length - 8;

            readerState.nextStartPos = readerState.startPos + blockHeader->block_total_length;
            if (blockHeader->block_type == 6) {
                readerState.state = ARKIME_SCHEME_NG_PACKET_HEADER;
            } else if (blockHeader->block_type == 1) {
                readerState.state = ARKIME_SCHEME_NG_INTERFACE;
            } else {
                readerState.state = ARKIME_SCHEME_NG_SKIP;
            }
            continue;
        }
        case ARKIME_SCHEME_NG_INTERFACE: {
            need = readerState.blockSize - readerState.tmpBufferLen;
            if (len < need) {
                memcpy(readerState.tmpBuffer + readerState.tmpBufferLen, data, len);
                readerState.blockSize -= len;
                readerState.tmpBufferLen += len;
                goto processNG;
            }

            memcpy(readerState.tmpBuffer + readerState.tmpBufferLen, data, need);
            readerState.blockSize -= need;
            data += need;
            len -= need;
            readerState.tmpBufferLen += need;

            uint16_t linkType;
            uint32_t snaplen;
            memcpy(&linkType, readerState.tmpBuffer, sizeof(linkType));
            memcpy(&snaplen, readerState.tmpBuffer + 4, sizeof(snaplen));
            if (readerState.needSwap) {
                linkType = SWAP16(linkType);
                snaplen = SWAP32(snaplen);
            }

            // ALW TODO: Currently we don't support multiple different interface linktypes
            if (readerState.haveInterface != -1 && readerState.haveInterface != linkType) {
                LOG("ERROR - Multiple interfaces in pcapNG file '%s', not supported", uri);
                return 1;
            }

            uint8_t *options = readerState.tmpBuffer + 8;
            uint8_t *optionsEnd = options + readerState.tmpBufferLen - 8;

            while (options + 4 <= optionsEnd) {
                uint16_t type = 0, len = 0;
                memcpy(&type, options, 2);
                options += 2;

                memcpy(&len, options, 2);
                options += 2;

                if (type == 0 && len == 0) {
                    break; // end of options
                }

                if (type == 9) {
                    if (options[0] & 0x80) {
                        readerState.tsresol = 1LL << (options[0] & 0x7F);
                    } else {
                        readerState.tsresol = 1;
                        for (int i = 0; i < options[0]; i++)
                            readerState.tsresol *= 10;
                    }
                }

                if (readerState.needSwap) {
                    type = SWAP16(type);
                    len = SWAP16(len);
                }

                options += len;
                options += (4 - (len & 3)) & 3; // align to 32 bits
            }


            readerState.haveInterface = linkType;

            reader_scheme_header_common(uri, arkime_packet_linktype_to_dlt(linkType), snaplen, extraInfo, actions);

            readerState.state = ARKIME_SCHEME_NG_HEADER;
            readerState.tmpBufferLen = 0;
            continue;
        }
        case ARKIME_SCHEME_NG_PACKET_HEADER: {
            need = 20 - readerState.tmpBufferLen;
            if (len < need) {
                memcpy(readerState.tmpBuffer + readerState.tmpBufferLen, data, len);
                readerState.tmpBufferLen += len;
                readerState.blockSize -= len;
                return 0;
            }

            memcpy(readerState.tmpBuffer + readerState.tmpBufferLen, data, need);
            data += need;
            len -= need;
            readerState.blockSize -= need;

            memcpy(&readerState.pktlen, readerState.tmpBuffer + 12, sizeof(readerState.pktlen));
            if (readerState.needSwap) {
                readerState.pktlen = SWAP32(readerState.pktlen);
            }

            if (unlikely(readerState.pktlen > 0xffff)) {
                readerState.state = ARKIME_SCHEME_NG_SKIP;
                continue;
            }

            readerState.packet = ARKIME_TYPE_ALLOC0(ArkimePacket_t);
            readerState.packet->pktlen = readerState.pktlen;
            readerState.packet->readerFilePos = readerState.startPos;

            uint32_t tsh, tsl;
            memcpy(&tsh, readerState.tmpBuffer + 4, 4);
            memcpy(&tsl, readerState.tmpBuffer + 8, 4);

            if (readerState.needSwap) {
                tsh = SWAP32(tsh);
                tsl = SWAP32(tsl);
            }

            uint64_t ts = ((uint64_t)tsh << 32) | tsl;

            readerState.packet->ts.tv_sec = ts / readerState.tsresol;
            readerState.packet->ts.tv_usec = (ts % readerState.tsresol) * 1000000 / readerState.tsresol;

            readerState.tmpBufferLen = 0;
            readerState.state = ARKIME_SCHEME_NG_PACKET;
            continue;
        }
        case ARKIME_SCHEME_NG_PACKET: {
            if (readerState.tmpBufferLen == 0) {
                if (len < readerState.pktlen) {
                    memcpy(readerState.tmpBuffer, data, len);
                    readerState.tmpBufferLen = len;
                    goto processNG;
                }
                readerState.packet->pkt = data;
                data += readerState.pktlen;
                len -= readerState.pktlen;
                readerState.blockSize -= readerState.pktlen;
            } else {
                int need = readerState.pktlen - readerState.tmpBufferLen;
                if (len < need) {
                    memcpy(readerState.tmpBuffer + readerState.tmpBufferLen, data, len);
                    readerState.tmpBufferLen += len;
                    goto processNG;
                }
                memcpy(readerState.tmpBuffer + readerState.tmpBufferLen, data, need);
                readerState.packet->pkt = readerState.tmpBuffer;
                data += need;
                len -= need;
                readerState.blockSize -= need;
                readerState.tmpBufferLen = 0;
            }
            totalPackets++;
            offlineInfo[readerState.readerPos].lastPackets++;
            offlineInfo[readerState.readerPos].lastPacketTime = readerState.packet->ts;
            if (deadPcap && bpf_filter(bpf.bf_insns, readerState.packet->pkt, readerState.pktlen, readerState.pktlen)) {
                ARKIME_TYPE_FREE(ArkimePacket_t, readerState.packet);
            } else {
                arkime_packet_batch(&batch, readerState.packet);
            }
            readerState.packet = 0;
            readerState.state = ARKIME_SCHEME_NG_SKIP; // skip options and 2nd block length
            continue;
        }
        case ARKIME_SCHEME_NG_SKIP: {
            if (len < readerState.blockSize) {
                readerState.blockSize -= len;
                goto processNG;
            } else {
                data += readerState.blockSize;
                len -= readerState.blockSize;
                readerState.state = ARKIME_SCHEME_NG_HEADER;
            }
            continue;
        }
        default:
            LOGEXIT("ERROR - Unknown readerState %d", readerState.state);
        } /* switch */
    } /* while */

processNG:
    // Record if any packets were batched
    if (batch.count > 0) {
        offlineInfo[readerState.readerPos].didBatch = 1;
        arkime_packet_batch_flush(&batch);
    }
    return 0;
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
int arkime_reader_scheme_process(const char *uri, uint8_t *data, int len, const char *extraInfo, ArkimeSchemeAction_t *actions)
{
    if (readerState.isPcapNG) {
        return arkime_reader_scheme_processNG(uri, data, len, extraInfo, actions);
    }

    ArkimePacketBatch_t   batch;
    arkime_packet_batch_init(&batch);

    reader_scheme_pause();

    // HACK: If state is 0 we haven't actually incremented readerPos yet, so do here
    if (readerState.state == 0) {
        offlineInfo[(readerState.readerPos + 1) & 0xff].lastBytes += len;
    } else {
        offlineInfo[readerState.readerPos].lastBytes += len;
    }

    while (len > 0) {
        if (readerState.state == ARKIME_SCHEME_FILEHEADER) {
            const uint8_t *header;
            if (readerState.tmpBufferLen == 0) {
                if (len < readerState.fileHeaderLen) {
                    memcpy(readerState.tmpBuffer, data, len);
                    readerState.tmpBufferLen = len;
                    return 0;
                }
                header = data;

                if (memcmp(header, "\x0a\x0d\x0d\x0a", 4) == 0) {
                    readerState.isPcapNG = 1;
                    return arkime_reader_scheme_processNG(uri, data, len, extraInfo, actions);
                }

                data += readerState.fileHeaderLen;
                len -= readerState.fileHeaderLen;
            } else {
                int need = readerState.fileHeaderLen - readerState.tmpBufferLen;
                if (len < need) {
                    memcpy(readerState.tmpBuffer + readerState.tmpBufferLen, data, len);
                    readerState.tmpBufferLen += len;
                    return 0;
                }
                memcpy(readerState.tmpBuffer + readerState.tmpBufferLen, data, need);
                header = readerState.tmpBuffer;

                data += need;
                len -= need;

                if (memcmp(header, "\x0a\x0d\x0d\x0a", 4) == 0) {
                    readerState.isPcapNG = 1;
                    return arkime_reader_scheme_processNG(uri, data, len, extraInfo, actions);
                }
                readerState.tmpBufferLen = 0;
            }
            if (reader_scheme_header(uri, header, extraInfo, actions)) {
                readerState.tmpBufferLen = 0;
                return 1;
            }
            readerState.startPos = readerState.fileHeaderLen;
            readerState.state = ARKIME_SCHEME_PACKET_HEADER;
            continue;
        }
        if (readerState.state == ARKIME_SCHEME_PACKET_HEADER) {
            uint8_t *pheader;
            if (readerState.tmpBufferLen == 0) {
                if (len < 16) {
                    memcpy(readerState.tmpBuffer, data, len);
                    readerState.tmpBufferLen = len;
                    goto process;
                }
                pheader = data;
                data += 16;
                len -= 16;
            } else {
                int need = 16 - readerState.tmpBufferLen;
                if (len < need) {
                    memcpy(readerState.tmpBuffer + readerState.tmpBufferLen, data, len);
                    readerState.tmpBufferLen += len;
                    goto process;
                }
                memcpy(readerState.tmpBuffer + readerState.tmpBufferLen, data, need);
                pheader = readerState.tmpBuffer;
                data += need;
                len -= need;
                readerState.tmpBufferLen = 0;
            }
            readerState.state = ARKIME_SCHEME_PACKET;
            readerState.packet = ARKIME_TYPE_ALLOC0(ArkimePacket_t);
            struct arkime_pcap_sf_pkthdr *h = (struct arkime_pcap_sf_pkthdr *)pheader;
            if (unlikely(h->caplen != h->pktlen) && !config.readTruncatedPackets && !config.ignoreErrors) {
                LOGEXIT("ERROR - Arkime requires full packet captures caplen: %u pktlen: %u. "
                        "If using tcpdump use the \"-s0\" option, or set readTruncatedPackets in ini file",
                        readerState.needSwap ? SWAP32(h->caplen) : h->caplen,
                        readerState.needSwap ? SWAP32(h->pktlen) : h->pktlen);
            }
            if (readerState.needSwap) {
                readerState.pktlen = SWAP32(h->caplen);
                readerState.packet->ts.tv_sec = SWAP32(h->ts.tv_sec);
                readerState.packet->ts.tv_usec = SWAP32(h->ts.tv_usec);
            } else {
                readerState.pktlen = h->caplen;
                readerState.packet->ts.tv_sec = h->ts.tv_sec;
                readerState.packet->ts.tv_usec = h->ts.tv_usec;
            }

            if (readerState.isNanosecond)
                readerState.packet->ts.tv_usec = readerState.packet->ts.tv_usec / 1000;

            readerState.packet->readerFilePos = readerState.startPos;
            readerState.packet->readerPos = readerState.readerPos;
            readerState.startPos += readerState.pktlen + 16;

            if (unlikely(readerState.pktlen > 0xffff)) {
                readerState.state = ARKIME_SCHEME_PACKET_SKIP;
            } else {
                readerState.packet->pktlen = readerState.pktlen;
            }
        }
        if (readerState.state == ARKIME_SCHEME_PACKET) {
            if (readerState.tmpBufferLen == 0) {
                if (len < readerState.pktlen) {
                    memcpy(readerState.tmpBuffer, data, len);
                    readerState.tmpBufferLen = len;
                    goto process;
                }
                readerState.packet->pkt = data;
                data += readerState.pktlen;
                len -= readerState.pktlen;
            } else {
                int need = readerState.pktlen - readerState.tmpBufferLen;
                if (len < need) {
                    memcpy(readerState.tmpBuffer + readerState.tmpBufferLen, data, len);
                    readerState.tmpBufferLen += len;
                    goto process;
                }
                memcpy(readerState.tmpBuffer + readerState.tmpBufferLen, data, need);
                readerState.packet->pkt = readerState.tmpBuffer;
                data += need;
                len -= need;
                readerState.tmpBufferLen = 0;
            }
            totalPackets++;
            offlineInfo[readerState.readerPos].lastPackets++;
            offlineInfo[readerState.readerPos].lastPacketTime = readerState.packet->ts;
            if (deadPcap && bpf_filter(bpf.bf_insns, readerState.packet->pkt, readerState.pktlen, readerState.pktlen)) {
                ARKIME_TYPE_FREE(ArkimePacket_t, readerState.packet);
            } else {
                arkime_packet_batch(&batch, readerState.packet);
            }
            readerState.packet = 0;
            readerState.state = ARKIME_SCHEME_PACKET_HEADER;
        }
        if (readerState.state == ARKIME_SCHEME_PACKET_SKIP) {
            ARKIME_TYPE_FREE(ArkimePacket_t, readerState.packet);
            readerState.packet = 0;
            if (len < readerState.pktlen) {
                data += len;
                readerState.pktlen -= len;
                len = 0;
                goto process;
            } else {
                data += readerState.pktlen;
                len -= readerState.pktlen;
                readerState.pktlen = 0;
                readerState.state = ARKIME_SCHEME_PACKET_HEADER;
            }
        }
    }
process:
    // Record if any packets were batched
    if (batch.count > 0) {
        offlineInfo[readerState.readerPos].didBatch = 1;
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
