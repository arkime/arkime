/* packet.c  -- Functions for acquiring data
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
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

//#define DEBUG_PACKET

/******************************************************************************/
extern MolochConfig_t        config;

MolochPcapFileHdr_t          pcapFileHeader;

uint64_t                     totalPackets;
LOCAL uint64_t               totalBytes[MOLOCH_MAX_PACKET_THREADS];

LOCAL uint32_t               initialDropped = 0;
struct timeval               initialPacket; // Don't make LOCAL for now because of netflow plugin

extern void                 *esServer;
extern uint32_t              pluginsCbs;

LOCAL int                    mac1Field;
LOCAL int                    mac2Field;
LOCAL int                    oui1Field;
LOCAL int                    oui2Field;
LOCAL int                    vlanField;
LOCAL int                    greIpField;
LOCAL int                    icmpTypeField;
LOCAL int                    icmpCodeField;

LOCAL uint64_t               droppedFrags;

time_t                       lastPacketSecs[MOLOCH_MAX_PACKET_THREADS];
LOCAL int                    inProgress[MOLOCH_MAX_PACKET_THREADS];

LOCAL patricia_tree_t       *ipTree4 = 0;
LOCAL patricia_tree_t       *ipTree6 = 0;

LOCAL int                    maxTcpOutOfOrderPackets;

extern MolochFieldOps_t      readerFieldOps[256];

/******************************************************************************/

#define MOLOCH_PACKET_SUCCESS          0
#define MOLOCH_PACKET_IP_DROPPED       1
#define MOLOCH_PACKET_OVERLOAD_DROPPED 2
#define MOLOCH_PACKET_CORRUPT          3
#define MOLOCH_PACKET_UNKNOWN          4
#define MOLOCH_PACKET_IPPORT_DROPPED   5
#define MOLOCH_PACKET_MAX              6

LOCAL uint64_t               packetStats[MOLOCH_PACKET_MAX];

/******************************************************************************/
extern MolochSessionHead_t   tcpWriteQ[MOLOCH_MAX_PACKET_THREADS];

LOCAL  MolochPacketHead_t    packetQ[MOLOCH_MAX_PACKET_THREADS];
LOCAL  uint32_t              overloadDrops[MOLOCH_MAX_PACKET_THREADS];

LOCAL  MOLOCH_LOCK_DEFINE(frags);

LOCAL int moloch_packet_ip4(MolochPacketBatch_t * batch, MolochPacket_t * const packet, const uint8_t *data, int len);
LOCAL int moloch_packet_ip6(MolochPacketBatch_t * batch, MolochPacket_t * const packet, const uint8_t *data, int len);
LOCAL int moloch_packet_frame_relay(MolochPacketBatch_t * batch, MolochPacket_t * const packet, const uint8_t *data, int len);
LOCAL int moloch_packet_ppp(MolochPacketBatch_t * batch, MolochPacket_t * const packet, const uint8_t *data, int len);

typedef struct molochfrags_t {
    struct molochfrags_t  *fragh_next, *fragh_prev;
    struct molochfrags_t  *fragl_next, *fragl_prev;
    uint32_t               fragh_bucket;
    uint32_t               fragh_hash;
    MolochPacketHead_t     packets;
    char                   key[10];
    uint32_t               secs;
    char                   haveNoFlags;
} MolochFrags_t;

typedef struct {
    struct molochfrags_t  *fragh_next, *fragh_prev;
    struct molochfrags_t  *fragl_next, *fragl_prev;
    short                  fragh_bucket;
    uint32_t               fragh_count;
    uint32_t               fragl_count;
} MolochFragsHead_t;

typedef HASH_VAR(h_, MolochFragsHash_t, MolochFragsHead_t, 199337);

MolochFragsHash_t          fragsHash;
MolochFragsHead_t          fragsList;

// These are in network byte order
MolochDropHashGroup_t      packetDrop4;
MolochDropHashGroup_t      packetDrop6;

#ifndef IPPROTO_IPV4
#define IPPROTO_IPV4            4
#endif

/******************************************************************************/
LOCAL void moloch_packet_free(MolochPacket_t *packet)
{
    if (packet->copied) {
        free(packet->pkt);
    }
    packet->pkt = 0;
    MOLOCH_TYPE_FREE(MolochPacket_t, packet);
}
/******************************************************************************/
void moloch_packet_tcp_free(MolochSession_t *session)
{
    MolochTcpData_t *td;
    while (DLL_POP_HEAD(td_, &session->tcpData, td)) {
        moloch_packet_free(td->packet);
        MOLOCH_TYPE_FREE(MolochTcpData_t, td);
    }
}
/******************************************************************************/
// Idea from gopacket tcpassembly/assemply.go
LOCAL int32_t moloch_packet_sequence_diff (uint32_t a, uint32_t b)
{
    if (a > 0xc0000000 && b < 0x40000000)
        return (a + 0xffffffffLL - b);

    if (b > 0xc0000000 && a < 0x40000000)
        return (a - b - 0xffffffffLL);

    return b - a;
}
/******************************************************************************/
void moloch_packet_process_data(MolochSession_t *session, const uint8_t *data, int len, int which)
{
    int i;
    int totConsumed = 0;
    int consumed = 0;

    for (i = 0; i < session->parserNum; i++) {
        if (session->parserInfo[i].parserFunc) {
            consumed = session->parserInfo[i].parserFunc(session, session->parserInfo[i].uw, data, len, which);
            if (consumed) {
                totConsumed += consumed;
                session->consumed[which] += consumed;
            }

            if (consumed >= len)
                break;
        }
    }
}
/******************************************************************************/
LOCAL void moloch_packet_tcp_finish(MolochSession_t *session)
{
    MolochTcpData_t            *ftd;
    MolochTcpData_t            *next;

    MolochTcpDataHead_t * const tcpData = &session->tcpData;

#ifdef DEBUG_PACKET
    LOG("START");
    DLL_FOREACH(td_, tcpData, ftd) {
        LOG("dir: %d seq: %8u ack: %8u len: %4u", ftd->packet->direction, ftd->seq, ftd->ack, ftd->len);
    }
#endif

    DLL_FOREACH_REMOVABLE(td_, tcpData, ftd, next) {
        const int which = ftd->packet->direction;
        const uint32_t tcpSeq = session->tcpSeq[which];

        if (tcpSeq >= ftd->seq && tcpSeq < (ftd->seq + ftd->len)) {
            const int offset = tcpSeq - ftd->seq;
            const uint8_t *data = ftd->packet->pkt + ftd->dataOffset + offset;
            const int len = ftd->len - offset;

            if (session->firstBytesLen[which] < 8) {
                int copy = MIN(8 - session->firstBytesLen[which], len);
                memcpy(session->firstBytes[which] + session->firstBytesLen[which], data, copy);
                session->firstBytesLen[which] += copy;
            }

            if (session->totalDatabytes[which] == session->consumed[which])  {
                moloch_parsers_classify_tcp(session, data, len, which);
            }

            moloch_packet_process_data(session, data, len, which);
            session->tcpSeq[which] += len;
            session->databytes[which] += len;
            session->totalDatabytes[which] += len;

            if (config.yara) {
                moloch_yara_execute(session, data, len, 0);
            }

            DLL_REMOVE(td_, tcpData, ftd);
            moloch_packet_free(ftd->packet);
            MOLOCH_TYPE_FREE(MolochTcpData_t, ftd);
        } else {
            return;
        }
    }
}

