/******************************************************************************/
/* reader-napatech.c -- Arkime packet reader using Napatech NTAPI
 *
 * Supports: NT200A02 (and other Napatech adapters)
 * Requires: Napatech Link-Capture Software (ntanl3), libntapi
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Arkime config.ini settings:
 *   pcapReadMethod   = napatech
 *   ntStreamIds       = 0-7          (comma-separated stream ID ranges to open, e.g. "0-3,5,8-10";
 *                                     default: auto-detect from driver via NT_INFO_CMD_READ_STREAM)
 *   ntplFile          = /path/to/arkime-streams.ntpl  (optional: apply NTPL at startup)
 *
 * NTPL must be applied before starting Arkime, or set ntplFile= to apply automatically.
 * Assign rules must set TimestampType = UNIX_NANOTIME and HashMode = Hash5TupleSorted.
 *****************************************************************************/

#define _FILE_OFFSET_BITS 64
#include "arkime.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Napatech NTAPI headers - installed by libntapi-dev package */
#include <nt.h>
#include "pcap.h"          /* bpf_filter, pcap_compile, pcap_open_dead */

extern ArkimeConfig_t config;

LOCAL int ntThreadNum;

/* -------------------------------------------------------------------------
 * Module-level state
 * ---------------------------------------------------------------------- */

typedef struct {
    NtNetStreamRx_t  hStream;        /* NTAPI stream handle            */
    int              streamId;       /* NTAPI stream ID (0-based)      */
    uint8_t          interfacePos;   /* Arkime slot index (0-based)    */
} NtStream_t;

LOCAL NtStream_t   *ntStreams    = NULL;  /* heap-allocated; size = ntNumStreams */
LOCAL int           ntNumStreams = 0;

/* Stats — indexed by slot (interfacePos), not by NTAPI stream ID */
LOCAL uint64_t     *ntTotalPkts  = NULL;  /* heap-allocated; size = ntNumStreams */
LOCAL uint64_t      ntDropped ARKIME_CACHE_ALIGN;

/* Software BPF filter (compiled from config.bpf at init time) */
LOCAL struct bpf_program  sw_bpf;
LOCAL gboolean            use_sw_bpf;

/* -------------------------------------------------------------------------
 * Timestamp helpers
 * The NTPL in arkime-streams.ntpl sets TimestampType = UNIX_NANOTIME.
 * NT_NET_GET_PKT_TIMESTAMP() therefore returns nanoseconds since Unix epoch.
 * ---------------------------------------------------------------------- */

static inline void nt_ts_to_timeval(uint64_t ts_ns, struct timeval *tv)
{
    tv->tv_sec  = (time_t)(ts_ns / 1000000000ULL);
    tv->tv_usec = (suseconds_t)((ts_ns % 1000000000ULL) / 1000ULL);
}

/* -------------------------------------------------------------------------
 * Stats callback
 * ---------------------------------------------------------------------- */

