/* garland.c
 *
 * Copyright 2025 Arkime. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"
extern ArkimeConfig_t        config;

/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL ArkimePacketRC arkime_packet_garland(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    if (len < 20)
        return ARKIME_PACKET_CORRUPT;

    return arkime_packet_run_ethernet_cb(batch, packet, data + 18, len - 18, ARKIME_ETHERTYPE_ETHER, "garland");
}
/******************************************************************************/
void arkime_plugin_init()
{
    uint32_t ethertype = arkime_config_int(NULL, "garlandEthertype", 0xff12, 0, 0xffff);
    arkime_packet_set_ethernet_cb(ethertype, arkime_packet_garland);
}
