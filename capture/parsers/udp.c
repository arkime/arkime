/* udp.c
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

extern int                   udpMProtocol;
extern uint32_t              pluginsCbs;

/******************************************************************************/
SUPPRESS_ALIGNMENT
void udp_create_sessionid(uint8_t *sessionId, ArkimePacket_t *packet)
{
    struct ip           *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
    struct ip6_hdr      *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);
    struct udphdr       *udphdr = (struct udphdr *)(packet->pkt + packet->payloadOffset);

    if (packet->v6) {
        arkime_session_id6(sessionId, ip6->ip6_src.s6_addr, udphdr->uh_sport,
                           ip6->ip6_dst.s6_addr, udphdr->uh_dport);
    } else {
        arkime_session_id(sessionId, ip4->ip_src.s_addr, udphdr->uh_sport,
                          ip4->ip_dst.s_addr, udphdr->uh_dport);
    }
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
int udp_pre_process(ArkimeSession_t *session, ArkimePacket_t *const packet, int isNewSession)
{
    struct ip           *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
    struct ip6_hdr      *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);
    struct udphdr       *udphdr = (struct udphdr *)(packet->pkt + packet->payloadOffset);

    if (isNewSession) {
        session->port1 = ntohs(udphdr->uh_sport);
        session->port2 = ntohs(udphdr->uh_dport);
        arkime_session_add_protocol(session, "udp");
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
    session->databytes[packet->direction] += (packet->pktlen - packet->payloadOffset - 8);

    return 0;
}
/******************************************************************************/
int udp_process(ArkimeSession_t *session, ArkimePacket_t *const packet)
{
    const uint8_t *data = packet->pkt + packet->payloadOffset + 8;
    int            len = packet->payloadLen - 8;

    if (len <= 0)
        return 1;

    if (session->firstBytesLen[packet->direction] == 0) {
        session->firstBytesLen[packet->direction] = MIN(8, len);
        memcpy(session->firstBytes[packet->direction], data, session->firstBytesLen[packet->direction]);

        arkime_parsers_classify_udp(session, data, len, packet->direction);

        if (config.yara && config.yaraEveryPacket && !session->stopYara) {
            arkime_yara_execute(session, data, len, 0);
        }
    }

    int i;
    for (i = 0; i < session->parserNum; i++) {
        if (session->parserInfo[i].parserFunc) {
            int consumed = session->parserInfo[i].parserFunc(session, session->parserInfo[i].uw, data, len, packet->direction);
            if (consumed == ARKIME_PARSER_UNREGISTER) {
                if (session->parserInfo[i].parserFreeFunc) {
                    session->parserInfo[i].parserFreeFunc(session, session->parserInfo[i].uw);
                }
                memset(&session->parserInfo[i], 0, sizeof(session->parserInfo[i]));
                continue;
            }
        }
    }

    if (pluginsCbs & ARKIME_PLUGIN_UDP)
        arkime_plugins_cb_udp(session, data, len, packet->direction);

    return 1;
}
/******************************************************************************/
void arkime_parser_init()
{
    udpMProtocol = arkime_mprotocol_register("udp",
                                             SESSION_UDP,
                                             udp_create_sessionid,
                                             udp_pre_process,
                                             udp_process,
                                             NULL);
}
