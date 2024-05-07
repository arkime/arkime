/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

//#define OSPFDEBUG 1

extern ArkimeConfig_t        config;

LOCAL ArkimePQ_t *ospfPq;

LOCAL int ospfMProtocol;

/******************************************************************************/
LOCAL void ospf_create_sessionid(uint8_t *sessionId, ArkimePacket_t *const UNUSED (packet))
{
    // uint8_t *data = packet->pkt + packet->payloadOffset;

    sessionId[0] = 2;
    sessionId[1] = 89;
    sessionId[2] = 89;
    sessionId[3] = 0;

    // for now, lump all ospf into the same session
}
/******************************************************************************/
LOCAL int ospf_pre_process(ArkimeSession_t *session, ArkimePacket_t *const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        arkime_session_add_protocol(session, "ospf");

    return 0;
}
/******************************************************************************/
LOCAL int ospf_process(ArkimeSession_t *UNUSED(session), ArkimePacket_t *const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL ArkimePacketRC ospf_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[ARKIME_SESSIONID_LEN];

    // no sanity checks until we parse.

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    ospf_create_sessionid(sessionId, packet);

    packet->hash = arkime_session_hash(sessionId);
    packet->mProtocol = ospfMProtocol;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
LOCAL void ospf_pq_cb(ArkimeSession_t *session, void UNUSED(*uw))
{
    session->midSave = 1;
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ip_cb(89, ospf_packet_enqueue);
    ospfPq = arkime_pq_alloc(10, ospf_pq_cb);
    ospfMProtocol = arkime_mprotocol_register("ospf",
                                              SESSION_OTHER,
                                              ospf_create_sessionid,
                                              ospf_pre_process,
                                              ospf_process,
                                              NULL);
}