/******************************************************************************/
LOCAL void moloch_packet_process_icmp(MolochSession_t * const UNUSED(session), MolochPacket_t * const packet)
{
    const uint8_t *data = packet->pkt + packet->payloadOffset;

    if (packet->payloadLen >= 2) {
        moloch_field_int_add(icmpTypeField, session, data[0]);
        moloch_field_int_add(icmpCodeField, session, data[1]);
    }
}
/******************************************************************************/
LOCAL void moloch_packet_process_udp(MolochSession_t * const session, MolochPacket_t * const packet)
{
    const uint8_t *data = packet->pkt + packet->payloadOffset + 8;
    int            len = packet->payloadLen - 8;

    if (len <= 0)
        return;

    if (session->firstBytesLen[packet->direction] == 0) {
        session->firstBytesLen[packet->direction] = MIN(8, len);
        memcpy(session->firstBytes[packet->direction], data, session->firstBytesLen[packet->direction]);

        moloch_parsers_classify_udp(session, data, len, packet->direction);

        if (config.yara) {
            moloch_yara_execute(session, data, len, 0);
        }
    }

    int i;
    for (i = 0; i < session->parserNum; i++) {
        if (session->parserInfo[i].parserFunc) {
            session->parserInfo[i].parserFunc(session, session->parserInfo[i].uw, data, len, packet->direction);
        }
    }
}
/******************************************************************************/
LOCAL int moloch_packet_process_tcp(MolochSession_t * const session, MolochPacket_t * const packet)
{
    struct tcphdr       *tcphdr = (struct tcphdr *)(packet->pkt + packet->payloadOffset);


    int            len = packet->payloadLen - 4*tcphdr->th_off;

#ifdef DEBUG_PACKET
    LOG("poffset: %d plen: %d len: %d", packet->payloadOffset, packet->payloadLen, len);
#endif

    const uint32_t seq = ntohl(tcphdr->th_seq);

    if (tcphdr->th_win == 0 && (tcphdr->th_flags & TH_RST) == 0) {
        session->tcpFlagCnt[MOLOCH_TCPFLAG_SRC_ZERO + packet->direction]++;
    }

    if (len < 0)
        return 1;

    if (tcphdr->th_flags & TH_URG) {
        session->tcpFlagCnt[MOLOCH_TCPFLAG_URG]++;
    }

    if (tcphdr->th_flags & TH_SYN) {
        if (tcphdr->th_flags & TH_ACK) {
            session->tcpFlagCnt[MOLOCH_TCPFLAG_SYN_ACK]++;

            if (!session->haveTcpSession && config.antiSynDrop) {
#ifdef DEBUG_PACKET
                LOG("syn-ack first");
#endif
                session->tcpSeq[(packet->direction+1)%2] = ntohl(tcphdr->th_ack);
            }
        } else {
            session->tcpFlagCnt[MOLOCH_TCPFLAG_SYN]++;
        }

        session->haveTcpSession = 1;
        session->tcpSeq[packet->direction] = seq + 1;
        if (!session->tcp_next) {
            DLL_PUSH_TAIL(tcp_, &tcpWriteQ[session->thread], session);
        }
        return 1;
    }

    if (tcphdr->th_flags & TH_RST) {
        session->tcpFlagCnt[MOLOCH_TCPFLAG_RST]++;
        if (moloch_packet_sequence_diff(seq, session->tcpSeq[packet->direction]) <= 0) {
            return 1;
        }

        session->tcpState[packet->direction] = MOLOCH_TCP_STATE_FIN_ACK;
    }

    if (tcphdr->th_flags & TH_FIN) {
        session->tcpFlagCnt[MOLOCH_TCPFLAG_FIN]++;
        session->tcpState[packet->direction] = MOLOCH_TCP_STATE_FIN;
    }

    if ((tcphdr->th_flags & (TH_FIN | TH_RST | TH_PUSH | TH_SYN | TH_ACK)) == TH_ACK) {
        session->tcpFlagCnt[MOLOCH_TCPFLAG_ACK]++;
    }

    if (tcphdr->th_flags & TH_PUSH) {
        session->tcpFlagCnt[MOLOCH_TCPFLAG_PSH]++;
    }

    if (session->stopTCP)
        return 1;

    // If we've seen SYN but no SYN_ACK and no tcpSeq set, then just assume we've missed the syn-ack
    if (session->haveTcpSession && session->tcpFlagCnt[MOLOCH_TCPFLAG_SYN_ACK] == 0 && session->tcpSeq[packet->direction] == 0) {
        moloch_session_add_tag(session, "no-syn-ack");
        session->tcpSeq[packet->direction] = seq;
    }

    MolochTcpDataHead_t * const tcpData = &session->tcpData;

    if (DLL_COUNT(td_, tcpData) > maxTcpOutOfOrderPackets) {
        moloch_packet_tcp_free(session);
        moloch_session_add_tag(session, "incomplete-tcp");
        session->stopTCP = 1;
        return 1;
    }

    if (tcphdr->th_flags & (TH_ACK | TH_RST)) {
        int owhich = (packet->direction + 1) & 1;
        if (session->tcpState[owhich] == MOLOCH_TCP_STATE_FIN) {
            session->tcpState[owhich] = MOLOCH_TCP_STATE_FIN_ACK;
            if (session->tcpState[packet->direction] == MOLOCH_TCP_STATE_FIN_ACK) {

                if (!session->closingQ) {
                    moloch_session_mark_for_close(session, SESSION_TCP);
                }
                return 1;
            }
        }
    }

    if (tcphdr->th_flags & TH_ACK) {
        if (session->haveTcpSession && (session->ackedUnseenSegment & (1 << packet->direction)) == 0 &&
            (moloch_packet_sequence_diff(session->tcpSeq[(packet->direction+1)%2], ntohl(tcphdr->th_ack)) > 1)) {
                static const char *tags[2] = {"acked-unseen-segment-src", "acked-unseen-segment-dst"};
                moloch_session_add_tag(session, tags[packet->direction]);
                session->ackedUnseenSegment |= (1 << packet->direction);
        }
    }

    // Empty packet, drop from tcp processing
    if (len <= 0 || tcphdr->th_flags & TH_RST)
        return 1;

    // This packet is before what we are processing
    int32_t diff = moloch_packet_sequence_diff(session->tcpSeq[packet->direction], seq + len);
    if (diff <= 0)
        return 1;

    MolochTcpData_t *ftd, *td = MOLOCH_TYPE_ALLOC(MolochTcpData_t);
    const uint32_t ack = ntohl(tcphdr->th_ack);

    td->packet = packet;
    td->ack = ack;
    td->seq = seq;
    td->len = len;
    td->dataOffset = packet->payloadOffset + 4*tcphdr->th_off;

#ifdef DEBUG_PACKET
    LOG("dir: %d seq: %u ack: %u len: %d diff0: %d", packet->direction, seq, ack, len, diff);
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

            diff = moloch_packet_sequence_diff(sortB, sortA);
            if (diff == 0) {
                if (packet->direction == ftd->packet->direction) {
                    if (td->len > ftd->len) {
                        DLL_ADD_AFTER(td_, tcpData, ftd, td);

                        DLL_REMOVE(td_, tcpData, ftd);
                        moloch_packet_free(ftd->packet);
                        MOLOCH_TYPE_FREE(MolochTcpData_t, ftd);
                        ftd = td;
                    } else {
                        MOLOCH_TYPE_FREE(MolochTcpData_t, td);
                        return 1;
                    }
                    break;
                } else if (moloch_packet_sequence_diff(ack, ftd->seq) < 0) {
                    DLL_ADD_AFTER(td_, tcpData, ftd, td);
                    break;
                }
            } else if (diff > 0) {
                DLL_ADD_AFTER(td_, tcpData, ftd, td);
                break;
            }
        }

        if ((void*)ftd == (void*)tcpData) {
            DLL_PUSH_HEAD(td_, tcpData, td);
        }

        if (session->haveTcpSession && (session->outOfOrder & (1 << packet->direction)) == 0) {
            static const char *tags[2] = {"out-of-order-src", "out-of-order-dst"};
            moloch_session_add_tag(session, tags[packet->direction]);
            session->outOfOrder |= (1 << packet->direction);
        }
    }

    return 0;
}

/******************************************************************************/
void moloch_packet_thread_wake(int thread)
{
    MOLOCH_LOCK(packetQ[thread].lock);
    MOLOCH_COND_SIGNAL(packetQ[thread].lock);
    MOLOCH_UNLOCK(packetQ[thread].lock);
}
/******************************************************************************/
/* Only called on main thread, we busy block until all packet threads are empty.
 * Should only be used by tests and at end
 */
void moloch_packet_flush()
{
    int flushed = 0;
    int t;
    while (!flushed) {
        flushed = !moloch_session_cmd_outstanding();

        for (t = 0; t < config.packetThreads; t++) {
            MOLOCH_LOCK(packetQ[t].lock);
            if (DLL_COUNT(packet_, &packetQ[t]) > 0) {
                flushed = 0;
            }
            MOLOCH_UNLOCK(packetQ[t].lock);
            usleep(10000);
        }
    }
}
/******************************************************************************/
LOCAL void *moloch_packet_thread(void *threadp)
{
    MolochPacket_t  *packet;
    int thread = (long)threadp;

    while (1) {
        MOLOCH_LOCK(packetQ[thread].lock);
        inProgress[thread] = 0;
        if (DLL_COUNT(packet_, &packetQ[thread]) == 0) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME_COARSE, &ts);
            ts.tv_sec++;
            MOLOCH_COND_TIMEDWAIT(packetQ[thread].lock, ts);
        }
        inProgress[thread] = 1;
        DLL_POP_HEAD(packet_, &packetQ[thread], packet);
        MOLOCH_UNLOCK(packetQ[thread].lock);

        moloch_session_process_commands(thread);

        if (!packet)
            continue;
#ifdef DEBUG_PACKET
        LOG("Processing %p %d", packet, packet->pktlen);
