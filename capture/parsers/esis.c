/* esis.c  -- ES-IS (End System to Intermediate System) Protocol
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL int esisMProtocol;
LOCAL int typeField;

/******************************************************************************/
LOCAL void esis_create_sessionid(uint8_t *sessionId, ArkimePacket_t *UNUSED(packet))
{
    sessionId[0] = 4;
    sessionId[1] = esisMProtocol;
    sessionId[2] = sessionId[3] = 0;

    // lump all esis into the same session
}
/******************************************************************************/
LOCAL int esis_pre_process(ArkimeSession_t *session, ArkimePacket_t *const UNUSED(packet), int isNewSession)
{
    char msg[32];

    if (isNewSession)
        arkime_session_add_protocol(session, "esis");

    if (packet->payloadLen < 1) {
        return 0;
    }

    const uint8_t *data = packet->pkt + packet->payloadOffset;

    // PDU type is at the first byte of the payload
    switch (data[0]) {
    case 2:
        arkime_field_string_add(typeField, session, "esh", -1, TRUE);
        break;
    case 4:
        arkime_field_string_add(typeField, session, "ish", -1, TRUE);
        break;
    case 6:
        arkime_field_string_add(typeField, session, "rd", -1, TRUE);
        break;
    default:
        snprintf(msg, sizeof(msg), "unknown-%d", data[0]);
        if (config.debug)
            LOG("esis %s\n", msg);
        arkime_field_string_add(typeField, session, msg, -1, TRUE);
    }

    return 0;
}
/******************************************************************************/
LOCAL int esis_process(ArkimeSession_t *UNUSED(session), ArkimePacket_t *const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL ArkimePacketRC esis_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[ARKIME_SESSIONID_LEN];

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    esis_create_sessionid(sessionId, packet);

    packet->hash = arkime_session_hash(sessionId);
    packet->mProtocol = esisMProtocol;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ethernet_cb(0x890D, esis_packet_enqueue);
    esisMProtocol = arkime_mprotocol_register("esis",
                                              SESSION_OTHER,
                                              esis_create_sessionid,
                                              esis_pre_process,
                                              esis_process,
                                              NULL,
                                              NULL,
                                              600);


    typeField = arkime_field_define("esis", "lotermfield",
                                    "esis.type", "Type", "esis.type",
                                    "ES-IS PDU Type",
                                    ARKIME_FIELD_TYPE_STR_GHASH, 0,
                                    (char *)NULL);
}