LOCAL int reader_napatech_stats(ArkimeReaderStats_t *stats)
{
    stats->total   = 0;
    stats->dropped = ntDropped;
    for (int s = 0; s < ntNumStreams; s++) {
        stats->total += ntTotalPkts[s];
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * Per-stream capture thread
 * ---------------------------------------------------------------------- */

LOCAL void *reader_napatech_thread(gpointer streamv)
{
    NtStream_t *st = (NtStream_t *)streamv;
    NtNetBuf_t  hSegBuf;     /* handle to one segment (N packets) */

    int threadNum = ARKIME_THREAD_INCROLD(ntThreadNum);
    int initFunc = arkime_get_named_func("arkime_reader_thread_init");
    arkime_call_named_func(initFunc, threadNum, NULL);

    ArkimePacketBatch_t batch;
    arkime_packet_batch_init(&batch);

    while (!config.quitting) {
        /* 100ms timeout so we can check config.quitting */
        int status = NT_NetRxGet(st->hStream, &hSegBuf, 100);

        if (unlikely(status == NT_STATUS_TIMEOUT || status == NT_STATUS_TRYAGAIN)) {
            continue;
        }

        if (unlikely(status != NT_SUCCESS)) {
            char errBuf[256];
            NT_ExplainError(status, errBuf, sizeof(errBuf));
            LOG("Napatech stream %d error: %s", st->streamId, errBuf);
            arkime_quit();
            break;
        }

        /* ------ Walk all packets in this segment (zero-copy) ------------ *
         *                                                                  *
         * NT_NetRxGet in segment mode returns a contiguous buffer          *
         * containing back-to-back packet records. Each record starts with  *
         * a NtStd0Descr_t (16-byte standard descriptor from                *
         * pktdescr_std0.h), followed immediately by the L2 frame bytes,    *
         * then 0-7 bytes of 8-byte-alignment padding.                      *
         *                                                                  *
         * NtStd0Descr_t layout (128 bits, confirmed from SDK headers):     *
         *   [63:0]  timestamp   — 64-bit Unix nanoseconds (UNIX_NANOTIME)  *
         *   [79:64] storedLength:16 — total record bytes (stride), incl.   *
         *                            descriptor + payload + padding        *
         *   [95:80] (bit fields: crcError, checksums, rxPort, etc.)        *
         *   [111:96] wireLength:16 — original on-wire frame length         *
         *   [127:112] (bit fields: txPort, flags, extensionLength:3, ...)  *
         *                                                                  *
         * We set packet->copied = 0 and point packet->pkt directly into    *
         * the segment buffer (zero-copy). The segment is NOT released      *
         * until after arkime_packet_batch_flush(). Arkime's async packet   *
         * threads will copy each packet (packet.c ~line 1556) before the   *
         * FPGA can rotate the host buffer back around (~1.6 s at           *
         * 10 Gbps), matching the safety model used by tpacketv3 blocks.    *
         * ---------------------------------------------------------------- */
        uint64_t segLen  = NT_NET_GET_SEGMENT_LENGTH(hSegBuf);
        uint8_t *segBase = (uint8_t *)NT_NET_GET_SEGMENT_PTR(hSegBuf);
        const uint8_t *segEnd  = segBase + segLen;

        for (uint8_t *p = segBase; p < segEnd;) {
            const NtStd0Descr_t *d = (const NtStd0Descr_t *)p;

            /* storedLength is in BYTES (already 8-byte-aligned) and is the
             * full record stride: descriptor + captured payload + padding.
             * Confirmed from pktdescr_std0.h setter:
             *   storedLength = (descrLength + capLength + 7) & ~7            */
            uint32_t  recSize = d->storedLength;

            if (unlikely(recSize == 0)) {
                /* Corrupt segment — stop walking to avoid infinite loop */
                LOG("Napatech stream %d: zero storedLength in segment, aborting walk",
                    st->streamId);
                break;
            }

            /* Descriptor length: 16-byte fixed header + optional extension
             * (extensionLength is in 8-byte units, per pktdescr_std0.h).    */
            uint16_t  descrLen = (uint16_t)sizeof(NtStd0Descr_t) +
                                 ((uint16_t)d->extensionLength << 3);
            uint8_t  *l2       = p + descrLen;

            /* wireLength = original frame bytes on the wire.
             * Captured payload may include 0-7 padding bytes due to alignment;
             * take the minimum so Arkime never sees garbage padding.          */
            uint16_t  wireLen  = d->wireLength;
            uint16_t  payLen   = (recSize > descrLen) ? (recSize - descrLen) : 0;
            uint16_t  capLen   = (payLen < wireLen) ? payLen : wireLen;
            uint64_t  ts      = d->timestamp;

            /* Software BPF filter — applied before allocation, so dropped
             * packets cost only this check with no malloc. */
            if (use_sw_bpf && bpf_filter(sw_bpf.bf_insns, l2, wireLen, capLen) == 0) {
                ARKIME_THREAD_INCR(ntDropped);
                p += recSize;
                continue;
            }

            if (unlikely(capLen != wireLen) &&
                !config.readTruncatedPackets && !config.ignoreErrors) {
                LOGEXIT("ERROR - Arkime requires full packet captures caplen: %d pktlen: %d\n"
                        "See https://arkime.com/faq#arkime_requires_full_packet_captures_error",
                        capLen, wireLen);
            }

            ArkimePacket_t *packet = ARKIME_TYPE_ALLOC0(ArkimePacket_t);
            packet->pkt       = l2;   /* zero-copy: pointer into segment buffer */
            packet->pktlen    = capLen;
            packet->readerPos = st->interfacePos;
            packet->copied    = 0;    /* Arkime will copy before freeing; segment
                                         held open until NT_NetRxRelease below */

            /* Hardware timestamp — nanosecond precision, no software jitter */
            nt_ts_to_timeval(ts, &packet->ts);

            /* VLANs preserved in wire order (no VLANStrip in NTPL) */

            ARKIME_THREAD_INCR(ntTotalPkts[st->interfacePos]);

            arkime_packet_batch(&batch, packet);

            p += recSize;
        }

        /* All packets in this segment have been batched. Flush to the
         * per-thread queues, then release the segment back to the FPGA.
         * The packet threads will copy asynchronously; the 2 GB host buffer
         * ring ensures the segment memory stays valid long enough. */
        arkime_packet_batch_flush(&batch);
        NT_NetRxRelease(st->hStream, hSegBuf);
    }

    arkime_packet_batch_flush(&batch);

    int exitFunc = arkime_get_named_func("arkime_reader_thread_exit");
    arkime_call_named_func(exitFunc, threadNum, NULL);
    return NULL;
}

/* -------------------------------------------------------------------------
 * Start: called after privilege drop — NT_Init and streams are already open
 * ---------------------------------------------------------------------- */

LOCAL void reader_napatech_start()
{
    /* NT_Init() and NT_NetRxOpen() were called in reader_napatech_init()
     * (which runs as root, before Arkime drops privileges to dropUser).
     * By the time start() is called, we are already running as nobody and
     * /dev/nt3gd is no longer accessible, so NTAPI calls that open the
     * device must NOT be placed here.
     *
     * All that remains is to tell Arkime the link type and spawn threads. */

    /* Tell Arkime the link type and snap length */
    for (int i = 0; config.interface[i]; i++)
        arkime_packet_set_interface(i, 0, DLT_EN10MB, config.snapLen);

    LOG("Napatech: starting %d capture thread(s)", ntNumStreams);
    for (int s = 0; s < ntNumStreams; s++) {
        char tname[64];
        snprintf(tname, sizeof(tname), "arkime-nt-stream%d", s);
        g_thread_unref(g_thread_new(tname, reader_napatech_thread, &ntStreams[s]));
    }
}

/* -------------------------------------------------------------------------
 * Stop / Exit
 * ---------------------------------------------------------------------- */

LOCAL void reader_napatech_stop()
{
    /* config.quitting is already set by caller; threads will drain */
}

LOCAL void reader_napatech_exit()
{
    for (int s = 0; s < ntNumStreams; s++) {
        if (ntStreams[s].hStream) {
            NT_NetRxClose(ntStreams[s].hStream);
            ntStreams[s].hStream = 0;
        }
    }
    g_free(ntStreams);
    ntStreams   = NULL;
    g_free(ntTotalPkts);
    ntTotalPkts = NULL;
    ntNumStreams = 0;
    if (use_sw_bpf)
        pcap_freecode(&sw_bpf);
    NT_Done();
}

/* -------------------------------------------------------------------------
 * Stream ID resolution
 * ---------------------------------------------------------------------- */

/* Parse a range string like "0-3,5,8-10" into a heap-allocated int[].
 * Sets *ids_out and *count_out.  Returns 0 on success, -1 on error. */
static int reader_napatech_parse_stream_range(const char *str,
                                              int **ids_out, int *count_out)
{
    int  capacity = 32;
    int *ids = g_malloc(capacity * sizeof(int));
    int  count = 0;
    const char *p = str;

    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        char *end;
        long lo = strtol(p, &end, 10);
        if (end == p || lo < 0 || lo > 255) {
            g_free(ids);
            return -1;
        }
        p = end;
        while (*p == ' ' || *p == '\t') p++;

        long hi = lo;
        if (*p == '-') {
            p++;
            while (*p == ' ' || *p == '\t') p++;
            hi = strtol(p, &end, 10);
            if (end == p || hi < lo || hi > 255) {
                g_free(ids);
                return -1;
            }
            p = end;
        }
        while (*p == ' ' || *p == '\t') p++;

        for (long s = lo; s <= hi; s++) {
            if (count == capacity) {
                capacity *= 2;
                ids = g_realloc(ids, capacity * sizeof(int));
            }
            ids[count++] = (int)s;
        }
        if (*p == ',') p++;
    }

    if (count == 0) {
        g_free(ids);
        return -1;
    }
    *ids_out   = ids;
    *count_out = count;
    return 0;
}

/* Resolve the set of stream IDs to open.
 * If ntStreamIds= is set in config.ini, parse the range string.
 * Otherwise query the driver (NT_INFO_CMD_READ_STREAM) for stream IDs
 * that have host buffers allocated in ntservice.ini HostBuffersRx.
 * Returns heap-allocated int[] via *ids_out, count via *count_out.
 * Calls CONFIGEXIT on fatal error; caller must g_free(*ids_out). */
static void reader_napatech_get_stream_ids(int **ids_out, int *count_out)
{
    char errBuf[NT_ERRBUF_SIZE];
    int  status;

    char *cfgStr = arkime_config_str(NULL, "ntStreamIds", NULL);
    if (cfgStr) {
        int *ids;
        int  count;
        if (reader_napatech_parse_stream_range(cfgStr, &ids, &count) != 0) {
            CONFIGEXIT("Napatech: ntStreamIds '%s' is invalid. "
                       "Expected comma-separated IDs/ranges, e.g. '0-3,5,8-10'", cfgStr);
        }
        g_free(cfgStr);
        *ids_out   = ids;
        *count_out = count;
        return;
    }

    /* Auto-detect: NT_INFO_CMD_READ_STREAM returns stream IDs that have
     * host buffers allocated in ntservice.ini HostBuffersRx. */
    NtInfoStream_t hInfo;
    if ((status = NT_InfoOpen(&hInfo, "arkime-info")) != NT_SUCCESS) {
        NT_ExplainError(status, errBuf, sizeof(errBuf));
        CONFIGEXIT("Napatech: NT_InfoOpen failed: %s", errBuf);
    }

    NtInfo_t info;
    memset(&info, 0, sizeof(info));
    info.cmd = NT_INFO_CMD_READ_STREAM;
    if ((status = NT_InfoRead(hInfo, &info)) != NT_SUCCESS) {
        NT_ExplainError(status, errBuf, sizeof(errBuf));
        NT_InfoClose(hInfo);
        CONFIGEXIT("Napatech: NT_INFO_CMD_READ_STREAM failed: %s", errBuf);
    }
    NT_InfoClose(hInfo);

    uint32_t count = info.u.stream.data.count;
    if (count == 0) {
        CONFIGEXIT("Napatech: No stream IDs with host buffers found. "
                   "Check ntservice.ini HostBuffersRx and apply NTPL Assign rules.");
    }

    int *ids = g_malloc(count * sizeof(int));
    for (uint32_t i = 0; i < count; i++)
        ids[i] = info.u.stream.data.streamIDList[i];
    *ids_out   = ids;
    *count_out = (int)count;
}

/* -------------------------------------------------------------------------
 * Init: called by Arkime on startup
 * ---------------------------------------------------------------------- */

LOCAL void reader_napatech_init(const char *UNUSED(name))
{
    int  status;
    char errBuf[NT_ERRBUF_SIZE];

    /* --------------------------------------------------------------------- *
     * NT_Init and stream opens MUST happen here (init), NOT in start().      *
     *                                                                        *
     * Arkime drops privileges (setuid to dropUser) immediately after calling *
     * arkime_readers_set() in main(), which is what triggers this init()     *
     * callback.  By the time start() runs (from arkime_ready_gfunc via the   *
     * GLib main loop), the process is already running as dropUser (nobody)   *
     * and cannot open /dev/nt3gd (crw------- root root).                     *
     *                                                                        *
     * NTAPI's NT_Init opens /dev/nt3gd for mmap; NT_NetRxOpen allocates     *
     * per-stream host buffers mapped from ntservice.  Both require root.     *
     * --------------------------------------------------------------------- */

    if ((status = NT_Init(NTAPI_VERSION)) != NT_SUCCESS) {
        NT_ExplainError(status, errBuf, sizeof(errBuf));
        CONFIGEXIT("Napatech: NT_Init failed: %s", errBuf);
    }

    /* Optionally apply NTPL from a file before opening streams.
     * Set ntplFile=/path/to/arkime-streams.ntpl in config.ini to have the
     * plugin configure stream/port assignments automatically at startup.
     * If not set, NTPL must be applied manually (e.g. via ntpl -f ...). */
    char *ntplFile = arkime_config_str(NULL, "ntplFile", NULL);
    if (ntplFile) {
        NtConfigStream_t hCfg;
        NtNtplInfo_t     ntplInfo;

        if ((status = NT_ConfigOpen(&hCfg, "arkime-ntpl")) != NT_SUCCESS) {
            NT_ExplainError(status, errBuf, sizeof(errBuf));
            g_free(ntplFile);
            CONFIGEXIT("Napatech: NT_ConfigOpen failed: %s", errBuf);
        }

        FILE *f = fopen(ntplFile, "r");
        if (!f) {
            NT_ConfigClose(hCfg);
            g_free(ntplFile);
            CONFIGEXIT("Napatech: Cannot open ntplFile '%s': %s", ntplFile, strerror(errno));
        }

        char line[4096];
        int  linenum = 0;
        while (fgets(line, sizeof(line), f)) {
            linenum++;
            char *p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') continue;
            size_t len = strlen(p);
            while (len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r')) p[--len] = '\0';
            if (len == 0) continue;

            if ((status = NT_NTPL(hCfg, p, &ntplInfo, NT_NTPL_PARSER_VALIDATE_NORMAL)) != NT_SUCCESS) {
                NT_ExplainError(status, errBuf, sizeof(errBuf));
                fclose(f);
                NT_ConfigClose(hCfg);
                g_free(ntplFile);
                CONFIGEXIT("Napatech: NTPL line %d failed: %s", linenum, errBuf);
            }
        }
        fclose(f);
        NT_ConfigClose(hCfg);
        LOG("Napatech: applied NTPL from '%s'", ntplFile);
        g_free(ntplFile);
    }

    /* Resolve stream IDs (from ntStreamIds= config or driver) and open them.
     * reader_napatech_get_stream_ids() returns a heap-allocated int[] whose
     * length drives the allocation of ntStreams/ntTotalPkts — no static cap. */
    {
        int *streamIds;
        int  streamCount;
        reader_napatech_get_stream_ids(&streamIds, &streamCount);

        ntStreams   = g_malloc0(streamCount * sizeof(NtStream_t));
        ntTotalPkts = g_malloc0(streamCount * sizeof(uint64_t));
        ntNumStreams = streamCount;

        for (int i = 0; i < streamCount; i++) {
            int s = streamIds[i];
            char streamName[64];
            snprintf(streamName, sizeof(streamName), "arkime_rx_%d", s);
            /* NT_NET_INTERFACE_SEGMENT: NT_NetRxGet returns a segment of N
             * packets at a time, enabling zero-copy batch release.
             * hostBufAllowance = -1: use the maximum host buffer for this
             * stream as configured in ntservice.ini HostBuffersRx. */
            status = NT_NetRxOpen(&ntStreams[i].hStream, streamName,
                                  NT_NET_INTERFACE_SEGMENT, s, -1);
            if (status != NT_SUCCESS) {
                NT_ExplainError(status, errBuf, sizeof(errBuf));
                g_free(streamIds);
                CONFIGEXIT("Napatech: NT_NetRxOpen(stream %d) failed: %s", s, errBuf);
            }
            ntStreams[i].streamId     = s;
            ntStreams[i].interfacePos = (uint8_t)i;
        }
        g_free(streamIds);
        LOG("Napatech: opened %d stream(s)", ntNumStreams);
    }

    /* Software BPF filter: compile config.bpf using a dead pcap handle.
     * Same pattern as reader-tzsp.c. Filtering runs per-packet in the
     * capture thread before any packet allocation. */
    use_sw_bpf = FALSE;
    if (config.bpf) {
        pcap_t *dead = pcap_open_dead(DLT_EN10MB, config.snapLen);
        if (pcap_compile(dead, &sw_bpf, config.bpf, 1, PCAP_NETMASK_UNKNOWN) == -1) {
            CONFIGEXIT("Napatech: Couldn't compile bpf filter '%s': %s",
                       config.bpf, pcap_geterr(dead));
        }
        pcap_close(dead);
        use_sw_bpf = TRUE;
        LOG("Napatech: software BPF filter active: '%s'", config.bpf);
    }

    ntDropped = 0;

    /* Register callbacks */
    arkime_reader_start = reader_napatech_start;
    arkime_reader_stats = reader_napatech_stats;
    arkime_reader_stop  = reader_napatech_stop;
    arkime_reader_exit  = reader_napatech_exit;

    LOG("Napatech reader initialized: %d stream(s) open", ntNumStreams);
}

/* -------------------------------------------------------------------------
 * Plugin entry point - registers the reader with Arkime
 * ---------------------------------------------------------------------- */

void arkime_plugin_init()
{
    arkime_readers_add("napatech", reader_napatech_init);
}
