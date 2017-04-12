/* reader-snf.c  -- snf instead of libpcap
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

#include "moloch.h"
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "snf.h"
#include "pcap.h"

extern MolochConfig_t        config;
extern MolochPcapFileHdr_t   pcapFileHeader;

#define MAX_RINGS 10

LOCAL snf_handle_t           handles[MAX_INTERFACES];
LOCAL snf_ring_t             rings[MAX_INTERFACES][MAX_RINGS];
LOCAL int                    portnums[MAX_INTERFACES];
LOCAL int                    snfNumRings;

LOCAL struct bpf_program    *bpf_programs[MOLOCH_FILTER_MAX];

/******************************************************************************/
int reader_snf_stats(MolochReaderStats_t *stats)
{
    struct snf_ring_stats ss;

    stats->dropped = 0;
    stats->total = 0;

    int i, r;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        for (r = 0; r < snfNumRings; r++) {
            int err = snf_ring_getstats(rings[i][r], &ss);

            if (err) 
                continue;
            stats->dropped += ss.ring_pkt_overflow;
            stats->total += ss.ring_pkt_recv;
        }
    }
    return 0;
}
/******************************************************************************/
LOCAL void *reader_snf_thread(gpointer ring)
{
    struct snf_recv_req req;

    MolochPacketBatch_t batch;
    moloch_packet_batch_init(&batch);
    while (!config.quitting) {
        int err = snf_ring_recv(ring, -1, &req);
        if (err) {
            if (err == EBUSY || err == EAGAIN || err == EINTR)
                continue;
            LOG("SNF quiting %d", err);
            moloch_quit();
            break;
        }

        MolochPacket_t *packet = MOLOCH_TYPE_ALLOC0(MolochPacket_t);

        packet->pkt           = (u_char *)req.pkt_addr;
        packet->ts.tv_sec     = req.timestamp / 1000000000;
        packet->ts.tv_usec    = req.timestamp % 1000000000000;
        packet->pktlen        = req.length;

        moloch_packet_batch(&batch, packet);

        if (batch.count > 10000)
            moloch_packet_batch_flush(&batch);
    }
    moloch_packet_batch_flush(&batch);
    return NULL;
}
/******************************************************************************/
int reader_snf_should_filter(const MolochPacket_t *packet, enum MolochFilterType *type, int *index)
{
    int t, i;
    for (t = 0; t < MOLOCH_FILTER_MAX; t++) {
        for (i = 0; i < config.bpfsNum[t]; i++) {
            if (bpf_filter(bpf_programs[t][i].bf_insns, packet->pkt, packet->pktlen, packet->pktlen)) {
                *type = t;
                *index = i;
                return 1;
            }
        }
    }
    return 0;
}
/******************************************************************************/
void reader_snf_start() {
    pcapFileHeader.linktype = DLT_EN10MB;
    pcapFileHeader.snaplen = config.snapLen;
    pcap_t *dpcap = pcap_open_dead(pcapFileHeader.linktype, pcapFileHeader.snaplen);
    int t;
    for (t = 0; t < MOLOCH_FILTER_MAX; t++) {
        if (config.bpfsNum[t]) {
            int i;
            if (bpf_programs[t]) {
                for (i = 0; i < config.bpfsNum[t]; i++) {
                    pcap_freecode(&bpf_programs[t][i]);
                }
            } else {
                bpf_programs[t] = malloc(config.bpfsNum[t]*sizeof(struct bpf_program));
            }
            for (i = 0; i < config.bpfsNum[t]; i++) {
                if (pcap_compile(dpcap, &bpf_programs[t][i], config.bpfs[t][i], 1, PCAP_NETMASK_UNKNOWN) == -1) {
                    LOG("ERROR - Couldn't compile filter: '%s' with %s", config.bpfs[t][i], pcap_geterr(dpcap));
                    exit(1);
                }
            }
            moloch_reader_should_filter = reader_snf_should_filter;
        }
    }

    int i, r;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        for (r = 0; r < snfNumRings; r++) {
            char name[100];
            snprintf(name, sizeof(name), "moloch-snf%d-%d", i, r);
            g_thread_new(name, &reader_snf_thread, rings[i][r]);
        }
        snf_start(handles[i]);
    }
}
/******************************************************************************/
void reader_snf_init(char *UNUSED(name))
{
    struct snf_ifaddrs *ifaddrs;

    snfNumRings = moloch_config_int(NULL, "snfNumRings", 1, 1, MAX_RINGS);
    int snfDataRingSize = moloch_config_int(NULL, "snfDataRingSize", 0, 0, 0x7fffffff);

    int err;
    if ( (err = snf_init(SNF_VERSION_API)) != 0) {
        LOG("Myricom: failed in snf_init(%d) = %d", SNF_VERSION_API, err);
        exit(0);
    }

    if ((err = snf_getifaddrs(&ifaddrs)) || ifaddrs == NULL) {
        LOG("Myricom: failed in snf_getifaddrs %d", err);
        exit(0);
    }

    int i, r;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        struct snf_ifaddrs *ifa = ifaddrs;
        portnums[i] = -1;

        while (ifa) {
            if (strcmp(config.interface[i], ifa->snf_ifa_name) == 0) {
                portnums[i] = ifa->snf_ifa_portnum;
                break;
            }
            ifa = ifa->snf_ifa_next;
        }

        if (portnums[i] == -1 && sscanf(config.interface[i], "snf%d", &portnums[i]) != 1) {
            LOG("Myricom: Couldn't find interface '%s'", config.interface[i]);
            exit(0);
        }

        int err;
        err  = snf_open(portnums[i], snfNumRings, NULL, snfDataRingSize, 0, &handles[i]);
        if (err != 0) {
            LOG("Myricom: Couldn't open interface '%s' %d", config.interface[i], err);
            exit(0);
        }

        for (r = 0; r < snfNumRings; r++) {
            err = snf_ring_open(handles[i], &rings[i][r]);
        }

    }

    snf_freeifaddrs(ifaddrs);

    moloch_reader_start         = reader_snf_start;
    moloch_reader_stats         = reader_snf_stats;
}
/******************************************************************************/
void moloch_plugin_init()
{
    LOG("ALW START");
    moloch_readers_add("snf", reader_snf_init);
}
