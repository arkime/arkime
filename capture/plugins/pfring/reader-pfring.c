/* reader-pfring.c  -- pfring instead of libpcap
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "pfring.h"
#include "pcap.h"

extern ArkimeConfig_t        config;

LOCAL pfring                *rings[MAX_INTERFACES];

/******************************************************************************/
int reader_pfring_stats(ArkimeReaderStats_t *stats)
{
    pfring_stat pfstats;

    stats->dropped = 0;
    stats->total = 0;

    int i;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        pfring_stats(rings[i], &pfstats);
        stats->dropped += pfstats.drop;
        stats->total += pfstats.recv;
    }
    return 0;
}
/******************************************************************************/
void reader_pfring_packet_cb(const struct pfring_pkthdr *h, const u_char *p, const u_char *user_bytes)
{
    ArkimePacketBatch_t *batch = (ArkimePacketBatch_t *)user_bytes;

    if (unlikely(h->caplen != h->len)) {
        LOGEXIT("ERROR - Arkime requires full packet captures caplen: %d pktlen: %d", h->caplen, h->len);
    }

    ArkimePacket_t *packet = ARKIME_TYPE_ALLOC0(ArkimePacket_t);

    packet->pkt           = (u_char *)p;
    packet->ts            = h->ts;
    packet->pktlen        = h->len;
    packet->readerPos     = batch->readerPos;

    arkime_packet_batch(batch, packet);
    if (batch->count > 10000)
        arkime_packet_batch_flush(batch);
}
/******************************************************************************/
LOCAL void *reader_pfring_thread(void *posv)
{
    long                   pos = (long)posv;
    pfring                *ring = rings[pos];

    ArkimePacketBatch_t batch;
    arkime_packet_batch_init(&batch);
    batch.readerPos = pos;
    pfring_enable_ring(ring);
    while (1) {
        int r = pfring_loop(ring, reader_pfring_packet_cb, (u_char *)&batch, -1);

        arkime_packet_batch_flush(&batch);

        // Some kind of failure we quit
        if (unlikely(r <= 0)) {
            arkime_quit();
            rings[pos] = 0;
            break;
        }
    }
    return NULL;
}
/******************************************************************************/
void reader_pfring_start() {
    arkime_packet_set_dltsnap(DLT_EN10MB, config.snapLen);

    int i;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        char name[100];
        snprintf(name, sizeof(name), "arkime-pfring%d", i);
        g_thread_unref(g_thread_new(name, &reader_pfring_thread, (gpointer)(long)i));
    }
}
/******************************************************************************/
void reader_pfring_stop()
{

    int i;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        if (rings[i])
            pfring_breakloop(rings[i]);
    }
}
/******************************************************************************/
void reader_pfring_exit()
{

    int i;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        if (rings[i]) {
            pfring_close(rings[i]);
        }
    }
}
/******************************************************************************/
void reader_pfring_init(char *UNUSED(name))
{
    int flags = PF_RING_PROMISC | PF_RING_TIMESTAMP;
    int clusterId = arkime_config_int(NULL, "pfringClusterId", 0, 0, 255);

    int i;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        rings[i] = pfring_open(config.interface[i], config.snapLen, flags);

        if (config.bpf) {
            int err = pfring_set_bpf_filter(rings[i], config.bpf);

            if (err < 0) {
                CONFIGEXIT("pfring set filter error %d  for  %s", err, config.bpf);
            }
        }


        if (!rings[i]) {
            CONFIGEXIT("pfring open failed! - %s", config.interface[i]);
        }

        pfring_set_cluster(rings[i], clusterId, cluster_per_flow_5_tuple);
        pfring_set_application_name(rings[i], "arkime-capture");
        pfring_set_poll_watermark(rings[i], 64);
        pfring_enable_rss_rehash(rings[i]);
    }

    arkime_reader_start         = reader_pfring_start;
    arkime_reader_stop          = reader_pfring_stop;
    arkime_reader_stats         = reader_pfring_stats;
    arkime_reader_exit          = reader_pfring_exit;
}
/******************************************************************************/
void arkime_plugin_init()
{
    arkime_readers_add("pfring", reader_pfring_init);
}
