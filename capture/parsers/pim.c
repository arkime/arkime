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

//#define PIMDEBUG 1

extern MolochConfig_t        config;

LOCAL MolochPQ_t *pimPq;

LOCAL int pimMProtocol;

/******************************************************************************/
LOCAL void pim_create_sessionid(uint8_t *sessionId, MolochPacket_t * const UNUSED (packet))
{
    sessionId[0] = 2;
    sessionId[1] = IPPROTO_PIM;
    sessionId[2] = IPPROTO_PIM;

    // for now, lump all pim into the same session
}
/******************************************************************************/
LOCAL void pim_pre_process(MolochSession_t *session, MolochPacket_t * const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        moloch_session_add_protocol(session, "pim");
}
/******************************************************************************/
LOCAL int pim_process(MolochSession_t *UNUSED(session), MolochPacket_t * const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL MolochPacketRC pim_packet_enqueue(MolochPacketBatch_t * UNUSED(batch), MolochPacket_t * const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[MOLOCH_SESSIONID_LEN];

    // no sanity checks until we parse.

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    pim_create_sessionid(sessionId, packet);

    packet->hash = moloch_session_hash(sessionId);
    packet->mProtocol = pimMProtocol;

    return MOLOCH_PACKET_DO_PROCESS;
}
/******************************************************************************/
LOCAL void pim_pq_cb(MolochSession_t *session, void UNUSED(*uw))
{
    session->midSave = 1;
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_packet_set_ip_cb(IPPROTO_PIM, pim_packet_enqueue);
    pimPq = moloch_pq_alloc(10, pim_pq_cb);
    pimMProtocol = moloch_mprotocol_register("pim",
                                             SESSION_OTHER,
                                             pim_create_sessionid,
                                             pim_pre_process,
                                             pim_process,
                                             NULL);
}
