/* netflow.c  -- Simple plugin that sends netflow data
 *
 * https://www.plixer.com/support/netflow_v5.html
 * https://www.plixer.com/support/netflow_v7.html
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
#include "arkime.h"
#include "bsb.h"

extern struct timeval initialPacket;


typedef struct {
    int                 fd;
    struct sockaddr_in  addr;
    uint32_t            seq;
} NetflowDest_t;

LOCAL int           netflowVersion;
LOCAL int           netflowSNMPInput = 0;
LOCAL int           netflowSNMPOutput = 0;
LOCAL int           numDests = 0;
LOCAL NetflowDest_t dests[100];
LOCAL int           headerSize;

/******************************************************************************/

extern ArkimeConfig_t        config;

LOCAL struct timeval lastTime[ARKIME_MAX_PACKET_THREADS];
LOCAL char           buf[ARKIME_MAX_PACKET_THREADS][1500];
LOCAL BSB            bsb[ARKIME_MAX_PACKET_THREADS];
LOCAL int            bufCount[ARKIME_MAX_PACKET_THREADS];
LOCAL uint32_t       totalFlows[ARKIME_MAX_PACKET_THREADS];

/******************************************************************************/
LOCAL void netflow_send(const int thread)
{
    BSB hbsb;

    BSB_INIT(hbsb, buf[thread], headerSize);

    uint32_t sys_uptime = (lastTime[thread].tv_sec - initialPacket.tv_sec) * 1000 + (lastTime[thread].tv_usec - initialPacket.tv_usec) / 1000;

    /* Header */
    BSB_EXPORT_u16(hbsb, netflowVersion);
    BSB_EXPORT_u16(hbsb, bufCount[thread]); // count
    BSB_EXPORT_u32(hbsb, sys_uptime); // sys_uptime
    BSB_EXPORT_u32(hbsb, lastTime[thread].tv_sec);
    BSB_EXPORT_u32(hbsb, lastTime[thread].tv_usec * 1000);

    switch (netflowVersion) {
    case 5:
        BSB_EXPORT_u32(hbsb, totalFlows[thread]); // flow_sequence
        BSB_EXPORT_u08(hbsb, 0); // engine_type
        BSB_EXPORT_u08(hbsb, 0); // engine_id
        BSB_EXPORT_u16(hbsb, 0); // mode/interval
        break;
    case 7:
        BSB_EXPORT_u32(hbsb, totalFlows[thread]); // flow_sequence
        BSB_EXPORT_u32(hbsb, 0); // reserved
        break;
    }

    int i;
    for (i = 0; i < numDests; i++) {
        int rc;

        if ((rc = send(dests[i].fd, buf[thread], BSB_LENGTH(bsb[thread]) + headerSize, 0)) < BSB_LENGTH(bsb[thread]) + headerSize) {
            LOG("Failed to send rc=%d size=%u error=%s", rc, (uint32_t)BSB_LENGTH(bsb[thread]) + headerSize, strerror(errno));
        }
    }

    totalFlows[thread] += bufCount[thread];
    BSB_INIT(bsb[thread], buf[thread] + headerSize, sizeof(buf[thread]) - headerSize);
    bufCount[thread] = 0;
}
/******************************************************************************/
/*
 * Called by arkime when a session is about to be saved
 */
