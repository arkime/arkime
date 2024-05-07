/* tcp.c
 *
 * Since this used to be part of packet.c still a little more coupled then we would like
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

extern int                   tcpMProtocol;

extern ArkimeSessionHead_t   tcpWriteQ[ARKIME_MAX_PACKET_THREADS];
LOCAL int                    maxTcpOutOfOrderPackets;
extern uint32_t              pluginsCbs;
LOCAL int                    tcp_raw_packet_func;

void arkime_packet_free(ArkimePacket_t *packet);

LOCAL int tcpflagsSynField;
LOCAL int tcpflagsSynAckField;
LOCAL int tcpflagsAckField;
LOCAL int tcpflagsPshField;
LOCAL int tcpflagsRstField;
LOCAL int tcpflagsFinField;
LOCAL int tcpflagsUrgField;

/******************************************************************************/
void tcp_session_free(ArkimeSession_t *session)
{
    if (session->tcpData.td_count == 1 && session->tcpFlagCnt[ARKIME_TCPFLAG_PSH] == 1) {
        ArkimeTcpData_t *ftd = DLL_PEEK_HEAD(td_, &session->tcpData);
        const int which = ftd->packet->direction;
        const uint8_t *data = ftd->packet->pkt + ftd->dataOffset;
        const int len = ftd->len;

        arkime_parsers_classify_tcp(session, data, len, which);
        arkime_packet_process_data(session, data, len, which);
    }

    ArkimeTcpData_t *td;
    while (DLL_POP_HEAD(td_, &session->tcpData, td)) {
        arkime_packet_free(td->packet);
        ARKIME_TYPE_FREE(ArkimeTcpData_t, td);
    }
}

