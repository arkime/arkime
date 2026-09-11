/******************************************************************************/
/* reader-xdp.c  -- Reader using AF_XDP sockets
 *
 * Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * One AF_XDP socket (xsk) is created per NIC receive queue, each with its own
 * UMEM and its own thread, so nothing is shared between reader threads. The
 * queue count is read from the nic, so interfaces with different queue counts
 * get different numbers of threads.
 *
 * Ideas from
 * https://www.kernel.org/doc/html/latest/networking/af_xdp.html
 * xdp-tools/xdp-bench and the kernel xdpsock sample
 */

#define _FILE_OFFSET_BITS 64
#include "arkime.h"
#include "arkimeconfig.h"

#if !defined(__linux) || !defined(HAVE_LIBXDP)
void reader_xdp_init(const char *UNUSED(name))
{
    CONFIGEXIT("xdp reader not supported, arkime must be built on linux with libxdp installed");
}
#else

#include "pcap.h"
#include <errno.h>
#include <poll.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <linux/ethtool.h>
#include <linux/if_link.h>
#include <linux/if_xdp.h>
#include <linux/sockios.h>

// libxdp pulls in libbpf.h -> linux/bpf.h, whose struct bpf_insn (eBPF) collides
// with the struct bpf_insn (cBPF) pcap.h just defined. Rename the eBPF one for
// the headers below, nothing here uses the libbpf program loading calls.
#define bpf_insn xdp_bpf_insn
#include <xdp/xsk.h>
#undef bpf_insn

extern ArkimeConfig_t        config;

// Kernel reserves this much of every chunk for XDP itself. Defined in
// linux/bpf.h, which can't be included here because it fights with pcap/bpf.h.
#ifndef XDP_PACKET_HEADROOM
#define XDP_PACKET_HEADROOM 256
#endif

#define XDP_BATCH_SIZE 64

typedef struct {
    struct xsk_umem      *umem;
    uint8_t              *umemArea;
    size_t                umemSize;
    struct xsk_ring_prod  fq;
    struct xsk_ring_cons  cq;
    struct xsk_socket    *xsk;
    struct xsk_ring_cons  rx;
    GThread              *thread;
    uint64_t              packets;
    uint32_t              queue;
    uint8_t               interfacePos;
    uint8_t               threadNum;
} ARKIME_CACHE_ALIGN ArkimeXDP_t;

LOCAL ArkimeXDP_t            infos[MAX_INTERFACES][MAX_THREADS_PER_INTERFACE];
LOCAL int                    numThreads[MAX_INTERFACES];
LOCAL uint32_t               queueStart;
LOCAL uint32_t               frameSize;
LOCAL uint32_t               numFrames;
LOCAL uint32_t               ringSize;

LOCAL struct bpf_program     bpfp;

LOCAL ArkimeReaderStats_t    gStats;
LOCAL ARKIME_LOCK_DEFINE(gStats);

/******************************************************************************/
/* Number of rx queues the nic is currently configured with, 0 if we can't tell.
 * A queue without an xsk bound to it isn't a queue we see traffic from, so this
 * has to match the number of reader threads or packets are silently missed.
 */
