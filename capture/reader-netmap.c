/******************************************************************************/
/* reader-netmap.c  -- Reader using NETMAP
 *
 * For Linux and FreeBSD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define _FILE_OFFSET_BITS 64
#include "arkime.h"

#if !defined(__linux__) && !defined(__FreeBSD__)
void reader_netmap_init(const char *UNUSED(name))
{
    CONFIGEXIT("Netmap reader only supported on Linux and FreeBSD");
}
#else

#include <errno.h>
#include <net/if.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <poll.h>
#include <net/netmap.h>
#define NETMAP_WITH_LIBS
#include <net/netmap_user.h>

extern ArkimeConfig_t        config;
extern ArkimePcapFileHdr_t   pcapFileHeader;

typedef struct {
    struct nm_desc          *nmd;
    uint8_t                  interfacePos;
    uint8_t                  threadNum;
    uint16_t                 ringStart;
    uint16_t                 ringEnd;
    gboolean                 initialized;
} ArkimeNetmap_t;

#define MAX_NETMAP_READERS (MAX_INTERFACES * MAX_THREADS_PER_INTERFACE)

LOCAL ArkimeNetmap_t         readers[MAX_NETMAP_READERS];
LOCAL int                    numReaders;
LOCAL int                    threadsPerInterface;
LOCAL ArkimeReaderStats_t    gStats;
LOCAL ARKIME_LOCK_DEFINE(gStats);

/******************************************************************************/
int reader_netmap_stats(ArkimeReaderStats_t *stats)
{
    ARKIME_LOCK(gStats);
    memset(&gStats, 0, sizeof(gStats));

    for (int i = 0; i < numReaders; i++) {
        if (readers[i].nmd) {
            for (uint16_t ring_idx = readers[i].ringStart; ring_idx <= readers[i].ringEnd; ring_idx++) {
                struct netmap_ring *ring = NETMAP_RXRING(readers[i].nmd->nifp, ring_idx);
                gStats.total += ring->stats.packets;
                gStats.dropped += ring->stats.drops;
            }
        }
    }
    *stats = gStats;
    ARKIME_UNLOCK(gStats);
    return 0;
}

/******************************************************************************/
LOCAL void *reader_netmap_thread(gpointer readerv)
{
    ArkimeNetmap_t *reader = (ArkimeNetmap_t *)readerv;
    int initFunc = arkime_get_named_func("arkime_reader_thread_init");
    arkime_call_named_func(initFunc, reader->interfacePos, NULL);

    ArkimePacketBatch_t batch;
    arkime_packet_batch_init(&batch);

    struct pollfd pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = NETMAP_FD(reader->nmd);
    pfd.events = POLLIN;

    while (!config.quitting) {
        int poll_result = poll(&pfd, 1, 100);
        if (poll_result == 0 || poll_result < 0) {
            if (poll_result < 0 && errno != EINTR && errno != EAGAIN) {
                LOG("Netmap poll error on %s (thread %d): %s", config.interface[reader->interfacePos], reader->threadNum, strerror(errno));
            }
            continue;
        }

        // Process received packets from assigned rings
        for (uint16_t ring_idx = reader->ringStart; ring_idx <= reader->ringEnd; ring_idx++) {
            struct netmap_ring *ring = NETMAP_RXRING(reader->nmd->nifp, ring_idx);

            while (!nm_ring_empty(ring)) {
                uint32_t i = ring->cur;
                struct netmap_slot *slot = &ring->slot[i];
                unsigned char *buf = (unsigned char *)NETMAP_BUF(ring, slot->buf_idx);

                ArkimePacket_t *packet = arkime_packet_alloc();
                packet->pkt = buf;
                packet->pktlen = slot->len;

                // Get current time for packet timestamp
                struct timeval tv;
                gettimeofday(&tv, NULL);
                packet->ts = tv;

                packet->readerPos = reader->interfacePos;

                arkime_packet_batch(&batch, packet);

                ring->head = ring->cur = nm_ring_next(ring, i);
            }
        }

        arkime_packet_batch_flush(&batch);
    }

    int exitFunc = arkime_get_named_func("arkime_reader_thread_exit");
    arkime_call_named_func(exitFunc, reader->interfacePos, NULL);
    return NULL;
}

