/******************************************************************************/
/* reader-pcapoverip.c  -- Reader supporting pcap-over-ip
 *
 * readerMethod=pcap-over-ip-client will connect out to host:port list in interface
 *   and expect to receive 1 PCAP file to process
 *   Test with nc -l 57013 < tests/pcap/bigendian.pcap
 * readerMethod=pcap-over-ip-server will listen to pcapOverIpPort
 *   and expect to receive 1 PCAP file to process per connection
 *   Test with nc localhost:57012 < tests/pcap/bigendian.pcap
 *
 * Copyright 2020 AOL Inc. All rights reserved.
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

LOCAL ArkimePacketBatch_t   batch;
LOCAL uint64_t              packets;

LOCAL int                   port;

LOCAL struct bpf_program    bpfp;
LOCAL pcap_t               *deadPcap;

typedef struct {
    GSocket                *socket;
    char                    data[ARKIME_PACKET_MAX_LEN + 24];
    uint32_t                len;
    int                     readWatch;
    int                     interface;
    uint16_t                state: 1;
    uint16_t                needSwap: 1;
    uint16_t                isClient: 1;
} POIClient_t;

LOCAL int                   isConnected[MAX_INTERFACES];

#define SWAP32(x) ((((x)&0xff000000) >> 24) | (((x)&0x00ff0000) >> 8) | (((x)&0x0000ff00) << 8) | (((x)&0x000000ff) << 24))
#define SWAP16(x) ((((x)&0xff00) >> 8) | (((x)&0x00ff) << 8))

/******************************************************************************/
LOCAL void pcapoverip_client_free (POIClient_t *poic)
{
    if (poic->isClient) {
        isConnected[poic->interface] = 0;
    }
    g_source_remove(poic->readWatch);
    g_object_unref (poic->socket);

    ARKIME_TYPE_FREE(POIClient_t, poic);
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL gboolean pcapoverip_client_read_cb(gint UNUSED(fd), GIOCondition cond, gpointer data)
{
    POIClient_t *poic = (POIClient_t *)data;

    //LOG("fd: %d cond: %x data: %p", fd, cond, data);
    GError              *error = 0;
    static int           first = 1;

    int len = g_socket_receive(poic->socket, poic->data + poic->len, sizeof(poic->data) - poic->len, NULL, &error);

    if (error || cond & (G_IO_HUP | G_IO_ERR) || len <= 0) {
        if (error) {
            LOG("ERROR: Receive Error: %s", error->message);
            g_error_free(error);
        }
        pcapoverip_client_free(poic);
        return FALSE;
    }
    poic->len += len;
    uint32_t pos = 0;
    while (pos < poic->len) {
        if (poic->state == 0) {
            if (poic->len - pos < 24) // Not enough for pcap file header
                break;

            ArkimePcapFileHdr_t *h = (ArkimePcapFileHdr_t *)(poic->data + pos);

            if (h->magic != 0xa1b2c3d4 && h->magic != 0xd4c3b2a1 &&
                h->magic != 0xa1b23c4d && h->magic != 0x4d3cb2a1) {
                LOG("ERROR - Unknown magic %xs", h->magic);
                return FALSE;
            }

            poic->needSwap = (h->magic == 0xd4c3b2a1 || h->magic == 0x4d3cb2a1);

            // TODO: Really we should save the header per connection and do stuff
            if (first) {
                if (poic->needSwap) {
                    h->dlt = SWAP32(h->dlt);
                }
                arkime_packet_set_dltsnap(h->dlt, config.snapLen);

                if (config.bpf && !deadPcap) {
                    deadPcap = pcap_open_dead(h->dlt, config.snapLen);
                    if (pcap_compile(deadPcap, &bpfp, config.bpf, 1, PCAP_NETMASK_UNKNOWN) == -1) {
                        CONFIGEXIT("Couldn't compile bpf filter '%s' with %s", config.bpf, pcap_geterr(deadPcap));
                    }
                }
                first = 0;
            }
            poic->state = 1;
            pos += 24;
            continue;
        }

        if (poic->len - pos < 16) // Not enough for packet header
            break;

        struct arkime_pcap_sf_pkthdr *ph = (struct arkime_pcap_sf_pkthdr *)(poic->data + pos);

        ArkimePacket_t *packet = ARKIME_TYPE_ALLOC0(ArkimePacket_t);
        uint32_t origlen = 0;
        uint32_t caplen = 0;
        if (poic->needSwap) {
            caplen = SWAP32(ph->caplen);
            origlen = SWAP32(ph->pktlen);
            packet->ts.tv_sec = SWAP32(ph->ts.tv_sec);
            packet->ts.tv_usec = SWAP32(ph->ts.tv_usec);
        } else {
            caplen = ph->caplen;
            origlen = ph->pktlen;
            packet->ts.tv_sec = ph->ts.tv_sec;
            packet->ts.tv_usec = ph->ts.tv_usec;
        }

        if (unlikely(caplen != origlen) && config.readTruncatedPackets && !config.ignoreErrors) {
            LOGEXIT("ERROR - Arkime requires full packet captures caplen: %u pktlen: %u\n"
                    "See https://arkime.com/faq#arkime_requires_full_packet_captures_error",
                    caplen, origlen);
        }

        if (unlikely(caplen > ARKIME_PACKET_MAX_LEN)) {
            if (!config.ignoreErrors) {
                LOGEXIT("ERROR - The packet length %u is too large.", caplen);
            } else {
                ARKIME_TYPE_FREE(ArkimePacket_t, packet);
                pcapoverip_client_free(poic);
                return FALSE;
            }

        }

        if (poic->len - pos < 16 + caplen) { // Not enough data for packet
            ARKIME_TYPE_FREE(ArkimePacket_t, packet);
            break;
        }

        packet->pktlen        = caplen;
        packet->pkt           = (u_char *)poic->data + pos + 16;
        packet->readerPos     = poic->interface;

        if (config.bpf && bpf_filter(bpfp.bf_insns, packet->pkt, packet->pktlen, packet->pktlen)) {
            ARKIME_TYPE_FREE(ArkimePacket_t, packet);
        } else {
            arkime_packet_batch(&batch, packet);
        }

        pos += 16 + caplen;
        packets++;
    }

    if (pos > 0) { // We processed some of the buffer!
        if (poic->state == 1) {
            arkime_packet_batch_flush(&batch);
        }
        memmove(poic->data, poic->data + pos, poic->len - pos);
        poic->len -= pos;
    }

    return TRUE;
}
/******************************************************************************/
LOCAL void pcapoverip_client_connect(int interface)
{
    GError                   *error = NULL;
    GSocketConnectable       *connectable;
    GSocketAddressEnumerator *enumerator;
    GSocketAddress           *sockaddr;

    connectable = g_network_address_parse(config.interface[interface], port, &error);

    if (error) {
        LOGEXIT("ERROR - Couldn't parse connect string of %s", config.interface[interface]);
    }

    enumerator = g_socket_connectable_enumerate (connectable);
    g_object_unref(connectable);

    GSocket *conn = NULL;
    while (!conn && (sockaddr = g_socket_address_enumerator_next (enumerator, NULL, &error))) {
        conn = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, &error);

        if (!error) {
            g_socket_set_blocking (conn, TRUE);
            g_socket_connect(conn, sockaddr, NULL, &error);
        }

        if (error && error->code != G_IO_ERROR_PENDING) {
            g_error_free(error);
            error = 0;
            g_object_unref (conn);
            conn = NULL;
        } else {
            g_socket_set_blocking (conn, FALSE);
            struct sockaddr_in localAddress, remoteAddress;
            socklen_t addressLength = sizeof(localAddress);
            getsockname(g_socket_get_fd(conn), (struct sockaddr *)&localAddress, &addressLength);
            g_socket_address_to_native(sockaddr, &remoteAddress, addressLength, NULL);
            if (config.debug > 0)
                LOG("connected %s:%d", config.interface[interface], port);
        }
        g_object_unref (sockaddr);
    }
    g_object_unref (enumerator);

    if (conn) {
        if (error) {
            g_error_free(error);
            error = 0;
        }
    } else if (error) {
        if (config.debug > 0)
            LOG("%s Error: %s", config.interface[interface], error->message);
    }

    if (error || !conn) {
        return;
    }

    g_socket_set_keepalive(conn, TRUE);
    int fd = g_socket_get_fd(conn);

    POIClient_t *poic = ARKIME_TYPE_ALLOC0(POIClient_t);
    poic->interface = interface;
    poic->isClient = 1;
    poic->socket = conn;
    poic->readWatch = arkime_watch_fd(fd, ARKIME_GIO_READ_COND, pcapoverip_client_read_cb, poic);
    isConnected[poic->interface] = 1;
}
/******************************************************************************/
LOCAL gboolean pcapoverip_client_check_connections (gpointer UNUSED(user_data))
{
    int i;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        if (!isConnected[i])
            pcapoverip_client_connect(i);
    }
    return G_SOURCE_CONTINUE;
}
/******************************************************************************/
LOCAL void pcapoverip_client_start()
{
    pcapoverip_client_check_connections(NULL);
    g_timeout_add_seconds(5, pcapoverip_client_check_connections, NULL);
    arkime_packet_set_dltsnap(DLT_EN10MB, config.snapLen);
}
/******************************************************************************/
LOCAL gboolean pcapoverip_server_read_cb(gint UNUSED(fd), GIOCondition UNUSED(cond), gpointer data)
{
    GError                   *error = NULL;

    GSocket *client = g_socket_accept((GSocket *)data, NULL, &error);
    if (!client || error) {
        LOGEXIT("ERROR - Error accepting pcap-over-ip: %s", error->message);
    }

    POIClient_t *poic = ARKIME_TYPE_ALLOC0(POIClient_t);
    poic->socket = client;

    int cfd = g_socket_get_fd(client);
    poic->readWatch = arkime_watch_fd(cfd, ARKIME_GIO_READ_COND, pcapoverip_client_read_cb, poic);
    return TRUE;
}
/******************************************************************************/
LOCAL void pcapoverip_server_start()
{
    GError                   *error = NULL;
    GSocket                  *socket;
    GSocketAddress           *addr;

    socket = g_socket_new (G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_STREAM, 0, &error);

    if (!socket || error) {
        CONFIGEXIT("Error creating pcap-over-ip: %s", error->message);
    }

    g_socket_set_blocking (socket, FALSE);
    addr = g_inet_socket_address_new (g_inet_address_new_any (G_SOCKET_FAMILY_IPV4), port);

    if (!g_socket_bind (socket, addr, TRUE, &error)) {
        CONFIGEXIT("Error binding pcap-over-ip: %s", error->message);
    }
    g_object_unref (addr);

    if (!g_socket_listen (socket, &error)) {
        CONFIGEXIT("Error listening pcap-over-ip: %s", error->message);
    }

    int fd = g_socket_get_fd(socket);

    arkime_watch_fd(fd, ARKIME_GIO_READ_COND, pcapoverip_server_read_cb, socket);
    arkime_packet_set_dltsnap(DLT_EN10MB, config.snapLen);
}
/******************************************************************************/
LOCAL int pcapoverip_stats(ArkimeReaderStats_t *stats)
{
    stats->dropped = 0;
    stats->total = packets;
    return 0;
}
/******************************************************************************/
void reader_pcapoverip_init(const char *name)
{
    port        = arkime_config_int(NULL, "pcapOverIpPort", 57012, 1, 0xffff);

    if (strcmp(name, "pcapoveripclient") == 0 || strcmp(name, "pcap-over-ip-client") == 0) {
        arkime_reader_start         = pcapoverip_client_start;
    } else {
        arkime_reader_start         = pcapoverip_server_start;
    }
    arkime_reader_stats         = pcapoverip_stats;
    arkime_packet_batch_init(&batch);
}
