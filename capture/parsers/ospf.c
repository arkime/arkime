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

//#define OSPFDEBUG 1

extern MolochConfig_t        config;

LOCAL MolochPQ_t *ospfPq;

LOCAL int ospfMProtocol;

/******************************************************************************/
LOCAL void ospf_create_sessionid(uint8_t *sessionId, MolochPacket_t * const UNUSED (packet))
{
    // uint8_t *data = packet->pkt + packet->payloadOffset;

    sessionId[0] = 2;
    sessionId[1] = 89;
    sessionId[2] = 89;

    // for now, lump all ospf into the same session
}
/******************************************************************************/
LOCAL void ospf_pre_process(MolochSession_t *session, MolochPacket_t * const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        moloch_session_add_protocol(session, "ospf");
}
/******************************************************************************/
LOCAL int ospf_process(MolochSession_t *UNUSED(session), MolochPacket_t * const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL MolochPacketRC ospf_packet_enqueue(MolochPacketBatch_t * UNUSED(batch), MolochPacket_t * const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[MOLOCH_SESSIONID_LEN];

    // no sanity checks until we parse.

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    ospf_create_sessionid(sessionId, packet);

    packet->hash = moloch_session_hash(sessionId);
    packet->mProtocol = ospfMProtocol;

    return MOLOCH_PACKET_DO_PROCESS;
}
/******************************************************************************/
LOCAL void ospf_pq_cb(MolochSession_t *session, void UNUSED(*uw))
{
    session->midSave = 1;
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_packet_set_ip_cb(89, ospf_packet_enqueue);
    ospfPq = moloch_pq_alloc(10, ospf_pq_cb);
    ospfMProtocol = moloch_mprotocol_register("ospf",
                                             SESSION_OTHER,
                                             ospf_create_sessionid,
                                             ospf_pre_process,
                                             ospf_process,
                                             NULL);
}
