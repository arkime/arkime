/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <net/ethernet.h>

extern ArkimeConfig_t        config;

/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL ArkimePacketRC nsh_packet_enqueue(ArkimePacketBatch_t * batch, ArkimePacket_t * const packet, const uint8_t *data, int len)
{
    if (len < 4) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: Too short %d", len);
#endif
        return ARKIME_PACKET_CORRUPT;
    }

    int length = (data[1] & 0x3f) * 4;

    if (length < 4) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: Invalid length %d", length);
#endif
        return ARKIME_PACKET_CORRUPT;
    }

    if (len < length) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: Too short %d < %d", len, length);
#endif
        return ARKIME_PACKET_CORRUPT;
    }

    switch (data[3]) {
    case 1:
        return arkime_packet_run_ethernet_cb(batch, packet, data+length, len-length, ETHERTYPE_IP, "NSH");
    case 2:
        return arkime_packet_run_ethernet_cb(batch, packet, data+length, len-length, ETHERTYPE_IPV6, "NSH");
    case 3:
        return arkime_packet_run_ethernet_cb(batch, packet, data+length, len-length, ARKIME_ETHERTYPE_ETHER, "NSH");
    case 4:
        return arkime_packet_run_ethernet_cb(batch, packet, data+length, len-length, ARKIME_ETHERTYPE_NSH, "NSH");
    case 5:
        return arkime_packet_run_ethernet_cb(batch, packet, data+length, len-length, ARKIME_ETHERTYPE_MPLS, "NSH");
    default:
        return ARKIME_PACKET_CORRUPT;
    }
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ethernet_cb(ARKIME_ETHERTYPE_NSH, nsh_packet_enqueue);
}
