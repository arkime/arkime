/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

//#define ARPDEBUG 1

extern ArkimeConfig_t        config;

LOCAL ArkimePQ_t *arpPq;

LOCAL int arpMProtocol;

/******************************************************************************/
LOCAL void arp_create_sessionid(uint8_t *sessionId, ArkimePacket_t *packet)
{
    uint8_t *data = packet->pkt + packet->payloadOffset;

    sessionId[0] = 9;
    sessionId[1] = 0x00;
    sessionId[2] = 0x00;
    sessionId[3] = 0x08;
    sessionId[4] = 0x06;
    if (data[7] == 1)
        memcpy(sessionId + 5, data + 24, 4);
    else
        memcpy(sessionId + 5, data + 14, 4);
}
/******************************************************************************/
LOCAL int arp_pre_process(ArkimeSession_t *session, ArkimePacket_t *const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        arkime_session_add_protocol(session, "arp");

    return 0;
}
/******************************************************************************/
LOCAL int arp_process(ArkimeSession_t *UNUSED(session), ArkimePacket_t *const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL ArkimePacketRC arp_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[ARKIME_SESSIONID_LEN];

    if (len < 28)
        return ARKIME_PACKET_CORRUPT;

    if (data[7] > 2)
        return ARKIME_PACKET_CORRUPT;

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    arp_create_sessionid(sessionId, packet);

    packet->hash = arkime_session_hash(sessionId);
    packet->mProtocol = arpMProtocol;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
LOCAL void arp_pq_cb(ArkimeSession_t *session, void UNUSED(*uw))
{
    session->midSave = 1;
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ethernet_cb(0x0806, arp_packet_enqueue);
    arpPq = arkime_pq_alloc(10, arp_pq_cb);
    arpMProtocol = arkime_mprotocol_register("arp",
                                             SESSION_OTHER,
                                             arp_create_sessionid,
                                             arp_pre_process,
                                             arp_process,
                                             NULL);
}
