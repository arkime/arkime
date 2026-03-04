/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <net/ethernet.h>

extern ArkimeConfig_t        config;

/******************************************************************************/
LOCAL ArkimePacketRC pppoe_packet_enqueue(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    BSB bsb;
    BSB_INIT(bsb, data, len);

    uint8_t ver_type = 0, code = 0;
    BSB_IMPORT_u08(bsb, ver_type);
    BSB_IMPORT_u08(bsb, code);

    if (BSB_IS_ERROR(bsb) || ver_type != 0x11 || code != 0)
        return ARKIME_PACKET_CORRUPT;

    BSB_IMPORT_skip(bsb, 2); // session_id

    uint16_t plen = 0, type = 0;
    BSB_IMPORT_u16(bsb, plen);
    BSB_IMPORT_u16(bsb, type);

    if (BSB_IS_ERROR(bsb) || plen != len - 6)
        return ARKIME_PACKET_CORRUPT;

    packet->tunnel |= ARKIME_PACKET_TUNNEL_PPPOE;
    switch (type) {
    case 0x21:
        return arkime_packet_run_ethernet_cb(batch, packet, BSB_WORK_PTR(bsb), BSB_REMAINING(bsb), ETHERTYPE_IP, "PPP");
    case 0x57:
        return arkime_packet_run_ethernet_cb(batch, packet, BSB_WORK_PTR(bsb), BSB_REMAINING(bsb), ETHERTYPE_IPV6, "PPP");
    default:
        return ARKIME_PACKET_UNKNOWN_ETHER;
    }
}
/******************************************************************************/
// PPP over GRE/PPTP (ethertype 0x880b)
// Matches Wireshark's dissect_ppp_hdlc_common + dissect_ppp_common logic
LOCAL ArkimePacketRC pptp_ppp_packet_enqueue(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    BSB bsb;
    BSB_INIT(bsb, data, len);

    // Check for HDLC-like framing: address(0xff) + control(0x03)
    uint8_t byte0 = 0;
    BSB_IMPORT_u08(bsb, byte0);
    if (BSB_IS_ERROR(bsb))
        return ARKIME_PACKET_CORRUPT;

    if (byte0 == 0xff) {
        BSB_IMPORT_skip(bsb, 1); // skip control byte
        if (BSB_IS_ERROR(bsb))
            return ARKIME_PACKET_CORRUPT;
        BSB_IMPORT_u08(bsb, byte0); // read first byte of protocol field
        if (BSB_IS_ERROR(bsb))
            return ARKIME_PACKET_CORRUPT;
    }

    // PPP protocol field with Protocol Field Compression (PFC) support
    uint16_t protocol;
    if (byte0 & 0x01) {
        // PFC: 1-byte protocol
        protocol = byte0;
    } else {
        uint8_t byte1 = 0;
        BSB_IMPORT_u08(bsb, byte1);
        if (BSB_IS_ERROR(bsb))
            return ARKIME_PACKET_CORRUPT;
        protocol = byte0 << 8 | byte1;
    }

    packet->tunnel |= ARKIME_PACKET_TUNNEL_PPP;

    switch (protocol) {
    case 0x0021:
        return arkime_packet_run_ethernet_cb(batch, packet, BSB_WORK_PTR(bsb), BSB_REMAINING(bsb), ETHERTYPE_IP, "PPP");
    case 0x0057:
        return arkime_packet_run_ethernet_cb(batch, packet, BSB_WORK_PTR(bsb), BSB_REMAINING(bsb), ETHERTYPE_IPV6, "PPP");
    default:
        return ARKIME_PACKET_UNKNOWN_ETHER;
    }
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ethernet_cb(0x880b, pptp_ppp_packet_enqueue);
    arkime_packet_set_ethernet_cb(0x8864, pppoe_packet_enqueue);
}
