/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

//#define UNKETHERNETDEBUG 1

extern ArkimeConfig_t        config;

LOCAL int unkEthernetMProtocol;

/******************************************************************************/
LOCAL void unkEthernet_create_sessionid(uint8_t *sessionId, ArkimePacket_t *const UNUSED (packet))
{
    sessionId[0] = 15;
    sessionId[1] = unkEthernetMProtocol;
    memcpy(sessionId + 2, packet->pkt + packet->etherOffset, 14); // Copy macs and ether type
    sessionId[15] = 0;
}
/******************************************************************************/
LOCAL int unkEthernet_pre_process(ArkimeSession_t *session, ArkimePacket_t *const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        arkime_session_add_protocol(session, "unkEthernet");
    return 0;
}
/******************************************************************************/
LOCAL int unkEthernet_process(ArkimeSession_t *UNUSED(session), ArkimePacket_t *const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL ArkimePacketRC unkEthernet_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[ARKIME_SESSIONID_LEN];

    // no sanity checks until we parse.  the thinking is that it will make sense to
    // high level parse to determine unkEthernet packet type (eg hello, csnp/psnp, lsp) and
    // protocol tag with these additional discriminators

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    unkEthernet_create_sessionid(sessionId, packet);

    packet->hash = arkime_session_hash(sessionId);
    packet->mProtocol = unkEthernetMProtocol;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
void arkime_plugin_init()
{
    arkime_packet_set_ethernet_cb(ARKIME_ETHERTYPE_UNKNOWN, unkEthernet_packet_enqueue);
    unkEthernetMProtocol = arkime_mprotocol_register("unkEthernet",
                                                     SESSION_OTHER,
                                                     unkEthernet_create_sessionid,
                                                     unkEthernet_pre_process,
                                                     unkEthernet_process,
                                                     NULL);
}
