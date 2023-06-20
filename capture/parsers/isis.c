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

//#define ISISDEBUG 1

extern ArkimeConfig_t        config;

LOCAL ArkimePQ_t *isisPq;

LOCAL int isisMProtocol;
LOCAL int typeField;

/******************************************************************************/
LOCAL void isis_create_sessionid(uint8_t *sessionId, ArkimePacket_t *UNUSED(packet))
{
    sessionId[0] = 1;
    sessionId[1] = 0x83;

    // for now, lump all isis into the same session
}
/******************************************************************************/
LOCAL int isis_pre_process(ArkimeSession_t *session, ArkimePacket_t * const UNUSED(packet), int isNewSession)
{
    char msg[32];

    if (isNewSession)
        arkime_session_add_protocol(session, "isis");

    if (packet->pktlen < 22) {
      snprintf (msg, sizeof(msg), "err-len-%d", packet->pktlen);
      arkime_field_string_add(typeField, session, msg, -1, TRUE);
      return 0;
    }

    switch (packet->pkt[21]) {
      case 15:
        arkime_field_string_add(typeField, session, "lan-l1-hello", -1, TRUE);
        break;
      case 16:
        arkime_field_string_add(typeField, session, "lan-l2-hello", -1, TRUE);
        break;
      case 17:
        arkime_field_string_add(typeField, session, "p2p-hello", -1, TRUE);
        break;
      case 18:
        arkime_field_string_add(typeField, session, "l1-lsp", -1, TRUE);
        break;
      case 20:
        arkime_field_string_add(typeField, session, "l2-lsp", -1, TRUE);
        break;
      case 24:
        arkime_field_string_add(typeField, session, "l1-csnp", -1, TRUE);
        break;
      case 25:
        arkime_field_string_add(typeField, session, "l2-csnp", -1, TRUE);
        break;
      case 26:
        arkime_field_string_add(typeField, session, "l1-psnp", -1, TRUE);
        break;
      case 27:
        arkime_field_string_add(typeField, session, "l2-psnp", -1, TRUE);
        break;
      default:
        snprintf (msg, sizeof(msg), "unk-%d", packet->pkt[21]);
        if (config.debug)
            LOG("isis %s\n", msg);
        arkime_field_string_add(typeField, session, msg, -1, TRUE);
    }

    return 0;
}
/******************************************************************************/
LOCAL int isis_process(ArkimeSession_t *UNUSED(session), ArkimePacket_t * const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL ArkimePacketRC isis_packet_enqueue(ArkimePacketBatch_t * UNUSED(batch), ArkimePacket_t * const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[ARKIME_SESSIONID_LEN];

    // no sanity checks until we parse.  the thinking is that it will make sense to 
    // high level parse to determine isis packet type (eg hello, csnp/psnp, lsp) and
    // protocol tag with these additional discriminators

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    isis_create_sessionid(sessionId, packet);

    packet->hash = arkime_session_hash(sessionId);
    packet->mProtocol = isisMProtocol;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
LOCAL void isis_pq_cb(ArkimeSession_t *session, void UNUSED(*uw))
{
    session->midSave = 1;
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ethernet_cb(0x83, isis_packet_enqueue);
    isisPq = arkime_pq_alloc(10, isis_pq_cb);
    isisMProtocol = arkime_mprotocol_register("isis",
                                             SESSION_OTHER,
                                             isis_create_sessionid,
                                             isis_pre_process,
                                             isis_process,
                                             NULL);


    typeField = arkime_field_define("isis","lotermfield",
        "isis.msgType", "isis.msgType", "isis.msgType",
        "ISIS Msg Type field",
        ARKIME_FIELD_TYPE_STR_GHASH, 0,
        (char *)NULL);
}
