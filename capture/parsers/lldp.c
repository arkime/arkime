/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

//#define LLDPDEBUG 1

extern ArkimeConfig_t        config;

LOCAL int mProtocolLldp;

/******************************************************************************/
LOCAL void lldp_create_sessionid(uint8_t *sessionId, ArkimePacket_t *const packet)
{
    // One session per talker pair instead of one session for everything
    arkime_session_id_ether(sessionId, packet, 12);
}
/******************************************************************************/
LOCAL int lldp_pre_process(ArkimeSession_t *session, ArkimePacket_t *const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        arkime_session_add_protocol(session, "lldp");

    return 0;
}
/******************************************************************************/
LOCAL int lldp_process(ArkimeSession_t *UNUSED(session), ArkimePacket_t *const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL ArkimePacketRC lldp_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[ARKIME_SESSIONID_LEN];

    // no sanity checks as we don't parse

    // Need src/dst MACs for the session id
    if ((int)packet->pktlen - (int)packet->etherOffset < 12)
        return ARKIME_PACKET_CORRUPT;

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    lldp_create_sessionid(sessionId, packet);

    packet->hash = arkime_session_hash(sessionId);
    packet->mProtocol = mProtocolLldp;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ethernet_cb(0x88cc, lldp_packet_enqueue);
    mProtocolLldp = arkime_mprotocol_register("lldp",
                                              0,
                                              lldp_create_sessionid,
                                              lldp_pre_process,
                                              lldp_process,
                                              NULL,
                                              NULL,
                                              600);
}
