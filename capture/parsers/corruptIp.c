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

//#define CORRUPTDEBUG 1

extern MolochConfig_t        config;

LOCAL MolochPQ_t *corruptIpPq;

LOCAL int corruptIpMProtocol;

/******************************************************************************/
void corruptIp_create_sessionid(char *sessionId, MolochPacket_t *packet)
{
    // uint8_t *data = packet->pkt + packet->payloadOffset;

		printf ("corruptIp-- creating session id\n");

    sessionId[0] = 3;
    sessionId[1] = 0xc;
    sessionId[2] = 0xc;
    sessionId[3] = 0xc;

    // for now, lump all corruptIp into the same session
}
/******************************************************************************/
void corruptIp_pre_process(MolochSession_t *session, MolochPacket_t * const UNUSED(packet), int isNewSession)
{
		printf ("corruptIp-- pre process\n");
    if (isNewSession)
        moloch_session_add_protocol(session, "corruptIp");
}
/******************************************************************************/
int corruptIp_process(MolochSession_t *UNUSED(session), MolochPacket_t * const UNUSED(packet))
{
		printf ("corruptIp process\n");
    return 1;
}
/******************************************************************************/
int corruptIp_packet_enqueue(MolochPacketBatch_t * UNUSED(batch), MolochPacket_t * const packet, const uint8_t *data, int len)
{
    char sessionId[MOLOCH_SESSIONID_LEN];

		printf ("corruptIp enqueue\n");

    // no sanity checks until we parse.  the thinking is that it will make sense to 
    // high level parse to determine corruptIp packet type (eg hello, csnp/psnp, lsp) and
    // protocol tag with these additional discriminators

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

		printf ("corruptIp packet->payloadLen=%d\n", packet->payloadLen);
		printf ("corruptIp packet->payloadOffset=%d\n", packet->payloadOffset);

    corruptIp_create_sessionid(sessionId, packet);

    packet->hash = moloch_session_hash(sessionId);
    packet->mProtocol = corruptIpMProtocol;

		printf ("assigning corruptIp packet->mProtocol=%d\n", packet->mProtocol);

    return MOLOCH_PACKET_DO_PROCESS;
}
/******************************************************************************/
LOCAL void corruptIp_pq_cb(MolochSession_t *session, void UNUSED(*uw))
{
		printf ("corruptIp-- pq cb\n");
    session->midSave = 1;
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_packet_set_ip_cb(MOLOCH_CORRUPT_IP, corruptIp_packet_enqueue);
    corruptIpPq = moloch_pq_alloc(10, corruptIp_pq_cb);
    corruptIpMProtocol = moloch_mprotocol_register("corruptIp",
                                             SESSION_OTHER,
                                             corruptIp_create_sessionid,
                                             corruptIp_pre_process,
                                             corruptIp_process,
                                             NULL);
}
