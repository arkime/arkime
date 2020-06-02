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

//#define LLDPDEBUG 1

extern MolochConfig_t        config;

LOCAL MolochPQ_t *lldpPq;

LOCAL int lldpMProtocol;

/******************************************************************************/
LOCAL void lldp_create_sessionid(uint8_t *sessionId, MolochPacket_t * const UNUSED (packet))
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
LOCAL void lldp_pre_process(MolochSession_t *session, MolochPacket_t * const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        moloch_session_add_protocol(session, "lldp");
}
/******************************************************************************/
LOCAL int lldp_process(MolochSession_t *UNUSED(session), MolochPacket_t * const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL MolochPacketRC lldp_packet_enqueue(MolochPacketBatch_t * UNUSED(batch), MolochPacket_t * const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[MOLOCH_SESSIONID_LEN];

    // no sanity checks as we don't parse

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    lldp_create_sessionid(sessionId, packet);

    packet->hash = moloch_session_hash(sessionId);
    packet->mProtocol = lldpMProtocol;

    return MOLOCH_PACKET_DO_PROCESS;
}
/******************************************************************************/
LOCAL void lldp_pq_cb(MolochSession_t *session, void UNUSED(*uw))
{
    session->midSave = 1;
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_packet_set_ethernet_cb(0x88cc, lldp_packet_enqueue);
    lldpPq = moloch_pq_alloc(10, lldp_pq_cb);
    lldpMProtocol = moloch_mprotocol_register("lldp",
                                             SESSION_OTHER,
                                             lldp_create_sessionid,
                                             lldp_pre_process,
                                             lldp_process,
                                             NULL);
}