#endif

        lastPacketSecs[thread] = packet->ts.tv_sec;

        MolochSession_t     *session;
        struct ip           *ip4 = (struct ip*)(packet->pkt + packet->ipOffset);
        struct ip6_hdr      *ip6 = (struct ip6_hdr*)(packet->pkt + packet->ipOffset);
        struct tcphdr       *tcphdr = 0;
        struct udphdr       *udphdr = 0;
        char                 sessionId[MOLOCH_SESSIONID_LEN];

        switch (packet->protocol) {
        case IPPROTO_TCP:
            tcphdr = (struct tcphdr *)(packet->pkt + packet->payloadOffset);

            if (packet->v6) {
                moloch_session_id6(sessionId, ip6->ip6_src.s6_addr, tcphdr->th_sport,
                                   ip6->ip6_dst.s6_addr, tcphdr->th_dport);
            } else {
                moloch_session_id(sessionId, ip4->ip_src.s_addr, tcphdr->th_sport,
                                  ip4->ip_dst.s_addr, tcphdr->th_dport);
            }
            break;
        case IPPROTO_UDP:
            udphdr = (struct udphdr *)(packet->pkt + packet->payloadOffset);
            if (packet->v6) {
                moloch_session_id6(sessionId, ip6->ip6_src.s6_addr, udphdr->uh_sport,
                                   ip6->ip6_dst.s6_addr, udphdr->uh_dport);
            } else {
                moloch_session_id(sessionId, ip4->ip_src.s_addr, udphdr->uh_sport,
                                  ip4->ip_dst.s_addr, udphdr->uh_dport);
            }
            break;
        case IPPROTO_ESP:
        case IPPROTO_ICMP:
            if (packet->v6) {
                moloch_session_id6(sessionId, ip6->ip6_src.s6_addr, 0,
                                   ip6->ip6_dst.s6_addr, 0);
            } else {
                moloch_session_id(sessionId, ip4->ip_src.s_addr, 0,
                                  ip4->ip_dst.s_addr, 0);
            }
            break;
        case IPPROTO_SCTP:
            udphdr = (struct udphdr *)(packet->pkt + packet->payloadOffset); /* Not really udp, but port in same location */
            if (packet->v6) {
                moloch_session_id6(sessionId, ip6->ip6_src.s6_addr, udphdr->uh_sport,
                                   ip6->ip6_dst.s6_addr, udphdr->uh_dport);
            } else {
                moloch_session_id(sessionId, ip4->ip_src.s_addr, udphdr->uh_sport,
                                  ip4->ip_dst.s_addr, udphdr->uh_dport);
            }
            break;
        case IPPROTO_ICMPV6:
            moloch_session_id6(sessionId, ip6->ip6_src.s6_addr, 0,
                               ip6->ip6_dst.s6_addr, 0);
            break;
        }

        int isNew;
        session = moloch_session_find_or_create(packet->ses, packet->hash, sessionId, &isNew); // Returns locked session

        if (isNew) {
            session->saveTime = packet->ts.tv_sec + config.tcpSaveTimeout;
            session->firstPacket = packet->ts;

            session->protocol = packet->protocol;
            if (ip4->ip_v == 4) {
                ((uint32_t *)session->addr1.s6_addr)[2] = htonl(0xffff);
                ((uint32_t *)session->addr1.s6_addr)[3] = ip4->ip_src.s_addr;
                ((uint32_t *)session->addr2.s6_addr)[2] = htonl(0xffff);
                ((uint32_t *)session->addr2.s6_addr)[3] = ip4->ip_dst.s_addr;
                session->ip_tos = ip4->ip_tos;
            } else {
                session->addr1 = ip6->ip6_src;
                session->addr2 = ip6->ip6_dst;
                session->ip_tos = 0;
            }
            session->thread = thread;

            moloch_parsers_initial_tag(session);

            if (readerFieldOps[packet->readerPos].num)
                moloch_field_ops_run(session, &readerFieldOps[packet->readerPos]);

            switch (session->protocol) {
            case IPPROTO_TCP:
               /* If antiSynDrop option is set to true, capture will assume that
                * if the syn-ack ip4 was captured first then the syn probably got dropped.*/
                if ((tcphdr->th_flags & TH_SYN) && (tcphdr->th_flags & TH_ACK) && (config.antiSynDrop)) {
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
                if (moloch_http_is_moloch(session->h_hash, sessionId)) {
                    if (config.debug) {
                        char buf[1000];
                        LOG("Ignoring connection %s", moloch_session_id_string(session->sessionId, buf));
                    }
                    session->stopSPI = 1;
                    moloch_packet_free(packet);
                    continue;
                }
                break;
            case IPPROTO_UDP:
                session->port1 = ntohs(udphdr->uh_sport);
                session->port2 = ntohs(udphdr->uh_dport);
                break;
            case IPPROTO_SCTP:
                session->port1 = ntohs(udphdr->uh_sport);
                session->port2 = ntohs(udphdr->uh_dport);
                break;
            case IPPROTO_ESP:
                session->stopSaving = 2;
                break;
            case IPPROTO_ICMP:
                break;
            }

            if (pluginsCbs & MOLOCH_PLUGIN_NEW)
                moloch_plugins_cb_new(session);
        } else if (session->stopSPI) {
            moloch_packet_free(packet);
            continue;
        }

        int dir;
        if (ip4->ip_v == 4) {
            dir = (MOLOCH_V6_TO_V4(session->addr1) == ip4->ip_src.s_addr &&
                   MOLOCH_V6_TO_V4(session->addr2) == ip4->ip_dst.s_addr);
        } else {
            dir = (memcmp(session->addr1.s6_addr, ip6->ip6_src.s6_addr, 16) == 0 &&
                   memcmp(session->addr2.s6_addr, ip6->ip6_dst.s6_addr, 16) == 0);
        }

        packet->direction = 0;
        switch (session->protocol) {
        case IPPROTO_UDP:
            udphdr = (struct udphdr *)(packet->pkt + packet->payloadOffset);
            packet->direction = (dir &&
                                 session->port1 == ntohs(udphdr->uh_sport) &&
                                 session->port2 == ntohs(udphdr->uh_dport))?0:1;
            session->databytes[packet->direction] += (packet->pktlen - 8);
            break;
        case IPPROTO_SCTP:
            udphdr = (struct udphdr *)(packet->pkt + packet->payloadOffset);
            packet->direction = (dir &&
                                 session->port1 == ntohs(udphdr->uh_sport) &&
                                 session->port2 == ntohs(udphdr->uh_dport))?0:1;
            session->databytes[packet->direction] += (packet->pktlen - 8);
            break;
        case IPPROTO_TCP:
            tcphdr = (struct tcphdr *)(packet->pkt + packet->payloadOffset);
            packet->direction = (dir &&
                                 session->port1 == ntohs(tcphdr->th_sport) &&
                                 session->port2 == ntohs(tcphdr->th_dport))?0:1;
            session->tcp_flags |= tcphdr->th_flags;
            break;
        case IPPROTO_ICMP:
        case IPPROTO_ESP:
            packet->direction = (dir)?0:1;
            break;
        }

        if (isNew) {
            moloch_rules_session_create(session);
        }

        /* Check if the stop saving bpf filters match */
        if (session->packets[packet->direction] == 0 && session->stopSaving == 0) {
            moloch_rules_run_session_setup(session, packet);
        }

        session->packets[packet->direction]++;
        session->bytes[packet->direction] += packet->pktlen;
        session->lastPacket = packet->ts;

        uint32_t packets = session->packets[0] + session->packets[1];

        if (session->stopSaving == 0 || packets < session->stopSaving) {
            moloch_writer_write(session, packet);

            int16_t len;
            if (session->lastFileNum != packet->writerFileNum) {
                session->lastFileNum = packet->writerFileNum;
                g_array_append_val(session->fileNumArray, packet->writerFileNum);
                int64_t pos = -1LL * packet->writerFileNum;
                g_array_append_val(session->filePosArray, pos);
                len = 0;
                g_array_append_val(session->fileLenArray, len);
            }

            g_array_append_val(session->filePosArray, packet->writerFilePos);
            len = 16 + packet->pktlen;
            g_array_append_val(session->fileLenArray, len);

            if (packets >= config.maxPackets || session->midSave) {
                moloch_session_mid_save(session, packet->ts.tv_sec);
            }
        }

        if (pcapFileHeader.linktype == 1 && session->firstBytesLen[packet->direction] < 8 && session->packets[packet->direction] < 10) {
            const uint8_t *pcapData = packet->pkt;

            if (packet->direction == 1) {
                moloch_field_macoui_add(session, mac1Field, oui1Field, pcapData+0);
                moloch_field_macoui_add(session, mac2Field, oui2Field, pcapData+6);
            } else {
                moloch_field_macoui_add(session, mac1Field, oui1Field, pcapData+6);
                moloch_field_macoui_add(session, mac2Field, oui2Field, pcapData+0);
            }

            int n = 12;
            while (pcapData[n] == 0x81 && pcapData[n+1] == 0x00) {
                uint16_t vlan = ((uint16_t)(pcapData[n+2] << 8 | pcapData[n+3])) & 0xfff;
                moloch_field_int_add(vlanField, session, vlan);
                n += 4;
            }

            if (packet->tunnel & MOLOCH_PACKET_TUNNEL_GRE) {
                ip4 = (struct ip*)(packet->pkt + packet->vpnIpOffset);
                moloch_field_ip4_add(greIpField, session, ip4->ip_src.s_addr);
                moloch_field_ip4_add(greIpField, session, ip4->ip_dst.s_addr);
                moloch_session_add_protocol(session, "gre");
            }

            if (packet->tunnel &  MOLOCH_PACKET_TUNNEL_PPPOE) {
                moloch_session_add_protocol(session, "pppoe");
            }

            if (packet->tunnel &  MOLOCH_PACKET_TUNNEL_PPP) {
                moloch_session_add_protocol(session, "ppp");
            }

            if (packet->tunnel &  MOLOCH_PACKET_TUNNEL_MPLS) {
                moloch_session_add_protocol(session, "mpls");
            }
        }


        int freePacket = 1;
        switch(packet->ses) {
        case SESSION_ICMP:
            moloch_packet_process_icmp(session, packet);
            break;
        case SESSION_UDP:
            moloch_packet_process_udp(session, packet);
            break;
        case SESSION_TCP:
            freePacket = moloch_packet_process_tcp(session, packet);
            moloch_packet_tcp_finish(session);
            break;
        }

        if (freePacket) {
            moloch_packet_free(packet);
        }
    }

    return NULL;
}

