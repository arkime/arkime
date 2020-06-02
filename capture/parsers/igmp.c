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

//#define IGMPDEBUG 1

extern MolochConfig_t        config;

LOCAL MolochPQ_t *igmpPq;

LOCAL int igmpMProtocol;

/******************************************************************************/
LOCAL void igmp_create_sessionid(uint8_t *sessionId, MolochPacket_t * const UNUSED (packet))
{
    sessionId[0] = 2;
    sessionId[1] = IPPROTO_IGMP;
    sessionId[2] = IPPROTO_IGMP;

    // for now, lump all igmp into the same session
}
/******************************************************************************/
LOCAL void igmp_pre_process(MolochSession_t *session, MolochPacket_t * const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        moloch_session_add_protocol(session, "igmp");
}
/******************************************************************************/
int igmp_process(MolochSession_t *UNUSED(session), MolochPacket_t * const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL MolochPacketRC igmp_packet_enqueue(MolochPacketBatch_t * UNUSED(batch), MolochPacket_t * const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[MOLOCH_SESSIONID_LEN];

    // no sanity checks until we parse.

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    igmp_create_sessionid(sessionId, packet);

    packet->hash = moloch_session_hash(sessionId);
    packet->mProtocol = igmpMProtocol;

    return MOLOCH_PACKET_DO_PROCESS;
}
/******************************************************************************/
LOCAL void igmp_pq_cb(MolochSession_t *session, void UNUSED(*uw))
{
    session->midSave = 1;
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_packet_set_ip_cb(IPPROTO_IGMP, igmp_packet_enqueue);
    igmpPq = moloch_pq_alloc(10, igmp_pq_cb);
    igmpMProtocol = moloch_mprotocol_register("igmp",
                                             SESSION_OTHER,
                                             igmp_create_sessionid,
                                             igmp_pre_process,
                                             igmp_process,
                                             NULL);
}
