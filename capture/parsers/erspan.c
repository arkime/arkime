/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

extern ArkimeConfig_t        config;

/******************************************************************************/
LOCAL ArkimePacketRC erspan_packet_enqueue(ArkimePacketBatch_t * UNUSED(batch), ArkimePacket_t * const packet, const uint8_t *data, int len)
{
    if (unlikely(len) < 8 || unlikely(!data))
        return ARKIME_PACKET_CORRUPT;

    if ((*data >> 4) != 1) {
        if (config.logUnknownProtocols)
            LOG("Unknown ERSPAN protocol %d", (*data >> 4));
        arkime_packet_save_ethernet(packet, 0x88be);
        return ARKIME_PACKET_UNKNOWN;
    }


    BSB bsb;

    BSB_INIT(bsb, data, len);

    BSB_IMPORT_u16(bsb, packet->vlan);
    packet->vlan &= 0x7f; // clear the version bits

    return arkime_packet_run_ethernet_cb(batch, packet, data + 8,len - 8, ARKIME_ETHERTYPE_ETHER, "ERSpan");
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ethernet_cb(0x88be, erspan_packet_enqueue);
}
