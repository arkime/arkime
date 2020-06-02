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

extern MolochConfig_t        config;

/******************************************************************************/
LOCAL MolochPacketRC erspan_packet_enqueue(MolochPacketBatch_t * UNUSED(batch), MolochPacket_t * const packet, const uint8_t *data, int len)
{
    if (unlikely(len) < 8 || unlikely(!data))
        return MOLOCH_PACKET_CORRUPT;

    if ((*data >> 4) == 1)
        return moloch_packet_run_ethernet_cb(batch, packet, data+8,len-8, MOLOCH_ETHERTYPE_ETHER, "ERSpan");

    if (config.logUnknownProtocols)
        LOG("Unknown ERSPAN protocol %d", *data >> 4);
    moloch_packet_save_ethernet(packet, 0x88be);
    return MOLOCH_PACKET_UNKNOWN;
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_packet_set_ethernet_cb(0x88be, erspan_packet_enqueue);
}
