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

#define SCTP_DATA_FLAG_E  0x01  /* End bit */
#define SCTP_DATA_FLAG_B  0x02  /* Beginning bit */
#define SCTP_DATA_FLAG_BE 0x03
#define SCTP_DATA_FLAG_U  0x04  /* Unordered bit */
#define SCTP_DATA_FLAG_I  0x08  /* Immediate SACK bit */

#define SCTP_DATA_FLAG_SEND_NOW(_f) (((_f) & (SCTP_DATA_FLAG_U)) || (((_f) & (SCTP_DATA_FLAG_BE)) == SCTP_DATA_FLAG_BE))

struct sctphdr {
    uint16_t sctp_sport;
    uint16_t sctp_dport;
    uint32_t sctp_verification_tag;
    uint32_t sctp_checksum;
} __attribute__((__packed__));

/******************************************************************************/
extern ArkimeConfig_t        config;

LOCAL int                    sctpMProtocol;
LOCAL int                    sctp_raw_packet_func;
LOCAL int                    protoIdField;
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

        DLL_INIT(sd_, &session->sctpData);
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
// Idea from gopacket tcpassembly/assemply.go
LOCAL int64_t sctp_tsn_diff (int64_t a, int64_t b)
{
    if (a > 0xc0000000 && b < 0x40000000)
        return a + 0x100000000LL - b;

    if (b > 0xc0000000 && a < 0x40000000)
        return a - b - 0x100000000LL;

    return b - a;
}
/******************************************************************************/
/* Add to the list of data chunk we have sorted by tsn */
LOCAL void sctp_add_data(ArkimeSCTP_t *sctp, uint32_t tsn, int which, BSB *const cbsb, int chunkFlags, int protoId)
{
    int addBefore = 0;
    ArkimeSctpData_t *fsd = 0 ;
    DLL_FOREACH(sd_, sctp, fsd) {
        if (tsn == fsd->tsn && which == fsd->which) { // Already have this tsn
            return;
        }
        if (tsn < fsd->tsn) {
            addBefore = 1;
            break;
        }
    }

    ArkimeSctpData_t *sd = ARKIME_TYPE_ALLOC0(ArkimeSctpData_t);
    sd->tsn = tsn;
    sd->which = which;
    sd->data = g_memdup(BSB_WORK_PTR(*cbsb), BSB_REMAINING(*cbsb));
    sd->len = BSB_REMAINING(*cbsb);
    sd->flags = chunkFlags;
    sd->protoId = protoId;

    if (addBefore) {
        DLL_ADD_BEFORE(sd_, sctp, fsd, sd);
    } else {
        DLL_PUSH_TAIL(sd_, sctp, sd);
    }

#ifdef DEBUG
    DLL_FOREACH(sd_, sctp, fsd) {
        printf(" tsn %u flags %x which: %d len %u\n", fsd->tsn, fsd->flags, fsd->which, fsd->len);
    }
#endif
}
/******************************************************************************/
/* Send the data to the right parser */
LOCAL void sctp_send_data(ArkimeSession_t *const session, const uint8_t *data, int len, int protoId, int which)
{
    int dir = ARKIME_WHICH_GET_DIR(which);
    if (session->firstBytesLen[dir] == 0) {
        session->firstBytesLen[dir] = MIN(8, len);
        memcpy(session->firstBytes[dir], data, session->firstBytesLen[dir]);
        arkime_parsers_classify_sctp(session, protoId, data, len, which);
        arkime_field_int_add(protoIdField, session, protoId);
    }

    arkime_packet_process_data(session, data, len, which);
}