/******************************************************************************/
static FILE *unknownPacketFile[2];
LOCAL void moloch_packet_save_unknown_packet(int type, MolochPacket_t * const packet)
{
    static const char *names[] = {"unknown.ether", "unknown.ip"};
    static MOLOCH_LOCK_DEFINE(lock);

    struct moloch_pcap_sf_pkthdr hdr;
    hdr.ts.tv_sec  = packet->ts.tv_sec;
    hdr.ts.tv_usec = packet->ts.tv_usec;
    hdr.caplen     = packet->pktlen;
    hdr.pktlen     = packet->pktlen;

    MOLOCH_LOCK(lock);
    if (!unknownPacketFile[type]) {
        char str[PATH_MAX];
        snprintf(str, sizeof(str), "%s/%s.%d.pcap", config.pcapDir[0], names[type], getpid());
        unknownPacketFile[type] = fopen(str, "w");
        fwrite(&pcapFileHeader, 24, 1, unknownPacketFile[type]);
    }

    fwrite(&hdr, 16, 1, unknownPacketFile[type]);
    fwrite(packet->pkt, packet->pktlen, 1, unknownPacketFile[type]);
    MOLOCH_UNLOCK(lock);
}

/******************************************************************************/
LOCAL int moloch_packet_gre4(MolochPacketBatch_t * batch, MolochPacket_t * const packet, const uint8_t *data, int len)
{
    BSB bsb;
    if (unlikely(len) < 4 || unlikely(!data))
        return MOLOCH_PACKET_CORRUPT;

    BSB_INIT(bsb, data, len);

    uint16_t flags_version = 0;
    BSB_IMPORT_u16(bsb, flags_version);
    uint16_t type = 0;
    BSB_IMPORT_u16(bsb, type);

    switch (type) {
    case 0x0800:
    case 0x86dd:
    case 0x6559:
    case 0x880b:
        break;
    default:
        if (config.logUnknownProtocols)
            LOG("Unknown GRE protocol 0x%04x(%d)", type, type);
        if (BIT_ISSET(type, config.etherSavePcap))
            moloch_packet_save_unknown_packet(0, packet);
        return MOLOCH_PACKET_UNKNOWN;
    }

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
            int len = 0;
            BSB_IMPORT_u08(bsb, len);
            if (len == 0)
                break;
            BSB_IMPORT_skip(bsb, len);
        }
    }

    // ack number
    if (flags_version & 0x0080) {
        BSB_IMPORT_skip(bsb, 4);
    }

    if (BSB_IS_ERROR(bsb))
        return MOLOCH_PACKET_CORRUPT;

    switch (type) {
    case 0x0800:
        return moloch_packet_ip4(batch, packet, BSB_WORK_PTR(bsb), BSB_REMAINING(bsb));
    case 0x86dd:
        return moloch_packet_ip6(batch, packet, BSB_WORK_PTR(bsb), BSB_REMAINING(bsb));
    case 0x6559:
        return moloch_packet_frame_relay(batch, packet, BSB_WORK_PTR(bsb), BSB_REMAINING(bsb));
    case 0x880b:
        return moloch_packet_ppp(batch, packet, BSB_WORK_PTR(bsb), BSB_REMAINING(bsb));
    default:
        return MOLOCH_PACKET_UNKNOWN;
    }

}
/******************************************************************************/
void moloch_packet_frags_free(MolochFrags_t * const frags)
{
    MolochPacket_t *packet;

    while (DLL_POP_HEAD(packet_, &frags->packets, packet)) {
        moloch_packet_free(packet);
    }
    HASH_REMOVE(fragh_, fragsHash, frags);
    DLL_REMOVE(fragl_, &fragsList, frags);
    MOLOCH_TYPE_FREE(MolochFrags_t, frags);
}
/******************************************************************************/
LOCAL gboolean moloch_packet_frags_process(MolochPacket_t * const packet)
{
    MolochPacket_t * fpacket;
    MolochFrags_t   *frags;
    char             key[10];

    struct ip * const ip4 = (struct ip*)(packet->pkt + packet->ipOffset);
    memcpy(key, &ip4->ip_src.s_addr, 4);
    memcpy(key+4, &ip4->ip_dst.s_addr, 4);
    memcpy(key+8, &ip4->ip_id, 2);

    HASH_FIND(fragh_, fragsHash, key, frags);

    if (!frags) {
        frags = MOLOCH_TYPE_ALLOC0(MolochFrags_t);
        memcpy(frags->key, key, 10);
        frags->secs = packet->ts.tv_sec;
        HASH_ADD(fragh_, fragsHash, key, frags);
        DLL_PUSH_TAIL(fragl_, &fragsList, frags);
        DLL_INIT(packet_, &frags->packets);
        DLL_PUSH_TAIL(packet_, &frags->packets, packet);

        if (DLL_COUNT(fragl_, &fragsList) > config.maxFrags) {
            droppedFrags++;
            moloch_packet_frags_free(DLL_PEEK_HEAD(fragl_, &fragsList));
        }
        return FALSE;
    } else {
        DLL_MOVE_TAIL(fragl_, &fragsList, frags);
    }

    uint16_t          ip_off = ntohs(ip4->ip_off);
    uint16_t          ip_flags = ip_off & ~IP_OFFMASK;
    ip_off &= IP_OFFMASK;


    // we might be done once we receive the packets with no flags
    if (ip_flags == 0) {
        frags->haveNoFlags = 1;
    }

    // Insert this packet in correct location sorted by offset
    DLL_FOREACH_REVERSE(packet_, &frags->packets, fpacket) {
        struct ip *fip4 = (struct ip*)(fpacket->pkt + fpacket->ipOffset);
        uint16_t fip_off = ntohs(fip4->ip_off) & IP_OFFMASK;
        if (ip_off >= fip_off) {
            DLL_ADD_AFTER(packet_, &frags->packets, fpacket, packet);
            break;
        }
    }
    if ((void*)fpacket == (void*)&frags->packets) {
        DLL_PUSH_HEAD(packet_, &frags->packets, packet);
    }

    // Don't bother checking until we get a packet with no flags
    if (!frags->haveNoFlags) {
        return FALSE;
    }

    int off = 0;
    struct ip *fip4;
    uint16_t fip_off;

    int payloadLen = 0;
    DLL_FOREACH(packet_, &frags->packets, fpacket) {
        fip4 = (struct ip*)(fpacket->pkt + fpacket->ipOffset);
        fip_off = ntohs(fip4->ip_off) & IP_OFFMASK;
        if (fip_off != off)
            break;
        off += fpacket->payloadLen/8;
        payloadLen = MAX(payloadLen, fip_off*8 + fpacket->payloadLen);
    }
    // We have a hole
    if ((void*)fpacket != (void*)&frags->packets) {
        return FALSE;
    }

    // Packet is too large, hacker
    if (payloadLen + packet->payloadOffset >= MOLOCH_PACKET_MAX_LEN) {
        droppedFrags++;
        moloch_packet_frags_free(frags);
        return FALSE;
    }

    // Now alloc the full packet
    packet->pktlen = packet->payloadOffset + payloadLen;
    uint8_t *pkt = malloc(packet->pktlen);

    // Copy packet header
    memcpy(pkt, packet->pkt, packet->payloadOffset);

    // Fix header of new packet
    fip4 = (struct ip*)(pkt + packet->ipOffset);
    fip4->ip_len = htons(payloadLen + 4*ip4->ip_hl);
    fip4->ip_off = 0;

    // Copy payload
    DLL_FOREACH(packet_, &frags->packets, fpacket) {
        struct ip *fip4 = (struct ip*)(fpacket->pkt + fpacket->ipOffset);
        uint16_t fip_off = ntohs(fip4->ip_off) & IP_OFFMASK;

        if (packet->payloadOffset+(fip_off*8) + fpacket->payloadLen <= packet->pktlen)
            memcpy(pkt+packet->payloadOffset+(fip_off*8), fpacket->pkt+fpacket->payloadOffset, fpacket->payloadLen);
        else
            LOG("WARNING - Not enough room for frag %d > %d", packet->payloadOffset+(fip_off*8) + fpacket->payloadLen, packet->pktlen);
    }

    // Set all the vars in the current packet to new defraged packet
    if (packet->copied)
        free(packet->pkt);
    packet->pkt = pkt;
    packet->copied = 1;
    packet->wasfrag = 1;
    packet->payloadLen = payloadLen;
    DLL_REMOVE(packet_, &frags->packets, packet); // Remove from list so we don't get freed in frags_free
    moloch_packet_frags_free(frags);
    return TRUE;
}
/******************************************************************************/
LOCAL void moloch_packet_frags4(MolochPacketBatch_t *batch, MolochPacket_t * const packet)
{
    MolochFrags_t *frags;

    // ALW - Should change frags_process to make the copy when needed
    uint8_t *pkt = malloc(packet->pktlen);
    memcpy(pkt, packet->pkt, packet->pktlen);
    packet->pkt = pkt;
    packet->copied = 1;

    MOLOCH_LOCK(frags);
    // Remove expired entries
    while ((frags = DLL_PEEK_HEAD(fragl_, &fragsList)) && (frags->secs + config.fragsTimeout < packet->ts.tv_sec)) {
        droppedFrags++;
        moloch_packet_frags_free(frags);
    }

    gboolean process = moloch_packet_frags_process(packet);
    MOLOCH_UNLOCK(frags);

    if (process)
        moloch_packet_batch(batch, packet);
}
/******************************************************************************/
int moloch_packet_frags_size()
{
    return DLL_COUNT(fragl_, &fragsList);
}
/******************************************************************************/
int moloch_packet_frags_outstanding()
{
    return 0;
}
/******************************************************************************/
LOCAL void moloch_packet_log(int ses)
{
    MolochReaderStats_t stats;
    if (moloch_reader_stats(&stats)) {
        stats.dropped = 0;
        stats.total = totalPackets;
    }

    LOG("packets: %" PRIu64 " current sessions: %u/%u oldest: %d - recv: %" PRIu64 " drop: %" PRIu64 " (%0.2f) queue: %d disk: %d packet: %d close: %d ns: %d frags: %d/%d pstats: %" PRIu64 "/%" PRIu64 "/%" PRIu64 "/%" PRIu64 "/%" PRIu64,
      totalPackets,
      moloch_session_watch_count(ses),
      moloch_session_monitoring(),
      moloch_session_idle_seconds(ses),
      stats.total,
      stats.dropped - initialDropped,
      (stats.dropped - initialDropped)*(double)100.0/stats.total,
      moloch_http_queue_length(esServer),
      moloch_writer_queue_length(),
      moloch_packet_outstanding(),
      moloch_session_close_outstanding(),
      moloch_session_need_save_outstanding(),
      moloch_packet_frags_outstanding(),
      moloch_packet_frags_size(),
      packetStats[MOLOCH_PACKET_SUCCESS],
      packetStats[MOLOCH_PACKET_IP_DROPPED],
      packetStats[MOLOCH_PACKET_OVERLOAD_DROPPED],
      packetStats[MOLOCH_PACKET_CORRUPT],
      packetStats[MOLOCH_PACKET_UNKNOWN]
      );
}
/******************************************************************************/
LOCAL int moloch_packet_ip(MolochPacketBatch_t *batch, MolochPacket_t * const packet, const char * const sessionId)
{
    if (totalPackets == 0) {
        MolochReaderStats_t stats;
        if (!moloch_reader_stats(&stats)) {
            initialDropped = stats.dropped;
        }
        initialPacket = packet->ts;
        LOG("Initial Packet = %ld", initialPacket.tv_sec);
        LOG("%" PRIu64 " Initial Dropped = %d", totalPackets, initialDropped);
    }

    MOLOCH_THREAD_INCR(totalPackets);
    if (totalPackets % config.logEveryXPackets == 0) {
        moloch_packet_log(packet->ses);
    }

    packet->hash = moloch_session_hash(sessionId);
    uint32_t thread = packet->hash % config.packetThreads;

    totalBytes[thread] += packet->pktlen;

    if (DLL_COUNT(packet_, &packetQ[thread]) >= config.maxPacketsInQueue) {
        MOLOCH_LOCK(packetQ[thread].lock);
        overloadDrops[thread]++;
        if ((overloadDrops[thread] % 10000) == 1) {
            LOG("WARNING - Packet Q %d is overflowing, total dropped %u, increase packetThreads or maxPacketsInQueue", thread, overloadDrops[thread]);
        }
        packet->pkt = 0;
        MOLOCH_COND_SIGNAL(packetQ[thread].lock);
        MOLOCH_UNLOCK(packetQ[thread].lock);
        return MOLOCH_PACKET_OVERLOAD_DROPPED;
    }

    if (!packet->copied) {
        uint8_t *pkt = malloc(packet->pktlen);
        memcpy(pkt, packet->pkt, packet->pktlen);
        packet->pkt = pkt;
        packet->copied = 1;
    }

    DLL_PUSH_TAIL(packet_, &batch->packetQ[thread], packet);
    batch->count++;
    return MOLOCH_PACKET_SUCCESS;
}
/******************************************************************************/
LOCAL int moloch_packet_ip4(MolochPacketBatch_t *batch, MolochPacket_t * const packet, const uint8_t *data, int len)
{
    struct ip           *ip4 = (struct ip*)data;
    struct tcphdr       *tcphdr = 0;
    struct udphdr       *udphdr = 0;
    char                 sessionId[MOLOCH_SESSIONID_LEN];

    if (len < (int)sizeof(struct ip)) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: too small for header %p %d", packet, len);
#endif
        return MOLOCH_PACKET_CORRUPT;
    }

    int ip_len = ntohs(ip4->ip_len);
    if (len < ip_len) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: incomplete %p %d %d", packet, len, ip_len);
