/* tcp.c
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

extern int                   tcpMProtocol;

/******************************************************************************/
void tcp_create_sessionid(char *sessionId, MolochPacket_t *packet)
{
    struct ip           *ip4 = (struct ip*)(packet->pkt + packet->ipOffset);
    struct ip6_hdr      *ip6 = (struct ip6_hdr*)(packet->pkt + packet->ipOffset);
    struct tcphdr       *tcphdr = (struct tcphdr *)(packet->pkt + packet->payloadOffset);

    if (packet->v6) {
        moloch_session_id6(sessionId, ip6->ip6_src.s6_addr, tcphdr->th_sport,
                           ip6->ip6_dst.s6_addr, tcphdr->th_dport);
    } else {
        moloch_session_id(sessionId, ip4->ip_src.s_addr, tcphdr->th_sport,
                          ip4->ip_dst.s_addr, tcphdr->th_dport);
    }
}
/******************************************************************************/
void tcp_pre_process(MolochSession_t *session, MolochPacket_t * const packet, int isNewSession)
{
    struct ip           *ip4 = (struct ip*)(packet->pkt + packet->ipOffset);
    struct ip6_hdr      *ip6 = (struct ip6_hdr*)(packet->pkt + packet->ipOffset);
    struct tcphdr       *tcphdr = (struct tcphdr *)(packet->pkt + packet->payloadOffset);

    if (isNewSession) {
       /* If antiSynDrop option is set to true, capture will assume that
        * if the syn-ack ip4 was captured first then the syn probably got dropped.*/
        if ((tcphdr->th_flags & TH_SYN) && (tcphdr->th_flags & TH_ACK) && (config.antiSynDrop)) {
            struct in6_addr tmp;
            tmp = session->addr1;
            session->addr1 = session->addr2;
            session->addr2 = tmp;
            session->port1 = ntohs(tcphdr->th_dport);
            session->port2 = ntohs(tcphdr->th_sport);
        } else {
            session->port1 = ntohs(tcphdr->th_sport);
            session->port2 = ntohs(tcphdr->th_dport);
        }
        if (moloch_http_is_moloch(session->h_hash, session->sessionId)) {
            if (config.debug) {
                char buf[1000];
                LOG("Ignoring connection %s", moloch_session_id_string(session->sessionId, buf));
            }
            session->stopSPI = 1;
        }
        moloch_session_add_protocol(session, "tcp");
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
                         session->port1 == ntohs(tcphdr->th_sport) &&
                         session->port2 == ntohs(tcphdr->th_dport))?0:1;
    session->tcp_flags |= tcphdr->th_flags;
}
/******************************************************************************/
int tcp_process(MolochSession_t *session, MolochPacket_t * const packet)
{
    void moloch_packet_tcp_finish(MolochSession_t *session);
    int moloch_packet_process_tcp(MolochSession_t * const session, MolochPacket_t * const packet);

    int freePacket = moloch_packet_process_tcp(session, packet);
    moloch_packet_tcp_finish(session);
    return freePacket;
}
/******************************************************************************/
void moloch_parser_init()
{
    tcpMProtocol = moloch_mprotocol_register(tcp_create_sessionid,
                                             tcp_pre_process,
                                             tcp_process);
}
