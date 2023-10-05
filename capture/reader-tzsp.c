/******************************************************************************/
/* reader-tzsp.c  -- Reader supporting tzsp
 *
 * readerMethod=tzsp
 *
 * Copyright 2022 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <sys/socket.h>
#include <arpa/inet.h>
#include "gio/gio.h"
#include "glib-object.h"
#include "pcap.h"
#include "arkime.h"
extern ArkimePcapFileHdr_t   pcapFileHeader;
extern ArkimeConfig_t        config;

LOCAL uint64_t              dropped;
LOCAL uint64_t              packets;

LOCAL int                   tzspPort;

LOCAL struct bpf_program    bpfp;
LOCAL pcap_t               *deadPcap;

/******************************************************************************/
LOCAL void *tzsp_thread(gpointer UNUSED(uw))
{
    GError                   *error = NULL;
    GSocket                  *socket;
    GSocketAddress           *addr;

    socket = g_socket_new (G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_DATAGRAM, G_SOCKET_PROTOCOL_UDP, &error);

    if (!socket || error) {
        CONFIGEXIT("Error creating tzsp: %s", error->message);
    }

    addr = g_inet_socket_address_new (g_inet_address_new_any (G_SOCKET_FAMILY_IPV4), tzspPort);

    if (!g_socket_bind (socket, addr, TRUE, &error)) {
        CONFIGEXIT("Error binding tzsp: %s", error->message);
    }

    g_object_unref (addr);

    ArkimePacketBatch_t   batch;
    arkime_packet_batch_init(&batch);
    while (TRUE) {
        gchar             buf[0xffff];

        gsize len = g_socket_receive(socket, buf, sizeof(buf), NULL, &error);
        if (error) {
            LOG("Error: %s", error->message);
            g_error_free(error);
            dropped++;
            continue;
        }
        
        if (len <= 10) {
            dropped++;
            continue;
        }

        BSB bsb;
        BSB_INIT(bsb, buf, len);

        int version = 0;
        BSB_IMPORT_u08(bsb, version);
        if (version != 1) { // Only support version 1
            dropped++;
            continue;
        }

        int type = 0;
        BSB_IMPORT_u08(bsb, type);

        if (type > 1) { // Only support RECEIVED/TRANSMIT
            dropped++;
            continue;
        }

        int encap = 0;
        BSB_IMPORT_u16(bsb, encap);
        if (encap != 1) { // Only support 1 Ethernet
            dropped++;
            continue;
        }

        while (!BSB_IS_ERROR(bsb) && BSB_REMAINING(bsb) > 2) {
            int tag = 0;
            BSB_IMPORT_u08(bsb, tag);
            if (tag == 0) // PADDING
                continue;
            if (tag == 1) // END
                break;

            int taglen = 0;
            BSB_IMPORT_u08(bsb, taglen);
            BSB_IMPORT_skip(bsb, taglen);
        }
        
        if (BSB_IS_ERROR(bsb) || BSB_REMAINING(bsb) < 6) {
            dropped++;
            continue;
        }

        packets++;

        if (config.bpf && bpf_filter(bpfp.bf_insns, BSB_WORK_PTR(bsb), BSB_REMAINING(bsb), BSB_REMAINING(bsb))) {
            // Not dropped
            continue;
        }

        ArkimePacket_t *packet = ARKIME_TYPE_ALLOC0(ArkimePacket_t);
        packet->pktlen        = BSB_REMAINING(bsb);
        packet->pkt           = BSB_WORK_PTR(bsb);
        packet->readerPos     = 0;
        gettimeofday(&packet->ts, NULL);

        arkime_packet_batch(&batch, packet);
        arkime_packet_batch_flush(&batch);
    }
}

/******************************************************************************/
LOCAL void tzsp_server_start()
{
    g_thread_unref(g_thread_new("reader-tzsp", &tzsp_thread, NULL));
}
/******************************************************************************/
LOCAL int tzsp_stats(ArkimeReaderStats_t *stats)
{
    stats->dropped = dropped;
    stats->total = packets;
    return 0;
}
/******************************************************************************/
void reader_tzsp_init(char *UNUSED(name))
{
    tzspPort = arkime_config_int(NULL, "tzspPort", 37008, 1, 0xffff);

    arkime_reader_start         = tzsp_server_start;
    arkime_reader_stats         = tzsp_stats;
    deadPcap = pcap_open_dead(DLT_EN10MB, config.snapLen);
    if (config.bpf) {
        if (pcap_compile(deadPcap, &bpfp, config.bpf, 1, PCAP_NETMASK_UNKNOWN) == -1) {
            CONFIGEXIT("Couldn't compile bpf filter '%s' with %s", config.bpf, pcap_geterr(deadPcap));
        }
    }

    arkime_packet_set_dltsnap(DLT_EN10MB, config.snapLen);
}
