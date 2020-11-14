/******************************************************************************/
/* reader-pcapoverip.c  -- Reader supporting pcap-over-ip
 *
 * readerMethod=pcap-over-ip-client will connect out to pcapOverIpHost:pcapOverIpPort
 *   and expect to receive 1 PCAP file to process
 * readerMethod=pcap-over-ip-server will listen to pcapOverIpPort
 *   and expect to receive 1 PCAP file to process per connection
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
 */
#include <sys/socket.h>
#include <arpa/inet.h>
#include "gio/gio.h"
#include "glib-object.h"
#include "pcap.h"
#include "moloch.h"
extern MolochPcapFileHdr_t   pcapFileHeader;
extern MolochConfig_t        config;

LOCAL MolochPacketBatch_t   batch;
LOCAL uint64_t              packets;

LOCAL char                 *host;
LOCAL int                   port;

typedef struct {
    GSocket                *client;
    char                    data[0xffff];
    int                     len;
    int                     readWatch;
    uint16_t                state:1;
    uint16_t                bigEndian:1;
} POIClient_t;

/******************************************************************************/
void pcapoverip_client_free (POIClient_t *poic)
{
    g_source_remove(poic->readWatch);
    g_object_unref (poic->client);
    MOLOCH_TYPE_FREE(POIClient_t, poic);
}
/******************************************************************************/
gboolean pcapoverip_client_read_cb(gint UNUSED(fd), GIOCondition cond, gpointer data) {
    POIClient_t *poic = (POIClient_t *)data;

    //LOG("fd: %d cond: %x data: %p", fd, cond, data);
    GError              *error = 0;

    int len = g_socket_receive(poic->client, poic->data + poic->len, sizeof(poic->data) - poic->len, NULL, &error);

    if (error || cond & (G_IO_HUP | G_IO_ERR) || len <= 0) {
        if (error) {
            LOG("ERROR: Receive Error: %s", error->message);
            g_error_free(error);
        }
        // TODO: In client mode should we quit and/or reconnect on disconnection?
        pcapoverip_client_free(poic);
        return FALSE;
    }
    poic->len += len;
    int pos = 0;
    while (pos < poic->len) {
        if (poic->state == 0) {
            if (poic->len - pos < 24) // Not enough for pcap file header
                break;
            if (memcmp(poic->data + pos, "\xa1\xb2\xc3\xd4", 4) == 0)
                poic->bigEndian = 1;
            else if (memcmp(poic->data + pos, "\xd4\xc3\xb2\xa1", 4) == 0)
                poic->bigEndian = 0;
            else {
                pcapoverip_client_free(poic);
                return FALSE;
            }
            // TODO: Really we should save the header per connection and do stuff
            // TODO: Setup bpf filter
            poic->state = 1;
            pos += 24;
            continue;
        }

        if (poic->len - pos < 16) // Not enough for packet header
            break;

        BSB bsb;
        BSB_INIT(bsb, poic->data + pos , poic->len - pos);

        MolochPacket_t *packet = MOLOCH_TYPE_ALLOC0(MolochPacket_t);

        uint32_t caplen = 0;
        uint32_t origlen = 0;
        if (poic->bigEndian) {
            BSB_IMPORT_u32(bsb, packet->ts.tv_sec);
            BSB_IMPORT_u32(bsb, packet->ts.tv_usec);
            BSB_IMPORT_u32(bsb, caplen);
            BSB_IMPORT_u32(bsb, origlen);
        } else {
            BSB_LIMPORT_u32(bsb, packet->ts.tv_sec);
            BSB_LIMPORT_u32(bsb, packet->ts.tv_usec);
            BSB_LIMPORT_u32(bsb, caplen);
            BSB_LIMPORT_u32(bsb, origlen);
        }

        if (unlikely(caplen != origlen)) {
            if (!config.readTruncatedPackets && !config.ignoreErrors) {
                LOGEXIT("ERROR - Moloch requires full packet captures caplen: %d pktlen: %d. "
                    "If using tcpdump use the \"-s0\" option, or set readTruncatedPackets in ini file",
                    caplen, origlen);
            }
        }

        if (poic->len - pos < 16 + caplen) { // Not enough data for packet
            MOLOCH_TYPE_FREE(MolochPacket_t, packet);
            break;
        }

        packet->pktlen = caplen;
        packet->pkt           = (u_char *)poic->data + pos + 16;
        packet->readerPos     = 0;

        // TODO: run config.bpf before adding packet to batch
        moloch_packet_batch(&batch, packet);

        pos += 16 + caplen;
        packets++;
    }

    if (pos > 0) { // We processed some of the buffer!
        if (poic->state == 1) {
            moloch_packet_batch_flush(&batch);
            moloch_packet_batch_init(&batch);
        }
        memmove(poic->data, poic->data + pos, poic->len - pos);
        poic->len -= pos;
    }

    return TRUE;
}
/******************************************************************************/
LOCAL void pcapoverip_client_connect() {
    GError                   *error = NULL;
    GSocketConnectable       *connectable;
    GSocketAddressEnumerator *enumerator;
    GSocketAddress           *sockaddr;

    connectable = g_network_address_parse(host, port, &error);

    if (error) {
        LOGEXIT("Couldn't parse connect string of %s", host);
    }

    enumerator = g_socket_connectable_enumerate (connectable);
    g_object_unref(connectable);

    GSocket *conn = NULL;
    while (!conn && (sockaddr = g_socket_address_enumerator_next (enumerator, NULL, &error)))
    {
        conn = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, &error);

        if (!error) {
            g_socket_set_blocking (conn, TRUE);
            g_socket_connect(conn, sockaddr, NULL, &error);
        }

        if (error && error->code != G_IO_ERROR_PENDING) {
            g_object_unref (conn);
            conn = NULL;
        } else {
            g_socket_set_blocking (conn, FALSE);
            struct sockaddr_in localAddress, remoteAddress;
            socklen_t addressLength = sizeof(localAddress);
            getsockname(g_socket_get_fd(conn), (struct sockaddr*)&localAddress, &addressLength);
            g_socket_address_to_native(sockaddr, &remoteAddress, addressLength, NULL);
            LOG("connected %s:%d", host, port);
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
        LOG("Error: %s", error->message);
    }

    if (error || !conn) {
        LOGEXIT("Couldn't connect %s %d", host, port);
    }

    g_socket_set_keepalive(conn, TRUE);
    int fd = g_socket_get_fd(conn);

    POIClient_t *poic = MOLOCH_TYPE_ALLOC0(POIClient_t);
    poic->client = conn;
    poic->readWatch = moloch_watch_fd(fd, MOLOCH_GIO_READ_COND, pcapoverip_client_read_cb, poic);
}
/******************************************************************************/
LOCAL void pcapoverip_client_start()
{
    pcapoverip_client_connect();
}
/******************************************************************************/
gboolean pcapoverip_server_read_cb(gint UNUSED(fd), GIOCondition cond, gpointer data)
{
    GError                   *error = NULL;

    GSocket *client = g_socket_accept((GSocket *)data, NULL, &error);
    if (!client || error) {
        LOGEXIT("Error accepting pcap-over-ip: %s", error->message);
    }

    POIClient_t *poic = MOLOCH_TYPE_ALLOC0(POIClient_t);
    poic->client = client;

    int cfd = g_socket_get_fd(client);
    poic->readWatch = moloch_watch_fd(cfd, MOLOCH_GIO_READ_COND, pcapoverip_client_read_cb, poic);
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
        LOGEXIT("Error creating pcap-over-ip: %s", error->message);
    }

    g_socket_set_blocking (socket, FALSE);
    addr = g_inet_socket_address_new (g_inet_address_new_any (G_SOCKET_FAMILY_IPV4), port);

    if (!g_socket_bind (socket, addr, TRUE, &error)) {
        LOGEXIT("Error binding pcap-over-ip: %s", error->message);
    }
    g_object_unref (addr);

    if (!g_socket_listen (socket, &error)) {
        LOGEXIT("Error listening pcap-over-ip: %s", error->message);
    }

    int fd = g_socket_get_fd(socket);

    moloch_watch_fd(fd, MOLOCH_GIO_READ_COND, pcapoverip_server_read_cb, socket);
    moloch_packet_set_dltsnap(DLT_EN10MB, config.snapLen);
}
/******************************************************************************/
LOCAL int pcapoverip_stats(MolochReaderStats_t *stats)
{
    stats->dropped = 0;
    stats->total = packets;
    return 0;
}
/******************************************************************************/
void reader_pcapoverip_init(char *name)
{
    host        = moloch_config_str(NULL, "pcapOverIpHost", "localhost");
    port        = moloch_config_int(NULL, "pcapOverIpPort", 57012, 1, 0xffff);

    if (strcmp(name, "pcapoveripclient") == 0 && strcmp(name, "pcap-over-ip-client") == 0) {
        moloch_reader_start         = pcapoverip_client_start;
    } else {
        moloch_reader_start         = pcapoverip_server_start;
    }
    moloch_reader_stats         = pcapoverip_stats;
    moloch_packet_batch_init(&batch);
}
