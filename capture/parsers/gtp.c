/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <net/ethernet.h>

extern ArkimeConfig_t        config;

/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL ArkimePacketRC gtp_packet_enqueue(ArkimePacketBatch_t * batch, ArkimePacket_t * const packet, const uint8_t *data, int len)
{
    if (len <= 12)
        return ARKIME_PACKET_UNKNOWN;

    if ((data[0] & 0xf0) != 0x30 || data[1] != 0xff || (data[2] << 8 | data[3]) != len - 8)
        return ARKIME_PACKET_UNKNOWN;

    BSB bsb;
    BSB_INIT(bsb, data, len);

    uint8_t  flags = 0;
    uint8_t  next = 0;

    BSB_IMPORT_u08(bsb, flags);
    BSB_IMPORT_skip(bsb, 1); // mtype
    BSB_IMPORT_skip(bsb, 2); // mlen
    BSB_IMPORT_skip(bsb, 4); // teid
    if (flags & 0x7) {
        BSB_IMPORT_skip(bsb, 3);
        BSB_IMPORT_u08(bsb, next);
    }

    while (next != 0 && !BSB_IS_ERROR(bsb)) {
        uint8_t extlen = 0;
        BSB_IMPORT_u08(bsb, extlen);
        if (extlen == 0) {
            return ARKIME_PACKET_UNKNOWN;
        }
        BSB_IMPORT_skip(bsb, extlen * 4 - 2);
        BSB_IMPORT_u08(bsb, next);
    }

    if (BSB_IS_ERROR(bsb)) {
        return ARKIME_PACKET_UNKNOWN;
    }

    packet->tunnel |= ARKIME_PACKET_TUNNEL_GTP;

    // Should check for v4 vs v6 here
    BSB_IMPORT_u08(bsb, flags);
    BSB_IMPORT_rewind(bsb, 1);

    if ((flags & 0xf0) == 0x60)
        return arkime_packet_run_ethernet_cb(batch, packet, BSB_WORK_PTR(bsb), BSB_REMAINING(bsb), ETHERTYPE_IPV6, "gtp");
    return arkime_packet_run_ethernet_cb(batch, packet, BSB_WORK_PTR(bsb), BSB_REMAINING(bsb), ETHERTYPE_IP, "gtp");
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_udpport_enqueue_cb(2152, gtp_packet_enqueue);
}
