/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

//#define PIMDEBUG 1

extern ArkimeConfig_t        config;

LOCAL int pimMProtocol;

/******************************************************************************/
LOCAL void pim_create_sessionid(uint8_t *sessionId, ArkimePacket_t *const UNUSED (packet))
{
    sessionId[0] = 3;
    sessionId[1] = IPPROTO_PIM;
    sessionId[2] = IPPROTO_PIM;
    sessionId[3] = 0;

    // for now, lump all pim into the same session
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
    packet->mProtocol = pimMProtocol;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ip_cb(IPPROTO_PIM, pim_packet_enqueue);
    pimMProtocol = arkime_mprotocol_register("pim",
                                             SESSION_OTHER,
                                             pim_create_sessionid,
                                             pim_pre_process,
                                             pim_process,
                                             NULL);
}
