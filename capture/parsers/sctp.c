/* sctp.c
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

#define SCTP_DATA_FLAG_E 0x01  /* End bit */
#define SCTP_DATA_FLAG_B 0x02  /* Beginning bit */
#define SCTP_DATA_FLAG_U 0x04  /* Unordered bit */
#define SCTP_DATA_FLAG_I 0x08  /* Immediate SACK bit */

struct sctphdr {
    uint16_t sctp_sport;
    uint16_t sctp_dport;
    uint32_t sctp_verification_tag;
    uint32_t sctp_checksum;
} __attribute__((__packed__));

/******************************************************************************/
extern ArkimeConfig_t        config;

LOCAL  int                   sctpMProtocol;
LOCAL int                    sctp_raw_packet_func;
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL ArkimePacketRC sctp_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *UNUSED(data), int UNUSED(len))
{
    uint8_t               sessionId[ARKIME_SESSIONID_LEN];
    const struct sctphdr *sctphdr = (struct sctphdr *)(packet->pkt + packet->payloadOffset);

    if (packet->payloadLen < (int)sizeof(struct sctphdr))
        return ARKIME_PACKET_CORRUPT;

    if (packet->v6) {
        const struct ip6_hdr *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);
        arkime_session_id6(sessionId, ip6->ip6_src.s6_addr, sctphdr->sctp_sport,
                           ip6->ip6_dst.s6_addr, sctphdr->sctp_dport,
                           packet->vlan, packet->vni);
    } else {
        const struct ip *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
        arkime_session_id(sessionId, ip4->ip_src.s_addr, sctphdr->sctp_sport,
                          ip4->ip_dst.s_addr, sctphdr->sctp_dport, packet->vlan, packet->vni);
    }
    packet->mProtocol = sctpMProtocol;
    packet->hash = arkime_session_hash(sessionId);
    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL void sctp_create_sessionid(uint8_t *sessionId, ArkimePacket_t *packet)
{
    const struct sctphdr    *sctphdr = (struct sctphdr *)(packet->pkt + packet->payloadOffset);

    if (packet->v6) {
        const struct ip6_hdr *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);
        arkime_session_id6(sessionId, ip6->ip6_src.s6_addr, sctphdr->sctp_sport,
                           ip6->ip6_dst.s6_addr, sctphdr->sctp_dport,
                           packet->vlan, packet->vni);
    } else {
        const struct ip *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
        arkime_session_id(sessionId, ip4->ip_src.s_addr, sctphdr->sctp_sport,
                          ip4->ip_dst.s_addr, sctphdr->sctp_dport, packet->vlan, packet->vni);
    }
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL int sctp_pre_process(ArkimeSession_t *session, ArkimePacket_t *const packet, int isNewSession)
{
    const struct ip        *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
    const struct ip6_hdr   *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);
    const struct sctphdr    *sctphdr = (struct sctphdr *)(packet->pkt + packet->payloadOffset);
    if (isNewSession) {
        session->port1 = ntohs(sctphdr->sctp_sport);
        session->port2 = ntohs(sctphdr->sctp_dport);
        arkime_session_add_protocol(session, "sctp");
    }

    int dir;
    if (ip4->ip_v == 4) {
        dir = (ARKIME_V6_TO_V4(session->addr1) == ip4->ip_src.s_addr &&
               ARKIME_V6_TO_V4(session->addr2) == ip4->ip_dst.s_addr);
    } else {
        dir = (memcmp(session->addr1.s6_addr, ip6->ip6_src.s6_addr, 16) == 0 &&
               memcmp(session->addr2.s6_addr, ip6->ip6_dst.s6_addr, 16) == 0);
    }

    packet->direction = (dir &&
                         session->port1 == ntohs(sctphdr->sctp_sport) &&
                         session->port2 == ntohs(sctphdr->sctp_dport)) ? 0 : 1;
    session->databytes[packet->direction] += (packet->pktlen - 8);

    return 0;
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL int sctp_packet_process(ArkimeSession_t *const session, ArkimePacket_t *const packet)
{
    const struct sctphdr    *sctphdr = (struct sctphdr *)(packet->pkt + packet->payloadOffset);

    BSB bsb;
    BSB_INIT(bsb, packet->pkt + packet->payloadOffset + sizeof(struct sctphdr), packet->payloadLen - sizeof(struct sctphdr));

    while (BSB_REMAINING(bsb) >= 4) {
        int chunkType = 0, chunkFlags = 0, chunkLen = 0;
        BSB_IMPORT_u08(bsb, chunkType);
        BSB_IMPORT_u08(bsb, chunkFlags);
        BSB_IMPORT_u16(bsb, chunkLen);

        BSB cbsb;
        BSB_IMPORT_bsb(bsb, cbsb, chunkLen - 4);

        switch (chunkType) {
        case 0: { // DATA 
            // DATA chunk
            uint32_t tsn = 0;
            uint16_t streamId = 0, streamSeq = 0;
            uint32_t payloadProtoId = 0;

            BSB_IMPORT_u32(cbsb, tsn);
            BSB_IMPORT_u16(cbsb, streamId);
            BSB_IMPORT_u16(cbsb, streamSeq);
            BSB_IMPORT_u32(cbsb, payloadProtoId);

            LOG("ALW   DATA: tsn: %u (%u) streamId: %u streamSeq: %u payloadProtoId: %u", tsn, session->sctpData.tsn[packet->direction], streamId, streamSeq, payloadProtoId);

            // Only reset the initial TSN if we haven't set it before in each direction
            if ((session->synSet & (1 << packet->direction)) == 0) {
                session->sctpData.tsn[packet->direction] = tsn;
                session->synSet |= (1 << packet->direction);
            }

            if (tsn == session->sctpData.tsn[packet->direction]) {
                LOG("ALW     In order packet");
                session->sctpData.tsn[packet->direction]++;

                if (session->firstBytesLen[packet->direction] == 0) {
                    session->firstBytesLen[packet->direction] = MIN(8, BSB_REMAINING(cbsb));
                    memcpy(session->firstBytes[packet->direction], BSB_WORK_PTR(cbsb), session->firstBytesLen[packet->direction]);
                    arkime_parsers_classify_sctp(session, BSB_WORK_PTR(cbsb), BSB_REMAINING(cbsb), ARKIME_WHICH_SET_ID(packet->direction, streamId));
                }

                arkime_packet_process_data(session, BSB_WORK_PTR(cbsb), BSB_REMAINING(cbsb), ARKIME_WHICH_SET_ID(packet->direction, streamId));
            } else if (tsn < session->sctpData.tsn[packet->direction]) {
                LOG("ALW     Duplicate packet");
            } else {
                // Out of order or duplicate packet
                LOG("ALW     Out of order or duplicate packet, expected %u got %u", session->sctpData.tsn[packet->direction], tsn);
            }
            break;

        }
        case 1: { // INIT
            uint32_t initTag = 0;
            uint32_t a_rwnd = 0;
            uint16_t numOutStreams = 0, numInStreams = 0;
            uint32_t tsn = 0;
            arkime_print_hex_string(BSB_WORK_PTR(cbsb), 4);
            BSB_IMPORT_u32(cbsb, initTag);
            BSB_IMPORT_u32(cbsb, a_rwnd);
            BSB_IMPORT_u16(cbsb, numOutStreams);
            BSB_IMPORT_u16(cbsb, numInStreams);
            BSB_IMPORT_u32(cbsb, tsn);

            LOG("ALW   INIT: initTag: %x a_rwnd: %u numOutStreams: %u numInStreams: %u tsn: %u",
                initTag, a_rwnd, numOutStreams, numInStreams, tsn);

            session->sctpData.tsn[packet->direction] = tsn;
            session->synSet |= (1 << packet->direction);
            break;
        }
        case 2: { // INIT ACK
            uint32_t initTag = 0;
            uint32_t a_rwnd = 0;
            uint16_t numOutStreams = 0, numInStreams = 0;
            uint32_t tsn = 0;
            BSB_IMPORT_u32(cbsb, initTag);
            BSB_IMPORT_u32(cbsb, a_rwnd);
            BSB_IMPORT_u16(cbsb, numOutStreams);
            BSB_IMPORT_u16(cbsb, numInStreams);
            BSB_IMPORT_u32(cbsb, tsn);

            LOG("ALW   INIT ACK: initTag: %u a_rwnd: %u numOutStreams: %u numInStreams: %u tsn: %u",
                initTag, a_rwnd, numOutStreams, numInStreams, tsn);

            session->sctpData.tsn[packet->direction] = tsn;
            session->synSet |= (1 << packet->direction);
            break;
        }
        case 3: { // SACK
            uint32_t cumTSNAck = 0;
            uint32_t a_rwnd = 0;
            uint16_t numGapBlocks = 0, numDupTSNs = 0;
            BSB_IMPORT_u32(cbsb, cumTSNAck);
            BSB_IMPORT_u32(cbsb, a_rwnd);
            BSB_IMPORT_u16(cbsb, numGapBlocks);
            BSB_IMPORT_u16(cbsb, numDupTSNs);

            LOG("ALW   SACK: cumTSNAck: %u a_rwnd: %u numGapBlocks: %u numDupTSNs: %u",
                cumTSNAck, a_rwnd, numGapBlocks, numDupTSNs);
            break;
        }
        case 7: { // SHUTDOWN
            uint32_t cumTSNAck = 0;
            BSB_IMPORT_u32(cbsb, cumTSNAck);
            LOG("ALW   SHUTDOWN: cumTSNAck: %u", cumTSNAck);
            break;
        }
        case 8: { // SHUTDOWN ACK
            uint32_t cumTSNAck = 0;
            BSB_IMPORT_u32(cbsb, cumTSNAck);
            LOG("ALW   SHUTDOWN ACK: cumTSNAck: %u", cumTSNAck);
            break;
        }
        case 9: { // SHUTDOWN COMPLETE
            LOG("ALW   SHUTDOWN COMPLETE");
            break;
        }
        case 10: { // COOKIE ECHO
            uint32_t cookieLen = 0;
            uint8_t *cookie = NULL;
            BSB_IMPORT_u32(cbsb, cookieLen);
            BSB_IMPORT_ptr(cbsb, cookie, cookieLen);
            LOG("ALW   COOKIE ECHO: cookieLen: %u", cookieLen);
            break;
        }
        case 11: { // COOKIE ACK
            uint32_t reserved = 0;
            BSB_IMPORT_u32(cbsb, reserved);
            LOG("ALW   COOKIE ACK: reserved: %u", reserved);
            break;
        }
        } /* switch */
    }

    return 0;
}
/******************************************************************************/
LOCAL int sctp_process(ArkimeSession_t *session, ArkimePacket_t *const packet)
{
    int freePacket = sctp_packet_process(session, packet);
    if (ARKIME_PARSERS_HAS_NAMED_FUNC(sctp_raw_packet_func)) {
        arkime_parsers_call_named_func(sctp_raw_packet_func, session, NULL, 0, packet);
    }
    return freePacket;
}
/******************************************************************************/
void arkime_parser_init()
{
    sctp_raw_packet_func = arkime_parsers_get_named_func("sctp_raw_packet");

    arkime_packet_set_ip_cb(IPPROTO_SCTP, sctp_packet_enqueue);
    sctpMProtocol = arkime_mprotocol_register("sctp",
                                              SESSION_SCTP,
                                              sctp_create_sessionid,
                                              sctp_pre_process,
                                              sctp_process,
                                              NULL);
}
