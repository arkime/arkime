/* udp.c
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

extern int                   udpMProtocol;
extern uint32_t              pluginsCbs;

/******************************************************************************/
SUPPRESS_ALIGNMENT
void udp_create_sessionid(uint8_t *sessionId, MolochPacket_t *packet)
{
    struct ip           *ip4 = (struct ip*)(packet->pkt + packet->ipOffset);
    struct ip6_hdr      *ip6 = (struct ip6_hdr*)(packet->pkt + packet->ipOffset);
    struct udphdr       *udphdr = (struct udphdr *)(packet->pkt + packet->payloadOffset);

    if (packet->v6) {
        moloch_session_id6(sessionId, ip6->ip6_src.s6_addr, udphdr->uh_sport,
                           ip6->ip6_dst.s6_addr, udphdr->uh_dport);
    } else {
        moloch_session_id(sessionId, ip4->ip_src.s_addr, udphdr->uh_sport,
                          ip4->ip_dst.s_addr, udphdr->uh_dport);
    }
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
void udp_pre_process(MolochSession_t *session, MolochPacket_t * const packet, int isNewSession)
{
    struct ip           *ip4 = (struct ip*)(packet->pkt + packet->ipOffset);
    struct ip6_hdr      *ip6 = (struct ip6_hdr*)(packet->pkt + packet->ipOffset);
    struct udphdr       *udphdr = (struct udphdr *)(packet->pkt + packet->payloadOffset);

    if (isNewSession) {
        session->port1 = ntohs(udphdr->uh_sport);
        session->port2 = ntohs(udphdr->uh_dport);
        moloch_session_add_protocol(session, "udp");
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
    session->databytes[packet->direction] += (packet->pktlen -packet->payloadOffset - 8);
}
/******************************************************************************/
int udp_process(MolochSession_t *session, MolochPacket_t * const packet)
{
    const uint8_t *data = packet->pkt + packet->payloadOffset + 8;
    int            len = packet->payloadLen - 8;

    if (len <= 0)
        return 1;

    if (session->firstBytesLen[packet->direction] == 0) {
        session->firstBytesLen[packet->direction] = MIN(8, len);
        memcpy(session->firstBytes[packet->direction], data, session->firstBytesLen[packet->direction]);

        moloch_parsers_classify_udp(session, data, len, packet->direction);

        if (config.yara && config.yaraEveryPacket && !session->stopYara) {
            moloch_yara_execute(session, data, len, 0);
        }
    }

    int i;
    for (i = 0; i < session->parserNum; i++) {
        if (session->parserInfo[i].parserFunc) {
            int consumed = session->parserInfo[i].parserFunc(session, session->parserInfo[i].uw, data, len, packet->direction);
            if (consumed == MOLOCH_PARSER_UNREGISTER) {
                if (session->parserInfo[i].parserFreeFunc) {
                    session->parserInfo[i].parserFreeFunc(session, session->parserInfo[i].uw);
                }
                memset(&session->parserInfo[i], 0, sizeof(session->parserInfo[i]));
                continue;
            }
        }
    }

    if (pluginsCbs & MOLOCH_PLUGIN_UDP)
        moloch_plugins_cb_udp(session, data, len, packet->direction);

    return 1;
}
/******************************************************************************/
void moloch_parser_init()
{
    udpMProtocol = moloch_mprotocol_register("udp",
                                             SESSION_UDP,
                                             udp_create_sessionid,
                                             udp_pre_process,
                                             udp_process,
                                             NULL);
}
