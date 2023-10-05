/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <net/ethernet.h>

extern ArkimeConfig_t        config;

/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL ArkimePacketRC ciscometadata_packet_enqueue(ArkimePacketBatch_t * UNUSED(batch), ArkimePacket_t * const packet, const uint8_t *data, int len)
{
    if (len < 8)
        return ARKIME_PACKET_CORRUPT;

    // Just ignore 6 bytes for now

    return arkime_packet_run_ethernet_cb(batch, packet, data + 6, len - 6, ARKIME_ETHERTYPE_DETECT, "CISCOMETADATA");
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ethernet_cb(0x8909, ciscometadata_packet_enqueue);
}
