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
 *   ntNumStreams      = 0             (0 = auto-detect from NTPL config; set >0 to override)
 *   ntHostBufferMB   = 2048          (host buffer size in MB per stream)
 *
 * NTPL must be applied before starting Arkime. See arkime-streams.ntpl.
 * The NTPL must assign ports to streams with TimestampType = UNIX_NANOTIME
 * and HashMode = Hash5TupleSorted for proper multi-thread flow affinity.
 *****************************************************************************/

#define _FILE_OFFSET_BITS 64
#include "arkime.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>

/* Napatech NTAPI headers - installed by libntapi-dev package */
#include <nt.h>
#include "pcap.h"          /* bpf_filter, pcap_compile, pcap_open_dead */

extern ArkimeConfig_t config;

/* -------------------------------------------------------------------------
 * Module-level state
 * ---------------------------------------------------------------------- */

/* Maximum streams supported (each stream can cover one or more ports) */
#define NT_MAX_STREAMS  16

typedef struct {
    NtNetStreamRx_t  hStream;        /* NTAPI stream handle            */
    int              streamId;       /* NTAPI stream ID (0-based)      */
    uint8_t          interfacePos;   /* Arkime interface index          */
} NtStream_t;

LOCAL NtStream_t    ntStreams[NT_MAX_STREAMS];
LOCAL int           ntNumStreams;
LOCAL int           ntHostBufferMB;

/* Stats */
LOCAL uint64_t      ntTotalPkts[NT_MAX_STREAMS];
LOCAL uint64_t      ntDropped;
LOCAL ARKIME_LOCK_DEFINE(ntStatsLock);

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
    ARKIME_LOCK(ntStatsLock);
    stats->total   = 0;
    stats->dropped = ntDropped;
    for (int s = 0; s < ntNumStreams; s++) {
        stats->total += ntTotalPkts[s];
    }
    ARKIME_UNLOCK(ntStatsLock);
    return 0;
}

/* -------------------------------------------------------------------------
 * Per-stream capture thread
 * ---------------------------------------------------------------------- */

