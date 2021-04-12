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
LOCAL MolochPacketRC ciscometadata_packet_enqueue(MolochPacketBatch_t * UNUSED(batch), MolochPacket_t * const packet, const uint8_t *data, int len)
{
    if (len < 8)
        return MOLOCH_PACKET_CORRUPT;

    // Just ignore 6 bytes for now

    return moloch_packet_run_ethernet_cb(batch, packet, data+6, len-6, MOLOCH_ETHERTYPE_DETECT, "CISCOMETADATA");
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_packet_set_ethernet_cb(0x8909, ciscometadata_packet_enqueue);
}
