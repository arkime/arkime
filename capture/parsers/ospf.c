/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

//#define OSPFDEBUG 1

extern ArkimeConfig_t        config;

LOCAL int ospfMProtocol;
LOCAL int msgTypeField;
LOCAL int routerIdField;
LOCAL int areaIdField;

/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL void ospf_create_sessionid(uint8_t *sessionId, ArkimePacket_t *const packet)
{
    const struct ip           *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
    const struct ip6_hdr      *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);

    if (packet->v6) {
        arkime_session_id6(sessionId, ip6->ip6_src.s6_addr, 0,
                           ip6->ip6_dst.s6_addr, 0, packet->vlan, packet->vni);
    } else {
        arkime_session_id(sessionId, ip4->ip_src.s_addr, 0,
                          ip4->ip_dst.s_addr, 0, packet->vlan, packet->vni);
    }
}
/******************************************************************************/
LOCAL int ospf_pre_process(ArkimeSession_t *session, ArkimePacket_t *const packet, int isNewSession)
{
    if (isNewSession)
        arkime_session_add_protocol(session, "ospf");

    // Only inspect the first few packets per session; OSPF identifiers
    // stabilize quickly and don't need to be re-parsed on every Hello.
    if (session->packets[0] + session->packets[1] > 10)
        return 0;

    if (packet->payloadLen < 12)
        return 0;

    const uint8_t *data = packet->pkt + packet->payloadOffset;
    const uint8_t version = data[0];
    const uint8_t type = data[1];

    if (version != 2 && version != 3)
        return 0;

    const char *typeName = NULL;
    char buf[32];
    switch (type) {
    case 1:
        typeName = "Hello";
        break;
    case 2:
        typeName = "DBDesc";
        break;
    case 3:
        typeName = "LSReq";
        break;
    case 4:
        typeName = "LSUpdate";
        break;
    case 5:
        typeName = "LSAck";
        break;
    default:
        snprintf(buf, sizeof(buf), "unk-%d", type);
        typeName = buf;
    }
    arkime_field_string_add(msgTypeField, session, typeName, -1, TRUE);

    char ipbuf[INET_ADDRSTRLEN];
    snprintf(ipbuf, sizeof(ipbuf), "%u.%u.%u.%u", data[4], data[5], data[6], data[7]);
    arkime_field_string_add(routerIdField, session, ipbuf, -1, TRUE);

    snprintf(ipbuf, sizeof(ipbuf), "%u.%u.%u.%u", data[8], data[9], data[10], data[11]);
    arkime_field_string_add(areaIdField, session, ipbuf, -1, TRUE);

    // OSPFv2 AuType at offset 14: tag weak/missing authentication
    if (version == 2 && packet->payloadLen >= 16) {
        const uint16_t auType = (data[14] << 8) | data[15];
        if (auType == 0)
            arkime_session_add_tag(session, "ospf:auth-null");
        else if (auType == 1)
            arkime_session_add_tag(session, "ospf:auth-simple");
    }

    return 0;
}
/******************************************************************************/
LOCAL int ospf_process(ArkimeSession_t *UNUSED(session), ArkimePacket_t *const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL ArkimePacketRC ospf_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[ARKIME_SESSIONID_LEN];

    // no sanity checks until we parse.

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    ospf_create_sessionid(sessionId, packet);

    packet->hash = arkime_session_hash(sessionId);
    packet->mProtocol = ospfMProtocol;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ip_cb(89, ospf_packet_enqueue);
    ospfMProtocol = arkime_mprotocol_register("ospf",
                                              SESSION_OTHER,
                                              ospf_create_sessionid,
                                              ospf_pre_process,
                                              ospf_process,
                                              NULL,
                                              NULL,
                                              600);

    msgTypeField = arkime_field_define("ospf", "lotermfield",
                                       "ospf.msgType", "OSPF Msg Type", "ospf.msgType",
                                       "OSPF packet type",
                                       ARKIME_FIELD_TYPE_STR_GHASH, 0,
                                       (char *)NULL);

    routerIdField = arkime_field_define("ospf", "termfield",
                                        "ospf.routerId", "Router ID", "ospf.routerId",
                                        "OSPF Router ID",
                                        ARKIME_FIELD_TYPE_STR_GHASH, 0,
                                        (char *)NULL);

    areaIdField = arkime_field_define("ospf", "termfield",
                                      "ospf.areaId", "Area ID", "ospf.areaId",
                                      "OSPF Area ID",
                                      ARKIME_FIELD_TYPE_STR_GHASH, 0,
                                      (char *)NULL);
}