#endif
        return MOLOCH_PACKET_CORRUPT;
    }

    int ip_hdr_len = 4 * ip4->ip_hl;
    if (len < ip_hdr_len) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: too small for header and options %p %d", packet, len);
#endif
        return MOLOCH_PACKET_CORRUPT;
    }
    if (ipTree4) {
        patricia_node_t *node;

        if ((node = patricia_search_best3 (ipTree4, (u_char*)&ip4->ip_src, 32)) && node->data == NULL)
            return MOLOCH_PACKET_IP_DROPPED;

        if ((node = patricia_search_best3 (ipTree4, (u_char*)&ip4->ip_dst, 32)) && node->data == NULL)
            return MOLOCH_PACKET_IP_DROPPED;
    }

    packet->ipOffset = (uint8_t*)data - packet->pkt;
    packet->v6 = 0;
    packet->payloadOffset = packet->ipOffset + ip_hdr_len;
    packet->payloadLen = ip_len - ip_hdr_len;

    uint16_t ip_off = ntohs(ip4->ip_off);
    uint16_t ip_flags = ip_off & ~IP_OFFMASK;
    ip_off &= IP_OFFMASK;


    if ((ip_flags & IP_MF) || ip_off > 0) {
        moloch_packet_frags4(batch, packet);
        return MOLOCH_PACKET_SUCCESS;
    }

    switch (ip4->ip_p) {
    case IPPROTO_TCP:
        if (len < ip_hdr_len + (int)sizeof(struct tcphdr)) {
#ifdef DEBUG_PACKET
            LOG("BAD PACKET: too small for tcp hdr %p %d", packet, len);
#endif
            return MOLOCH_PACKET_CORRUPT;
        }

        tcphdr = (struct tcphdr *)((char*)ip4 + ip_hdr_len);

        if (packetDrop4.drops[tcphdr->th_sport] &&
            moloch_drophash_should_drop(&packetDrop4, tcphdr->th_sport, &ip4->ip_src.s_addr, packet->ts.tv_sec)) {

            return MOLOCH_PACKET_IPPORT_DROPPED;
        }

        if (packetDrop4.drops[tcphdr->th_dport] &&
            moloch_drophash_should_drop(&packetDrop4, tcphdr->th_dport, &ip4->ip_dst.s_addr, packet->ts.tv_sec)) {

            return MOLOCH_PACKET_IPPORT_DROPPED;
        }

        moloch_session_id(sessionId, ip4->ip_src.s_addr, tcphdr->th_sport,
                          ip4->ip_dst.s_addr, tcphdr->th_dport);
        packet->ses = SESSION_TCP;

        break;
    case IPPROTO_UDP:
        if (len < ip_hdr_len + (int)sizeof(struct udphdr)) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: too small for udp header %p %d", packet, len);
#endif
            return MOLOCH_PACKET_CORRUPT;
        }

        udphdr = (struct udphdr *)((char*)ip4 + ip_hdr_len);

        moloch_session_id(sessionId, ip4->ip_src.s_addr, udphdr->uh_sport,
                          ip4->ip_dst.s_addr, udphdr->uh_dport);
        packet->ses = SESSION_UDP;
        break;
    case IPPROTO_SCTP:
        if (len < ip_hdr_len + 12) {
#ifdef DEBUG_PACKET
            LOG("BAD PACKET: too small for sctp hdr %p %d", packet, len);
#endif
            return MOLOCH_PACKET_CORRUPT;
        }
        udphdr = (struct udphdr *)((char*)ip4 + ip_hdr_len); /* Not really udp, but port in same location */

        moloch_session_id(sessionId, ip4->ip_src.s_addr, udphdr->uh_sport,
                          ip4->ip_dst.s_addr, udphdr->uh_dport);
        packet->ses = SESSION_SCTP;
        break;
    case IPPROTO_ESP:
        if (!config.trackESP)
            return MOLOCH_PACKET_UNKNOWN;
        moloch_session_id(sessionId, ip4->ip_src.s_addr, 0,
                          ip4->ip_dst.s_addr, 0);
        packet->ses = SESSION_ESP;
        break;
    case IPPROTO_ICMP:
        moloch_session_id(sessionId, ip4->ip_src.s_addr, 0,
                          ip4->ip_dst.s_addr, 0);
        packet->ses = SESSION_ICMP;
        break;
    case IPPROTO_GRE:
        packet->tunnel |= MOLOCH_PACKET_TUNNEL_GRE;
        packet->vpnIpOffset = packet->ipOffset; // ipOffset will get reset
        return moloch_packet_gre4(batch, packet, data + ip_hdr_len, len - ip_hdr_len);
    case IPPROTO_IPV6:
        return moloch_packet_ip6(batch, packet, data + ip_hdr_len, len - ip_hdr_len);
    default:
        if (config.logUnknownProtocols)
            LOG("Unknown protocol %d", ip4->ip_p);
        if (BIT_ISSET(ip4->ip_p, config.ipSavePcap))
            moloch_packet_save_unknown_packet(1, packet);
        return MOLOCH_PACKET_UNKNOWN;
    }
    packet->protocol = ip4->ip_p;

    return moloch_packet_ip(batch, packet, sessionId);
}
/******************************************************************************/
LOCAL int moloch_packet_ip6(MolochPacketBatch_t * batch, MolochPacket_t * const packet, const uint8_t *data, int len)
{
    struct ip6_hdr      *ip6 = (struct ip6_hdr *)data;
    struct tcphdr       *tcphdr = 0;
    struct udphdr       *udphdr = 0;
    char                 sessionId[MOLOCH_SESSIONID_LEN];

    if (len < (int)sizeof(struct ip6_hdr)) {
        return MOLOCH_PACKET_CORRUPT;
    }

    int ip_len = ntohs(ip6->ip6_plen);
    if (len < ip_len) {
        return MOLOCH_PACKET_CORRUPT;
    }

    if (ipTree6) {
        patricia_node_t *node;

        if ((node = patricia_search_best3 (ipTree6, (u_char*)&ip6->ip6_src, 128)) && node->data == NULL)
            return MOLOCH_PACKET_IP_DROPPED;

        if ((node = patricia_search_best3 (ipTree6, (u_char*)&ip6->ip6_dst, 128)) && node->data == NULL)
            return MOLOCH_PACKET_IP_DROPPED;
    }

    int ip_hdr_len = sizeof(struct ip6_hdr);

    packet->ipOffset = (uint8_t*)data - packet->pkt;
    packet->v6 = 1;


    int nxt = ip6->ip6_nxt;
    int done = 0;
    do {
        switch (nxt) {
        case IPPROTO_HOPOPTS:
        case IPPROTO_DSTOPTS:
        case IPPROTO_ROUTING:
            nxt = data[ip_hdr_len];
            ip_hdr_len += ((data[ip_hdr_len+1] + 1) << 3);
            break;
        case IPPROTO_FRAGMENT:
            LOG("ERROR - Don't support ip6 fragements yet!");
            return MOLOCH_PACKET_UNKNOWN;
        case IPPROTO_TCP:
            if (len < ip_hdr_len + (int)sizeof(struct tcphdr)) {
                return MOLOCH_PACKET_CORRUPT;
            }

            tcphdr = (struct tcphdr *)(data + ip_hdr_len);


            if (packetDrop6.drops[tcphdr->th_sport] &&
                moloch_drophash_should_drop(&packetDrop6, tcphdr->th_sport, &ip6->ip6_src, packet->ts.tv_sec)) {

                return MOLOCH_PACKET_IPPORT_DROPPED;
            }

            if (packetDrop6.drops[tcphdr->th_dport] &&
                moloch_drophash_should_drop(&packetDrop6, tcphdr->th_dport, &ip6->ip6_dst, packet->ts.tv_sec)) {

                return MOLOCH_PACKET_IPPORT_DROPPED;
            }

            moloch_session_id6(sessionId, ip6->ip6_src.s6_addr, tcphdr->th_sport,
                               ip6->ip6_dst.s6_addr, tcphdr->th_dport);
            packet->ses = SESSION_TCP;
            done = 1;
            break;
        case IPPROTO_UDP:
            if (len < ip_hdr_len + (int)sizeof(struct udphdr)) {
                return MOLOCH_PACKET_CORRUPT;
            }

            udphdr = (struct udphdr *)(data + ip_hdr_len);

            moloch_session_id6(sessionId, ip6->ip6_src.s6_addr, udphdr->uh_sport,
                               ip6->ip6_dst.s6_addr, udphdr->uh_dport);

            packet->ses = SESSION_UDP;
            done = 1;
            break;
        case IPPROTO_SCTP:
            if (len < ip_hdr_len + 12) {
                return MOLOCH_PACKET_CORRUPT;
            }

            udphdr = (struct udphdr *)(data + ip_hdr_len); /* Not really udp, but port in same location */

            moloch_session_id6(sessionId, ip6->ip6_src.s6_addr, udphdr->uh_sport,
                               ip6->ip6_dst.s6_addr, udphdr->uh_dport);
            packet->ses = SESSION_SCTP;
            break;
        case IPPROTO_ESP:
            if (!config.trackESP)
                return MOLOCH_PACKET_UNKNOWN;
            moloch_session_id6(sessionId, ip6->ip6_src.s6_addr, 0,
                               ip6->ip6_dst.s6_addr, 0);
            packet->ses = SESSION_ESP;
            done = 1;
            break;
        case IPPROTO_ICMP:
            moloch_session_id6(sessionId, ip6->ip6_src.s6_addr, 0,
                               ip6->ip6_dst.s6_addr, 0);
            packet->ses = SESSION_ICMP;
            done = 1;
            break;
        case IPPROTO_ICMPV6:
            moloch_session_id6(sessionId, ip6->ip6_src.s6_addr, 0,
                               ip6->ip6_dst.s6_addr, 0);
            packet->ses = SESSION_ICMP;
            done = 1;
            break;
        case IPPROTO_IPV4:
            return moloch_packet_ip4(batch, packet, data + ip_hdr_len, len - ip_hdr_len);
        case IPPROTO_IPV6:
            return moloch_packet_ip6(batch, packet, data + ip_hdr_len, len - ip_hdr_len);
        default:
            if (config.logUnknownProtocols)
                LOG("Unknown protocol %d %d", ip6->ip6_nxt, nxt);
            if (BIT_ISSET(nxt, config.ipSavePcap))
                moloch_packet_save_unknown_packet(1, packet);
            return MOLOCH_PACKET_UNKNOWN;
        }
        if (ip_hdr_len > len) {
            LOG ("ERROR - Corrupt packet ip_hdr_len = %d nxt = %d len = %d", ip_hdr_len, nxt, len);
            return MOLOCH_PACKET_CORRUPT;
        }
    } while (!done);

    packet->protocol = nxt;
    packet->payloadOffset = packet->ipOffset + ip_hdr_len;
    packet->payloadLen = ip_len - ip_hdr_len + sizeof(struct ip6_hdr);
    return moloch_packet_ip(batch, packet, sessionId);
}
/******************************************************************************/
LOCAL int moloch_packet_frame_relay(MolochPacketBatch_t *batch, MolochPacket_t * const packet, const uint8_t *data, int len)
{
    if (len < 4)
        return MOLOCH_PACKET_CORRUPT;

    if (data[2] == 0x03 || data[3] == 0xcc)
        return moloch_packet_ip4(batch, packet, data+4, len-4);
    if (data[2] == 0x08 || data[3] == 0x00)
        return moloch_packet_ip4(batch, packet, data+4, len-4);
    if (data[2] == 0x86 || data[3] == 0xdd)
        return moloch_packet_ip6(batch, packet, data+4, len-4);

    return MOLOCH_PACKET_UNKNOWN;
}
/******************************************************************************/
LOCAL int moloch_packet_pppoe(MolochPacketBatch_t * batch, MolochPacket_t * const packet, const uint8_t *data, int len)
{
    if (len < 8 || data[0] != 0x11 || data[1] != 0) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: Len or bytes %d %d %d", len, data[0], data[1]);
#endif
        return MOLOCH_PACKET_CORRUPT;
    }

    uint16_t plen = data[4] << 8 | data[5];
    uint16_t type = data[6] << 8 | data[7];
    if (plen != len-6)
        return MOLOCH_PACKET_CORRUPT;

    packet->tunnel |= MOLOCH_PACKET_TUNNEL_PPPOE;
    switch (type) {
    case 0x21:
        return moloch_packet_ip4(batch, packet, data + 8, plen-2);
    case 0x57:
        return moloch_packet_ip6(batch, packet, data + 8, plen-2);
    default:
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: Unknown pppoe type %d", type);
#endif
        return MOLOCH_PACKET_UNKNOWN;
    }
}
/******************************************************************************/
LOCAL int moloch_packet_ppp(MolochPacketBatch_t * batch, MolochPacket_t * const packet, const uint8_t *data, int len)
{
    if (len < 4 || data[2] != 0x00) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: Len or bytes %d %d %d", len, data[2], data[3]);
#endif
        return MOLOCH_PACKET_CORRUPT;
    }

    packet->tunnel |= MOLOCH_PACKET_TUNNEL_PPP;
    switch (data[3]) {
    case 0x21:
        return moloch_packet_ip4(batch, packet, data + 4, len-4);
    case 0x57:
        return moloch_packet_ip6(batch, packet, data + 4, len-4);
    default:
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: Unknown ppp type %d", data[3]);
#endif
        return MOLOCH_PACKET_UNKNOWN;
    }
}
/******************************************************************************/
LOCAL int moloch_packet_mpls(MolochPacketBatch_t * batch, MolochPacket_t * const packet, const uint8_t *data, int len)
{
    while (1) {
        if (len < 4 + (int)sizeof(struct ip)) {
#ifdef DEBUG_PACKET
            LOG("BAD PACKET: Len %d", len);
#endif
            return MOLOCH_PACKET_CORRUPT;
        }

        int S = data[3] & 0x1;

        data += 4;
        len -= 4;

        if (S) {
            packet->tunnel |= MOLOCH_PACKET_TUNNEL_MPLS;
            switch (data[0] >> 4) {
            case 4:
                return moloch_packet_ip4(batch, packet, data, len);
            case 6:
                return moloch_packet_ip6(batch, packet, data, len);
            default:
#ifdef DEBUG_PACKET
                LOG("BAD PACKET: Unknown mpls type %d", data[0] >> 4);
#endif
                return MOLOCH_PACKET_UNKNOWN;
            }
        }
    }
    return MOLOCH_PACKET_CORRUPT;
}
/******************************************************************************/
LOCAL int moloch_packet_ether(MolochPacketBatch_t * batch, MolochPacket_t * const packet, const uint8_t *data, int len)
{
    if (len < 14) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: Too short %d", len);
#endif
        return MOLOCH_PACKET_CORRUPT;
    }
    int n = 12;
    while (n+2 < len) {
        int ethertype = data[n] << 8 | data[n+1];
        n += 2;
        switch (ethertype) {
        case 0x0800:
            return moloch_packet_ip4(batch, packet, data+n, len - n);
        case 0x86dd:
            return moloch_packet_ip6(batch, packet, data+n, len - n);
        case 0x8864:
            return moloch_packet_pppoe(batch, packet, data+n, len - n);
        case 0x8847:
            return moloch_packet_mpls(batch, packet, data+n, len - n);
        case 0x8100:
            n += 2;
            break;
        default:
#ifdef DEBUG_PACKET
            LOG("BAD PACKET: Unknown ethertype %x", ethertype);
#endif
            if (BIT_ISSET(ethertype, config.etherSavePcap))
                moloch_packet_save_unknown_packet(0, packet);
            return MOLOCH_PACKET_UNKNOWN;
        } // switch
    }
