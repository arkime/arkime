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
