/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

//#define PIMDEBUG 1

extern ArkimeConfig_t        config;

LOCAL int mProtocolPim;

/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL void pim_create_sessionid(uint8_t *sessionId, ArkimePacket_t *const packet)
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
LOCAL int pim_pre_process(ArkimeSession_t *session, ArkimePacket_t *const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        arkime_session_add_protocol(session, "pim");

    return 0;
}
/******************************************************************************/
LOCAL int pim_process(ArkimeSession_t *UNUSED(session), ArkimePacket_t *const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL ArkimePacketRC pim_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[ARKIME_SESSIONID_LEN];

    // no sanity checks until we parse.

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    pim_create_sessionid(sessionId, packet);

    packet->hash = arkime_session_hash(sessionId);
    packet->mProtocol = mProtocolPim;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ip_cb(IPPROTO_PIM, pim_packet_enqueue);
    mProtocolPim = arkime_mprotocol_register("pim",
                                             0,
                                             pim_create_sessionid,
                                             pim_pre_process,
                                             pim_process,
                                             NULL,
                                             NULL,
                                             600);
}
