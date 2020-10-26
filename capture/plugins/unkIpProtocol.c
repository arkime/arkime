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

//#define UNKIPPROTODEBUG 1

extern MolochConfig_t        config;

LOCAL MolochPQ_t *unkIpProtocolPq;

LOCAL int unkIpProtocolMProtocol;

/******************************************************************************/
LOCAL void unkIpProtocol_create_sessionid(uint8_t *sessionId, MolochPacket_t * const UNUSED (packet))
{
    // uint8_t *data = packet->pkt + packet->payloadOffset;

    sessionId[0] = 2;
    sessionId[1] = 0x9a;
    sessionId[2] = 0x9a;

    // for now, lump all unkIpProtocol into the same session
}
/******************************************************************************/
LOCAL void unkIpProtocol_pre_process(MolochSession_t *session, MolochPacket_t * const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        moloch_session_add_protocol(session, "unkIpProtocol");
}
/******************************************************************************/
LOCAL int unkIpProtocol_process(MolochSession_t *UNUSED(session), MolochPacket_t * const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL MolochPacketRC unkIpProtocol_packet_enqueue(MolochPacketBatch_t * UNUSED(batch), MolochPacket_t * const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[MOLOCH_SESSIONID_LEN];

    // no sanity checks

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    unkIpProtocol_create_sessionid(sessionId, packet);

    packet->hash = moloch_session_hash(sessionId);
    packet->mProtocol = unkIpProtocolMProtocol;

    return MOLOCH_PACKET_DO_PROCESS;
}
/******************************************************************************/
LOCAL void unkIpProtocol_pq_cb(MolochSession_t *session, void UNUSED(*uw))
{
    session->midSave = 1;
}
/******************************************************************************/
void moloch_plugin_init()
{
    moloch_packet_set_ip_cb(MOLOCH_IPPROTO_UNKNOWN, unkIpProtocol_packet_enqueue);
    unkIpProtocolPq = moloch_pq_alloc(10, unkIpProtocol_pq_cb);
    unkIpProtocolMProtocol = moloch_mprotocol_register("unkIpProtocol",
                                             SESSION_OTHER,
                                             unkIpProtocol_create_sessionid,
                                             unkIpProtocol_pre_process,
                                             unkIpProtocol_process,
                                             NULL);
}