LOCAL uint32_t reader_xdp_queue_count(const char *ifname)
{
    struct ethtool_channels channels;
    struct ifreq            ifr;

    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return 0;

    memset(&channels, 0, sizeof(channels));
    memset(&ifr, 0, sizeof(ifr));
    channels.cmd = ETHTOOL_GCHANNELS;
    g_strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    ifr.ifr_data = (void *)&channels;

    uint32_t count = 0;
    if (ioctl(fd, SIOCETHTOOL, &ifr) == 0) {
        // combined queues are rx+tx pairs, some drivers only report rx
        count = channels.combined_count ? channels.combined_count : channels.rx_count;
    }
    close(fd);
    return count;
}
/******************************************************************************/
LOCAL uint32_t reader_xdp_mtu(const char *ifname)
{
    struct ifreq ifr;

    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return 0;

    memset(&ifr, 0, sizeof(ifr));
    g_strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

    uint32_t mtu = 0;
    if (ioctl(fd, SIOCGIFMTU, &ifr) == 0)
        mtu = ifr.ifr_mtu;
    close(fd);
    return mtu;
}
/******************************************************************************/
int reader_xdp_stats(ArkimeReaderStats_t *stats)
{
    ARKIME_LOCK(gStats);

    // Unlike PACKET_STATISTICS these counters are absolute and not reset when
    // read, so they are summed fresh each time instead of accumulated.
    uint64_t dropped = 0;
    uint64_t received = 0;

    for (int i = 0; config.interface[i]; i++) {
        for (int t = 0; t < numThreads[i]; t++) {
            if (!infos[i][t].xsk)
                continue;

            received += infos[i][t].packets;

            struct xdp_statistics xdpStats;
            socklen_t len = sizeof(xdpStats);
            memset(&xdpStats, 0, sizeof(xdpStats));
            if (getsockopt(xsk_socket__fd(infos[i][t].xsk), SOL_XDP, XDP_STATISTICS, &xdpStats, &len) == 0) {
                dropped += xdpStats.rx_dropped;
                // Only present on newer kernels, len tells us if it was filled in
                if (len >= offsetof(struct xdp_statistics, rx_ring_full) + sizeof(xdpStats.rx_ring_full))
                    dropped += xdpStats.rx_ring_full;
            }
        }
    }

    gStats.dropped = dropped;
    gStats.total = received + dropped;
    *stats = gStats;
    ARKIME_UNLOCK(gStats);
    return 0;
}
/******************************************************************************/
LOCAL void *reader_xdp_thread(gpointer infov)
{
    ArkimeXDP_t *info = (ArkimeXDP_t *)infov;
    struct pollfd  pfd;

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = xsk_socket__fd(info->xsk);
    pfd.events = POLLIN;

    ArkimePacketBatch_t batch;
    arkime_packet_batch_init(&batch);

    const int initFunc = arkime_get_named_func("arkime_reader_thread_init");
    arkime_call_named_func(initFunc, info->interfacePos * MAX_THREADS_PER_INTERFACE + info->threadNum, NULL);

    while (!config.quitting) {
        uint32_t idxRx = 0;
        const uint32_t rcvd = xsk_ring_cons__peek(&info->rx, XDP_BATCH_SIZE, &idxRx);

        if (!rcvd) {
            if (xsk_ring_prod__needs_wakeup(&info->fq))
                recvfrom(pfd.fd, NULL, 0, MSG_DONTWAIT, NULL, NULL);
            poll(&pfd, 1, 100);
            continue;
        }

        // AF_XDP descriptors carry no timestamp, so all packets in a burst get
        // the time the burst was collected.
        struct timeval ts;
        gettimeofday(&ts, NULL);

        // Get room to hand the chunks back before consuming them
        uint32_t idxFq = 0;
        while (xsk_ring_prod__reserve(&info->fq, rcvd, &idxFq) != rcvd) {
            if (config.quitting)
                goto done;
            if (xsk_ring_prod__needs_wakeup(&info->fq))
                recvfrom(pfd.fd, NULL, 0, MSG_DONTWAIT, NULL, NULL);
        }

        for (uint32_t i = 0; i < rcvd; i++) {
            const struct xdp_desc *desc = xsk_ring_cons__rx_desc(&info->rx, idxRx + i);
            const uint64_t addr = desc->addr;
            const uint32_t len = desc->len;

            uint8_t *pkt = xsk_umem__get_data(info->umemArea, addr);

            if (!config.bpf || bpf_filter(bpfp.bf_insns, pkt, len, len)) {
                ArkimePacket_t *packet = arkime_packet_alloc();
                packet->pkt        = pkt;
                packet->pktlen     = len;
                packet->ts         = ts;
                packet->readerPos  = info->interfacePos;

                arkime_packet_batch(&batch, packet);
                info->packets++;
            }

            // Aligned mode, the kernel masks to the chunk boundary itself but be explicit
            *xsk_ring_prod__fill_addr(&info->fq, idxFq + i) = addr & ~((uint64_t)frameSize - 1);
        }

        // Packet data is copied out of the umem by arkime_packet_batch, but only
        // release the chunks after the flush to keep the same ordering as the
        // other ring based readers.
        arkime_packet_batch_flush(&batch);

        xsk_ring_prod__submit(&info->fq, rcvd);
        xsk_ring_cons__release(&info->rx, rcvd);
    }

done:
    arkime_packet_batch_flush(&batch);

    const int exitFunc = arkime_get_named_func("arkime_reader_thread_exit");
    arkime_call_named_func(exitFunc, info->interfacePos * MAX_THREADS_PER_INTERFACE + info->threadNum, NULL);
    return NULL;
}
/******************************************************************************/
void reader_xdp_start()
{
    char name[100];
    for (int i = 0; config.interface[i]; i++) {
        for (int t = 0; t < numThreads[i]; t++) {
            snprintf(name, sizeof(name), "arkime-xdp%d-%d", i, t);
            infos[i][t].thread = g_thread_new(name, &reader_xdp_thread, &infos[i][t]);
        }
    }
}
/******************************************************************************/
void reader_xdp_exit()
{
    // The threads still reference the rings and the umem, so wait for them to
    // notice config.quitting before any of it goes away.
    for (int i = 0; config.interface[i]; i++) {
        for (int t = 0; t < numThreads[i]; t++) {
            if (infos[i][t].thread) {
                g_thread_join(infos[i][t].thread);
                infos[i][t].thread = NULL;
            }
        }
    }

    for (int i = 0; config.interface[i]; i++) {
        for (int t = 0; t < numThreads[i]; t++) {
            if (infos[i][t].xsk) {
                xsk_socket__delete(infos[i][t].xsk);
                infos[i][t].xsk = NULL;
            }
            if (infos[i][t].umem) {
                xsk_umem__delete(infos[i][t].umem);
                infos[i][t].umem = NULL;
            }
            if (infos[i][t].umemArea) {
                munmap(infos[i][t].umemArea, infos[i][t].umemSize);
                infos[i][t].umemArea = NULL;
            }
        }
    }
}
/******************************************************************************/
LOCAL void reader_xdp_create(ArkimeXDP_t *info, const char *ifname, int zeroCopy, uint32_t xdpFlags)
{
    info->umemSize = (size_t)numFrames * frameSize;
    info->umemArea = mmap(NULL, info->umemSize, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (info->umemArea == MAP_FAILED) {
        info->umemArea = NULL;
        CONFIGEXIT("Couldn't allocate %zu bytes of umem for %s queue %u, lower xdpRingSize or xdpFrameSize: %s",
                   info->umemSize, ifname, info->queue, strerror(errno));
    }

    struct xsk_umem_config ucfg;
    memset(&ucfg, 0, sizeof(ucfg));
    ucfg.fill_size      = ringSize * 2;
    ucfg.comp_size      = ringSize;
    ucfg.frame_size     = frameSize;
    ucfg.frame_headroom = 0;
    ucfg.flags          = 0;

    int err = xsk_umem__create(&info->umem, info->umemArea, info->umemSize, &info->fq, &info->cq, &ucfg);
    if (err) {
        if (-err == EPERM || -err == ENOMEM)
            CONFIGEXIT("Couldn't create umem for %s queue %u: %s. May need to run as root or raise the memlock rlimit",
                       ifname, info->queue, strerror(-err));
        CONFIGEXIT("Couldn't create umem for %s queue %u: %s", ifname, info->queue, strerror(-err));
    }

    struct xsk_socket_config scfg;
    memset(&scfg, 0, sizeof(scfg));
    scfg.rx_size      = ringSize;
    scfg.tx_size      = 0;
    scfg.libxdp_flags = 0;
    scfg.xdp_flags    = xdpFlags;
    scfg.bind_flags   = XDP_USE_NEED_WAKEUP | (zeroCopy ? XDP_ZEROCOPY : XDP_COPY);

    err = xsk_socket__create(&info->xsk, ifname, info->queue, info->umem, &info->rx, NULL, &scfg);
    if (err && zeroCopy) {
        LOG("WARNING - Zero copy not available on %s queue %u (%s), falling back to copy mode", ifname, info->queue, strerror(-err));
        scfg.bind_flags = XDP_USE_NEED_WAKEUP | XDP_COPY;
        err = xsk_socket__create(&info->xsk, ifname, info->queue, info->umem, &info->rx, NULL, &scfg);
    }

    if (err) {
        if (-err == EBUSY)
            CONFIGEXIT("Couldn't create AF_XDP socket on %s queue %u: %s. Another socket or xdp program may already be attached, or the queue doesn't exist", ifname, info->queue, strerror(-err));
        CONFIGEXIT("Couldn't create AF_XDP socket on %s queue %u: %s", ifname, info->queue, strerror(-err));
    }

    // Hand every frame we own to the kernel, capped by what the fill ring holds
    uint32_t toFill = MIN(numFrames, ucfg.fill_size);
    uint32_t idx = 0;
    if (xsk_ring_prod__reserve(&info->fq, toFill, &idx) != toFill)
        CONFIGEXIT("Couldn't populate the fill ring for %s queue %u", ifname, info->queue);

    for (uint32_t i = 0; i < toFill; i++)
        *xsk_ring_prod__fill_addr(&info->fq, idx + i) = (uint64_t)i * frameSize;
    xsk_ring_prod__submit(&info->fq, toFill);

    if (config.debug)
        LOG("AF_XDP socket on %s queue %u, %u frames of %u bytes, rings of %u", ifname, info->queue, numFrames, frameSize, ringSize);
}
/******************************************************************************/
void reader_xdp_init(const char *UNUSED(name))
{
    arkime_config_check("xdp", "xdpNumThreads", "xdpQueueStart", "xdpFrameSize",
                        "xdpRingSize", "xdpZeroCopy", "xdpMode",
                        "xdpIgnoreQueueMismatch", NULL);

    // 0 means use however many rx queues each interface actually has, which is
    // the only setting that captures everything, so it is also the default.
    const int configNumThreads = arkime_config_int(NULL, "xdpNumThreads", 0, 0, MAX_THREADS_PER_INTERFACE);
    queueStart = arkime_config_int(NULL, "xdpQueueStart", 0, 0, 0xffff);
    frameSize  = arkime_config_int(NULL, "xdpFrameSize", 4096, 2048, 4096);
    ringSize   = arkime_config_int(NULL, "xdpRingSize", XSK_RING_CONS__DEFAULT_NUM_DESCS, 64, 1 << 16);

    const int zeroCopy = arkime_config_boolean(NULL, "xdpZeroCopy", TRUE);
    const int ignoreQueueMismatch = arkime_config_boolean(NULL, "xdpIgnoreQueueMismatch", FALSE);

    // Aligned umem chunks, the kernel only accepts 2048 or 4096
    if (frameSize != 2048 && frameSize != 4096)
        CONFIGEXIT("xdpFrameSize must be 2048 or 4096, not %u", frameSize);

    if ((ringSize & (ringSize - 1)) != 0)
        CONFIGEXIT("xdpRingSize=%u must be a power of 2", ringSize);

    // The fill ring caps how many chunks can be in flight, so owning more than
    // that many frames would just waste memory.
    numFrames = ringSize * 2;

    uint32_t xdpFlags = 0;
    char *mode = arkime_config_str(NULL, "xdpMode", "auto");
    if (strcmp(mode, "drv") == 0)
        xdpFlags = XDP_FLAGS_DRV_MODE;
    else if (strcmp(mode, "skb") == 0)
        xdpFlags = XDP_FLAGS_SKB_MODE;
    else if (strcmp(mode, "auto") != 0)
        CONFIGEXIT("xdpMode must be auto, drv, or skb, not '%s'", mode);
    g_free(mode);

    // A packet bigger than a chunk is dropped by the kernel and never delivered,
    // which would silently break the full packet captures arkime expects.
    const uint32_t maxPacket = frameSize - XDP_PACKET_HEADROOM;

    if (config.bpf) {
        pcap_t *dpcap = pcap_open_dead(DLT_EN10MB, config.snapLen);
        if (pcap_compile(dpcap, &bpfp, config.bpf, 1, PCAP_NETMASK_UNKNOWN) == -1) {
            CONFIGEXIT("Couldn't compile bpf filter: '%s' with %s", config.bpf, pcap_geterr(dpcap));
        }
        pcap_close(dpcap);
    }

    for (int i = 0; config.interface[i]; i++) {
        if (i >= MAX_INTERFACES)
            CONFIGEXIT("Too many interfaces, max is %d", MAX_INTERFACES);

        if (if_nametoindex(config.interface[i]) == 0)
            CONFIGEXIT("Couldn't find interface '%s': %s", config.interface[i], strerror(errno));

        // An xsk is bound to one queue, so a queue without one is a queue whose
        // traffic goes to the kernel instead of us. Default to a thread per queue.
        const uint32_t queues = reader_xdp_queue_count(config.interface[i]);

        if (configNumThreads) {
            numThreads[i] = configNumThreads;
            if (queues && (queueStart != 0 || (uint32_t)numThreads[i] != queues) && !ignoreQueueMismatch) {
                CONFIGEXIT("Interface %s has %u rx queues but xdpNumThreads=%d with xdpQueueStart=%u only covers queues %u-%u. "
                           "Traffic arriving on the other queues will NOT be captured. Either remove xdpNumThreads to use one "
                           "thread per queue, or set xdpIgnoreQueueMismatch=true if another process is reading the rest",
                           config.interface[i], queues, numThreads[i], queueStart, queueStart, queueStart + numThreads[i] - 1);
            }
        } else if (queues == 0) {
            CONFIGEXIT("Couldn't get the rx queue count for %s, set xdpNumThreads to the combined count from 'ethtool -l %s'",
                       config.interface[i], config.interface[i]);
        } else if (queues > MAX_THREADS_PER_INTERFACE) {
            CONFIGEXIT("Interface %s has %u rx queues, more than the %d threads arkime allows per interface. "
                       "Run 'ethtool -L %s combined %d', or split the queues across capture processes with xdpNumThreads and xdpQueueStart",
                       config.interface[i], queues, MAX_THREADS_PER_INTERFACE, config.interface[i], MAX_THREADS_PER_INTERFACE);
        } else if (queueStart != 0) {
            CONFIGEXIT("xdpQueueStart=%u only makes sense with xdpNumThreads set, otherwise a thread is started for every queue", queueStart);
        } else {
            numThreads[i] = queues;
            LOG("Using %d threads for %s, one per rx queue", numThreads[i], config.interface[i]);
        }

        const uint32_t mtu = reader_xdp_mtu(config.interface[i]);
        if (mtu && mtu + 18 > maxPacket) {
            CONFIGEXIT("Interface %s has an mtu of %u which doesn't fit in an xdpFrameSize of %u (%u usable after the xdp headroom). %s",
                       config.interface[i], mtu, frameSize, maxPacket,
                       frameSize < 4096 ? "Raise xdpFrameSize to 4096" : "4096 is the largest chunk the kernel allows, jumbo frames aren't supported by this reader");
        }

        arkime_packet_set_interface(i, 0, DLT_EN10MB, MIN(config.snapLen, maxPacket));
    }

    // Every interface is checked before any socket is made so all the config
    // problems are reported at once instead of one restart at a time.
    for (int i = 0; config.interface[i]; i++) {
        for (int t = 0; t < numThreads[i]; t++) {
            infos[i][t].interfacePos = i;
            infos[i][t].threadNum    = t;
            infos[i][t].queue        = queueStart + t;
            reader_xdp_create(&infos[i][t], config.interface[i], zeroCopy, xdpFlags);
        }
    }

    // xsk_socket__delete needs CAP_NET_ADMIN to take the xdp program back off the
    // nic, which we no longer have once privileges are dropped.
    if (config.dropUser || config.dropGroup)
        LOG("WARNING - dropUser/dropGroup is set, the xdp program will be left attached on exit. Remove it with 'ip link set dev <interface> xdp off'");

    arkime_reader_start = reader_xdp_start;
    arkime_reader_exit  = reader_xdp_exit;
    arkime_reader_stats = reader_xdp_stats;
}
#endif // __linux && HAVE_LIBXDP