LOCAL void netflow_plugin_save(ArkimeSession_t *session, int UNUSED(final))
{
    static const char zero[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    const int thread = session->thread;

    if (bufCount[thread] == -1) {
        bufCount[thread] = 0;
        BSB_INIT(bsb[thread], buf[thread] + headerSize, sizeof(buf[thread]) - headerSize);
    } else if (bufCount[thread] > 20) {
        netflow_send(thread);
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
    if (!IN6_IS_ADDR_V4MAPPED(&session->addr1)) {
        return;
    }
#pragma GCC diagnostic pop

    if ((lastTime[thread].tv_sec < session->lastPacket.tv_sec) || (lastTime[thread].tv_sec == session->lastPacket.tv_sec && lastTime[thread].tv_usec < session->lastPacket.tv_usec)) {
        lastTime[thread] = session->lastPacket;
    }
    uint32_t first = (session->firstPacket.tv_sec - initialPacket.tv_sec) * 1000 + (session->firstPacket.tv_usec - initialPacket.tv_usec) / 1000;
    uint32_t last  = (session->lastPacket.tv_sec - initialPacket.tv_sec) * 1000 + (session->lastPacket.tv_usec - initialPacket.tv_usec) / 1000;

    if (session->packets[0]) {
        /* Body */
        BSB_EXPORT_ptr(bsb[thread], &ARKIME_V6_TO_V4(session->addr1), 4);
        BSB_EXPORT_ptr(bsb[thread], &ARKIME_V6_TO_V4(session->addr2), 4);
        BSB_EXPORT_u32(bsb[thread], 0); // nexthop
        BSB_EXPORT_u16(bsb[thread], netflowSNMPInput); // snmp input
        BSB_EXPORT_u16(bsb[thread], netflowSNMPOutput); // snmp output
        BSB_EXPORT_u32(bsb[thread], session->packets[0]);
        BSB_EXPORT_u32(bsb[thread], session->bytes[0]);
        BSB_EXPORT_u32(bsb[thread], first);
        BSB_EXPORT_u32(bsb[thread], last);
        BSB_EXPORT_u16(bsb[thread], session->port1);
        BSB_EXPORT_u16(bsb[thread], session->port2);

        switch (netflowVersion) {
        case 1:
            BSB_EXPORT_u08(bsb[thread], 0); // pad
            BSB_EXPORT_u08(bsb[thread], session->ipProtocol);
            BSB_EXPORT_u08(bsb[thread], session->ip_tos); // tos
            BSB_EXPORT_u08(bsb[thread], session->tcp_flags); // tcp_flags
            BSB_EXPORT_ptr(bsb[thread], zero, 8); //pad
            break;
        case 5:
            BSB_EXPORT_u08(bsb[thread], 0); // pad
            BSB_EXPORT_u08(bsb[thread], session->tcp_flags); // tcp_flags
            BSB_EXPORT_u08(bsb[thread], session->ipProtocol);
            BSB_EXPORT_u08(bsb[thread], session->ip_tos); // tos
            BSB_EXPORT_u16(bsb[thread], 0); // src as
            BSB_EXPORT_u16(bsb[thread], 0); // dst as
            BSB_EXPORT_u08(bsb[thread], 0); // src_mask
            BSB_EXPORT_u08(bsb[thread], 0); // dst_mask
            BSB_EXPORT_ptr(bsb[thread], zero, 2); //pad
            break;
        case 7:
            BSB_EXPORT_u08(bsb[thread], 0); // pad
            BSB_EXPORT_u08(bsb[thread], session->tcp_flags);
            BSB_EXPORT_u08(bsb[thread], session->ipProtocol);
            BSB_EXPORT_u08(bsb[thread], session->ip_tos);
            BSB_EXPORT_u16(bsb[thread], 0); // src as
            BSB_EXPORT_u16(bsb[thread], 0); // dst as
            BSB_EXPORT_u08(bsb[thread], 0); // src_mask
            BSB_EXPORT_u08(bsb[thread], 0); // dst_mask
            BSB_EXPORT_u16(bsb[thread], 0); // flags
            BSB_EXPORT_u32(bsb[thread], 0); // router_sc
            break;
        }
        bufCount[thread]++;
    }

    if (session->packets[1]) {
        /* Body */
        BSB_EXPORT_ptr(bsb[thread], &ARKIME_V6_TO_V4(session->addr2), 4);
        BSB_EXPORT_ptr(bsb[thread], &ARKIME_V6_TO_V4(session->addr1), 4);
        BSB_EXPORT_u32(bsb[thread], 0); // nexthop
        BSB_EXPORT_u16(bsb[thread], netflowSNMPInput); // snmp input
        BSB_EXPORT_u16(bsb[thread], netflowSNMPOutput); // snmp output
        BSB_EXPORT_u32(bsb[thread], session->packets[1]);
        BSB_EXPORT_u32(bsb[thread], session->bytes[1]);
        BSB_EXPORT_u32(bsb[thread], first);
        BSB_EXPORT_u32(bsb[thread], last);
        BSB_EXPORT_u16(bsb[thread], session->port2);
        BSB_EXPORT_u16(bsb[thread], session->port1);

        switch (netflowVersion) {
        case 1:
            BSB_EXPORT_u08(bsb[thread], 0); // pad
            BSB_EXPORT_u08(bsb[thread], session->ipProtocol);
            BSB_EXPORT_u08(bsb[thread], session->ip_tos); // tos
            BSB_EXPORT_u08(bsb[thread], session->tcp_flags); // tcp_flags
            BSB_EXPORT_ptr(bsb[thread], zero, 8); //pad
            break;
        case 5:
            BSB_EXPORT_u08(bsb[thread], 0); // pad
            BSB_EXPORT_u08(bsb[thread], session->tcp_flags); // tcp_flags
            BSB_EXPORT_u08(bsb[thread], session->ipProtocol);
            BSB_EXPORT_u08(bsb[thread], session->ip_tos); // tos
            BSB_EXPORT_u16(bsb[thread], 0); // src as
            BSB_EXPORT_u16(bsb[thread], 0); // dst as
            BSB_EXPORT_u08(bsb[thread], 0); // src_mask
            BSB_EXPORT_u08(bsb[thread], 0); // dst_mask
            BSB_EXPORT_ptr(bsb[thread], zero, 2); //pad
            break;
        case 7:
            BSB_EXPORT_u08(bsb[thread], 0); // pad
            BSB_EXPORT_u08(bsb[thread], session->tcp_flags);
            BSB_EXPORT_u08(bsb[thread], session->ipProtocol);
            BSB_EXPORT_u08(bsb[thread], session->ip_tos);
            BSB_EXPORT_u16(bsb[thread], 0); // src as
            BSB_EXPORT_u16(bsb[thread], 0); // dst as
            BSB_EXPORT_u08(bsb[thread], 0); // src_mask
            BSB_EXPORT_u08(bsb[thread], 0); // dst_mask
            BSB_EXPORT_u16(bsb[thread], 0); // flags
            BSB_EXPORT_u32(bsb[thread], 0); // router_sc
            break;
        }
        bufCount[thread]++;
    }
}

/******************************************************************************/
/*
 * Called by arkime when arkime is quiting
 */
LOCAL void netflow_plugin_exit()
{
    int thread;
    for (thread = 0; thread < config.packetThreads; thread++) {
        netflow_send(thread);
    }
}

/******************************************************************************/
/*
 * Called by arkime when the plugin is loaded
 */
void arkime_plugin_init()
{
    arkime_plugins_register("netflow", FALSE);

    arkime_plugins_set_cb("netflow",
                          NULL,
                          NULL,
                          NULL,
                          NULL,
                          netflow_plugin_save,
                          NULL,
                          netflow_plugin_exit,
                          NULL
                         );

    netflowSNMPInput = arkime_config_int(NULL, "netflowSNMPInput", 0, 0, 0xffffffff);
    netflowSNMPOutput = arkime_config_int(NULL, "netflowSNMPOutput", 0, 0, 0xffffffff);
    netflowVersion = arkime_config_int(NULL, "netflowVersion", 5, 1, 7);
    LOG("version = %d", netflowVersion);
    if (netflowVersion != 1 && netflowVersion != 5 && netflowVersion != 7) {
        CONFIGEXIT("Unsupported netflowVersion: %d", netflowVersion);
    }

    if (netflowVersion == 1)
        headerSize = 16;
    else
        headerSize = 24;

    char **dsts = arkime_config_str_list(NULL, "netflowDestinations", NULL);
    if (dsts == NULL || dsts[0] == NULL || dsts[0][0] == 0) {
        CONFIGEXIT("netflowDestinations must be set");
    }

    int i;
    for (i = 0; i < 100 && dsts[i]; i++) {
        char *colon = strchr(dsts[i], ':');
        if (!colon) {
            CONFIGEXIT("netflowDestination (%s) needs a destination port", dsts[i]);
        }
        *colon = 0;

        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        if (getaddrinfo(dsts[i], colon + 1, &hints, &res)) {
            CONFIGEXIT("Failed looking up %s:%s", dsts[i], colon + 1);
        }

        dests[numDests].addr = *((struct sockaddr_in *) res->ai_addr);
        dests[numDests].seq = 0;

        if ((dests[numDests].fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            CONFIGEXIT("Socket failed: %s", strerror(errno));
        }

        if (connect(dests[numDests].fd, (struct sockaddr *) &dests[numDests].addr, sizeof(struct sockaddr_in))) {
            LOGEXIT("ERROR - Connect failed: %s", strerror(errno));
        }
        numDests++;
        freeaddrinfo(res);
    }
    g_strfreev(dsts);

    int thread;
    for (thread = 0; thread < config.packetThreads; thread++) {
        bufCount[thread] = -1;
    }
}
