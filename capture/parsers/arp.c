/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "moloch.h"

//#define ARPDEBUG 1

extern MolochConfig_t        config;

LOCAL MolochPQ_t *arpPq;

LOCAL int arpMProtocol;

/******************************************************************************/
LOCAL void arp_create_sessionid(uint8_t *sessionId, MolochPacket_t *packet)
{
    uint8_t *data = packet->pkt + packet->payloadOffset;

    sessionId[0] = 9;
    sessionId[1] = 0x00;
    sessionId[2] = 0x00;
    sessionId[3] = 0x08;
    sessionId[4] = 0x06;
    if (data[7] == 1)
        memcpy(sessionId+5, data+24, 4);
    else
        memcpy(sessionId+5, data+14, 4);
}
/******************************************************************************/
LOCAL void arp_pre_process(MolochSession_t *session, MolochPacket_t * const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        moloch_session_add_protocol(session, "arp");
}
/******************************************************************************/
LOCAL int arp_process(MolochSession_t *UNUSED(session), MolochPacket_t * const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL MolochPacketRC arp_packet_enqueue(MolochPacketBatch_t * UNUSED(batch), MolochPacket_t * const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[MOLOCH_SESSIONID_LEN];

    if (len < 28)
        return MOLOCH_PACKET_CORRUPT;

    if (data[7] > 2)
        return MOLOCH_PACKET_CORRUPT;

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    arp_create_sessionid(sessionId, packet);

    packet->hash = moloch_session_hash(sessionId);
    packet->mProtocol = arpMProtocol;

    return MOLOCH_PACKET_DO_PROCESS;
}
/******************************************************************************/
LOCAL void arp_pq_cb(MolochSession_t *session, void UNUSED(*uw))
{
    session->midSave = 1;
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_packet_set_ethernet_cb(0x0806, arp_packet_enqueue);
    arpPq = moloch_pq_alloc(10, arp_pq_cb);
    arpMProtocol = moloch_mprotocol_register("arp",
                                             SESSION_OTHER,
                                             arp_create_sessionid,
                                             arp_pre_process,
                                             arp_process,
                                             NULL);
}
