/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

//#define ARPDEBUG 1

extern ArkimeConfig_t        config;

LOCAL int arpMProtocol;
LOCAL int macField;
LOCAL int ouiField;
LOCAL int opcodeField;
LOCAL int ipField;

/******************************************************************************/
LOCAL void arp_create_sessionid(uint8_t *sessionId, ArkimePacket_t *packet)
{
    const uint8_t *data = packet->pkt + packet->payloadOffset;

    sessionId[0] = 8;
    sessionId[1] = arpMProtocol;
    if (data[7] == 1)
        memcpy(sessionId + 2, data + 24, 4);
    else
        memcpy(sessionId + 2, data + 14, 4);
    sessionId[6] = sessionId[7] = 0;
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL int arp_pre_process(ArkimeSession_t *session, ArkimePacket_t *const packet, int isNewSession)
{
    const uint8_t *data = packet->pkt + packet->payloadOffset;
    const int isReply = (data[7] == 2);

    if (isNewSession)
        arkime_session_add_protocol(session, "arp");

    arkime_field_int_add(opcodeField, session, data[7]);

    // ARP replies are the trustworthy IP↔MAC binding: announce arp.ip (sender
    // protocol address) + arp.mac (sender hardware address). Polling/hunts
    // recover the (ip, mac) tuple from session fields without parsing pcap.
    // Requests get no arp.ip/arp.mac so they're naturally filtered out.
    if (isReply) {
        arkime_field_ip4_add(ipField, session, *(uint32_t *)(data + 14));
        arkime_field_macoui_add(session, macField, ouiField, data + 8);
    }

    return 0;
}
/******************************************************************************/
LOCAL int arp_process(ArkimeSession_t *UNUSED(session), ArkimePacket_t *const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL ArkimePacketRC arp_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[ARKIME_SESSIONID_LEN];

    if (len < 28)
        return ARKIME_PACKET_CORRUPT;

    // Validate Ethernet/IPv4 ARP: htype=1, ptype=0x0800, hlen=6, plen=4, opcode 1 or 2
    if (data[0] != 0x00 || data[1] != 0x01 ||
        data[2] != 0x08 || data[3] != 0x00 ||
        data[4] != 6 || data[5] != 4 ||
        data[6] != 0 || data[7] < 1 || data[7] > 2)
        return ARKIME_PACKET_CORRUPT;

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    arp_create_sessionid(sessionId, packet);

    packet->hash = arkime_session_hash(sessionId);
    packet->mProtocol = arpMProtocol;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
void arkime_parser_init()
{
    macField = arkime_field_define("arp", "lotermfield",
                                   "arp.mac", "ARP Sender MAC", "arp.mac",
                                   "Ethernet sender hardware address from ARP replies (the trustworthy IP↔MAC binding for srcIp)",
                                   ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                   (char *)NULL);

    ouiField = arkime_field_define("arp", "termfield",
                                   "arp.oui", "ARP Sender OUI", "arp.oui",
                                   "Ethernet sender OUI from ARP replies",
                                   ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                   (char *)NULL);

    opcodeField = arkime_field_define("arp", "integer",
                                      "arp.opcode", "ARP Opcode", "arp.opcode",
                                      "ARP operation (1=request, 2=reply)",
                                      ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                      (char *)NULL);

    ipField = arkime_field_define("arp", "ip",
                                  "arp.ip", "ARP Sender IP", "arp.ip",
                                  "Sender protocol address from ARP replies (the IP side of the trustworthy IP↔MAC binding)",
                                  ARKIME_FIELD_TYPE_IP_GHASH, ARKIME_FIELD_FLAG_CNT,
                                  (char *)NULL);

    arkime_packet_set_ethernet_cb(0x0806, arp_packet_enqueue);
    arpMProtocol = arkime_mprotocol_register("arp",
                                             SESSION_OTHER,
                                             arp_create_sessionid,
                                             arp_pre_process,
                                             arp_process,
                                             NULL,
                                             NULL,
                                             600);
}
