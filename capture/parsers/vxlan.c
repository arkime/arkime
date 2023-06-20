/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "arkime.h"
#include <net/ethernet.h>

extern ArkimeConfig_t        config;

/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL ArkimePacketRC vxlan_packet_enqueue(ArkimePacketBatch_t * batch, ArkimePacket_t * const packet, const uint8_t *data, int len)
{
    if (len <= 8)
        return ARKIME_PACKET_UNKNOWN;

    if ((data[0] & 0x77) != 0 || (data[1] & 0xb7) != 0)
        return ARKIME_PACKET_UNKNOWN;

    if ((data[0] & 0x08) == 0x08) {
        packet->vni = (data[4] << 16) | (data[5] << 8) | data[6];
    }

    packet->tunnel |= ARKIME_PACKET_TUNNEL_VXLAN;
    return arkime_packet_run_ethernet_cb(batch, packet, data+8, len-8, ARKIME_ETHERTYPE_ETHER, "vxlan");
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL ArkimePacketRC vxlan_gpe_packet_enqueue(ArkimePacketBatch_t * batch, ArkimePacket_t * const packet, const uint8_t *data, int len)
{
    if (len <= 8)
        return ARKIME_PACKET_UNKNOWN;

    if ((data[0] & 0xf0) != 0 || (data[1] & 0xff) != 0)
        return ARKIME_PACKET_UNKNOWN;

    packet->tunnel |= ARKIME_PACKET_TUNNEL_VXLAN_GPE;
    
    switch (data[3]) {
    case 1:
        return arkime_packet_run_ethernet_cb(batch, packet, data+8, len-8, ETHERTYPE_IP, "vxlan-gpe");
    case 2:
        return arkime_packet_run_ethernet_cb(batch, packet, data+8, len-8, ETHERTYPE_IPV6, "vxlan-gpe");
    case 3:
        return arkime_packet_run_ethernet_cb(batch, packet, data+8, len-8, ARKIME_ETHERTYPE_ETHER, "vxlan-gpe");
    case 4:
        return arkime_packet_run_ethernet_cb(batch, packet, data+8, len-8, ARKIME_ETHERTYPE_NSH, "vxlan-gpe");
    }
    return ARKIME_PACKET_UNKNOWN;
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_udpport_enqueue_cb(4789, vxlan_packet_enqueue);
    arkime_packet_set_udpport_enqueue_cb(4790, vxlan_gpe_packet_enqueue);
}
