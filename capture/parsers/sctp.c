/* sctp.c
 *
 * Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"
#include "patricia.h"
#include <inttypes.h>
#include <arpa/inet.h>
#include <errno.h>


/******************************************************************************/
extern ArkimeConfig_t        config;

LOCAL  int                   sctpMProtocol;
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL ArkimePacketRC sctp_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *UNUSED(data), int UNUSED(len))
{
    uint8_t             sessionId[ARKIME_SESSIONID_LEN];
    struct udphdr       *udphdr = (struct udphdr *)(packet->pkt + packet->payloadOffset); /* Not really udp, but port in same location */

    if (packet->payloadLen < (int)sizeof(struct udphdr))
        return ARKIME_PACKET_CORRUPT;

    if (packet->v6) {
        struct ip6_hdr *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);
        arkime_session_id6(sessionId, ip6->ip6_src.s6_addr, udphdr->uh_sport,
                           ip6->ip6_dst.s6_addr, udphdr->uh_dport);
    } else {
        struct ip *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
        arkime_session_id(sessionId, ip4->ip_src.s_addr, udphdr->uh_sport,
                          ip4->ip_dst.s_addr, udphdr->uh_dport);
    }
    packet->mProtocol = sctpMProtocol;
    packet->hash = arkime_session_hash(sessionId);
    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL void sctp_create_sessionid(uint8_t *sessionId, ArkimePacket_t *packet)
{
    struct udphdr       *udphdr = (struct udphdr *)(packet->pkt + packet->payloadOffset); /* Not really udp, but port in same location */

    if (packet->v6) {
        struct ip6_hdr *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);
        arkime_session_id6(sessionId, ip6->ip6_src.s6_addr, udphdr->uh_sport,
                           ip6->ip6_dst.s6_addr, udphdr->uh_dport);
    } else {
        struct ip *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
        arkime_session_id(sessionId, ip4->ip_src.s_addr, udphdr->uh_sport,
                          ip4->ip_dst.s_addr, udphdr->uh_dport);
    }
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL int sctp_pre_process(ArkimeSession_t *session, ArkimePacket_t *const packet, int isNewSession)
{
    struct ip           *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
    struct ip6_hdr      *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);
    struct udphdr       *udphdr = (struct udphdr *)(packet->pkt + packet->payloadOffset);
    if (isNewSession) {
        session->port1 = ntohs(udphdr->uh_sport);
        session->port2 = ntohs(udphdr->uh_dport);
        arkime_session_add_protocol(session, "sctp");
    }

    int dir;
    if (ip4->ip_v == 4) {
        dir = (ARKIME_V6_TO_V4(session->addr1) == ip4->ip_src.s_addr &&
               ARKIME_V6_TO_V4(session->addr2) == ip4->ip_dst.s_addr);
    } else {
        dir = (memcmp(session->addr1.s6_addr, ip6->ip6_src.s6_addr, 16) == 0 &&
               memcmp(session->addr2.s6_addr, ip6->ip6_dst.s6_addr, 16) == 0);
    }

    packet->direction = (dir &&
                         session->port1 == ntohs(udphdr->uh_sport) &&
                         session->port2 == ntohs(udphdr->uh_dport)) ? 0 : 1;
    session->databytes[packet->direction] += (packet->pktlen - 8);

    return 0;
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ip_cb(IPPROTO_SCTP, sctp_packet_enqueue);
    sctpMProtocol = arkime_mprotocol_register("sctp",
                                              SESSION_SCTP,
                                              sctp_create_sessionid,
                                              sctp_pre_process,
                                              NULL,
                                              NULL);
}
