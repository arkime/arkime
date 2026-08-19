/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

//#define IGMPDEBUG 1

extern ArkimeConfig_t        config;

LOCAL int mProtocolIgmp;

/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL void igmp_create_sessionid(uint8_t *sessionId, ArkimePacket_t *const packet)
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
LOCAL int igmp_pre_process(ArkimeSession_t *session, ArkimePacket_t *const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        arkime_session_add_protocol(session, "igmp");

    return 0;
}
/******************************************************************************/
LOCAL int igmp_process(ArkimeSession_t *UNUSED(session), ArkimePacket_t *const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL ArkimePacketRC igmp_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[ARKIME_SESSIONID_LEN];

    // no sanity checks until we parse.

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    igmp_create_sessionid(sessionId, packet);

    packet->hash = arkime_session_hash(sessionId);
    packet->mProtocol = mProtocolIgmp;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ip_cb(IPPROTO_IGMP, igmp_packet_enqueue);
    mProtocolIgmp = arkime_mprotocol_register("igmp",
                                              0,
                                              igmp_create_sessionid,
                                              igmp_pre_process,
                                              igmp_process,
                                              NULL,
                                              NULL,
                                              600);
}
