/* esp.c
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

LOCAL  int                   espMProtocol;

/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL ArkimePacketRC esp_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *UNUSED(data), int UNUSED(len))
{
    uint8_t                 sessionId[ARKIME_SESSIONID_LEN];

    if (packet->v6) {
        const struct ip6_hdr *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);
        arkime_session_id6(sessionId, ip6->ip6_src.s6_addr, 0,
                           ip6->ip6_dst.s6_addr, 0, packet->vlan, packet->vni);
    } else {
        const struct ip *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
        arkime_session_id(sessionId, ip4->ip_src.s_addr, 0, ip4->ip_dst.s_addr,
                          0, packet->vlan, packet->vni);
    }

    packet->mProtocol = espMProtocol;
    packet->hash = arkime_session_hash(sessionId);

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL void esp_create_sessionid(uint8_t *sessionId, ArkimePacket_t *packet)
{
    const struct ip           *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
    const struct ip6_hdr      *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);

    if (packet->v6) {
        arkime_session_id6(sessionId, ip6->ip6_src.s6_addr, 0,
                           ip6->ip6_dst.s6_addr, 0, packet->vlan, packet->vni);
    } else {
        arkime_session_id(sessionId, ip4->ip_src.s_addr, 0,
                          ip4->ip_dst.s_addr, 0, packet->vlan, packet->vni);
    }
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL int esp_pre_process(ArkimeSession_t *session, ArkimePacket_t *const UNUSED(packet), int isNewSession)
{
    const struct ip           *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
    const struct ip6_hdr      *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);

    if (isNewSession)
        arkime_session_add_protocol(session, "esp");
    session->stopSaving = 1;

    int dir;
    if (ip4->ip_v == 4) {
        dir = (ARKIME_V6_TO_V4(session->addr1) == ip4->ip_src.s_addr &&
               ARKIME_V6_TO_V4(session->addr2) == ip4->ip_dst.s_addr);
    } else {
        dir = (memcmp(session->addr1.s6_addr, ip6->ip6_src.s6_addr, 16) == 0 &&
               memcmp(session->addr2.s6_addr, ip6->ip6_dst.s6_addr, 16) == 0);
    }

    packet->direction = dir;
    session->databytes[packet->direction] += (packet->pktlen - packet->payloadOffset - 8);

    return 0;
}
/******************************************************************************/
void arkime_parser_init()
{
    if (!config.trackESP)
        return;

    arkime_packet_set_ip_cb(IPPROTO_ESP, esp_packet_enqueue);
    espMProtocol = arkime_mprotocol_register("esp",
                                             SESSION_ESP,
                                             esp_create_sessionid,
                                             esp_pre_process,
                                             NULL,
                                             NULL);
}
