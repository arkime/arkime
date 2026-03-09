/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

extern ArkimeConfig_t        config;

/******************************************************************************/
LOCAL ArkimePacketRC erspan_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    if (unlikely(len) < 8 || unlikely(!data))
        return ARKIME_PACKET_CORRUPT;

    if ((*data >> 4) != 1) {
        if (config.logUnknownProtocols)
            LOG("Unknown ERSPAN protocol %d", (*data >> 4));
        return ARKIME_PACKET_UNKNOWN_ETHER;
    }

    BSB bsb;
    BSB_INIT(bsb, data, len);

    /* Type II header (8 bytes):
     * Bytes 0-1: Ver(4) | VLAN(12)
     * Bytes 2-3: COS(3) | Encap(1) | T(1) | SpanID(10)
     * Bytes 4-7: Reserved(12) | Index(20)
     */
    uint16_t first16 = 0;
    uint16_t second16 = 0;
    uint32_t index_word = 0;
    BSB_IMPORT_u16(bsb, first16);
    BSB_IMPORT_u16(bsb, second16);
    BSB_IMPORT_u32(bsb, index_word);

    packet->erspan_ver       = (first16 >> 12) & 0x0f;
    packet->vlan             = first16 & 0x0fff;
    packet->erspan_cos       = (second16 >> 13) & 0x07;
    packet->erspan_truncated = (second16 >> 10) & 0x01;
    packet->erspan_id        = second16 & 0x03ff;
    packet->erspan_port      = index_word & 0x000fffff;
    packet->erspan_type      = 2;

    return arkime_packet_run_ethernet_cb(batch, packet, data + 8, len - 8, ARKIME_ETHERTYPE_ETHER, "ERSpan");
}
/******************************************************************************/
LOCAL ArkimePacketRC erspan_packet_enqueue3(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    if (unlikely(len) < 12 || unlikely(!data))
        return ARKIME_PACKET_CORRUPT;

    if ((*data >> 4) != 2) {
        if (config.logUnknownProtocols)
            LOG("Unknown ERSPAN protocol %d", (*data >> 4));
        return ARKIME_PACKET_UNKNOWN_ETHER;
    }

    BSB bsb;
    BSB_INIT(bsb, data, len);

    /* Type III header (12 bytes):
     * Bytes 0-3:  Ver(4) | VLAN(12) | COS(3) | BSO(2) | T(1) | SessionID(10)
     * Bytes 4-7:  Timestamp(32)
     * Bytes 8-9:  SGT(16)
     * Bytes 10-11: P(1) | FT(5) | Hw(6) | D(1) | Gra(2) | O(1)
     */
    uint32_t word0 = 0;
    uint32_t timestamp32 = 0;
    uint16_t sgt16 = 0;
    uint16_t flags16 = 0;
    BSB_IMPORT_u32(bsb, word0);
    BSB_IMPORT_u32(bsb, timestamp32);
    BSB_IMPORT_u16(bsb, sgt16);
    BSB_IMPORT_u16(bsb, flags16);

    (void)timestamp32; // not stored; reserved for future use

    packet->erspan_ver       = (word0 >> 28) & 0x0f;
    packet->vlan             = (word0 >> 16) & 0x0fff;
    packet->erspan_cos       = (word0 >> 13) & 0x07;
    packet->erspan_bso       = (word0 >> 11) & 0x03;
    packet->erspan_truncated = (word0 >> 10) & 0x01;
    packet->erspan_id        = word0 & 0x03ff;
    packet->erspan_sgt       = sgt16;
    packet->erspan_hw        = (flags16 >> 4) & 0x3f;
    packet->erspan_dir       = (flags16 >> 3) & 0x01;
    packet->erspan_type      = 3;

    /* Optional platform subheader (O bit = bit 0 of flags16) */
    if ((flags16 & 0x0001) && BSB_REMAINING(bsb) >= 8) {
        const uint8_t *subhdr = BSB_WORK_PTR(bsb);
        uint8_t platf_id = (subhdr[0] >> 2) & 0x3f;
        if (platf_id == 0x1) {
            packet->erspan_port = (subhdr[4] << 24) | (subhdr[5] << 16) |
                                  (subhdr[6] << 8)  |  subhdr[7];
        } else if (platf_id == 0x3) {
            packet->erspan_port = (subhdr[2] << 8) | subhdr[3];
        }
        BSB_IMPORT_skip(bsb, 8);
    }

    if (BSB_IS_ERROR(bsb))
        return ARKIME_PACKET_CORRUPT;

    return arkime_packet_run_ethernet_cb(batch, packet, BSB_WORK_PTR(bsb), BSB_REMAINING(bsb), ARKIME_ETHERTYPE_ETHER, "ERSpan");
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ethernet_cb(0x88be, erspan_packet_enqueue); // I & II
    arkime_packet_set_ethernet_cb(0x22eb, erspan_packet_enqueue3); // III
}
