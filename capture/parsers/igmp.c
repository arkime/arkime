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
#include "arkime.h"

//#define IGMPDEBUG 1

extern ArkimeConfig_t        config;

LOCAL ArkimePQ_t *igmpPq;

LOCAL int igmpMProtocol;

/******************************************************************************/
LOCAL void igmp_create_sessionid(uint8_t *sessionId, ArkimePacket_t * const UNUSED (packet))
{
    sessionId[0] = 2;
    sessionId[1] = IPPROTO_IGMP;
    sessionId[2] = IPPROTO_IGMP;

    // for now, lump all igmp into the same session
}
/******************************************************************************/
LOCAL int igmp_pre_process(ArkimeSession_t *session, ArkimePacket_t * const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        arkime_session_add_protocol(session, "igmp");

    return 0;
}
/******************************************************************************/
int igmp_process(ArkimeSession_t *UNUSED(session), ArkimePacket_t * const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL ArkimePacketRC igmp_packet_enqueue(ArkimePacketBatch_t * UNUSED(batch), ArkimePacket_t * const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[ARKIME_SESSIONID_LEN];

    // no sanity checks until we parse.

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    igmp_create_sessionid(sessionId, packet);

    packet->hash = arkime_session_hash(sessionId);
    packet->mProtocol = igmpMProtocol;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
LOCAL void igmp_pq_cb(ArkimeSession_t *session, void UNUSED(*uw))
{
    session->midSave = 1;
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ip_cb(IPPROTO_IGMP, igmp_packet_enqueue);
    igmpPq = arkime_pq_alloc(10, igmp_pq_cb);
    igmpMProtocol = arkime_mprotocol_register("igmp",
                                             SESSION_OTHER,
                                             igmp_create_sessionid,
                                             igmp_pre_process,
                                             igmp_process,
                                             NULL);
}
