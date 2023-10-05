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
LOCAL ArkimePacketRC pppoe_packet_enqueue(ArkimePacketBatch_t * batch, ArkimePacket_t * const packet, const uint8_t *data, int len)
{
    if (len < 8 || data[0] != 0x11 || data[1] != 0) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: Len or bytes %d %d %d", len, data[0], data[1]);
#endif
        return ARKIME_PACKET_CORRUPT;
    }

    uint16_t plen = data[4] << 8 | data[5];
    uint16_t type = data[6] << 8 | data[7];
    if (plen != len - 6)
        return ARKIME_PACKET_CORRUPT;

    packet->tunnel |= ARKIME_PACKET_TUNNEL_PPPOE;
    switch (type) {
    case 0x21:
        return arkime_packet_run_ethernet_cb(batch, packet, data + 8, len - 8, ETHERTYPE_IP, "PPP");
    case 0x57:
        return arkime_packet_run_ethernet_cb(batch, packet, data + 8, len- 8, ETHERTYPE_IPV6, "PPP");
    default:
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: Unknown pppoe type %d", type);
#endif
        arkime_packet_save_ethernet(packet, 0x8864);
        return ARKIME_PACKET_UNKNOWN;
    }
}
/******************************************************************************/
LOCAL ArkimePacketRC ppp_packet_enqueue(ArkimePacketBatch_t * batch, ArkimePacket_t * const packet, const uint8_t *data, int len)
{
    if (len < 4 || data[2] != 0x00) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: Len or bytes %d %d %d", len, data[2], data[3]);
#endif
        return ARKIME_PACKET_CORRUPT;
    }

    packet->tunnel |= ARKIME_PACKET_TUNNEL_PPP;
    switch (data[3]) {
    case 0x21:
        return arkime_packet_run_ethernet_cb(batch, packet, data + 4, len - 4, ETHERTYPE_IP, "PPP");
    case 0x57:
        return arkime_packet_run_ethernet_cb(batch, packet, data + 4, len - 4, ETHERTYPE_IPV6, "PPP");
    default:
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: Unknown ppp type %d", data[3]);
#endif
        arkime_packet_save_ethernet(packet, 0x880b);
        return ARKIME_PACKET_UNKNOWN;
    }
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ethernet_cb(0x880b, ppp_packet_enqueue);
    arkime_packet_set_ethernet_cb(0x8864, pppoe_packet_enqueue);
}
