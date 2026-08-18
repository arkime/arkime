/* Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL int mProtocolXid;

/******************************************************************************/
LOCAL void xid_create_sessionid(uint8_t *sessionId, ArkimePacket_t *const packet)
{
    // One session per talker pair instead of one session for everything
    sessionId[0] = 16;
    memcpy(sessionId + 1, packet->pkt + packet->etherOffset, 12);
    sessionId[13] = sessionId[14] = sessionId[15] = 0;
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

    // Need src/dst MACs for the session id
    if ((int)packet->pktlen - (int)packet->etherOffset < 12)
        return ARKIME_PACKET_CORRUPT;

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    xid_create_sessionid(sessionId, packet);

    packet->hash = arkime_session_hash(sessionId);
    packet->mProtocol = mProtocolXid;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ethernet_cb(ARKIME_ETHERTYPE_XID, xid_packet_enqueue);
    mProtocolXid = arkime_mprotocol_register("xid",
                                             0,
                                             xid_create_sessionid,
                                             xid_pre_process,
                                             xid_process,
                                             NULL,
                                             NULL,
                                             600);
}
