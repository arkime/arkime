/******************************************************************************/
/* reader-tpacketv3.c  -- Reader using tpacketv3
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
 *
 * Ideas from
 * https://github.com/google/stenographer/tree/master/stenotype
 * https://www.kernel.org/doc/Documentation/networking/packet_mmap.txt
 * https://github.com/rusticata/suricata/blob/rust/src/runmode-af-packet.c
 * libpcap src/pcap-linux.c
 *
 */

#include "moloch.h"
extern MolochConfig_t        config;

#ifndef __linux
void reader_tpacketv3_init(char *UNUSED(name))
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
void reader_tpacketv3_init(char *UNUSED(name))
{
    CONFIGEXIT("tpacketv3 not supported");
}

#else

#define MAX_TPACKETV3_THREADS 12

typedef struct {
    int                  fd;
    struct tpacket_req3  req;
    uint8_t             *map;
    struct iovec        *rd;
    uint8_t              interfacePos;
} MolochTPacketV3_t;

LOCAL MolochTPacketV3_t infos[MAX_INTERFACES][MAX_TPACKETV3_THREADS];

LOCAL int numThreads;

extern MolochPcapFileHdr_t   pcapFileHeader;
LOCAL struct bpf_program     bpf;

LOCAL MolochReaderStats_t gStats;
LOCAL MOLOCH_LOCK_DEFINE(gStats);