/******************************************************************************/
LOCAL void netmap_set_filter(ArkimeNetmap_t *reader UNUSED(const char *filterstr UNUSED))
{
    // Netmap doesn't support in-kernel BPF filtering the same way
    // Filtering would need to be done at application level if needed
    if (config.debug) {
        LOG("Netmap filtering at application level not yet implemented");
    }
}

/******************************************************************************/
void reader_netmap_start()
{
    char name[100];
    for (int i = 0; i < numReaders; i++) {
        snprintf(name, sizeof(name), "arkime-netmap%d-%d", readers[i].interfacePos, readers[i].threadNum);
        g_thread_unref(g_thread_new(name, &reader_netmap_thread, &readers[i]));
    }
}

/******************************************************************************/
void reader_netmap_exit()
{
    for (int i = 0; i < numReaders; i++) {
        if (readers[i].nmd) {
            nm_close(readers[i].nmd);
            readers[i].nmd = NULL;
        }
    }
}

/******************************************************************************/
void reader_netmap_init(char *UNUSED(name))
{
    arkime_config_check("netmap", "netmapThreads", NULL);

    threadsPerInterface = arkime_config_int(NULL, "netmapThreads", 1, 1, MAX_THREADS_PER_INTERFACE);
    arkime_packet_set_dltsnap(DLT_EN10MB, config.snapLen);

    char nmspec[256];
    numReaders = 0;

    for (int i = 0; config.interface[i]; i++) {
        // Open a single netmap descriptor per interface
        snprintf(nmspec, sizeof(nmspec), "netmap:%s", config.interface[i]);

        struct nm_open_flags flags = {0};
        flags.extra_bufs = 0;
        flags.timestamp = 0;
        flags.ring_flags = 0;

        struct nm_desc *nmd = nm_open(nmspec, NULL, NM_OPEN_ARG(&flags), NULL);
        if (!nmd) {
            CONFIGEXIT("Failed to open netmap on interface %s: %s", config.interface[i], strerror(errno));
        }

        if (config.debug) {
            LOG("Netmap opened on interface %s with %d RX rings", config.interface[i], nmd->nifp->ni_rx_rings);
        }

        uint16_t ringsPerThread = nmd->nifp->ni_rx_rings / threadsPerInterface;
        if (ringsPerThread == 0) {
            ringsPerThread = 1;
        }

        // Create reader threads with assigned rings
        for (int t = 0; t < threadsPerInterface; t++) {
            if (numReaders >= MAX_NETMAP_READERS) {
                CONFIGEXIT("Too many reader threads, max is %d", MAX_NETMAP_READERS);
            }

            ArkimeNetmap_t *reader = &readers[numReaders];
            reader->nmd = nmd;
            reader->interfacePos = i;
            reader->threadNum = t;
            reader->ringStart = t * ringsPerThread;
            reader->ringEnd = (t + 1) * ringsPerThread - 1;

            // Last thread gets any remaining rings
            if (t == threadsPerInterface - 1) {
                reader->ringEnd = nmd->nifp->ni_rx_rings - 1;
            }

            if (config.debug) {
                LOG("Thread %d for interface %s assigned rings %u-%u", t, config.interface[i], reader->ringStart, reader->ringEnd);
            }

            // Set up netmap filter if provided
            if (config.bpf) {
                netmap_set_filter(reader, config.bpf);
            }

            reader->initialized = TRUE;
            numReaders++;
        }
    }

    arkime_reader_start = reader_netmap_start;
    arkime_reader_exit = reader_netmap_exit;
    arkime_reader_stats = reader_netmap_stats;
}

#endif // __linux__ || __FreeBSD__
