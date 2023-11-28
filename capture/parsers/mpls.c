/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <net/ethernet.h>

extern ArkimeConfig_t        config;

/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL ArkimePacketRC mpls_packet_enqueue(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    while (1) {
        if (len < 4 + (int)sizeof(struct ip)) {
            return ARKIME_PACKET_CORRUPT;
        }

        int S = data[2] & 0x1;

        data += 4;
        len -= 4;

        if (S) {
            packet->tunnel |= ARKIME_PACKET_TUNNEL_MPLS;
            switch (data[0] >> 4) {
            case 4:
                return arkime_packet_run_ethernet_cb(batch, packet, data, len, ETHERTYPE_IP, "MPLS");
            case 6:
                return arkime_packet_run_ethernet_cb(batch, packet, data, len, ETHERTYPE_IPV6, "MPLS");
            default:
#ifdef DEBUG_PACKET
                LOG("BAD PACKET: Unknown mpls type %d", data[0] >> 4);
#endif
                arkime_packet_save_ethernet(packet, 0x8847);
                return ARKIME_PACKET_UNKNOWN;
            }
        }
    }
    return ARKIME_PACKET_CORRUPT;
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ethernet_cb(ARKIME_ETHERTYPE_MPLS, mpls_packet_enqueue);
}