LOCAL void *reader_napatech_thread(gpointer streamv)
{
    NtStream_t *st = (NtStream_t *)streamv;
    NtNetBuf_t  hSegBuf;     /* handle to one segment (N packets) */
    int         status;

    ArkimePacketBatch_t batch;
    arkime_packet_batch_init(&batch);

    while (!config.quitting) {
        /* 100ms timeout so we can check config.quitting */
        status = NT_NetRxGet(st->hStream, &hSegBuf, 100);

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

        /* ------ Walk all packets in this segment (zero-copy) ----------- *
         *                                                                  *
         * NT_NetRxGet in segment mode returns a contiguous buffer          *
         * containing back-to-back packet records. Each record starts with  *
         * a NtStd0Descr_t (16-byte standard descriptor from               *
         * pktdescr_std0.h), followed immediately by the L2 frame bytes,   *
         * then 0-7 bytes of 8-byte-alignment padding.                      *
         *                                                                  *
         * NtStd0Descr_t layout (128 bits, confirmed from SDK headers):     *
         *   [63:0]  timestamp   — 64-bit Unix nanoseconds (UNIX_NANOTIME)  *
         *   [79:64] storedLength:16 — total record bytes (stride), incl.   *
         *                            descriptor + payload + padding         *
         *   [95:80] (bit fields: crcError, checksums, rxPort, etc.)        *
         *   [111:96] wireLength:16 — original on-wire frame length          *
         *   [127:112] (bit fields: txPort, flags, extensionLength:3, ...)   *
         *                                                                  *
         * We set packet->copied = 0 and point packet->pkt directly into   *
         * the segment buffer (zero-copy). The segment is NOT released      *
         * until after arkime_packet_batch_flush(). Arkime's async packet   *
         * threads will copy each packet (packet.c ~line 1556) before the  *
         * FPGA can rotate the 2 GB host buffer back around (~1.6 s at     *
         * 10 Gbps), matching the safety model used by tpacketv3 blocks.   *
         * ---------------------------------------------------------------- */
        uint64_t segLen  = NT_NET_GET_SEGMENT_LENGTH(hSegBuf);
        uint8_t *segBase = (uint8_t *)NT_NET_GET_SEGMENT_PTR(hSegBuf);
        uint8_t *segEnd  = segBase + segLen;

        for (uint8_t *p = segBase; p < segEnd; ) {
            NtStd0Descr_t *d = (NtStd0Descr_t *)p;

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
                ARKIME_LOCK(ntStatsLock);
                ntDropped++;
                ARKIME_UNLOCK(ntStatsLock);
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

            ARKIME_LOCK(ntStatsLock);
            ntTotalPkts[st->streamId]++;
            ARKIME_UNLOCK(ntStatsLock);

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
    return NULL;
}

/* -------------------------------------------------------------------------
 * Start: called after init, spawns capture threads
 * ---------------------------------------------------------------------- */

LOCAL void reader_napatech_start()
{
    int  status;
    char errBuf[256];

    /* NT_Init and NT_NetRxOpen are both called here (not in init) so that
     * ALL Arkime heap initialisation — parsers, session tables, plugins —
     * completes before NTAPI's mmap64 calls.
     *
     * NT_Init calls mmap64 internally to map shared memory segments from
     * ntservice. When called during init() (i.e. directly after dlopen of
     * this plugin), those mmaps can collide with glibc's malloc arenas that
     * Arkime is still setting up, causing silent heap corruption that then
     * manifests as a SIGSEGV in arkime_parsers_load() or elsewhere.
     *
     * By deferring to start(), all arkime_parsers_init() / plugin loading /
     * session table allocation has already completed before we touch NTAPI.
     *
     * NOTE: NT_INFO_CMD_READ_STREAM counts currently-open streams (always 0
     * before our app opens any), NOT NTPL assignments.  Auto-detect by
     * probing NT_NetRxOpen(0,1,2,...) until failure.  Set ntNumStreams>0 in
     * config.ini to open an explicit count instead. */

    if ((status = NT_Init(NTAPI_VERSION)) != NT_SUCCESS) {
        NT_ExplainError(status, errBuf, sizeof(errBuf));
        LOGEXIT("Napatech: NT_Init failed: %s", errBuf);
    }

    if (ntNumStreams == 0) {
        int probed;
        for (probed = 0; probed < NT_MAX_STREAMS; probed++) {
            char streamName[64];
            snprintf(streamName, sizeof(streamName), "arkime_rx_%d", probed);
            status = NT_NetRxOpen(&ntStreams[probed].hStream, streamName,
                                  NT_NET_INTERFACE_SEGMENT, probed, -1);
            if (status != NT_SUCCESS)
                break;
            ntStreams[probed].streamId     = probed;
            ntStreams[probed].interfacePos = (uint8_t)probed;
            ntTotalPkts[probed]            = 0;
        }
        ntNumStreams = probed;
        if (ntNumStreams == 0) {
            NT_ExplainError(status, errBuf, sizeof(errBuf));
            LOGEXIT("Napatech: No streams available (stream 0: %s). "
                    "Check ntservice.ini HostBuffersRx and apply NTPL.", errBuf);
        }
        LOG("Napatech: auto-detected %d stream(s)", ntNumStreams);
    } else {
        LOG("Napatech: opening %d stream(s) from config.ini", ntNumStreams);
        for (int s = 0; s < ntNumStreams; s++) {
            char streamName[64];
            snprintf(streamName, sizeof(streamName), "arkime_rx_%d", s);
            status = NT_NetRxOpen(&ntStreams[s].hStream, streamName,
                                  NT_NET_INTERFACE_SEGMENT, s, -1);
            if (status != NT_SUCCESS) {
                NT_ExplainError(status, errBuf, sizeof(errBuf));
                LOGEXIT("Napatech: NT_NetRxOpen(stream %d) failed: %s. "
                        "Check ntservice.ini HostBuffersRx and NTPL streamid=%d",
                        s, errBuf, s);
            }
            ntStreams[s].streamId     = s;
            ntStreams[s].interfacePos = (uint8_t)s;
            ntTotalPkts[s]            = 0;
        }
    }

    /* Tell Arkime the link type and snap length */
    arkime_packet_set_dltsnap(DLT_EN10MB, config.snapLen);

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
    if (use_sw_bpf)
        pcap_freecode(&sw_bpf);
    NT_Done();
}

/* -------------------------------------------------------------------------
 * Init: called by Arkime on startup
 * ---------------------------------------------------------------------- */

LOCAL void reader_napatech_init(const char *UNUSED(name))
{
    /* Read config */
    ntNumStreams    = arkime_config_int(NULL, "ntNumStreams",    0,    0,  NT_MAX_STREAMS);
    ntHostBufferMB  = arkime_config_int(NULL, "ntHostBufferMB", 2048, 64, 65536);

    /* NT_Init is deferred to start() to prevent NTAPI's mmap64 calls from
     * corrupting glibc's heap arenas during Arkime's init phase. */

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

    LOG("Napatech reader registered: ntNumStreams=%d (0=auto-detect), %d MB host buffer each",
        ntNumStreams, ntHostBufferMB);
}

/* -------------------------------------------------------------------------
 * Stub extension-load callback: pre-initializes extensionsArr so that
 * arkime_parsers_load() does not crash when called (via tail-call) from
 * arkime_plugins_load() before arkime_python_init() runs.
 * ---------------------------------------------------------------------- */
LOCAL int reader_napatech_ext_load(const char *path)
{
    (void)path;
    return 0;
}

/* -------------------------------------------------------------------------
 * Plugin entry point - registers the reader with Arkime
 * ---------------------------------------------------------------------- */

void arkime_plugin_init()
{
    /* Pre-initialize extensionsArr so that the tail-call from
     * arkime_plugins_load(rootPlugins) to arkime_parsers_load() does not
     * dereference a NULL extensionsArr pointer (which only gets populated
     * later by arkime_python_init).  Registering a dummy ".nt" extension
     * is sufficient to allocate the GPtrArray; the callback is never invoked
     * because no .nt files exist in the parsers directory. */
    arkime_parsers_register_load_extension(".nt", reader_napatech_ext_load);

    arkime_readers_add("napatech", reader_napatech_init);
}
