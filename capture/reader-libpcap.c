/******************************************************************************/
/* reader-libpcap.c  -- Reader using libpcap
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define _FILE_OFFSET_BITS 64
#include "arkime.h"
#include <errno.h>
#include <sys/stat.h>
#include <gio/gio.h>
#include "pcap.h"

extern ArkimeConfig_t        config;

LOCAL  pcap_t               *pcaps[MAX_INTERFACES];

/******************************************************************************/
int reader_libpcap_stats(ArkimeReaderStats_t *stats)
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
        LOGEXIT("ERROR - Arkime requires full packet captures caplen: %d pktlen: %d\n"
            "See https://arkime.com/faq#arkime_requires_full_packet_captures_error",
            h->caplen, h->len);
    }

    ArkimePacket_t *packet = ARKIME_TYPE_ALLOC0(ArkimePacket_t);

    packet->pkt           = (u_char *)bytes;
    /* libpcap casts to int32_t which sign extends, undo that */
    packet->ts.tv_sec     = (uint32_t)h->ts.tv_sec;
    packet->ts.tv_usec    = h->ts.tv_usec;
    packet->pktlen        = h->len;
    packet->readerPos     = ((ArkimePacketBatch_t *)batch)->readerPos;

    arkime_packet_batch((ArkimePacketBatch_t *)batch, packet);
}
/******************************************************************************/
LOCAL void *reader_libpcap_thread(gpointer posv)
{
    long    pos = (long)posv;
    pcap_t *pcap = pcaps[pos];
    if (config.debug)
        LOG("THREAD %p", (gpointer)pthread_self());

    ArkimePacketBatch_t   batch;
    arkime_packet_batch_init(&batch);
    batch.readerPos = pos;
    while (1) {
        int r = pcap_dispatch(pcap, 10000, reader_libpcap_pcap_cb, (u_char*)&batch);
        arkime_packet_batch_flush(&batch);

        // Some kind of failure we quit
        if (unlikely(r < 0)) {
            arkime_quit();
            break;
        }
    }
    //ALW - Need to close after packet finishes
    //pcap_close(pcap);
    return NULL;
}
/******************************************************************************/
void reader_libpcap_start() {
    //ALW - Bug: assumes all linktypes are the same
    arkime_packet_set_dltsnap(pcap_datalink(pcaps[0]), pcap_snapshot(pcaps[0]));

    int i;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        if (config.bpf) {
            struct bpf_program   bpf;

            if (pcap_compile(pcaps[i], &bpf, config.bpf, 1, PCAP_NETMASK_UNKNOWN) == -1) {
                CONFIGEXIT("Couldn't compile bpf filter: '%s' with %s", config.bpf, pcap_geterr(pcaps[i]));
            }

            if (pcap_setfilter(pcaps[i], &bpf) == -1) {
                CONFIGEXIT("Couldn't set bpf filter: '%s' with %s", config.bpf, pcap_geterr(pcaps[i]));
            }
            pcap_freecode(&bpf);
        }

        char name[100];
        snprintf(name, sizeof(name), "arkime-pcap%d", i);
        g_thread_unref(g_thread_new(name, &reader_libpcap_thread, (gpointer)(long)i));
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
        pcaps[i] = pcap_open_live(config.interface[i], config.snapLen, 1, 1000, errbuf);
#else
        pcaps[i] = reader_libpcap_open_live(config.interface[i], config.snapLen, 1, 1000, errbuf);
#endif

        if (!pcaps[i]) {
            CONFIGEXIT("pcap open live failed! %s", errbuf);
        }

        pcap_setnonblock(pcaps[i], FALSE, errbuf);
    }

    if (i == MAX_INTERFACES && config.interface[MAX_INTERFACES]) {
        CONFIGEXIT("Only support up to %d interfaces", MAX_INTERFACES);
    }

    arkime_reader_start         = reader_libpcap_start;
    arkime_reader_stop          = reader_libpcap_stop;
    arkime_reader_stats         = reader_libpcap_stats;
}