/******************************************************************************/
int reader_tpacketv3_stats(MolochReaderStats_t *stats)
{
    MOLOCH_LOCK(gStats);

    struct tpacket_stats_v3 tpstats;
    for (int i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        for (int t = 0; t < MAX_TPACKETV3_THREADS; t++) {
            socklen_t len = sizeof(tpstats);
            getsockopt(infos[i][t].fd, SOL_PACKET, PACKET_STATISTICS, &tpstats, &len);

            gStats.dropped += tpstats.tp_drops;
            gStats.total += tpstats.tp_packets;
        }
    }
    *stats = gStats;
    MOLOCH_UNLOCK(gStats);
    return 0;
}
/******************************************************************************/
LOCAL void *reader_tpacketv3_thread(gpointer infov)
{
    MolochTPacketV3_t *info = (MolochTPacketV3_t *)infov;
    struct pollfd pfd;
    int pos = 0;

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = info->fd;
    pfd.events = POLLIN | POLLERR;
    pfd.revents = 0;

    MolochPacketBatch_t batch;
    moloch_packet_batch_init(&batch);

    while (!config.quitting) {
        struct tpacket_block_desc *tbd = info->rd[pos].iov_base;
        if (config.debug > 2) {
            int i;
            int cnt = 0;
            int waiting = 0;

            for (i = 0; i < (int)info->req.tp_block_nr; i++) {
                struct tpacket_block_desc *stbd = info->rd[i].iov_base;
                if (stbd->hdr.bh1.block_status & TP_STATUS_USER) {
                    cnt++;
                    waiting += stbd->hdr.bh1.num_pkts;
                }
            }

            LOG("Stats pos:%d info:%d status:%x waiting:%d total cnt:%d total waiting:%d", pos, info->interfacePos, tbd->hdr.bh1.block_status, tbd->hdr.bh1.num_pkts, cnt, waiting);
        }

        // Wait until the block is owned by moloch
        if ((tbd->hdr.bh1.block_status & TP_STATUS_USER) == 0) {
            poll(&pfd, 1, -1);
            continue;
        }

        struct tpacket3_hdr *th;

        th = (struct tpacket3_hdr *) ((uint8_t *) tbd + tbd->hdr.bh1.offset_to_first_pkt);
        uint16_t p;

        for (p = 0; p < tbd->hdr.bh1.num_pkts; p++) {
            if (unlikely(th->tp_snaplen != th->tp_len)) {
                LOGEXIT("ERROR - Arkime requires full packet captures caplen: %d pktlen: %d\n"
                    "See https://arkime.com/faq#moloch_requires_full_packet_captures_error",
                    th->tp_snaplen, th->tp_len);
            }

            MolochPacket_t *packet = MOLOCH_TYPE_ALLOC0(MolochPacket_t);
            packet->pkt           = (u_char *)th + th->tp_mac;
            packet->pktlen        = th->tp_len;
            packet->ts.tv_sec     = th->tp_sec;
            packet->ts.tv_usec    = th->tp_nsec/1000;
            packet->readerPos     = info->interfacePos;

            if ((th->tp_status & TP_STATUS_VLAN_VALID) && th->hv1.tp_vlan_tci) {
                packet->vlan = th->hv1.tp_vlan_tci & 0xfff;
            }

            moloch_packet_batch(&batch, packet);

            th = (struct tpacket3_hdr *) ((uint8_t *) th + th->tp_next_offset);
        }
        moloch_packet_batch_flush(&batch);

        tbd->hdr.bh1.block_status = TP_STATUS_KERNEL;
        pos = (pos + 1) % info->req.tp_block_nr;
    }
    return NULL;
}
/******************************************************************************/
void reader_tpacketv3_start() {
    char name[100];
    for (int i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        for (int t = 0; t < numThreads; t++) {
            snprintf(name, sizeof(name), "moloch-af3%d-%d", i, t);
            g_thread_unref(g_thread_new(name, &reader_tpacketv3_thread, &infos[i][t]));
        }
    }
}
/******************************************************************************/
void reader_tpacketv3_exit()
{
    for (int i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        for (int t = 0; t < numThreads; t++) {
            close(infos[i][t].fd);
        }
    }
}
/******************************************************************************/
void reader_tpacketv3_init(char *UNUSED(name))
{
    int blocksize = moloch_config_int(NULL, "tpacketv3BlockSize", 1<<21, 1<<16, 1U<<31);
    numThreads = moloch_config_int(NULL, "tpacketv3NumThreads", 2, 1, MAX_TPACKETV3_THREADS);

    if (blocksize % getpagesize() != 0) {
        CONFIGEXIT("block size %d not divisible by pagesize %d", blocksize, getpagesize());
    }

    if (blocksize % config.snapLen != 0) {
        CONFIGEXIT("block size %d not divisible by %u", blocksize, config.snapLen);
    }

    moloch_packet_set_dltsnap(DLT_EN10MB, config.snapLen);

    pcap_t *dpcap = pcap_open_dead(pcapFileHeader.dlt, pcapFileHeader.snaplen);

    if (config.bpf) {
        if (pcap_compile(dpcap, &bpf, config.bpf, 1, PCAP_NETMASK_UNKNOWN) == -1) {
            CONFIGEXIT("Couldn't compile bpf filter: '%s' with %s", config.bpf, pcap_geterr(dpcap));
        }
    }

    int fanout_group_id = moloch_config_int(NULL, "tpacketv3ClusterId", 8005, 0x0001, 0xffff);

    int version = TPACKET_V3;
    int i;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        int ifindex = if_nametoindex(config.interface[i]);

        for (int t = 0; t < numThreads; t++) {
            infos[i][t].fd = socket(AF_PACKET, SOCK_RAW, 0);
            infos[i][t].interfacePos = i;

            if (setsockopt(infos[i][t].fd, SOL_PACKET, PACKET_VERSION, &version, sizeof(version)) < 0)
                CONFIGEXIT("Error setting TPACKET_V3, might need a newer kernel: %s", strerror(errno));

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

            infos[i][t].map = mmap64(NULL, infos[i][t].req.tp_block_size * infos[i][t].req.tp_block_nr,
                                 PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, infos[i][t].fd, 0);
            if (unlikely(infos[i][t].map == MAP_FAILED)) {
                CONFIGEXIT("MMap64 failure in reader_tpacketv3_init, %d: %s. Tried to allocate %d bytes (tpacketv3BlockSize: %d * tpacketv3NumThreads: %d * 64) which was probbaly too large for this host, you probably need to reduce one of the values.", errno, strerror(errno), infos[i][t].req.tp_block_size * infos[i][t].req.tp_block_nr, blocksize, numThreads);
            }
            infos[i][t].rd = malloc(infos[i][t].req.tp_block_nr * sizeof(struct iovec));

            uint16_t j;
            for (j = 0; j < infos[i][t].req.tp_block_nr; j++) {
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

            if (fanout_group_id != 0) {
                int fanout_type = PACKET_FANOUT_HASH;
                int fanout_arg = ((fanout_group_id+i) | (fanout_type << 16));
                if(setsockopt(infos[i][t].fd, SOL_PACKET, PACKET_FANOUT, &fanout_arg, sizeof(fanout_arg)) < 0)
                    CONFIGEXIT("Error setting packet fanout parameters: tpacketv3ClusterId: %d (%s)", fanout_group_id, strerror(errno));
            }
        }

        fanout_group_id++;
    }

    pcap_close(dpcap);

    if (i == MAX_INTERFACES) {
        CONFIGEXIT("Only support up to %d interfaces", MAX_INTERFACES);
    }

    moloch_reader_start         = reader_tpacketv3_start;
    moloch_reader_exit          = reader_tpacketv3_exit;
    moloch_reader_stats         = reader_tpacketv3_stats;
}
#endif // TPACKET_V3
#endif // _linux