/******************************************************************************/
// Idea from gopacket tcpassembly/assemply.go
LOCAL int64_t tcp_sequence_diff (int64_t a, int64_t b)
{
    if (a > 0xc0000000 && b < 0x40000000)
        return a + 0x100000000LL - b;

    if (b > 0xc0000000 && a < 0x40000000)
        return a - b - 0x100000000LL;

    return b - a;
}
/******************************************************************************/
void tcp_packet_finish(ArkimeSession_t *session)
{
    ArkimeTcpData_t            *ftd;
    ArkimeTcpData_t            *next;

    ArkimeTcpDataHead_t *const tcpData = &session->tcpData;

#ifdef DEBUG_TCP
    LOG("START %u %u", session->tcpSeq[0], session->tcpSeq[1]);
    DLL_FOREACH(td_, tcpData, ftd) {
        LOG("dir: %u seq: %8u ack: %8u len: %4u", ftd->packet->direction, ftd->seq, ftd->ack, ftd->len);
    }
#endif

    DLL_FOREACH_REMOVABLE(td_, tcpData, ftd, next) {
        const int which = ftd->packet->direction;
        const uint32_t tcpSeq = session->tcpSeq[which];

        /* The sequence number we are looking for is past the start of the packet */
        if (tcpSeq >= ftd->seq) {

            /* The sequence number we are looking for is past the end of the packet, free it */
            if (tcpSeq >= ftd->seq + ftd->len) {
                DLL_REMOVE(td_, tcpData, ftd);
                arkime_packet_free(ftd->packet);
                ARKIME_TYPE_FREE(ArkimeTcpData_t, ftd);
                continue;
            }

            /* This packet has the sequence number we are looking for */
            const int offset = tcpSeq - ftd->seq;
            const uint8_t *data = ftd->packet->pkt + ftd->dataOffset + offset;
            const int len = ftd->len - offset;

            if (session->firstBytesLen[which] < 8) {
                int copy = MIN(8 - session->firstBytesLen[which], len);
                memcpy(session->firstBytes[which] + session->firstBytesLen[which], data, copy);
                session->firstBytesLen[which] += copy;
            }

            if (session->totalDatabytes[which] == session->consumed[which]) {
                arkime_parsers_classify_tcp(session, data, len, which);
            }

            arkime_packet_process_data(session, data, len, which);
            session->tcpSeq[which] += len;
            session->databytes[which] += len;
            session->totalDatabytes[which] += len;

            if (config.yara && config.yaraEveryPacket && !session->stopYara) {
                arkime_yara_execute(session, data, len, 0);
            }

            if (pluginsCbs & ARKIME_PLUGIN_TCP)
                arkime_plugins_cb_tcp(session, data, len, which);

            DLL_REMOVE(td_, tcpData, ftd);
            arkime_packet_free(ftd->packet);
            ARKIME_TYPE_FREE(ArkimeTcpData_t, ftd);
        } else {
            return;
        }
    }
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
int tcp_packet_process(ArkimeSession_t *const session, ArkimePacket_t *const packet)
{
    struct tcphdr       *tcphdr = (struct tcphdr *)(packet->pkt + packet->payloadOffset);


    int            len = packet->payloadLen - 4 * tcphdr->th_off;

    const uint32_t seq = ntohl(tcphdr->th_seq);

#ifdef DEBUG_TCP
    LOG("poffset: %d plen: %d len: %d seq: %u ack: %u", packet->payloadOffset, packet->payloadLen, len, seq, ntohl(tcphdr->th_ack));
#endif

    if (tcphdr->th_win == 0 && (tcphdr->th_flags & TH_RST) == 0) {
        session->tcpFlagCnt[ARKIME_TCPFLAG_SRC_ZERO + packet->direction]++;
    }

    if (len < 0)
        return 1;

    if (tcphdr->th_flags & TH_URG) {
        session->tcpFlagCnt[ARKIME_TCPFLAG_URG]++;
        ARKIME_RULES_RUN_FIELD_SET(session, tcpflagsUrgField, (gpointer)(long)session->tcpFlagCnt[ARKIME_TCPFLAG_URG]);
    }

    // add to the long open
    if (!session->tcp_next) {
        DLL_PUSH_TAIL(tcp_, &tcpWriteQ[session->thread], session);
    }

    if (tcphdr->th_flags & TH_SYN) {
        if (tcphdr->th_flags & TH_ACK) {
            session->tcpFlagCnt[ARKIME_TCPFLAG_SYN_ACK]++;
            ARKIME_RULES_RUN_FIELD_SET(session, tcpflagsSynAckField, (gpointer)(long)session->tcpFlagCnt[ARKIME_TCPFLAG_SYN_ACK]);

            if (!session->haveTcpSession) {
#ifdef DEBUG_TCP
                LOG("syn-ack first");
#endif
                session->tcpSeq[(packet->direction + 1) % 2] = ntohl(tcphdr->th_ack);
            }
        } else {
            session->tcpFlagCnt[ARKIME_TCPFLAG_SYN]++;
            ARKIME_RULES_RUN_FIELD_SET(session, tcpflagsSynField, (gpointer)(long)session->tcpFlagCnt[ARKIME_TCPFLAG_SYN]);
            if (session->synTime == 0) {
                session->synTime = (packet->ts.tv_sec - session->firstPacket.tv_sec) * 1000000 +
                                   (packet->ts.tv_usec - session->firstPacket.tv_usec) + 1;
                session->ackTime = 0;
            }
        }

        session->haveTcpSession = 1;

        // Only reset the initial SYN if we haven't set it before in each direction
        if ((session->synSet & (1 << packet->direction)) == 0) {
            session->tcpSeq[packet->direction] = seq + 1;
            session->synSet |= (1 << packet->direction);
        }
        return 1;
    }

    if (tcphdr->th_flags & TH_RST) {
        session->tcpFlagCnt[ARKIME_TCPFLAG_RST]++;
        ARKIME_RULES_RUN_FIELD_SET(session, tcpflagsRstField, (gpointer)(long)session->tcpFlagCnt[ARKIME_TCPFLAG_RST]);
        int64_t diff = tcp_sequence_diff(seq, session->tcpSeq[packet->direction]);
        if (diff  <= 0) {
            if (diff == 0 && !session->closingQ) {
                arkime_session_mark_for_close(session, SESSION_TCP);
            }
            return 1;
        }

        session->tcpState[packet->direction] = ARKIME_TCP_STATE_FIN_ACK;
    }

    if (tcphdr->th_flags & TH_FIN) {
        session->tcpFlagCnt[ARKIME_TCPFLAG_FIN]++;
        ARKIME_RULES_RUN_FIELD_SET(session, tcpflagsFinField, (gpointer)(long)session->tcpFlagCnt[ARKIME_TCPFLAG_FIN]);
        session->tcpState[packet->direction] = ARKIME_TCP_STATE_FIN;
    }

    if ((tcphdr->th_flags & (TH_FIN | TH_RST | TH_PUSH | TH_SYN | TH_ACK)) == TH_ACK) {
        session->tcpFlagCnt[ARKIME_TCPFLAG_ACK]++;
        if (session->tcpFlagAckCnt[packet->direction] < 0xff) {
            session->tcpFlagAckCnt[packet->direction]++;
        }
        ARKIME_RULES_RUN_FIELD_SET(session, tcpflagsAckField, (gpointer)(long)session->tcpFlagCnt[ARKIME_TCPFLAG_ACK]);
        if (session->ackTime == 0) {
            session->ackTime = (packet->ts.tv_sec - session->firstPacket.tv_sec) * 1000000 +
                               (packet->ts.tv_usec - session->firstPacket.tv_usec) + 1;
        }
    }

    if (tcphdr->th_flags & TH_PUSH) {
        session->tcpFlagCnt[ARKIME_TCPFLAG_PSH]++;
        ARKIME_RULES_RUN_FIELD_SET(session, tcpflagsPshField, (gpointer)(long)session->tcpFlagCnt[ARKIME_TCPFLAG_PSH]);
    }

    if (session->stopTCP)
        return 1;

    // If we've seen SYN but no SYN_ACK and no tcpSeq set, then just assume we've missed the syn-ack
    if (session->haveTcpSession && session->tcpFlagCnt[ARKIME_TCPFLAG_SYN_ACK] == 0 && session->tcpSeq[packet->direction] == 0) {
        arkime_session_add_tag(session, "no-syn-ack");
        session->tcpSeq[packet->direction] = seq;
    }

    ArkimeTcpDataHead_t *const tcpData = &session->tcpData;

    if (DLL_COUNT(td_, tcpData) > maxTcpOutOfOrderPackets) {
        tcp_session_free(session);
        arkime_session_add_tag(session, "incomplete-tcp");
        session->stopTCP = 1;
        return 1;
    }

    if (tcphdr->th_flags & (TH_ACK | TH_RST)) {
        int owhich = (packet->direction + 1) & 1;
        if (session->tcpState[owhich] == ARKIME_TCP_STATE_FIN) {
            session->tcpState[owhich] = ARKIME_TCP_STATE_FIN_ACK;
            if (session->tcpState[packet->direction] == ARKIME_TCP_STATE_FIN_ACK) {

                if (!session->closingQ) {
                    arkime_session_mark_for_close(session, SESSION_TCP);
                }
                return 1;
            }
        }
    }

    if (tcphdr->th_flags & TH_ACK) {
        if (session->haveTcpSession &&  // Seen a SYN
            (session->ackedUnseenSegment & (1 << packet->direction)) == 0 &&  // Haven't already tagged
            session->tcpSeq[(packet->direction + 1) % 2] != 0 &&                  // The syn-ack isn't what is missing
            (tcp_sequence_diff(session->tcpSeq[(packet->direction + 1) % 2], ntohl(tcphdr->th_ack)) > 1)) { // more then one byte missing

            static const char *tags[2] = {"acked-unseen-segment-src", "acked-unseen-segment-dst"};
            arkime_session_add_tag(session, tags[packet->direction]);
            session->ackedUnseenSegment |= (1 << packet->direction);
        }
    }

    // Empty packet, drop from tcp processing
    if (len <= 0 || tcphdr->th_flags & TH_RST)
        return 1;

    // This packet is before what we are processing
    int64_t diff = tcp_sequence_diff(session->tcpSeq[packet->direction], seq + len);
    if (session->haveTcpSession && diff <= 0)
        return 1;

    ArkimeTcpData_t *ftd, *td = ARKIME_TYPE_ALLOC(ArkimeTcpData_t);
    const uint32_t ack = ntohl(tcphdr->th_ack);

    td->packet = packet;
    td->ack = ack;
    td->seq = seq;
    td->len = len;
    td->dataOffset = packet->payloadOffset + 4 * tcphdr->th_off;

#ifdef DEBUG_TCP
    LOG("dir: %u seq: %u ack: %u len: %d diff0: %" PRIu64, packet->direction, seq, ack, len, diff);
#endif

    if (DLL_COUNT(td_, tcpData) == 0) {
        DLL_PUSH_TAIL(td_, tcpData, td);
    } else {
        uint32_t sortA, sortB;
        DLL_FOREACH_REVERSE(td_, tcpData, ftd) {
            if (packet->direction == ftd->packet->direction) {
                sortA = seq;
                sortB = ftd->seq;
            } else {
                sortA = seq;
                sortB = ftd->ack;
            }

            diff = tcp_sequence_diff(sortB, sortA);
            if (diff == 0) {
                if (packet->direction == ftd->packet->direction) {
                    if (td->len > ftd->len) {
                        DLL_ADD_AFTER(td_, tcpData, ftd, td);

                        DLL_REMOVE(td_, tcpData, ftd);
                        arkime_packet_free(ftd->packet);
                        ARKIME_TYPE_FREE(ArkimeTcpData_t, ftd);
                        ftd = td;
                    } else {
                        ARKIME_TYPE_FREE(ArkimeTcpData_t, td);
                        return 1;
                    }
                    break;
                } else if (tcp_sequence_diff(ack, ftd->seq) < 0) {
                    DLL_ADD_AFTER(td_, tcpData, ftd, td);
                    break;
                }
            } else if (diff > 0) {
                DLL_ADD_AFTER(td_, tcpData, ftd, td);
                break;
            }
        }

        if ((void * )ftd == (void * )tcpData) {
            DLL_PUSH_HEAD(td_, tcpData, td);
        }

        if (session->haveTcpSession && (session->outOfOrder & (1 << packet->direction)) == 0) {
            static const char *tags[2] = {"out-of-order-src", "out-of-order-dst"};
            arkime_session_add_tag(session, tags[packet->direction]);
            session->outOfOrder |= (1 << packet->direction);
        }
    }

    return 0;
}

/******************************************************************************/
SUPPRESS_ALIGNMENT
void tcp_create_sessionid(uint8_t *sessionId, ArkimePacket_t *packet)
{
    const struct ip           *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
    const struct ip6_hdr      *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);
    const struct tcphdr       *tcphdr = (struct tcphdr *)(packet->pkt + packet->payloadOffset);

    if (packet->v6) {
        arkime_session_id6(sessionId, ip6->ip6_src.s6_addr, tcphdr->th_sport,
                           ip6->ip6_dst.s6_addr, tcphdr->th_dport,
                           packet->vlan, packet->vni);
    } else {
        arkime_session_id(sessionId, ip4->ip_src.s_addr, tcphdr->th_sport,
                          ip4->ip_dst.s_addr, tcphdr->th_dport, packet->vlan, packet->vni);
    }
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
int tcp_pre_process(ArkimeSession_t *session, ArkimePacket_t *const packet, int isNewSession)
{
    const struct ip           *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
    const struct ip6_hdr      *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);
    const struct tcphdr       *tcphdr = (struct tcphdr *)(packet->pkt + packet->payloadOffset);

    // If this is an old session that hash RSTs and we get a syn, probably a port reuse, close old session
    if (!isNewSession && (tcphdr->th_flags & TH_SYN) && ((tcphdr->th_flags & TH_ACK) == 0) &&
        (session->tcpFlagCnt[ARKIME_TCPFLAG_RST] || session->tcpFlagCnt[ARKIME_TCPFLAG_FIN])) {
        return 1;
    }

    if (isNewSession) {
        /* if the syn-ack was captured first then the syn probably got dropped.*/
        if ((tcphdr->th_flags & TH_SYN) && (tcphdr->th_flags & TH_ACK)) {
            struct in6_addr tmp;
            tmp = session->addr1;
            session->addr1 = session->addr2;
            session->addr2 = tmp;
            session->port1 = ntohs(tcphdr->th_dport);
            session->port2 = ntohs(tcphdr->th_sport);
        } else {
            session->port1 = ntohs(tcphdr->th_sport);
            session->port2 = ntohs(tcphdr->th_dport);
        }
        if (arkime_http_is_arkime(session->h_hash, session->sessionId)) {
            if (config.debug) {
                char buf[1000];
                LOG("Ignoring connection %s", arkime_session_id_string(session->sessionId, buf));
            }
            session->stopSPI = 1;
        }
        arkime_session_add_protocol(session, "tcp");
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
                         session->port1 == ntohs(tcphdr->th_sport) &&
                         session->port2 == ntohs(tcphdr->th_dport)) ? 0 : 1;
    session->tcp_flags |= tcphdr->th_flags;

    return 0;
}
/******************************************************************************/
int tcp_process(ArkimeSession_t *session, ArkimePacket_t *const packet)
{
    int freePacket = tcp_packet_process(session, packet);
    if (ARKIME_PARSERS_HAS_NAMED_FUNC(tcp_raw_packet_func)) {
        arkime_parsers_call_named_func(tcp_raw_packet_func, session, NULL, 0, packet);
    }
    tcp_packet_finish(session);
    return freePacket;
}
/******************************************************************************/
void arkime_parser_init()
{
    maxTcpOutOfOrderPackets = arkime_config_int(NULL, "maxTcpOutOfOrderPackets", 256, 64, 10000);
    tcp_raw_packet_func = arkime_parsers_get_named_func("tcp_raw_packet");

    tcpMProtocol = arkime_mprotocol_register("tcp",
                                             SESSION_TCP,
                                             tcp_create_sessionid,
                                             tcp_pre_process,
                                             tcp_process,
                                             tcp_session_free);

    tcpflagsSynField = arkime_field_by_exp("tcpflags.syn");
    tcpflagsSynAckField = arkime_field_by_exp("tcpflags.syn-ack");
    tcpflagsAckField = arkime_field_by_exp("tcpflags.ack");
    tcpflagsPshField = arkime_field_by_exp("tcpflags.psh");
    tcpflagsRstField = arkime_field_by_exp("tcpflags.rst");
    tcpflagsFinField = arkime_field_by_exp("tcpflags.fin");
    tcpflagsUrgField = arkime_field_by_exp("tcpflags.urg");
}
