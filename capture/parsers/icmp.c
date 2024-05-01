/* icmp.c
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

LOCAL int                    icmpMProtocol;
LOCAL int                    icmpv6MProtocol;

LOCAL int                    icmpTypeField;
LOCAL int                    icmpCodeField;


/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL ArkimePacketRC icmp_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *UNUSED(data), int UNUSED(len))
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
    packet->mProtocol = icmpMProtocol;
    packet->hash = arkime_session_hash(sessionId);
    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL ArkimePacketRC icmpv6_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *UNUSED(data), int UNUSED(len))
{
    uint8_t                 sessionId[ARKIME_SESSIONID_LEN];

    if (!packet->v6)
        return ARKIME_PACKET_CORRUPT;

    const struct ip6_hdr *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);
    arkime_session_id6(sessionId, ip6->ip6_src.s6_addr, 0, ip6->ip6_dst.s6_addr,
                       0, packet->vlan, packet->vni);
    packet->mProtocol = icmpv6MProtocol;
    packet->hash = arkime_session_hash(sessionId);
    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL void icmp_create_sessionid(uint8_t *sessionId, ArkimePacket_t *packet)
{
    const struct ip           *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
    const struct ip6_hdr      *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);

    if (packet->v6) {
        arkime_session_id6(sessionId, ip6->ip6_src.s6_addr, 0,
                           ip6->ip6_dst.s6_addr, 0, packet->vlan, packet->vni);
    } else {
        arkime_session_id(sessionId, ip4->ip_src.s_addr, 0, ip4->ip_dst.s_addr,
                          0, packet->vlan, packet->vni);
    }
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL int icmp_pre_process(ArkimeSession_t *session, ArkimePacket_t *const packet, int isNewSession)
{
    const struct ip           *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
    const struct ip6_hdr      *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);

    if (isNewSession)
        arkime_session_add_protocol(session, "icmp");

    int dir;
    if (ip4->ip_v == 4) {
        dir = (ARKIME_V6_TO_V4(session->addr1) == ip4->ip_src.s_addr &&
               ARKIME_V6_TO_V4(session->addr2) == ip4->ip_dst.s_addr);
    } else {
        dir = (memcmp(session->addr1.s6_addr, ip6->ip6_src.s6_addr, 16) == 0 &&
               memcmp(session->addr2.s6_addr, ip6->ip6_dst.s6_addr, 16) == 0);
    }

    packet->direction = (dir) ? 0 : 1;
    session->databytes[packet->direction] += (packet->pktlen - packet->payloadOffset);

    return 0;
}
/******************************************************************************/
LOCAL int icmp_process(ArkimeSession_t *session, ArkimePacket_t *const packet)
{
    const uint8_t *data = packet->pkt + packet->payloadOffset;

    if (packet->payloadLen >= 2) {
        if (session->icmpInfo[0] == 0 && session->icmpInfo[0] == 0) {
            session->icmpInfo[0] = data[0];
            session->icmpInfo[1] = data[1];
        }
        arkime_field_int_add(icmpTypeField, session, data[0]);
        arkime_field_int_add(icmpCodeField, session, data[1]);
    }
    return 1;
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL void icmpv6_create_sessionid(uint8_t *sessionId, ArkimePacket_t *packet)
{
    const struct ip6_hdr      *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);
    arkime_session_id6(sessionId, ip6->ip6_src.s6_addr, 0, ip6->ip6_dst.s6_addr,
                       0, packet->vlan, packet->vni);
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL int icmpv6_pre_process(ArkimeSession_t *session, ArkimePacket_t *const packet, int isNewSession)
{
    const struct ip6_hdr      *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);

    if (isNewSession)
        arkime_session_add_protocol(session, "icmp");

    int dir = (memcmp(session->addr1.s6_addr, ip6->ip6_src.s6_addr, 16) == 0 &&
               memcmp(session->addr2.s6_addr, ip6->ip6_dst.s6_addr, 16) == 0);

    packet->direction = (dir) ? 0 : 1;
    session->databytes[packet->direction] += (packet->pktlen - packet->payloadOffset);

    return 0;
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ip_cb(IPPROTO_ICMP, icmp_packet_enqueue);
    arkime_packet_set_ip_cb(IPPROTO_ICMPV6, icmpv6_packet_enqueue);

    icmpMProtocol = arkime_mprotocol_register("icmp",
                                              SESSION_ICMP,
                                              icmp_create_sessionid,
                                              icmp_pre_process,
                                              icmp_process,
                                              NULL);

    icmpv6MProtocol = arkime_mprotocol_register("icmpv6",
                                                SESSION_ICMP,
                                                icmpv6_create_sessionid,
                                                icmpv6_pre_process,
                                                icmp_process,
                                                NULL);

    icmpTypeField = arkime_field_define("general", "integer",
                                        "icmp.type", "ICMP Type", "icmp.type",
                                        "ICMP type field values",
                                        ARKIME_FIELD_TYPE_INT_GHASH, 0,
                                        (char *)NULL);

    icmpCodeField = arkime_field_define("general", "integer",
                                        "icmp.code", "ICMP Code", "icmp.code",
                                        "ICMP code field values",
                                        ARKIME_FIELD_TYPE_INT_GHASH, 0,
                                        (char *)NULL);

}
