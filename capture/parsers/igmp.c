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
    // One session per talker pair instead of one session for everything
    if (packet->v6) {
        const struct ip6_hdr *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);
        sessionId[0] = 36;
        memcpy(sessionId + 1, &ip6->ip6_src, 16);
        memcpy(sessionId + 17, &ip6->ip6_dst, 16);
        sessionId[33] = sessionId[34] = sessionId[35] = 0;
    } else {
        const struct ip *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
        sessionId[0] = 12;
        memcpy(sessionId + 1, &ip4->ip_src, 4);
        memcpy(sessionId + 5, &ip4->ip_dst, 4);
        sessionId[9] = sessionId[10] = sessionId[11] = 0;
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
