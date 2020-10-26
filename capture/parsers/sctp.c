/* sctp.c
 *
 * Copyright 2019 AOL Inc. All rights reserved.
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
#include "patricia.h"
#include <inttypes.h>
#include <arpa/inet.h>
#include <errno.h>


/******************************************************************************/
extern MolochConfig_t        config;

LOCAL  int                   sctpMProtocol;
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL MolochPacketRC sctp_packet_enqueue(MolochPacketBatch_t * UNUSED(batch), MolochPacket_t * const packet, const uint8_t *UNUSED(data), int UNUSED(len))
{
    uint8_t             sessionId[MOLOCH_SESSIONID_LEN];
    struct udphdr       *udphdr = (struct udphdr *)(packet->pkt + packet->payloadOffset); /* Not really udp, but port in same location */

    if (packet->payloadLen < (int)sizeof(struct udphdr))
        return MOLOCH_PACKET_CORRUPT;

    if (packet->v6) {
       struct ip6_hdr *ip6 = (struct ip6_hdr*)(packet->pkt + packet->ipOffset);
        moloch_session_id6(sessionId, ip6->ip6_src.s6_addr, udphdr->uh_sport,
                           ip6->ip6_dst.s6_addr, udphdr->uh_dport);
    } else {
        struct ip *ip4 = (struct ip*)(packet->pkt + packet->ipOffset);
        moloch_session_id(sessionId, ip4->ip_src.s_addr, udphdr->uh_sport,
                          ip4->ip_dst.s_addr, udphdr->uh_dport);
    }
    packet->mProtocol = sctpMProtocol;
    packet->hash = moloch_session_hash(sessionId);
    return MOLOCH_PACKET_DO_PROCESS;
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL void sctp_create_sessionid(uint8_t *sessionId, MolochPacket_t *packet)
{
    struct udphdr       *udphdr = (struct udphdr *)(packet->pkt + packet->payloadOffset); /* Not really udp, but port in same location */

    if (packet->v6) {
        struct ip6_hdr *ip6 = (struct ip6_hdr*)(packet->pkt + packet->ipOffset);
        moloch_session_id6(sessionId, ip6->ip6_src.s6_addr, udphdr->uh_sport,
                           ip6->ip6_dst.s6_addr, udphdr->uh_dport);
    } else {
        struct ip *ip4 = (struct ip*)(packet->pkt + packet->ipOffset);
        moloch_session_id(sessionId, ip4->ip_src.s_addr, udphdr->uh_sport,
                          ip4->ip_dst.s_addr, udphdr->uh_dport);
    }
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL void sctp_pre_process(MolochSession_t *session, MolochPacket_t * const packet, int isNewSession)
{
    struct ip           *ip4 = (struct ip*)(packet->pkt + packet->ipOffset);
    struct ip6_hdr      *ip6 = (struct ip6_hdr*)(packet->pkt + packet->ipOffset);
    struct udphdr       *udphdr = (struct udphdr *)(packet->pkt + packet->payloadOffset);
    if (isNewSession) {
        session->port1 = ntohs(udphdr->uh_sport);
        session->port2 = ntohs(udphdr->uh_dport);
        moloch_session_add_protocol(session, "sctp");
    }

    int dir;
    if (ip4->ip_v == 4) {
        dir = (MOLOCH_V6_TO_V4(session->addr1) == ip4->ip_src.s_addr &&
               MOLOCH_V6_TO_V4(session->addr2) == ip4->ip_dst.s_addr);
    } else {
        dir = (memcmp(session->addr1.s6_addr, ip6->ip6_src.s6_addr, 16) == 0 &&
               memcmp(session->addr2.s6_addr, ip6->ip6_dst.s6_addr, 16) == 0);
    }

    packet->direction = (dir &&
                         session->port1 == ntohs(udphdr->uh_sport) &&
                         session->port2 == ntohs(udphdr->uh_dport))?0:1;
    session->databytes[packet->direction] += (packet->pktlen - 8);
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_packet_set_ip_cb(IPPROTO_SCTP, sctp_packet_enqueue);
    sctpMProtocol = moloch_mprotocol_register("sctp",
                                              SESSION_SCTP,
                                              sctp_create_sessionid,
                                              sctp_pre_process,
                                              NULL,
                                              NULL);
}