#ifdef DEBUG_PACKET
    LOG("BAD PACKET: bad len %d < %d", n+2, len);
#endif
    return MOLOCH_PACKET_CORRUPT;
}
/******************************************************************************/
LOCAL int moloch_packet_sll(MolochPacketBatch_t * batch, MolochPacket_t * const packet, const uint8_t *data, int len)
{
    if (len < 16) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: Too short %d", len);
#endif
        return MOLOCH_PACKET_CORRUPT;
    }

    int ethertype = data[14] << 8 | data[15];
    switch (ethertype) {
    case 0x0800:
        return moloch_packet_ip4(batch, packet, data+16, len - 16);
    case 0x86dd:
        return moloch_packet_ip6(batch, packet, data+16, len - 16);
    case 0x8864:
        return moloch_packet_pppoe(batch, packet, data+16, len - 16);
    case 0x8847:
        return moloch_packet_mpls(batch, packet, data+16, len - 16);
    default:
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: Unknown ethertype %x", ethertype);
#endif
        if (BIT_ISSET(ethertype, config.etherSavePcap))
            moloch_packet_save_unknown_packet(0, packet);
        return MOLOCH_PACKET_UNKNOWN;
    } // switch
    return MOLOCH_PACKET_CORRUPT;
}
/******************************************************************************/
LOCAL int moloch_packet_nflog(MolochPacketBatch_t * batch, MolochPacket_t * const packet, const uint8_t *data, int len)
{
    if (len < 14 ||
        (data[0] != AF_INET && data[0] != AF_INET6) ||
        data[1] != 0) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: Wrong type %d", data[0]);
#endif
        return MOLOCH_PACKET_CORRUPT;
    }
    int n = 4;
    while (n+4 < len) {
        int length = data[n+1] << 8 | data[n];

        // Make sure length is at least header and not bigger then remaining packet
        if (length < 4 || length > len - n) {
#ifdef DEBUG_PACKET
            LOG("BAD PACKET: Wrong len %d", length);
#endif
            return MOLOCH_PACKET_CORRUPT;
        }

        if (data[n+3] == 0 && data[n+2] == 9) {
            if (data[0] == AF_INET) {
                return moloch_packet_ip4(batch, packet, data+n+4, length - 4);
            } else {
                return moloch_packet_ip6(batch, packet, data+n+4, length - 4);
            }
        } else {
            n += ((length + 3) & 0xfffffc);
        }
    }
