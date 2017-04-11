/******************************************************************************/
/* reader-libpcap.c  -- Reader using libpcap
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
#define _FILE_OFFSET_BITS 64
#include "moloch.h"
#include <errno.h>
#include <sys/stat.h>
#include <gio/gio.h>
#include "pcap.h"

extern MolochPcapFileHdr_t   pcapFileHeader;

extern MolochConfig_t        config;

static pcap_t               *pcaps[MAX_INTERFACES];

static struct bpf_program   *bpf_programs[MOLOCH_FILTER_MAX];

/******************************************************************************/
int reader_libpcap_stats(MolochReaderStats_t *stats)
{
    stats->dropped = 0;
    stats->total = 0;

    int i;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        struct pcap_stat ps;
        if (unlikely(!pcaps[i]))
            continue;
        int rc = pcap_stats (pcaps[i], &ps);
        if (unlikely(rc))
            continue;
        stats->dropped += ps.ps_drop;
        stats->total += ps.ps_recv;
    }
    return 0;
}
/******************************************************************************/
void reader_libpcap_pcap_cb(u_char *batch, const struct pcap_pkthdr *h, const u_char *bytes)
{
    if (unlikely(h->caplen != h->len)) {
        LOG("ERROR - Moloch requires full packet captures caplen: %d pktlen: %d\n"
            "turning offloading off may fix, something like 'ethtool -K INTERFACE tx off sg off gro off gso off lro off tso off'", 
            h->caplen, h->len);
        exit (0);
    }

    MolochPacket_t *packet = MOLOCH_TYPE_ALLOC0(MolochPacket_t);

    packet->pkt           = (u_char *)bytes;
    packet->ts            = h->ts;
    packet->pktlen        = h->len;

    moloch_packet_batch((MolochPacketBatch_t *)batch, packet);
}
/******************************************************************************/
static void *reader_libpcap_thread(gpointer pcapv)
{
    pcap_t *pcap = pcapv;
    LOG("THREAD %p", (gpointer)pthread_self());

    MolochPacketBatch_t   batch;
    moloch_packet_batch_init(&batch);
    while (1) {
        int r = pcap_dispatch(pcap, 10000, reader_libpcap_pcap_cb, (u_char*)&batch);
        moloch_packet_batch_flush(&batch);

        // Some kind of failure we quit
        if (unlikely(r < 0)) {
            moloch_quit();
            pcap = 0;
            break;
        }
    }
    //ALW - Need to close after packet finishes
    //pcap_close(pcap);
    return NULL;
}
/******************************************************************************/
int reader_libpcap_should_filter(const MolochPacket_t *packet, enum MolochFilterType *type, int *index)
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
void reader_libpcap_start() {
    int dlt_to_linktype(int dlt);

    //ALW - Bug: assumes all linktypes are the same
    pcapFileHeader.linktype = dlt_to_linktype(pcap_datalink(pcaps[0])) | pcap_datalink_ext(pcaps[0]);
    pcapFileHeader.snaplen = pcap_snapshot(pcaps[0]);

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
            moloch_reader_should_filter = reader_libpcap_should_filter;
        }
    }

    int i;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        if (config.bpf) {
            struct bpf_program   bpf;

            if (pcap_compile(pcaps[i], &bpf, config.bpf, 1, PCAP_NETMASK_UNKNOWN) == -1) {
                LOG("ERROR - Couldn't compile filter: '%s' with %s", config.bpf, pcap_geterr(pcaps[i]));
                exit(1);
            }

            if (pcap_setfilter(pcaps[i], &bpf) == -1) {
                LOG("ERROR - Couldn't set filter: '%s' with %s", config.bpf, pcap_geterr(pcaps[i]));
                exit(1);
            }
        }

        char name[100];
        snprintf(name, sizeof(name), "moloch-pcap%d", i);
        g_thread_new(name, &reader_libpcap_thread, (gpointer)pcaps[i]);
    }
}
/******************************************************************************/
void reader_libpcap_stop() 
{
    int i;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        if (pcaps[i])
            pcap_breakloop(pcaps[i]);
    }
}
/******************************************************************************/
pcap_t *
reader_libpcap_open_live(const char *source, int snaplen, int promisc, int to_ms, char *errbuf)
{
    pcap_t *p;
    int status;

    p = pcap_create(source, errbuf);
    if (p == NULL)
        return (NULL);
    status = pcap_set_snaplen(p, snaplen);
    if (status < 0)
        goto fail;
    status = pcap_set_promisc(p, promisc);
    if (status < 0)
        goto fail;
    status = pcap_set_timeout(p, to_ms);
    if (status < 0)
        goto fail;
    status = pcap_set_buffer_size(p, config.pcapBufferSize);
    if (status < 0)
        goto fail;
    status = pcap_activate(p);
    if (status < 0)
        goto fail;
    status = pcap_setnonblock(p, TRUE, errbuf);
    if (status < 0) {
        pcap_close(p);
        return (NULL);
    }

    return (p);
fail:
    if (status == PCAP_ERROR)
        snprintf(errbuf, PCAP_ERRBUF_SIZE, "%s: %s", source,
            pcap_geterr(p));
    else if (status == PCAP_ERROR_NO_SUCH_DEVICE ||
        status == PCAP_ERROR_PERM_DENIED)
        snprintf(errbuf, PCAP_ERRBUF_SIZE, "%s: %s (%s)", source,
            pcap_statustostr(status), pcap_geterr(p));
    else
        snprintf(errbuf, PCAP_ERRBUF_SIZE, "%s: %s", source,
            pcap_statustostr(status));
    pcap_close(p);
    return (NULL);
}
/******************************************************************************/
void reader_libpcap_init(char *UNUSED(name))
{
    char errbuf[1024];

    int i;

    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {

#ifdef SNF
        pcaps[i] = pcap_open_live(config.interface[i], config.snapLen, 1, 0, errbuf);
#else
        pcaps[i] = reader_libpcap_open_live(config.interface[i], config.snapLen, 1, 0, errbuf);
#endif

        if (!pcaps[i]) {
            LOG("pcap open live failed! %s", errbuf);
            exit(1);
        }

        pcap_setnonblock(pcaps[i], FALSE, errbuf);
    }

    if (i == MAX_INTERFACES) {
        LOG("Only support up to %d interfaces", MAX_INTERFACES);
        exit(1);
    }

    moloch_reader_start         = reader_libpcap_start;
    moloch_reader_stop          = reader_libpcap_stop;
    moloch_reader_stats         = reader_libpcap_stats;
}
