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

//#define UNKIPPROTODEBUG 1

extern ArkimeConfig_t        config;

LOCAL ArkimePQ_t *unkIpProtocolPq;

LOCAL int unkIpProtocolMProtocol;

/******************************************************************************/
LOCAL void unkIpProtocol_create_sessionid(uint8_t *sessionId, ArkimePacket_t * const UNUSED (packet))
{
    // uint8_t *data = packet->pkt + packet->payloadOffset;

    sessionId[0] = 2;
    sessionId[1] = 0x9a;
    sessionId[2] = 0x9a;

    // for now, lump all unkIpProtocol into the same session
}
/******************************************************************************/
LOCAL int unkIpProtocol_pre_process(ArkimeSession_t *session, ArkimePacket_t * const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        arkime_session_add_protocol(session, "unkIpProtocol");

    return 0;
}
/******************************************************************************/
LOCAL int unkIpProtocol_process(ArkimeSession_t *UNUSED(session), ArkimePacket_t * const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL ArkimePacketRC unkIpProtocol_packet_enqueue(ArkimePacketBatch_t * UNUSED(batch), ArkimePacket_t * const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[ARKIME_SESSIONID_LEN];

    // no sanity checks

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    unkIpProtocol_create_sessionid(sessionId, packet);

    packet->hash = arkime_session_hash(sessionId);
    packet->mProtocol = unkIpProtocolMProtocol;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
LOCAL void unkIpProtocol_pq_cb(ArkimeSession_t *session, void UNUSED(*uw))
{
    session->midSave = 1;
}
/******************************************************************************/
void arkime_plugin_init()
{
    arkime_packet_set_ip_cb(ARKIME_IPPROTO_UNKNOWN, unkIpProtocol_packet_enqueue);
    unkIpProtocolPq = arkime_pq_alloc(10, unkIpProtocol_pq_cb);
    unkIpProtocolMProtocol = arkime_mprotocol_register("unkIpProtocol",
                                             SESSION_OTHER,
                                             unkIpProtocol_create_sessionid,
                                             unkIpProtocol_pre_process,
                                             unkIpProtocol_process,
                                             NULL);
}