#ifdef DEBUG_PACKET
    LOG("BAD PACKET: Not sure");
#endif
    return MOLOCH_PACKET_CORRUPT;
}
/******************************************************************************/
LOCAL int moloch_packet_radiotap(MolochPacketBatch_t * batch, MolochPacket_t * const packet, const uint8_t *data, int len)
{
    if (data[0] != 0 || len < 36)
        return MOLOCH_PACKET_UNKNOWN;

    int hl = packet->pkt[2];
    if (hl + 24 + 8 >= len)
        return MOLOCH_PACKET_UNKNOWN;

    if (data[hl] != 8)
        return MOLOCH_PACKET_UNKNOWN;

    hl += 24 + 3;

    if (data[hl] != 0 || data[hl+1] != 0 || data[hl+2] != 0)
        return MOLOCH_PACKET_UNKNOWN;

    hl += 3;

    if (data[hl] == 0x08 && data[hl+1] == 0x00) {
        hl += 2;
        return moloch_packet_ip4(batch, packet, data+hl, len - hl);
    }
    if (data[hl] == 0x86 && data[hl+1] == 0xdd) {
        hl += 2;
        return moloch_packet_ip6(batch, packet, data+hl, len - hl);
    }
    return MOLOCH_PACKET_UNKNOWN;
}
/******************************************************************************/
void moloch_packet_batch_init(MolochPacketBatch_t *batch)
{
    int t;

    for (t = 0; t < config.packetThreads; t++) {
        DLL_INIT(packet_, &batch->packetQ[t]);
    }
    batch->count = 0;
}
/******************************************************************************/
void moloch_packet_batch_flush(MolochPacketBatch_t *batch)
{
    int t;

    for (t = 0; t < config.packetThreads; t++) {
        if (DLL_COUNT(packet_, &batch->packetQ[t]) > 0) {
            MOLOCH_LOCK(packetQ[t].lock);
            DLL_PUSH_TAIL_DLL(packet_, &packetQ[t], &batch->packetQ[t]);
            MOLOCH_COND_SIGNAL(packetQ[t].lock);
            MOLOCH_UNLOCK(packetQ[t].lock);
        }
    }
    batch->count = 0;
}
/******************************************************************************/
void moloch_packet_batch(MolochPacketBatch_t * batch, MolochPacket_t * const packet)
{
    int rc;

#ifdef DEBUG_PACKET
    LOG("enter %p %d %d", packet, pcapFileHeader.linktype, packet->pktlen);
#endif

    switch(pcapFileHeader.linktype) {
    case 0: // NULL
        if (packet->pktlen > 4)
            rc = moloch_packet_ip4(batch, packet, packet->pkt+4, packet->pktlen-4);
        else {
#ifdef DEBUG_PACKET
            LOG("BAD PACKET: Too short %d", packet->pktlen);
#endif
            rc = MOLOCH_PACKET_CORRUPT;
        }
        break;
    case 1: // Ether
        rc = moloch_packet_ether(batch, packet, packet->pkt, packet->pktlen);
        break;
    case 12: // LOOP
    case 101: // RAW
        rc = moloch_packet_ip4(batch, packet, packet->pkt, packet->pktlen);
        break;
    case 107: // Frame Relay
        rc = moloch_packet_frame_relay(batch, packet, packet->pkt, packet->pktlen);
        break;
    case 113: // SLL
        if (packet->pkt[0] == 0 && packet->pkt[1] <= 4)
            rc = moloch_packet_sll(batch, packet, packet->pkt, packet->pktlen);
        else
            rc = moloch_packet_ip4(batch, packet, packet->pkt, packet->pktlen);
        break;
    case 127: // radiotap
        rc = moloch_packet_radiotap(batch, packet, packet->pkt, packet->pktlen);
        break;
    case 239: // NFLOG
        rc = moloch_packet_nflog(batch, packet, packet->pkt, packet->pktlen);
        break;
    default:
        LOGEXIT("ERROR - Unsupported pcap link type %d", pcapFileHeader.linktype);
    }
    MOLOCH_THREAD_INCR(packetStats[rc]);

    if (rc) {
        moloch_packet_free(packet);
    }
}
/******************************************************************************/
int moloch_packet_outstanding()
{
    int count = 0;
    int t;

    for (t = 0; t < config.packetThreads; t++) {
        count += DLL_COUNT(packet_, &packetQ[t]);
        count += inProgress[t];
    }
    return count;
}
/******************************************************************************/
LOCAL uint32_t moloch_packet_frag_hash(const void *key)
{
    int i;
    uint32_t n = 0;
    for (i = 0; i < 10; i++) {
        n = (n << 5) - n + ((char*)key)[i];
    }
    return n;
}
/******************************************************************************/
LOCAL int moloch_packet_frag_cmp(const void *keyv, const void *elementv)
{
    MolochFrags_t *element = (MolochFrags_t *)elementv;

    return memcmp(keyv, element->key, 10) == 0;
}
/******************************************************************************/
LOCAL gboolean moloch_packet_save_drophash(gpointer UNUSED(user_data))
{
    if (packetDrop4.changed)
        moloch_drophash_save(&packetDrop4);

    if (packetDrop6.changed)
        moloch_drophash_save(&packetDrop6);

    return TRUE;
}
/******************************************************************************/
void moloch_packet_init()
{
    pcapFileHeader.magic = 0xa1b2c3d4;
    pcapFileHeader.version_major = 2;
    pcapFileHeader.version_minor = 4;

    pcapFileHeader.thiszone = 0;
    pcapFileHeader.sigfigs = 0;

    char filename[PATH_MAX];
    snprintf(filename, sizeof(filename), "/tmp/%s.tcp.drops.4", config.nodeName);
    moloch_drophash_init(&packetDrop4, filename, TRUE);

    snprintf(filename, sizeof(filename), "/tmp/%s.tcp.drops.6", config.nodeName);
    moloch_drophash_init(&packetDrop6, filename, FALSE);
    g_timeout_add_seconds(10, moloch_packet_save_drophash, 0);

    mac1Field = moloch_field_define("general", "lotermfield",
        "mac.src", "Src MAC", "srcMac",
        "Source ethernet mac addresses set for session",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);

    mac2Field = moloch_field_define("general", "lotermfield",
        "mac.dst", "Dst MAC", "dstMac",
        "Destination ethernet mac addresses set for session",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);

    moloch_field_define("general", "lotermfield",
        "mac", "Src or Dst MAC", "macall",
        "Shorthand for mac.src or mac.dst",
        0,  MOLOCH_FIELD_FLAG_FAKE,
        "regex", "^mac\\\\.(?:(?!\\\\.cnt$).)*$",
        NULL);

    oui1Field = moloch_field_define("general", "termfield",
        "oui.src", "Src OUI", "srcOui",
        "Source ethernet oui set for session",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);

    oui2Field = moloch_field_define("general", "termfield",
        "oui.dst", "Dst OUI", "dstOui",
        "Destination ethernet oui set for session",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);


    vlanField = moloch_field_define("general", "integer",
        "vlan", "VLan", "vlan",
        "vlan value",
        MOLOCH_FIELD_TYPE_INT_GHASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);

    greIpField = moloch_field_define("general", "ip",
        "gre.ip", "GRE IP", "greIp",
        "GRE ip addresses for session",
        MOLOCH_FIELD_TYPE_IP_GHASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);

    moloch_field_define("general", "integer",
        "tcpflags.syn", "TCP Flag SYN", "tcpflags.syn",
        "Count of packets with SYN and no ACK flag set",
        0,  MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    moloch_field_define("general", "integer",
        "tcpflags.syn-ack", "TCP Flag SYN-ACK", "tcpflags.syn-ack",
        "Count of packets with SYN and ACK flag set",
        0,  MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    moloch_field_define("general", "integer",
        "tcpflags.ack", "TCP Flag ACK", "tcpflags.ack",
        "Count of packets with only the ACK flag set",
        0,  MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    moloch_field_define("general", "integer",
        "tcpflags.psh", "TCP Flag PSH", "tcpflags.psh",
        "Count of packets with PSH flag set",
        0,  MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    moloch_field_define("general", "integer",
        "tcpflags.fin", "TCP Flag FIN", "tcpflags.fin",
        "Count of packets with FIN flag set",
        0,  MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    moloch_field_define("general", "integer",
        "tcpflags.rst", "TCP Flag RST", "tcpflags.rst",
        "Count of packets with RST flag set",
        0,  MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    moloch_field_define("general", "integer",
        "tcpflags.urg", "TCP Flag URG", "tcpflags.urg",
        "Count of packets with URG flag set",
        0,  MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    icmpTypeField = moloch_field_define("general", "integer",
        "icmp.type", "ICMP Type", "icmp.type",
        "ICMP type field values",
        MOLOCH_FIELD_TYPE_INT_GHASH, 0,
        NULL);

    icmpCodeField = moloch_field_define("general", "integer",
        "icmp.code", "ICMP Code", "icmp.code",
        "ICMP code field values",
        MOLOCH_FIELD_TYPE_INT_GHASH, 0,
        NULL);

    moloch_field_define("general", "integer",
        "packets.src", "Src Packets", "srcPackets",
        "Total number of packets sent by source in a session",
        0,  MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    moloch_field_define("general", "integer",
        "packets.dst", "Dst Packets", "dstPackets",
        "Total number of packets sent by destination in a session",
        0,  MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    int t;
    for (t = 0; t < config.packetThreads; t++) {
        char name[100];
        DLL_INIT(packet_, &packetQ[t]);
        MOLOCH_LOCK_INIT(packetQ[t].lock);
        MOLOCH_COND_INIT(packetQ[t].lock);
        snprintf(name, sizeof(name), "moloch-pkt%d", t);
        g_thread_new(name, &moloch_packet_thread, (gpointer)(long)t);
    }

    HASH_INIT(fragh_, fragsHash, moloch_packet_frag_hash, moloch_packet_frag_cmp);
    DLL_INIT(fragl_, &fragsList);

    moloch_add_can_quit(moloch_packet_outstanding, "packet outstanding");
    moloch_add_can_quit(moloch_packet_frags_outstanding, "packet frags outstanding");
    maxTcpOutOfOrderPackets = moloch_config_int(NULL, "maxTcpOutOfOrderPackets", 256, 64, 10000);
}
/******************************************************************************/
uint64_t moloch_packet_dropped_packets()
{
    MolochReaderStats_t stats;
    if (moloch_reader_stats(&stats)) {
        return 0;
    }
    return stats.dropped - initialDropped;
}
/******************************************************************************/
uint64_t moloch_packet_dropped_frags()
{
    return droppedFrags;
}
/******************************************************************************/
uint64_t moloch_packet_dropped_overload()
{
    uint64_t count = 0;

    int t;

    for (t = 0; t < config.packetThreads; t++) {
        count += overloadDrops[t];
    }
    return count;
}
/******************************************************************************/
uint64_t moloch_packet_total_bytes()
{
    uint64_t count = 0;

    int t;

    for (t = 0; t < config.packetThreads; t++) {
        count += totalBytes[t];
    }
    return count;
}
/******************************************************************************/
void moloch_packet_add_packet_ip(char *ipstr, int mode)
{
    patricia_node_t *node;
    if (strchr(ipstr, '.') != 0) {
        if (!ipTree4)
            ipTree4 = New_Patricia(32);
        node = make_and_lookup(ipTree4, ipstr);
    } else {
        if (!ipTree6)
            ipTree6 = New_Patricia(128);
        node = make_and_lookup(ipTree6, ipstr);
    }
    node->data = (void *)(long)mode;
}
/******************************************************************************/
void moloch_packet_set_linksnap(int linktype, int snaplen)
{
    pcapFileHeader.linktype = linktype;
    pcapFileHeader.snaplen = snaplen;
    moloch_rules_recompile();
}
/******************************************************************************/
void moloch_packet_drophash_add(MolochSession_t *session, int which, int min)
{
    if (session->ses != SESSION_TCP)
        return;

    // packetDrop is kept in network byte order
    const int port = (which == 0)?htons(session->port1):htons(session->port2);

    if (MOLOCH_SESSION_v6(session)) {
        if (which == 0) {
            moloch_drophash_add(&packetDrop6, port, (void*)&session->addr1, session->lastPacket.tv_sec, min*60);
        } else {
            moloch_drophash_add(&packetDrop6, port, (void*)&session->addr2, session->lastPacket.tv_sec, min*60);
        }
    } else {
        if (which == 0) {
            moloch_drophash_add(&packetDrop4, port, &((uint32_t *)session->addr1.s6_addr)[3], session->lastPacket.tv_sec, min*60);
        } else {
            moloch_drophash_add(&packetDrop4, port, &((uint32_t *)session->addr2.s6_addr)[3], session->lastPacket.tv_sec, min*60);
        }
    }
}
/******************************************************************************/
void moloch_packet_exit()
{
    if (ipTree4) {
        Destroy_Patricia(ipTree4, NULL);
        ipTree4 = 0;
    }

    if (ipTree6) {
        Destroy_Patricia(ipTree6, NULL);
        ipTree6 = 0;
    }
    moloch_packet_log(SESSION_TCP);
    if (unknownPacketFile[0])
        fclose(unknownPacketFile[0]);
    if (unknownPacketFile[1])
        fclose(unknownPacketFile[1]);
}
