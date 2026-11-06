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

extern int                   mProtocolEsp;
LOCAL  int                   espSavePackets;

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

    packet->mProtocol = mProtocolEsp;
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
LOCAL int esp_pre_process(ArkimeSession_t *session, ArkimePacket_t *const packet, int isNewSession)
{
    const struct ip           *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
    const struct ip6_hdr      *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);

    if (isNewSession) {
        arkime_session_add_protocol(session, "esp");
        if (!espSavePackets)
            session->stopSaving = 1;
    }

    int dir;
    if (ip4->ip_v == 4) {
        dir = (ARKIME_V6_TO_V4(session->addr1) == ip4->ip_src.s_addr &&
               ARKIME_V6_TO_V4(session->addr2) == ip4->ip_dst.s_addr);
    } else {
        dir = (memcmp(session->addr1.s6_addr, ip6->ip6_src.s6_addr, 16) == 0 &&
               memcmp(session->addr2.s6_addr, ip6->ip6_dst.s6_addr, 16) == 0);
    }

    packet->direction = dir ? 0 : 1;
    if (packet->pktlen >= packet->payloadOffset + 8)
        session->databytes[packet->direction] += (packet->pktlen - packet->payloadOffset - 8);

    return 0;
}
/******************************************************************************/
LOCAL void esp_natt_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    // RFC 3948 UDP encapsulated ESP, skip the 4 zero byte non-ESP marker (IKE) and 1 byte keepalives,
    // and require a non zero sequence number
    if (len < 8 || (data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 0))
        return;
    if (data[4] == 0 && data[5] == 0 && data[6] == 0 && data[7] == 0)
        return;
    arkime_session_add_protocol(session, "esp");
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_parsers_classifier_register_port("esp", NULL, 4500, ARKIME_PARSERS_PORT_UDP, esp_natt_classify);

    if (!config.trackESP)
        return;

    espSavePackets = arkime_config_boolean(NULL, "espSavePackets", FALSE);
    arkime_packet_set_ip_cb(IPPROTO_ESP, esp_packet_enqueue);
    mProtocolEsp = arkime_mprotocol_register("esp",
                                             ARKIME_MPROTOCOL_FLAG_COMMUNITYID | ARKIME_MPROTOCOL_FLAG_STREAMS_LOW,
                                             esp_create_sessionid,
                                             esp_pre_process,
                                             NULL,
                                             NULL,
                                             NULL,
                                             arkime_config_int(NULL, "espTimeout", 600, 10, 0xffff));

}
