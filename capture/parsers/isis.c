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

//#define ISISDEBUG 1

extern MolochConfig_t        config;

LOCAL MolochPQ_t *isisPq;

LOCAL int isisMProtocol;

/******************************************************************************/
void isis_create_sessionid(char *sessionId, MolochPacket_t *packet)
{
    uint8_t *data = packet->pkt + packet->payloadOffset;

    sessionId[0] = 1;
    sessionId[1] = 0x83;

    // for now, lump all isis into the same session
}
/******************************************************************************/
void isis_pre_process(MolochSession_t *session, MolochPacket_t * const UNUSED(packet), int isNewSession)
{
    char msg[32];

    if (isNewSession)
        moloch_session_add_protocol(session, "isis");

    if (packet->pktlen < 22) 
      return;

    switch (packet->pkt[21]) {
      case 0x10: 
        moloch_session_add_protocol(session, "isis-hello");
        break;
      case 0x19: 
        moloch_session_add_protocol(session, "isis-csnp");
        break;
      case 0x14: 
      case 0x16: 
        moloch_session_add_protocol(session, "isis-lsp");
        break;
      default: 
        sprintf (msg, "isis-unkmsg-%d", packet->pkt[21]);
        moloch_session_add_protocol(session, msg);
    }
}
/******************************************************************************/
int isis_process(MolochSession_t *UNUSED(session), MolochPacket_t * const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
int isis_packet_enqueue(MolochPacketBatch_t * UNUSED(batch), MolochPacket_t * const packet, const uint8_t *data, int len)
{
    char sessionId[MOLOCH_SESSIONID_LEN];

    // no sanity checks until we parse.  the thinking is that it will make sense to 
    // high level parse to determine isis packet type (eg hello, csnp/psnp, lsp) and
    // protocol tag with these additional discriminators

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    isis_create_sessionid(sessionId, packet);

    packet->hash = moloch_session_hash(sessionId);
    packet->mProtocol = isisMProtocol;

    return MOLOCH_PACKET_DO_PROCESS;
}
/******************************************************************************/
LOCAL void isis_pq_cb(MolochSession_t *session, void UNUSED(*uw))
{
    session->midSave = 1;
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_packet_set_ethernet_cb(0x83, isis_packet_enqueue);
    isisPq = moloch_pq_alloc(10, isis_pq_cb);
    isisMProtocol = moloch_mprotocol_register("isis",
                                             SESSION_OTHER,
                                             isis_create_sessionid,
                                             isis_pre_process,
                                             isis_process,
                                             NULL);
}
