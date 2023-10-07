/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

//#define LLDPDEBUG 1

extern ArkimeConfig_t        config;

LOCAL ArkimePQ_t *lldpPq;

LOCAL int lldpMProtocol;

/******************************************************************************/
LOCAL void lldp_create_sessionid(uint8_t *sessionId, ArkimePacket_t *const UNUSED (packet))
{
    // not used, but leaving for now
    // uint8_t *data = packet->pkt + packet->payloadOffset;

    sessionId[0] = 4;
    sessionId[1] = 0x00;
    sessionId[2] = 0x00;
    sessionId[3] = 0x88;
    sessionId[4] = 0xcc;

    // I'm not sure what fields are required and if one can expect a specific ordering.
    // so not sure it makes sense to try and further tease out sessions from the lldp traffic here.
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

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    lldp_create_sessionid(sessionId, packet);

    packet->hash = arkime_session_hash(sessionId);
    packet->mProtocol = lldpMProtocol;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
LOCAL void lldp_pq_cb(ArkimeSession_t *session, void UNUSED(*uw))
{
    session->midSave = 1;
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ethernet_cb(0x88cc, lldp_packet_enqueue);
    lldpPq = arkime_pq_alloc(10, lldp_pq_cb);
    lldpMProtocol = arkime_mprotocol_register("lldp",
                                              SESSION_OTHER,
                                              lldp_create_sessionid,
                                              lldp_pre_process,
                                              lldp_process,
                                              NULL);
}
