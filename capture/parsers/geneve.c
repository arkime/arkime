/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <net/ethernet.h>

extern ArkimeConfig_t        config;

/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL ArkimePacketRC geneve_packet_enqueue(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    if (len <= 8)
        return ARKIME_PACKET_UNKNOWN;

    if ((data[0] & 0xc0) != 0 || (data[1] & 0x3f) != 0)
        return ARKIME_PACKET_UNKNOWN;

    uint8_t  veroptlen = 0;
    uint16_t protocol;

    BSB bsb;
    BSB_INIT(bsb, data, len);

    BSB_IMPORT_u08(bsb, veroptlen);
    BSB_IMPORT_skip(bsb, 1);
    BSB_IMPORT_u16(bsb, protocol);
    BSB_IMPORT_skip(bsb, 4);

    if (BSB_IS_ERROR(bsb)) {
        return ARKIME_PACKET_UNKNOWN;
    }

    veroptlen &= 0x3f;
    BSB_IMPORT_skip(bsb, veroptlen * 4);

    if (BSB_IS_ERROR(bsb)) {
        return ARKIME_PACKET_UNKNOWN;
    }

    packet->tunnel |= ARKIME_PACKET_TUNNEL_GENEVE;

    return arkime_packet_run_ethernet_cb(batch, packet, BSB_WORK_PTR(bsb), BSB_REMAINING(bsb), protocol, "geneve");
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_udpport_enqueue_cb(6081, geneve_packet_enqueue);
}
