/* esp.c
 *
 * Copyright 2019 AOL Inc. All rights reserved.
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
#include "patricia.h"
#include <inttypes.h>
#include <arpa/inet.h>
#include <errno.h>


/******************************************************************************/
extern MolochConfig_t        config;

/******************************************************************************/
LOCAL MolochPacketRC gre_packet_enqueue(MolochPacketBatch_t * UNUSED(batch), MolochPacket_t * const packet, const uint8_t *data, int UNUSED(len))
{
    packet->tunnel |= MOLOCH_PACKET_TUNNEL_GRE;
    packet->vpnIpOffset = packet->ipOffset; // ipOffset will get reset

    //LOG("ALW ipOffset %d payloadOffset %d diff %d", packet->ipOffset, packet->payloadOffset, (int)(data - packet->pkt));

    BSB bsb;
    if (unlikely(len) < 4 || unlikely(!data))
        return MOLOCH_PACKET_CORRUPT;

    BSB_INIT(bsb, data, len);

    uint16_t flags_version = 0;
    BSB_IMPORT_u16(bsb, flags_version);
    uint16_t type = 0;
    BSB_IMPORT_u16(bsb, type);

    if (flags_version & (0x8000 | 0x4000)) {
        BSB_IMPORT_skip(bsb, 4); // skip len and offset
    }

    // key
    if (flags_version & 0x2000) {
        BSB_IMPORT_skip(bsb, 4);
    }

    // sequence number
    if (flags_version & 0x1000) {
        BSB_IMPORT_skip(bsb, 4);
    }

    // routing
    if (flags_version & 0x4000) {
        while (BSB_NOT_ERROR(bsb)) {
            BSB_IMPORT_skip(bsb, 3);
            int tlen = 0;
            BSB_IMPORT_u08(bsb, tlen);
            if (tlen == 0)
                break;
            BSB_IMPORT_skip(bsb, tlen);
        }
    }

    // ack number
    if (flags_version & 0x0080) {
        BSB_IMPORT_skip(bsb, 4);
    }

    if (BSB_IS_ERROR(bsb))
        return MOLOCH_PACKET_CORRUPT;

    return moloch_packet_run_ethernet_cb(batch, packet, BSB_WORK_PTR(bsb), BSB_REMAINING(bsb), type, "GRE");
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_packet_set_ip_cb(IPPROTO_GRE, gre_packet_enqueue);
}
