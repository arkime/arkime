/* esp.c
 *
 * Copyright 2019 AOL Inc. All rights reserved.
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
#include "patricia.h"
#include <inttypes.h>
#include <arpa/inet.h>
#include <errno.h>


/******************************************************************************/
extern MolochConfig_t        config;

LOCAL  int                   espMProtocol;

/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL MolochPacketRC esp_packet_enqueue(MolochPacketBatch_t * UNUSED(batch), MolochPacket_t * const packet, const uint8_t *UNUSED(data), int UNUSED(len))
{
    uint8_t                 sessionId[MOLOCH_SESSIONID_LEN];

    if (packet->v6) {
        struct ip6_hdr *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);
        moloch_session_id6(sessionId, ip6->ip6_src.s6_addr, 0, ip6->ip6_dst.s6_addr, 0);
    } else {
        struct ip *ip4 = (struct ip*)(packet->pkt + packet->ipOffset);
        moloch_session_id(sessionId, ip4->ip_src.s_addr, 0, ip4->ip_dst.s_addr, 0);
    }

    packet->mProtocol = espMProtocol;
    packet->hash = moloch_session_hash(sessionId);

    return MOLOCH_PACKET_DO_PROCESS;
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL void esp_create_sessionid(uint8_t *sessionId, MolochPacket_t *packet)
{
    struct ip           *ip4 = (struct ip*)(packet->pkt + packet->ipOffset);
    struct ip6_hdr      *ip6 = (struct ip6_hdr*)(packet->pkt + packet->ipOffset);

    if (packet->v6) {
        moloch_session_id6(sessionId, ip6->ip6_src.s6_addr, 0,
                           ip6->ip6_dst.s6_addr, 0);
    } else {
        moloch_session_id(sessionId, ip4->ip_src.s_addr, 0,
                          ip4->ip_dst.s_addr, 0);
    }
}
/******************************************************************************/
LOCAL void esp_pre_process(MolochSession_t *session, MolochPacket_t * const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        moloch_session_add_protocol(session, "esp");
    session->stopSaving = 1;
}
/******************************************************************************/
void moloch_parser_init()
{
    if (!config.trackESP)
        return;

    moloch_packet_set_ip_cb(IPPROTO_ESP, esp_packet_enqueue);
    espMProtocol = moloch_mprotocol_register("esp",
                                             SESSION_ESP,
                                             esp_create_sessionid,
                                             esp_pre_process,
                                             NULL,
                                             NULL);
}
