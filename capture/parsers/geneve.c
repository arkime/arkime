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
LOCAL MolochPacketRC geneve_packet_enqueue(MolochPacketBatch_t * batch, MolochPacket_t * const packet, const uint8_t *data, int len)
{
    if (len <= 8)
        return MOLOCH_PACKET_UNKNOWN;

    if ((data[0] & 0xc0) != 0 || (data[1] & 0x3f) != 0)
        return MOLOCH_PACKET_UNKNOWN;

    uint8_t  veroptlen = 0;
    uint16_t protocol;

    BSB bsb;
    BSB_INIT(bsb, data, len);

    BSB_IMPORT_u08(bsb, veroptlen);
    BSB_IMPORT_skip(bsb, 1);
    BSB_IMPORT_u16(bsb, protocol);
    BSB_IMPORT_skip(bsb, 4);

    if (BSB_IS_ERROR(bsb)) {
        return MOLOCH_PACKET_UNKNOWN;
    }

    veroptlen &= 0x3f;
    BSB_IMPORT_skip(bsb, veroptlen * 4);

    if (BSB_IS_ERROR(bsb)) {
        return MOLOCH_PACKET_UNKNOWN;
    }

    packet->tunnel |= MOLOCH_PACKET_TUNNEL_GENEVE;

    return moloch_packet_run_ethernet_cb(batch, packet, BSB_WORK_PTR(bsb), BSB_REMAINING(bsb), protocol, "geneve");
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_packet_set_udpport_enqueue_cb(6081, geneve_packet_enqueue);
}
