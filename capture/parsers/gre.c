/* gre.c
 *
 * Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"
#include "patricia.h"
#include <inttypes.h>
#include <arpa/inet.h>
#include <errno.h>


/******************************************************************************/
extern ArkimeConfig_t        config;

/******************************************************************************/
LOCAL ArkimePacketRC gre_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *data, int UNUSED(len))
{
    packet->tunnel |= ARKIME_PACKET_TUNNEL_GRE;

    BSB bsb;
    if (unlikely(len) < 4 || unlikely(!data))
        return ARKIME_PACKET_CORRUPT;

    BSB_INIT(bsb, data, len);

    uint16_t flags_version = 0;
    BSB_IMPORT_u16(bsb, flags_version);
    uint16_t type = 0;
    BSB_IMPORT_u16(bsb, type);

    int version = flags_version & 0x0007;

    if (version == 1) {
        // Enhanced GRE (RFC 2637 - PPTP)
        // After flags(2)+protocol(2): payload_length(2) + call_id(2)
        BSB_IMPORT_skip(bsb, 4);

        // sequence number
        if (flags_version & 0x1000) {
            BSB_IMPORT_skip(bsb, 4);
        }

        // ack number
        if (flags_version & 0x0080) {
            BSB_IMPORT_skip(bsb, 4);
        }
    } else {
        // Standard GRE (RFC 2784/2890)
        if (flags_version & (0x8000 | 0x4000)) {
            BSB_IMPORT_skip(bsb, 4); // skip checksum and offset
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
    }

    if (BSB_IS_ERROR(bsb))
        return ARKIME_PACKET_CORRUPT;

    // Type I of ERSPAN doesn't have a ERSPAN header
    if (type == 0x88be && (flags_version & 0x1000) == 0) {
        type = ARKIME_ETHERTYPE_ETHER;
    }

    return arkime_packet_run_ethernet_cb(batch, packet, BSB_WORK_PTR(bsb), BSB_REMAINING(bsb), type, "GRE");
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ip_cb(IPPROTO_GRE, gre_packet_enqueue);
}
