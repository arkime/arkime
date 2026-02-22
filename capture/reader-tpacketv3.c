/******************************************************************************/
/* reader-tpacketv3.c  -- Reader using tpacketv3
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Ideas from
 * https://github.com/google/stenographer/tree/master/stenotype
 * https://www.kernel.org/doc/Documentation/networking/packet_mmap.txt
 * https://github.com/rusticata/suricata/blob/rust/src/runmode-af-packet.c
 * libpcap src/pcap-linux.c
 *
 */

#define _FILE_OFFSET_BITS 64
#include "arkime.h"
extern ArkimeConfig_t        config;

#ifndef __linux
void reader_tpacketv3_init(const char *UNUSED(name))
{
    CONFIGEXIT("tpacketv3 not supported");
}
#else

#include "pcap.h"
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/filter.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <errno.h>
#include <poll.h>

#ifndef TPACKET3_HDRLEN
void reader_tpacketv3_init(const char *UNUSED(name))
{
    CONFIGEXIT("tpacketv3 not supported");
}

#else

typedef struct {
    int                  fd;
    struct tpacket_req3  req;
    uint8_t             *map;
    struct iovec        *rd;
    uint8_t              interfacePos;
    uint8_t              thread;
} ARKIME_CACHE_ALIGN ArkimeTPacketV3_t;

LOCAL ArkimeTPacketV3_t infos[MAX_INTERFACES][MAX_THREADS_PER_INTERFACE];

LOCAL int numThreads;

extern ArkimePcapFileHdr_t   pcapFileHeader;
LOCAL struct bpf_program     bpf;

LOCAL ArkimeReaderStats_t gStats;
LOCAL ARKIME_LOCK_DEFINE(gStats);

LOCAL gboolean tpacketv3OldVlan;

