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
#include "moloch.h"
#include <net/ethernet.h>

extern MolochConfig_t        config;

/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL MolochPacketRC mpls_packet_enqueue(MolochPacketBatch_t * UNUSED(batch), MolochPacket_t * const packet, const uint8_t *data, int len)
{
    while (1) {
        if (len < 4 + (int)sizeof(struct ip)) {
            return MOLOCH_PACKET_CORRUPT;
        }

        int S = data[2] & 0x1;

        data += 4;
        len -= 4;

        if (S) {
            packet->tunnel |= MOLOCH_PACKET_TUNNEL_MPLS;
            switch (data[0] >> 4) {
            case 4:
                return moloch_packet_run_ethernet_cb(batch, packet, data, len, ETHERTYPE_IP, "MPLS");
            case 6:
                return moloch_packet_run_ethernet_cb(batch, packet, data, len, ETHERTYPE_IPV6, "MPLS");
            default:
#ifdef DEBUG_PACKET
                LOG("BAD PACKET: Unknown mpls type %d", data[0] >> 4);
#endif
                moloch_packet_save_ethernet(packet, 0x8847);
                return MOLOCH_PACKET_UNKNOWN;
            }
        }
    }
    return MOLOCH_PACKET_CORRUPT;
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_packet_set_ethernet_cb(0x8847, mpls_packet_enqueue);
}