/******************************************************************************/
/* See if we can reassmble any data */
LOCAL void sctp_maybe_send(ArkimeSession_t *const session, int which)
{
    int               dir = ARKIME_WHICH_GET_DIR(which);
    int               state = 0;
    ArkimeSctpData_t *sd = 0;
    ArkimeSctpData_t *fsd;
    int               totalsize = 0;
    uint32_t          tsn;

    /* First pass check if we have a full set and calculate size */
    DLL_FOREACH(sd_, &session->sctpData, sd) {
        // Only look at our direction
        if (ARKIME_WHICH_GET_DIR(sd->which) != dir) {
            continue;
        }

        switch (state) {
        case 0: // Looking for start
            if (sd->flags & SCTP_DATA_FLAG_B) {
                tsn = sd->tsn + 1;
                state = 1;
                totalsize = sd->len;
                fsd = sd;
            } else {
                state = 3;
            }
            break;
        case 1: // Have start looking for end
            if (sd->tsn != tsn) {
                state = 3;
                break;
            }
            tsn++;
            totalsize += sd->len;
            if (sd->flags & SCTP_DATA_FLAG_E) {
                state = 2;
            }
            break;
        } /* switch */
        if (state >= 2) {
            break;
        }
    } /* DLL_FOREACH */

    // Found a full set
    if (state != 2)
        return;

    uint8_t *data = g_malloc(totalsize);
    int     off = 0;
    int     protoId = fsd->protoId;
    sd = fsd;

    DLL_FOREACH_REMOVABLE_START(sd_, &session->sctpData, sd, fsd) {
        // Only look at our direction
        if (ARKIME_WHICH_GET_DIR(sd->which) != dir) {
            continue;
        }
        memcpy(data + off, sd->data, sd->len);
        off += sd->len;
        DLL_REMOVE(sd_, &session->sctpData, sd);
        g_free(sd->data);
        if (sd->flags & SCTP_DATA_FLAG_E) {
            ARKIME_TYPE_FREE(ArkimeSctpData_t, sd);
            break;
        }
        ARKIME_TYPE_FREE(ArkimeSctpData_t, sd);
    }
    sctp_send_data(session, data, totalsize, protoId, which);
    g_free(data);
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL int sctp_packet_process(ArkimeSession_t *const session, ArkimePacket_t *const packet)
{
    if (packet->payloadLen < (int)sizeof(struct sctphdr))
        return 1;

    BSB bsb;
    BSB_INIT(bsb, packet->pkt + packet->payloadOffset + sizeof(struct sctphdr), packet->payloadLen - sizeof(struct sctphdr));

    //LOG("SCTP: len %d dir %d", packet->payloadLen, packet->direction);
    while (BSB_REMAINING(bsb) >= 4) {
        int chunkType = 0, chunkFlags = 0, chunkLen = 0;
        BSB_IMPORT_u08(bsb, chunkType);
        BSB_IMPORT_u08(bsb, chunkFlags);
        BSB_IMPORT_u16(bsb, chunkLen);

        BSB cbsb;
        BSB_IMPORT_bsb(bsb, cbsb, chunkLen - 4);

        if (BSB_IS_ERROR(bsb) || BSB_IS_ERROR(cbsb)) {
            return 1;
        }

        switch (chunkType) {
        case 0: { // DATA
            // DATA chunk
            uint32_t tsn = 0;
            uint16_t streamId = 0;
            uint32_t payloadProtoId = 0;

            BSB_IMPORT_u32(cbsb, tsn);
            BSB_IMPORT_u16(cbsb, streamId);
            BSB_IMPORT_skip(cbsb, 2); // streamSeq
            BSB_IMPORT_u32(cbsb, payloadProtoId);

            int which = ARKIME_WHICH_SET_ID(packet->direction, streamId);

            // Only reset the initial TSN if we haven't set it before in each direction
            if ((session->synSet & (1 << packet->direction)) == 0) {
                session->sctpData.tsn[packet->direction] = tsn;
                session->synSet |= (1 << packet->direction);
            }

            if (tsn == session->sctpData.tsn[packet->direction]) {
                session->sctpData.tsn[packet->direction]++;

                if (SCTP_DATA_FLAG_SEND_NOW(chunkFlags)) {
                    sctp_send_data(session, BSB_WORK_PTR(cbsb), BSB_REMAINING(cbsb), payloadProtoId, which);
                } else {
                    sctp_add_data(&session->sctpData, tsn, which, &cbsb, chunkFlags, payloadProtoId);
                }

                sctp_maybe_send(session, which);
            } else if (sctp_tsn_diff(tsn, session->sctpData.tsn[packet->direction]) < 0)  {
                // ALW duplicate tsn drop
            } else {
                sctp_add_data(&session->sctpData, tsn, which, &cbsb, chunkFlags, payloadProtoId);
                sctp_maybe_send(session, which);
            }
            break;

        }
        case 1: { // INIT
            uint32_t initTag = 0;
            uint32_t tsn = 0;
            BSB_IMPORT_u32(cbsb, initTag);
            BSB_IMPORT_skip(cbsb, 8); // a_rwnd, numOutStreams, numInStreams
            BSB_IMPORT_u32(cbsb, tsn);

            session->sctpData.tsn[packet->direction] = tsn;
            session->synSet |= (1 << packet->direction);
            session->sctpData.initTag[packet->direction] = initTag;
            break;
        }
        case 2: { // INIT ACK
            uint32_t initTag = 0;
            uint32_t tsn = 0;
            BSB_IMPORT_u32(cbsb, initTag);
            BSB_IMPORT_skip(cbsb, 8); // a_rwnd, numOutStreams, numInStreams
            BSB_IMPORT_u32(cbsb, tsn);

            /*
            if (session->sctpData.initTag != 0 && session->sctpData.initTag != initTag) {
                LOG("INIT ACK: initTag mismatch: %u != %u", session->sctpData.initTag, initTag);
                break;
            } */

            session->sctpData.initTag[packet->direction] = initTag;
            session->sctpData.tsn[packet->direction] = tsn;
            session->synSet |= (1 << packet->direction);
            break;
        }
        case 3: { // SACK
            // TODO - track it
            break;
        }
        case 7: { // SHUTDOWN
            break;
        }
        case 8: { // SHUTDOWN ACK
            break;
        }
        case 9: { // SHUTDOWN COMPLETE
            arkime_session_mark_for_close(session, SESSION_SCTP);
            break;
        }
        case 10: { // COOKIE ECHO
            break;
        }
        case 11: { // COOKIE ACK
            break;
        }
        } /* switch */
    }

    return 1;
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
LOCAL void sctp_session_free(ArkimeSession_t *session)
{
    ArkimeSctpData_t *sd = 0;
    while (DLL_POP_HEAD(sd_, &session->sctpData, sd)) {
        g_free(sd->data);
        ARKIME_TYPE_FREE(ArkimeSctpData_t, sd);
    }
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
                                              sctp_session_free);

    protoIdField = arkime_field_define("sctp", "integer",
                                       "sctp.protoId", "Proto Id", "sctp.protoId",
                                       "SCTP protocol id",
                                       ARKIME_FIELD_TYPE_INT_GHASH,       0,
                                       (char *)NULL);
}