/******************************************************************************/
int reader_tpacketv3_stats(ArkimeReaderStats_t *stats)
{
    ARKIME_LOCK(gStats);

    struct tpacket_stats_v3 tpstats;
    for (int i = 0; config.interface[i]; i++) {
        for (int t = 0; t < numThreads; t++) {
            socklen_t len = sizeof(tpstats);
            getsockopt(infos[i][t].fd, SOL_PACKET, PACKET_STATISTICS, &tpstats, &len);

            gStats.dropped += tpstats.tp_drops;
            gStats.total += tpstats.tp_packets;
        }
    }
    *stats = gStats;
    ARKIME_UNLOCK(gStats);
    return 0;
}
/******************************************************************************/
LOCAL void *reader_tpacketv3_thread(gpointer infov)
{
    ArkimeTPacketV3_t *info = (ArkimeTPacketV3_t *)infov;
    struct pollfd pfd;
    int pos = 0;

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = info->fd;
    pfd.events = POLLIN | POLLERR;
    pfd.revents = 0;

    ArkimePacketBatch_t batch;
    arkime_packet_batch_init(&batch);

    uint32_t vlanHeader;

    int initFunc = arkime_get_named_func("arkime_reader_thread_init");
    arkime_call_named_func(initFunc, info->interfacePos * MAX_THREADS_PER_INTERFACE + info->thread, NULL);

    while (!config.quitting) {
        struct tpacket_block_desc *tbd = info->rd[pos].iov_base;
        if (config.debug > 2) {
            int cnt = 0;
            int waiting = 0;

            for (int i = 0; i < (int)info->req.tp_block_nr; i++) {
                struct tpacket_block_desc *stbd = info->rd[i].iov_base;
                if (stbd->hdr.bh1.block_status & TP_STATUS_USER) {
                    cnt++;
                    waiting += stbd->hdr.bh1.num_pkts;
                }
            }

            LOG("Stats pos:%d info:%d status:%x waiting:%d total cnt:%d total waiting:%d", pos, info->interfacePos, tbd->hdr.bh1.block_status, tbd->hdr.bh1.num_pkts, cnt, waiting);
        }

        // Wait until the block is owned by capture
        if ((tbd->hdr.bh1.block_status & TP_STATUS_USER) == 0) {
            poll(&pfd, 1, -1);
            continue;
        }

        struct tpacket3_hdr *th;

        th = (struct tpacket3_hdr *) ((uint8_t *) tbd + tbd->hdr.bh1.offset_to_first_pkt);

        for (uint32_t p = 0; p < tbd->hdr.bh1.num_pkts; p++) {
            if (unlikely(th->tp_snaplen != th->tp_len) && !config.readTruncatedPackets && !config.ignoreErrors) {
                LOGEXIT("ERROR - Arkime requires full packet captures caplen: %d pktlen: %d\n"
                        "See https://arkime.com/faq#arkime_requires_full_packet_captures_error",
                        th->tp_snaplen, th->tp_len);
            }

            ArkimePacket_t *packet = arkime_packet_alloc();
            packet->pktlen        = th->tp_snaplen;
            packet->pkt           = (u_char *)th + th->tp_mac;
            packet->ts.tv_sec     = th->tp_sec;
            packet->ts.tv_usec    = th->tp_nsec / 1000;
            packet->readerPos     = info->interfacePos;

            if ((th->tp_status & TP_STATUS_VLAN_VALID) && th->hv1.tp_vlan_tci) {
                if (tpacketv3OldVlan) {
                    packet->vlan = th->hv1.tp_vlan_tci & 0xfff;
                } else {
                    // AFPacket removes the first VLAN so add it back in. Thanks to Suricata for the idea.
                    packet->pktlen += 4;
                    packet->pkt -= 4;

                    // Move MACs back to make room
                    memmove(packet->pkt, packet->pkt + 4, 12);

                    // Build VLAN header (network byte order)
                    vlanHeader = htonl((0x8100u << 16) | (th->hv1.tp_vlan_tci & 0xfff));
                    memcpy(packet->pkt + 12, &vlanHeader, 4);
                }
            }

            arkime_packet_batch(&batch, packet);

            th = (struct tpacket3_hdr *) ((uint8_t *) th + th->tp_next_offset);
        }
        arkime_packet_batch_flush(&batch);

        tbd->hdr.bh1.block_status = TP_STATUS_KERNEL;
        pos = (pos + 1) % info->req.tp_block_nr;
    }

    int exitFunc = arkime_get_named_func("arkime_reader_thread_exit");
    arkime_call_named_func(exitFunc, info->interfacePos * MAX_THREADS_PER_INTERFACE + info->thread, NULL);
    return NULL;
}
/******************************************************************************/
void reader_tpacketv3_start()
{
    char name[100];
    for (int i = 0; config.interface[i]; i++) {
        for (int t = 0; t < numThreads; t++) {
            snprintf(name, sizeof(name), "arkime-af3%d-%d", i, t);
            g_thread_unref(g_thread_new(name, &reader_tpacketv3_thread, &infos[i][t]));
        }
    }
}
/******************************************************************************/
void reader_tpacketv3_exit()
{
    for (int i = 0; config.interface[i]; i++) {
        for (int t = 0; t < numThreads; t++) {
            close(infos[i][t].fd);
        }
    }
}
/******************************************************************************/
void reader_tpacketv3_init(char *UNUSED(name))
{
    arkime_config_check("tpacketv3", "tpacketv3BlockSize", "tpacketv3NumThreads", "tpacketv3ClusterId", NULL);

    int blocksize = arkime_config_int(NULL, "tpacketv3BlockSize", 1 << 21, 1 << 16, 1U << 31);
    numThreads = arkime_config_int(NULL, "tpacketv3NumThreads", 2, 1, MAX_THREADS_PER_INTERFACE);

    if (blocksize % getpagesize() != 0) {
        CONFIGEXIT("tpacketv3BlockSize=%d not divisible by pagesize %d", blocksize, getpagesize());
    }

    if (blocksize % config.snapLen != 0) {
        CONFIGEXIT("tpacketv3BlockSize=%d not divisible by snapLen=%u", blocksize, config.snapLen);
    }

    arkime_packet_set_dltsnap(DLT_EN10MB, config.snapLen);

    pcap_t *dpcap = pcap_open_dead(pcapFileHeader.dlt, pcapFileHeader.snaplen);

    if (config.bpf) {
        if (pcap_compile(dpcap, &bpf, config.bpf, 1, PCAP_NETMASK_UNKNOWN) == -1) {
            CONFIGEXIT("Couldn't compile bpf filter: '%s' with %s", config.bpf, pcap_geterr(dpcap));
        }
    }

    int fanout_group_id = arkime_config_int(NULL, "tpacketv3ClusterId", 8005, 0x0001, 0xffff);

    tpacketv3OldVlan = arkime_config_boolean(NULL, "tpacketv3OldVlan", FALSE);

    int version = TPACKET_V3;
    int reserve = 4;
    for (int i = 0; config.interface[i]; i++) {
        int ifindex = if_nametoindex(config.interface[i]);

        for (int t = 0; t < numThreads; t++) {
            infos[i][t].fd = socket(AF_PACKET, SOCK_RAW, 0);
            infos[i][t].interfacePos = i;
            infos[i][t].thread = t;

            if (setsockopt(infos[i][t].fd, SOL_PACKET, PACKET_VERSION, &version, sizeof(version)) < 0)
                CONFIGEXIT("Error setting TPACKET_V3, might need a newer kernel: %s", strerror(errno));

            if (!tpacketv3OldVlan && setsockopt(infos[i][t].fd, SOL_PACKET, PACKET_RESERVE, &reserve, sizeof(reserve)) < 0)
                CONFIGEXIT("Error setting RESERVE, might need a newer kernel: %s", strerror(errno));

            memset(&infos[i][t].req, 0, sizeof(infos[i][t].req));
            infos[i][t].req.tp_block_size = blocksize;
            infos[i][t].req.tp_block_nr = 64;
            infos[i][t].req.tp_frame_size = config.snapLen;
            infos[i][t].req.tp_frame_nr = (blocksize * infos[i][t].req.tp_block_nr) / infos[i][t].req.tp_frame_size;
            infos[i][t].req.tp_retire_blk_tov = 60;
            infos[i][t].req.tp_feature_req_word = 0;
            if (setsockopt(infos[i][t].fd, SOL_PACKET, PACKET_RX_RING, &infos[i][t].req, sizeof(infos[i][t].req)) < 0)
                CONFIGEXIT("Error setting PACKET_RX_RING: %s", strerror(errno));

            struct packet_mreq      mreq;
            memset(&mreq, 0, sizeof(mreq));
            mreq.mr_ifindex = ifindex;
            mreq.mr_type    = PACKET_MR_PROMISC;
            if (setsockopt(infos[i][t].fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
                CONFIGEXIT("Error setting PROMISC: %s", strerror(errno));

            if (config.bpf) {
                struct sock_fprog       fcode;
                fcode.len = bpf.bf_len;
                fcode.filter = (struct sock_filter *)bpf.bf_insns;
                if (setsockopt(infos[i][t].fd, SOL_SOCKET, SO_ATTACH_FILTER, &fcode, sizeof(fcode)) < 0)
                    CONFIGEXIT("Error setting SO_ATTACH_FILTER: %s", strerror(errno));
            }

            infos[i][t].map = mmap(NULL, (size_t)infos[i][t].req.tp_block_size * infos[i][t].req.tp_block_nr,
                                   PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, infos[i][t].fd, 0);
            if (unlikely(infos[i][t].map == MAP_FAILED)) {
                CONFIGEXIT("mmap failure in reader_tpacketv3_init, %d: %s. Tried to allocate %" PRId64 " bytes (tpacketv3BlockSize: %d * 64) which was probably too large for this host, you probably need to reduce one of the values.", errno, strerror(errno), (size_t)infos[i][t].req.tp_block_size * infos[i][t].req.tp_block_nr, blocksize);
            }
            infos[i][t].rd = malloc(infos[i][t].req.tp_block_nr * sizeof(struct iovec));

            for (uint16_t j = 0; j < infos[i][t].req.tp_block_nr; j++) {
                infos[i][t].rd[j].iov_base = infos[i][t].map + (j * infos[i][t].req.tp_block_size);
                infos[i][t].rd[j].iov_len = infos[i][t].req.tp_block_size;
            }

            struct sockaddr_ll ll;
            memset(&ll, 0, sizeof(ll));
            ll.sll_family = PF_PACKET;
            ll.sll_protocol = htons(ETH_P_ALL);
            ll.sll_ifindex = ifindex;

            if (bind(infos[i][t].fd, (struct sockaddr *) &ll, sizeof(ll)) < 0)
                CONFIGEXIT("Error binding %s: %s", config.interface[i], strerror(errno));

            int fanout_type = PACKET_FANOUT_HASH;
            int fanout_arg = ((fanout_group_id + i) | (fanout_type << 16));
            if (setsockopt(infos[i][t].fd, SOL_PACKET, PACKET_FANOUT, &fanout_arg, sizeof(fanout_arg)) < 0)
                CONFIGEXIT("Error setting packet fanout parameters: tpacketv3ClusterId: %d (%s)", fanout_group_id, strerror(errno));
        }

        fanout_group_id++;
    }

    pcap_close(dpcap);

    arkime_reader_start         = reader_tpacketv3_start;
    arkime_reader_exit          = reader_tpacketv3_exit;
    arkime_reader_stats         = reader_tpacketv3_stats;
}
#endif // TPACKET_V3
#endif // _linux
