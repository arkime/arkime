/* Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL int xidMProtocol;

/******************************************************************************/
LOCAL void xid_create_sessionid(uint8_t *sessionId, ArkimePacket_t *const UNUSED(packet))
{
    sessionId[0] = 4;
    sessionId[1] = xidMProtocol;
    sessionId[2] = sessionId[3] = 0;
}
/******************************************************************************/
LOCAL int xid_pre_process(ArkimeSession_t *session, ArkimePacket_t *const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        arkime_session_add_protocol(session, "xid");

    return 0;
}
/******************************************************************************/
LOCAL int xid_process(ArkimeSession_t *UNUSED(session), ArkimePacket_t *const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL ArkimePacketRC xid_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[ARKIME_SESSIONID_LEN];

    // XID frame must have at least DSAP + SSAP + Control (3 bytes)
    if (len < 3)
        return ARKIME_PACKET_CORRUPT;

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    xid_create_sessionid(sessionId, packet);

    packet->hash = arkime_session_hash(sessionId);
    packet->mProtocol = xidMProtocol;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ethernet_cb(ARKIME_ETHERTYPE_XID, xid_packet_enqueue);
    xidMProtocol = arkime_mprotocol_register("xid",
                                             SESSION_OTHER,
                                             xid_create_sessionid,
                                             xid_pre_process,
                                             xid_process,
                                             NULL,
                                             NULL,
                                             600);
}
