/* packet.c  -- Functions for acquiring data
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"
#include "patricia.h"
#include <inttypes.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <errno.h>
#include "pcap.h"

//#define DEBUG_PACKET

/******************************************************************************/
extern ArkimeConfig_t        config;

ArkimePcapFileHdr_t          pcapFileHeader;

uint64_t                     totalPackets;
LOCAL uint64_t               totalBytes[ARKIME_MAX_PACKET_THREADS];

LOCAL uint64_t               initialDropped = 0;
struct timeval               initialPacket; // Don't make LOCAL for now because of netflow plugin

extern void                 *esServer;
extern uint32_t              pluginsCbs;

uint64_t                     writtenBytes;
uint64_t                     unwrittenBytes;

int                          mac1Field;
int                          mac2Field;
int                          vlanField;
int                          vniField;
LOCAL int                    oui1Field;
LOCAL int                    oui2Field;
LOCAL int                    outermac1Field;
LOCAL int                    outermac2Field;
LOCAL int                    outeroui1Field;
LOCAL int                    outeroui2Field;
LOCAL int                    outerip1Field;
LOCAL int                    outerip2Field;
LOCAL int                    dscpField[2];

LOCAL uint64_t               droppedFrags;

time_t                       currentTime[ARKIME_MAX_PACKET_THREADS];
time_t                       lastPacketSecs[ARKIME_MAX_PACKET_THREADS];
LOCAL int                    inProgress[ARKIME_MAX_PACKET_THREADS];

LOCAL patricia_tree_t       *ipTree4 = 0;
LOCAL patricia_tree_t       *ipTree6 = 0;
LOCAL patricia_tree_t       *newipTree4 = 0;
LOCAL patricia_tree_t       *newipTree6 = 0;

extern ArkimeFieldOps_t      readerFieldOps[256];

LOCAL ArkimePacketEnqueue_cb udpPortCbs[0x10000];
LOCAL ArkimePacketEnqueue_cb ethernetCbs[0x10000];
LOCAL ArkimePacketEnqueue_cb ipCbs[ARKIME_IPPROTO_MAX];

int                          tcpMProtocol;
int                          udpMProtocol;

LOCAL int                    mProtocolCnt;
ArkimeProtocol_t             mProtocols[0x100];

/******************************************************************************/

uint64_t                     packetStats[ARKIME_PACKET_MAX];

/******************************************************************************/
LOCAL  ArkimePacketHead_t    packetQ[ARKIME_MAX_PACKET_THREADS];
LOCAL  uint32_t              overloadDrops[ARKIME_MAX_PACKET_THREADS];
LOCAL  uint32_t              overloadDropTimes[ARKIME_MAX_PACKET_THREADS];

LOCAL  ARKIME_LOCK_DEFINE(frags);

LOCAL ArkimePacketRC arkime_packet_ip4(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len);
LOCAL ArkimePacketRC arkime_packet_ip6(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len);
LOCAL ArkimePacketRC arkime_packet_frame_relay(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len);
LOCAL ArkimePacketRC arkime_packet_ether(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len);

typedef struct arkimefrags_t {
    struct arkimefrags_t  *fragh_next, *fragh_prev;
    struct arkimefrags_t  *fragl_next, *fragl_prev;
    uint32_t               fragh_bucket;
    uint32_t               fragh_hash;
    ArkimePacketHead_t     packets;
    char                   key[10];
    uint32_t               secs;
    char                   haveNoFlags;
} ArkimeFrags_t;

typedef struct {
    struct arkimefrags_t  *fragh_next, *fragh_prev;
    struct arkimefrags_t  *fragl_next, *fragl_prev;
    short                  fragh_bucket;
    uint32_t               fragh_count;
    uint32_t               fragl_count;
} ArkimeFragsHead_t;

typedef HASH_VAR(h_, ArkimeFragsHash_t, ArkimeFragsHead_t, 199337);

LOCAL ArkimeFragsHash_t          fragsHash;
LOCAL ArkimeFragsHead_t          fragsList;

// These are in network byte order
LOCAL ArkimeDropHashGroup_t      packetDrop4;
LOCAL ArkimeDropHashGroup_t      packetDrop6;
LOCAL ArkimeDropHashGroup_t      packetDrop4S;
LOCAL ArkimeDropHashGroup_t      packetDrop6S;

#ifndef IPPROTO_IPV4
#define IPPROTO_IPV4            4
#endif

/******************************************************************************/
void arkime_packet_free(ArkimePacket_t *packet)
{
    if (packet->copied) {
        free(packet->pkt);
    }
    packet->pkt = 0;
    ARKIME_TYPE_FREE(ArkimePacket_t, packet);
}
/******************************************************************************/
void arkime_packet_process_data(ArkimeSession_t *session, const uint8_t *data, int len, int which)
{
    int i;

    for (i = 0; i < session->parserNum; i++) {
        if (session->parserInfo[i].parserFunc) {
            int consumed = session->parserInfo[i].parserFunc(session, session->parserInfo[i].uw, data, len, which);
            if (consumed) {
                if (consumed == ARKIME_PARSER_UNREGISTER) {
                    if (session->parserInfo[i].parserFreeFunc) {
                        session->parserInfo[i].parserFreeFunc(session, session->parserInfo[i].uw);
                    }
                    memset(&session->parserInfo[i], 0, sizeof(session->parserInfo[i]));
                    continue;
                }
                session->consumed[which] += consumed;
            }

            if (consumed >= len)
                break;
        }
    }
}
/******************************************************************************/
void arkime_packet_thread_wake(int thread)
{
    ARKIME_LOCK(packetQ[thread].lock);
    ARKIME_COND_SIGNAL(packetQ[thread].lock);
    ARKIME_UNLOCK(packetQ[thread].lock);
}
/******************************************************************************/
/* Only called on main thread, we busy block until all packet threads are empty.
 * Should only be used by tests and at end
 */
void arkime_packet_flush()
{
    int flushed = 0;
    int t;
    while (!flushed) {
        flushed = !arkime_session_cmd_outstanding();

        for (t = 0; t < config.packetThreads; t++) {
            ARKIME_LOCK(packetQ[t].lock);
            if (DLL_COUNT(packet_, &packetQ[t]) > 0) {
                flushed = 0;
            }
            ARKIME_UNLOCK(packetQ[t].lock);
            usleep(10000);
        }
    }
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL void arkime_packet_process(ArkimePacket_t *packet, int thread)
{
#ifdef DEBUG_PACKET
    LOG("Processing %p %d", packet, packet->pktlen);
#endif

    lastPacketSecs[thread] = packet->ts.tv_sec;

    arkime_pq_run(thread, 10);

    ArkimeSession_t     *session;
    struct ip           *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
    struct ip6_hdr      *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);
    uint8_t              sessionId[ARKIME_SESSIONID_LEN];


    mProtocols[packet->mProtocol].createSessionId(sessionId, packet);

    // Try at most 2 times
    int isNew;

    for (int i = 0; i < 2; i++) {
        session = arkime_session_find_or_create(packet->mProtocol, packet->hash, sessionId, &isNew);

        if (isNew) {
            session->saveTime = packet->ts.tv_sec + config.tcpSaveTimeout;
            session->firstPacket = packet->ts;
            session->thread = thread;

            if (packet->ipProtocol) {
                session->ipProtocol = packet->ipProtocol;
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
            }
        }

        int rc = mProtocols[packet->mProtocol].preProcess(session, packet, isNew);

        // Close out the old session and create a new one
        if (rc == 1) {
            void arkime_session_save(ArkimeSession_t *session);
            arkime_session_save(session);
            continue;
        }
        break;
    }

    if (session->stopSPI) {
        arkime_packet_free(packet);
        return;
    }

    if (isNew) {
        arkime_parsers_initial_tag(session);
        if (readerFieldOps[packet->readerPos].num)
            arkime_field_ops_run(session, &readerFieldOps[packet->readerPos]);

        if (pluginsCbs & ARKIME_PLUGIN_NEW)
            arkime_plugins_cb_new(session);

        arkime_rules_session_create(session);
    }

    /* Check if the stop saving bpf filters match */
    if (session->packets[packet->direction] == 0 && session->stopSaving == 0xffff) {
        arkime_rules_run_session_setup(session, packet);
    }

    session->packets[packet->direction]++;
    session->bytes[packet->direction] += packet->pktlen;
    session->lastPacket = packet->ts;

    uint32_t packets = session->packets[0] + session->packets[1];

    if (packets <= session->stopSaving) {
        arkime_writer_write(session, packet);

        // If writerFilePos is 0, then the writer couldn't save the packet
        if (packet->writerFilePos == 0) {
            if (!session->diskOverload) {
                arkime_session_add_tag(session, "pcap-disk-overload");
                session->diskOverload = 1;
            }
            ARKIME_THREAD_INCR_NUM(unwrittenBytes, packet->pktlen);
        } else {

            ARKIME_THREAD_INCR_NUM(writtenBytes, packet->pktlen);

            // If the last fileNum used in the session isn't the same as the
            // lastest packets fileNum then we need to add to the filePos and
            // fileNum arrays.
            uint16_t len;
            if (session->lastFileNum != packet->writerFileNum) {
                session->lastFileNum = packet->writerFileNum;
                g_array_append_val(session->fileNumArray, packet->writerFileNum);
                int64_t pos = -1LL * packet->writerFileNum;
                g_array_append_val(session->filePosArray, pos);

                if (config.enablePacketLen) {
                    len = 0;
                    g_array_append_val(session->fileLenArray, len);
                }
            }

            g_array_append_val(session->filePosArray, packet->writerFilePos);

            if (config.enablePacketLen) {
                len = 16 + packet->pktlen;
                g_array_append_val(session->fileLenArray, len);
            }
        }

        if (packets >= config.maxPackets || session->midSave) {
            arkime_session_mid_save(session, packet->ts.tv_sec);
        }
    } else {
        // If we hit stopSaving for this session and try and save 1 more packet then
        // add truncated-pcap tag to the session
        if (packets - 1 == session->stopSaving) {
            arkime_session_set_stop_saving(session);
        }
        ARKIME_THREAD_INCR_NUM(unwrittenBytes, packet->pktlen);
    }

    // Check the first 10 packets for dscp, vlans, tunnels, and macs
    if (session->packets[packet->direction] <= 10) {
        const uint8_t *pcapData = packet->pkt;

        if (packet->ipProtocol) {
            int tc = ip4->ip_v == 4 ? ip4->ip_tos >> 2 : ip6->ip6_vfc & 0xf;
            if (tc != 0) {
                arkime_field_int_add(dscpField[packet->direction], session, tc);
            }
        }

        if (pcapFileHeader.dlt == DLT_EN10MB) {
            if (packet->direction == 1) {
                arkime_field_macoui_add(session, mac1Field, oui1Field, packet->pkt + packet->etherOffset);
                arkime_field_macoui_add(session, mac2Field, oui2Field, packet->pkt + packet->etherOffset + 6);
            } else {
                arkime_field_macoui_add(session, mac1Field, oui1Field, packet->pkt + packet->etherOffset + 6);
                arkime_field_macoui_add(session, mac2Field, oui2Field, packet->pkt + packet->etherOffset);
            }

            int n = 12;
            while (pcapData[n] == 0x81 && pcapData[n + 1] == 0x00) {
                uint16_t vlan = ((uint16_t)(pcapData[n + 2] << 8 | pcapData[n + 3])) & 0xfff;
                arkime_field_int_add(vlanField, session, vlan);
                n += 4;
            }
        }

        if (packet->vlan)
            arkime_field_int_add(vlanField, session, packet->vlan);

        if (packet->vni)
            arkime_field_int_add(vniField, session, packet->vni);

        if (packet->etherOffset != 0 && packet->outerEtherOffset != packet->etherOffset) {
            arkime_field_macoui_add(session, outermac1Field, outeroui1Field, packet->pkt + packet->outerEtherOffset);
            arkime_field_macoui_add(session, outermac2Field, outeroui2Field, packet->pkt + packet->outerEtherOffset + 6);
        }
        if(packet->outerIpOffset != 0 && packet->outerIpOffset != packet->ipOffset) {
            if (packet->outerv6 == 0) {
                ip4 = (struct ip *) (packet->pkt + packet->outerIpOffset);
                arkime_field_ip4_add(outerip1Field, session, ip4->ip_src.s_addr);
                arkime_field_ip4_add(outerip2Field, session, ip4->ip_dst.s_addr);
            }
            else {
                ip6 = (struct ip6_hdr *) (packet->pkt + packet->outerIpOffset);
                arkime_field_ip6_add(outerip1Field, session, ip6->ip6_src.s6_addr);
                arkime_field_ip6_add(outerip2Field, session, ip6->ip6_dst.s6_addr);
            }
        }

        if (packet->tunnel & ARKIME_PACKET_TUNNEL_GRE) {
            arkime_session_add_protocol(session, "gre");
        }

        if (packet->tunnel & ARKIME_PACKET_TUNNEL_PPPOE) {
            arkime_session_add_protocol(session, "pppoe");
        }

        if (packet->tunnel & ARKIME_PACKET_TUNNEL_PPP) {
            arkime_session_add_protocol(session, "ppp");
        }

        if (packet->tunnel & ARKIME_PACKET_TUNNEL_MPLS) {
            arkime_session_add_protocol(session, "mpls");
        }

        if (packet->tunnel & ARKIME_PACKET_TUNNEL_GTP) {
            arkime_session_add_protocol(session, "gtp");
        }

        if (packet->tunnel & ARKIME_PACKET_TUNNEL_VXLAN) {
            arkime_session_add_protocol(session, "vxlan");
        }

        if (packet->tunnel & ARKIME_PACKET_TUNNEL_VXLAN_GPE) {
            arkime_session_add_protocol(session, "vxlan-gpe");
        }

        if (packet->tunnel & ARKIME_PACKET_TUNNEL_GENEVE) {
            arkime_session_add_protocol(session, "geneve");
        }


    }

    if (mProtocols[packet->mProtocol].process) {
        // If there is a process callback, call and determine if we free the packet.

        if (mProtocols[packet->mProtocol].process(session, packet))
            arkime_packet_free(packet);

    } else {
        // No process callback, always free

        arkime_packet_free(packet);
    }
}
/******************************************************************************/
#ifndef FUZZLOCH
LOCAL void *arkime_packet_thread(void *threadp)
{
    int thread = (long)threadp;
    const uint32_t maxPackets75 = config.maxPackets * 0.75;
    uint32_t skipCount = 0;

    while (1) {
        ArkimePacket_t  *packet;

        ARKIME_LOCK(packetQ[thread].lock);
        inProgress[thread] = 0;
        if (DLL_COUNT(packet_, &packetQ[thread]) == 0) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME_COARSE, &ts);
            currentTime[thread] = ts.tv_sec;
            ts.tv_sec++;
            ARKIME_COND_TIMEDWAIT(packetQ[thread].lock, ts);

            /* If we are in live capture mode and we haven't received any packets for 10 seconds we set current time to 10
             * seconds in the past so arkime_session_process_commands will clean things up.  10 seconds is arbitrary but
             * we want to make sure we don't set the time ahead of any packets that are currently being read off the wire
             */
            if (!config.pcapReadOffline && DLL_COUNT(packet_, &packetQ[thread]) == 0 && ts.tv_sec - 10 > lastPacketSecs[thread]) {
                lastPacketSecs[thread] = ts.tv_sec - 10;
            }
        }
        inProgress[thread] = 1;
        DLL_POP_HEAD(packet_, &packetQ[thread], packet);
        ARKIME_UNLOCK(packetQ[thread].lock);

        // Only process commands if the packetQ is less then 75% full or every 8 packets
        if (likely(DLL_COUNT(packet_, &packetQ[thread]) < maxPackets75) || (skipCount & 0x7) == 0) {
            arkime_session_process_commands(thread);
            if (!packet)
                continue;
        } else {
            skipCount++;
        }
        arkime_packet_process(packet, thread);
    }

    return NULL;
}
#endif
/******************************************************************************/
static FILE *unknownPacketFile[3];
LOCAL void arkime_packet_save_unknown_packet(int type, ArkimePacket_t *const packet)
{
    static ARKIME_LOCK_DEFINE(lock);

    struct arkime_pcap_sf_pkthdr hdr;
    hdr.ts.tv_sec  = packet->ts.tv_sec;
    hdr.ts.tv_usec = packet->ts.tv_usec;
    hdr.caplen     = packet->pktlen;
    hdr.pktlen     = packet->pktlen;

    ARKIME_LOCK(lock);
    if (!unknownPacketFile[type]) {
        char               str[PATH_MAX];
        static const char *names[] = {"unknown.ether", "unknown.ip", "corrupt"};

        snprintf(str, sizeof(str), "%s/%s.%d.pcap", config.pcapDir[0], names[type], getpid());
        unknownPacketFile[type] = fopen(str, "w");

        // TODO-- should we also add logic to pick right pcapDir when there are multiple?
        if (unknownPacketFile[type] == NULL) {
            LOGEXIT("ERROR - Unable to open pcap file %s to store unknown type %s.  Error %s", str, names[type], strerror (errno));
            ARKIME_UNLOCK(lock);
            return;
        }

        fwrite(&pcapFileHeader, 24, 1, unknownPacketFile[type]);
    }

    fwrite(&hdr, 16, 1, unknownPacketFile[type]);
    fwrite(packet->pkt, packet->pktlen, 1, unknownPacketFile[type]);
    ARKIME_UNLOCK(lock);
}

/******************************************************************************/
void arkime_packet_frags_free(ArkimeFrags_t *const frags)
{
    ArkimePacket_t *packet;

    while (DLL_POP_HEAD(packet_, &frags->packets, packet)) {
        arkime_packet_free(packet);
    }
    HASH_REMOVE(fragh_, fragsHash, frags);
    DLL_REMOVE(fragl_, &fragsList, frags);
    ARKIME_TYPE_FREE(ArkimeFrags_t, frags);
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL gboolean arkime_packet_frags_process(ArkimePacket_t *const packet)
{
    ArkimePacket_t *fpacket;
    ArkimeFrags_t   *frags;
    char             key[10];

    struct ip *const ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
    memcpy(key, &ip4->ip_src.s_addr, 4);
    memcpy(key + 4, &ip4->ip_dst.s_addr, 4);
    memcpy(key + 8, &ip4->ip_id, 2);

    HASH_FIND(fragh_, fragsHash, key, frags);

    if (!frags) {
        frags = ARKIME_TYPE_ALLOC0(ArkimeFrags_t);
        memcpy(frags->key, key, 10);
        frags->secs = packet->ts.tv_sec;
        HASH_ADD(fragh_, fragsHash, key, frags);
        DLL_PUSH_TAIL(fragl_, &fragsList, frags);
        DLL_INIT(packet_, &frags->packets);
        DLL_PUSH_TAIL(packet_, &frags->packets, packet);

        if (DLL_COUNT(fragl_, &fragsList) > config.maxFrags) {
            droppedFrags++;
            arkime_packet_frags_free(DLL_PEEK_HEAD(fragl_, &fragsList));
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
        struct ip *fip4 = (struct ip *)(fpacket->pkt + fpacket->ipOffset);
        uint16_t fip_off = ntohs(fip4->ip_off) & IP_OFFMASK;
        if (ip_off >= fip_off) {
            DLL_ADD_AFTER(packet_, &frags->packets, fpacket, packet);
            break;
        }
    }
    if ((void * )fpacket == (void * )&frags->packets) {
        DLL_PUSH_HEAD(packet_, &frags->packets, packet);
    }

    if (DLL_COUNT(packet_, &frags->packets) > 50) {
        droppedFrags++;
        arkime_packet_frags_free(frags);
        return FALSE;
    }

    // Don't bother checking until we get a packet with no flags
    if (!frags->haveNoFlags) {
        return FALSE;
    }

    int off = 0;
    struct ip *fip4;

    int payloadLen = 0;
    DLL_FOREACH(packet_, &frags->packets, fpacket) {
        fip4 = (struct ip *)(fpacket->pkt + fpacket->ipOffset);
        uint16_t fip_off = ntohs(fip4->ip_off) & IP_OFFMASK;
        if (fip_off != off)
            break;
        off += fpacket->payloadLen / 8;
        payloadLen = MAX(payloadLen, fip_off * 8 + fpacket->payloadLen);
    }
    // We have a hole
    if ((void * )fpacket != (void * )&frags->packets) {
        return FALSE;
    }

    // Packet is too large, hacker
    if (payloadLen + packet->payloadOffset >= ARKIME_PACKET_MAX_LEN) {
        droppedFrags++;
        arkime_packet_frags_free(frags);
        return FALSE;
    }

    // Now alloc the full packet
    packet->pktlen = packet->payloadOffset + payloadLen;
    uint8_t *pkt = malloc(packet->pktlen);

    // Copy packet header
    memcpy(pkt, packet->pkt, packet->payloadOffset);

    // Fix header of new packet
    fip4 = (struct ip *)(pkt + packet->ipOffset);
    fip4->ip_len = htons(payloadLen + 4 * ip4->ip_hl);
    fip4->ip_off = 0;

    // Copy payload
    DLL_FOREACH(packet_, &frags->packets, fpacket) {
        fip4 = (struct ip *)(fpacket->pkt + fpacket->ipOffset);
        uint16_t fip_off = ntohs(fip4->ip_off) & IP_OFFMASK;

        if (packet->payloadOffset + (fip_off * 8) + fpacket->payloadLen <= packet->pktlen)
            memcpy(pkt + packet->payloadOffset + (fip_off * 8), fpacket->pkt + fpacket->payloadOffset, fpacket->payloadLen);
        else
            LOG("WARNING - Not enough room for frag %d > %d", packet->payloadOffset + (fip_off * 8) + fpacket->payloadLen, packet->pktlen);
    }

    // Set all the vars in the current packet to new defraged packet
    if (packet->copied)
        free(packet->pkt);
    packet->pkt = pkt;
    packet->copied = 1;
    packet->wasfrag = 1;
    packet->payloadLen = payloadLen;
    DLL_REMOVE(packet_, &frags->packets, packet); // Remove from list so we don't get freed in frags_free
    arkime_packet_frags_free(frags);
    return TRUE;
}
/******************************************************************************/
LOCAL void arkime_packet_frags4(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet)
{
    ArkimeFrags_t *frags;

    // ALW - Should change frags_process to make the copy when needed
    if (!packet->copied) {
        uint8_t *pkt = malloc(packet->pktlen);
        memcpy(pkt, packet->pkt, packet->pktlen);
        packet->pkt = pkt;
        packet->copied = 1;
    }

    ARKIME_LOCK(frags);
    // Remove expired entries
    while ((frags = DLL_PEEK_HEAD(fragl_, &fragsList)) && (frags->secs + config.fragsTimeout < packet->ts.tv_sec)) {
        droppedFrags++;
        arkime_packet_frags_free(frags);
    }

    gboolean process = arkime_packet_frags_process(packet);
    ARKIME_UNLOCK(frags);

    if (process)
        arkime_packet_batch(batch, packet);
}
/******************************************************************************/
int arkime_packet_frags_size()
{
    return DLL_COUNT(fragl_, &fragsList);
}
/******************************************************************************/
int arkime_packet_frags_outstanding()
{
    return 0;
}
/******************************************************************************/
LOCAL void arkime_packet_log(SessionTypes ses)
{
    ArkimeReaderStats_t stats;
    if (arkime_reader_stats(&stats)) {
        stats.dropped = 0;
        stats.total = totalPackets;
    }

    uint32_t wql = arkime_writer_queue_length();

    LOG("packets: %" PRIu64 " current sessions: %u/%u oldest: %d - recv: %" PRIu64 " drop: %" PRIu64 " (%0.2f) queue: %d disk: %d packet: %d close: %d ns: %d frags: %d/%d pstats: %" PRIu64 "/%" PRIu64 "/%" PRIu64 "/%" PRIu64 "/%" PRIu64 "/%" PRIu64 "/%" PRIu64,
        totalPackets,
        arkime_session_watch_count(ses),
        arkime_session_monitoring(),
        arkime_session_idle_seconds(ses),
        stats.total,
        stats.dropped - initialDropped,
        (stats.total ? (stats.dropped - initialDropped) * (double)100.0 / stats.total : 0),
        arkime_http_queue_length(esServer),
        wql,
        arkime_packet_outstanding(),
        arkime_session_close_outstanding(),
        arkime_session_need_save_outstanding(),
        arkime_packet_frags_outstanding(),
        arkime_packet_frags_size(),
        packetStats[ARKIME_PACKET_DO_PROCESS],
        packetStats[ARKIME_PACKET_IP_DROPPED],
        packetStats[ARKIME_PACKET_OVERLOAD_DROPPED],
        packetStats[ARKIME_PACKET_CORRUPT],
        packetStats[ARKIME_PACKET_UNKNOWN],
        packetStats[ARKIME_PACKET_IPPORT_DROPPED],
        packetStats[ARKIME_PACKET_DUPLICATE_DROPPED]
       );

    if (config.debug > 0) {
        arkime_rules_stats();
    }
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL ArkimePacketRC arkime_packet_ip4(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    struct ip           *ip4 = (struct ip *)data;
    struct tcphdr       *tcphdr = 0;
    struct udphdr       *udphdr = 0;
    uint8_t              sessionId[ARKIME_SESSIONID_LEN];

#ifdef DEBUG_PACKET
    LOG("enter %p %p %d", packet, data, len);
#endif

    if (len < (int)sizeof(struct ip)) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: too small for header %p %d", packet, len);
#endif
        return ARKIME_PACKET_CORRUPT;
    }

    if (ip4->ip_v != 4) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: ip4->ip_v4 %d != 4", ip4->ip_v);
#endif
        return ARKIME_PACKET_CORRUPT;
    }

    int ip_len = ntohs(ip4->ip_len);
    if (len < ip_len) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: incomplete %p %d %d", packet, len, ip_len);
#endif
        return ARKIME_PACKET_CORRUPT;
    }

    int ip_hdr_len = 4 * ip4->ip_hl;
    if (ip_hdr_len < 4 * 5 || len < ip_hdr_len || ip_len < ip_hdr_len) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: too small for header and options %p %d %d", packet, len, ip_hdr_len);
#endif
        return ARKIME_PACKET_CORRUPT;
    }
    if (ipTree4) {
        patricia_node_t *node;

        if ((node = patricia_search_best3 (ipTree4, (u_char * )&ip4->ip_src, 32)) && node->data == NULL)
            return ARKIME_PACKET_IP_DROPPED;

        if ((node = patricia_search_best3 (ipTree4, (u_char * )&ip4->ip_dst, 32)) && node->data == NULL)
            return ARKIME_PACKET_IP_DROPPED;
    }

    if ((uint8_t *)data - packet->pkt >= 2048)
        return ARKIME_PACKET_CORRUPT;

    packet->outerv6 = packet->v6; // v6 will get reset
    packet->v6 = 0;
    packet->outerIpOffset = packet->ipOffset; // ipOffset will get reset
    packet->ipOffset = (uint8_t *)data - packet->pkt;
    packet->payloadOffset = packet->ipOffset + ip_hdr_len;
    packet->payloadLen = ip_len - ip_hdr_len;

    uint16_t ip_off = ntohs(ip4->ip_off);
    uint16_t ip_flags = ip_off & ~IP_OFFMASK;
    ip_off &= IP_OFFMASK;


    if ((ip_flags & IP_MF) || ip_off > 0) {
        arkime_packet_frags4(batch, packet);
        return ARKIME_PACKET_DONT_PROCESS_OR_FREE;
    }

    packet->mProtocol = 0;
    packet->ipProtocol = ip4->ip_p;
    switch (ip4->ip_p) {
    case IPPROTO_IPV4:
        return arkime_packet_ip4(batch, packet, data + ip_hdr_len, len - ip_hdr_len);
        break;
    case IPPROTO_TCP:
        if (len < ip_hdr_len + (int)sizeof(struct tcphdr)) {
#ifdef DEBUG_PACKET
            LOG("BAD PACKET: too small for tcp hdr %p %d", packet, len);
#endif
            return ARKIME_PACKET_CORRUPT;
        }

        tcphdr = (struct tcphdr *)((char *)ip4 + ip_hdr_len);

        if (packetDrop4.drops[tcphdr->th_sport] &&
            arkime_drophash_should_drop(&packetDrop4, tcphdr->th_sport, &ip4->ip_src.s_addr, packet->ts.tv_sec)) {

            return ARKIME_PACKET_IPPORT_DROPPED;
        }

        if (packetDrop4.drops[tcphdr->th_dport] &&
            arkime_drophash_should_drop(&packetDrop4, tcphdr->th_dport, &ip4->ip_dst.s_addr, packet->ts.tv_sec)) {

            return ARKIME_PACKET_IPPORT_DROPPED;
        }

        if (config.enablePacketDedup && arkime_dedup_should_drop(packet, ip_hdr_len + sizeof(struct tcphdr)))
            return ARKIME_PACKET_DUPLICATE_DROPPED;

        arkime_session_id(sessionId, ip4->ip_src.s_addr, tcphdr->th_sport,
                          ip4->ip_dst.s_addr, tcphdr->th_dport);
        packet->mProtocol = tcpMProtocol;

        const int dropPort = ((uint32_t)tcphdr->th_dport * (uint32_t)tcphdr->th_sport) & 0xffff;
        if (packetDrop4S.drops[dropPort] &&
            arkime_drophash_should_drop(&packetDrop4, dropPort, sessionId + 1, packet->ts.tv_sec)) {

            return ARKIME_PACKET_IPPORT_DROPPED;
        }

        break;
    case IPPROTO_UDP:
        if (len < ip_hdr_len + (int)sizeof(struct udphdr)) {
#ifdef DEBUG_PACKET
            LOG("BAD PACKET: too small for udp header %p %d", packet, len);
#endif
            return ARKIME_PACKET_CORRUPT;
        }

        udphdr = (struct udphdr *)((char *)ip4 + ip_hdr_len);

        if (len > ip_hdr_len + (int)sizeof(struct udphdr) + 8 && udpPortCbs[udphdr->uh_dport]) {
            int rc = udpPortCbs[udphdr->uh_dport](batch, packet, (uint8_t *)ip4 + ip_hdr_len + sizeof(struct udphdr *), len - ip_hdr_len - sizeof(struct udphdr *));
            if (rc != ARKIME_PACKET_UNKNOWN)
                return rc;

            // Reset state on UNKNOWN
            packet->v6 = 0;
            packet->ipOffset = (uint8_t *)data - packet->pkt;
            packet->payloadOffset = packet->ipOffset + ip_hdr_len;
            packet->payloadLen = ip_len - ip_hdr_len;
        }

        if (config.enablePacketDedup && arkime_dedup_should_drop(packet, ip_hdr_len + sizeof(struct udphdr)))
            return ARKIME_PACKET_DUPLICATE_DROPPED;

        arkime_session_id(sessionId, ip4->ip_src.s_addr, udphdr->uh_sport,
                          ip4->ip_dst.s_addr, udphdr->uh_dport);
        packet->mProtocol = udpMProtocol;
        break;
    case IPPROTO_IPV6:
        return arkime_packet_ip6(batch, packet, data + ip_hdr_len, len - ip_hdr_len);
    default:
        return arkime_packet_run_ip_cb(batch, packet, data + ip_hdr_len, len - ip_hdr_len, ip4->ip_p, "IP4");
    }
    packet->hash = arkime_session_hash(sessionId);
    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL ArkimePacketRC arkime_packet_ip6(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    struct ip6_hdr      *ip6 = (struct ip6_hdr *)data;
    struct tcphdr       *tcphdr = 0;
    struct udphdr       *udphdr = 0;
    uint8_t              sessionId[ARKIME_SESSIONID_LEN];

#ifdef DEBUG_PACKET
    LOG("enter %p %p %d", packet, data, len);
#endif

    if (len < (int)sizeof(struct ip6_hdr)) {
        return ARKIME_PACKET_CORRUPT;
    }

    int ip_len = ntohs(ip6->ip6_plen);
    if (len < ip_len) {
        return ARKIME_PACKET_CORRUPT;
    }

    // Corrupt ip6 header
    if ((ip6->ip6_vfc & 0xf0) != 0x60) {
        return ARKIME_PACKET_CORRUPT;
    }

    if (ipTree6) {
        patricia_node_t *node;

        if ((node = patricia_search_best3 (ipTree6, (u_char * )&ip6->ip6_src, 128)) && node->data == NULL)
            return ARKIME_PACKET_IP_DROPPED;

        if ((node = patricia_search_best3 (ipTree6, (u_char * )&ip6->ip6_dst, 128)) && node->data == NULL)
            return ARKIME_PACKET_IP_DROPPED;
    }

    int ip_hdr_len = sizeof(struct ip6_hdr);

    packet->outerv6 = packet->v6; // v6 will get reset
    packet->v6 = 1;
    packet->outerIpOffset = packet->ipOffset; // ipOffset will get reset
    packet->ipOffset = (uint8_t *)data - packet->pkt;
    packet->payloadOffset = packet->ipOffset + ip_hdr_len;

    if (ip_len + (int)sizeof(struct ip6_hdr) < ip_hdr_len) {
#ifdef DEBUG_PACKET
        LOG ("ERROR - %d + %ld < %d", ip_len, (long)sizeof(struct ip6_hdr), ip_hdr_len);
#endif
        return ARKIME_PACKET_CORRUPT;
    }
    packet->payloadLen = ip_len + sizeof(struct ip6_hdr) - ip_hdr_len;

    if (packet->pktlen < packet->payloadOffset + packet->payloadLen) {
#ifdef DEBUG_PACKET
        LOG ("ERROR - %d < %d + %d", packet->pktlen, packet->payloadOffset, packet->payloadLen);
#endif
        return ARKIME_PACKET_CORRUPT;
    }


    packet->mProtocol = 0;
    int nxt = ip6->ip6_nxt;
    int done = 0;

#ifdef DEBUG_PACKET
    LOG("Got ip6 header %p %d nxt: %d", packet, packet->pktlen, nxt);
#endif
    do {
        packet->ipProtocol = nxt;

        switch (nxt) {
        case IPPROTO_HOPOPTS:
        case IPPROTO_DSTOPTS:
        case IPPROTO_ROUTING:
            if (len < ip_hdr_len + 2) {
#ifdef DEBUG_PACKET
                LOG("ERROR - %d < %d + 2", len, ip_hdr_len);
#endif
                return ARKIME_PACKET_CORRUPT;
            }
            nxt = data[ip_hdr_len];
            ip_hdr_len += ((data[ip_hdr_len + 1] + 1) << 3);

            packet->payloadOffset = packet->ipOffset + ip_hdr_len;

            if (ip_len + (int)sizeof(struct ip6_hdr) < ip_hdr_len) {
#ifdef DEBUG_PACKET
                LOG ("ERROR - %d + %ld < %d", ip_len, (long)sizeof(struct ip6_hdr), ip_hdr_len);
#endif
                return ARKIME_PACKET_CORRUPT;
            }
            packet->payloadLen = ip_len + sizeof(struct ip6_hdr) - ip_hdr_len;

            if (packet->pktlen < packet->payloadOffset + packet->payloadLen) {
#ifdef DEBUG_PACKET
                LOG ("ERROR - %d < %d + %d", packet->pktlen, packet->payloadOffset, packet->payloadLen);
#endif
                return ARKIME_PACKET_CORRUPT;
            }

            break;
        case IPPROTO_FRAGMENT:
#ifdef DEBUG_PACKET
            LOG("ERROR - Don't support ip6 fragements yet!");
#endif
            return ARKIME_PACKET_UNKNOWN;

        case IPPROTO_TCP:
            if (len < ip_hdr_len + (int)sizeof(struct tcphdr)) {
                return ARKIME_PACKET_CORRUPT;
            }

            tcphdr = (struct tcphdr *)(data + ip_hdr_len);


            if (packetDrop6.drops[tcphdr->th_sport] &&
                arkime_drophash_should_drop(&packetDrop6, tcphdr->th_sport, &ip6->ip6_src, packet->ts.tv_sec)) {

                return ARKIME_PACKET_IPPORT_DROPPED;
            }

            if (packetDrop6.drops[tcphdr->th_dport] &&
                arkime_drophash_should_drop(&packetDrop6, tcphdr->th_dport, &ip6->ip6_dst, packet->ts.tv_sec)) {

                return ARKIME_PACKET_IPPORT_DROPPED;
            }

            if (config.enablePacketDedup && arkime_dedup_should_drop(packet, ip_hdr_len + sizeof(struct tcphdr)))
                return ARKIME_PACKET_DUPLICATE_DROPPED;

            arkime_session_id6(sessionId, ip6->ip6_src.s6_addr, tcphdr->th_sport,
                               ip6->ip6_dst.s6_addr, tcphdr->th_dport);

            const int dropPort = ((uint32_t)tcphdr->th_dport * (uint32_t)tcphdr->th_sport) & 0xffff;
            if (packetDrop6S.drops[dropPort] &&
                arkime_drophash_should_drop(&packetDrop6, dropPort, sessionId + 1, packet->ts.tv_sec)) {

                return ARKIME_PACKET_IPPORT_DROPPED;
            }
            packet->mProtocol = tcpMProtocol;
            done = 1;
            break;
        case IPPROTO_UDP:
            if (len < ip_hdr_len + (int)sizeof(struct udphdr)) {
                return ARKIME_PACKET_CORRUPT;
            }

            udphdr = (struct udphdr *)(data + ip_hdr_len);

            arkime_session_id6(sessionId, ip6->ip6_src.s6_addr, udphdr->uh_sport,
                               ip6->ip6_dst.s6_addr, udphdr->uh_dport);

            if (len > ip_hdr_len + (int)sizeof(struct udphdr) + 8 && udpPortCbs[udphdr->uh_dport]) {
                int rc = udpPortCbs[udphdr->uh_dport](batch, packet, (uint8_t *)udphdr + sizeof(struct udphdr *), len - ip_hdr_len - sizeof(struct udphdr *));
                if (rc != ARKIME_PACKET_UNKNOWN)
                    return rc;

                // Reset state on UNKNOWN
                packet->v6 = 1;
                packet->ipOffset = (uint8_t *)data - packet->pkt;
                packet->payloadOffset = packet->ipOffset + ip_hdr_len;
                packet->payloadLen = ip_len + sizeof(struct ip6_hdr) - ip_hdr_len;
            }

            if (config.enablePacketDedup && arkime_dedup_should_drop(packet, ip_hdr_len + sizeof(struct udphdr)))
                return ARKIME_PACKET_DUPLICATE_DROPPED;

            packet->mProtocol = udpMProtocol;
            done = 1;
            break;
        case IPPROTO_IPV4:
            return arkime_packet_ip4(batch, packet, data + ip_hdr_len, len - ip_hdr_len);
        case IPPROTO_IPV6:
            return arkime_packet_ip6(batch, packet, data + ip_hdr_len, len - ip_hdr_len);
        default:
            return arkime_packet_run_ip_cb(batch, packet, data + ip_hdr_len, len - ip_hdr_len, nxt, "IP6");
        }
        if (ip_hdr_len > len) {
#ifdef DEBUG_PACKET
            LOG ("ERROR - Corrupt packet ip_hdr_len = %d nxt = %d len = %d", ip_hdr_len, nxt, len);
#endif
            return ARKIME_PACKET_CORRUPT;
        }
    } while (!done);

    packet->hash = arkime_session_hash(sessionId);
    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
LOCAL ArkimePacketRC arkime_packet_frame_relay(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    if (len < 4)
        return ARKIME_PACKET_CORRUPT;

    uint16_t type = data[2] << 8 | data[3];

    if (type == 0x03cc)
        return arkime_packet_ip4(batch, packet, data + 4, len - 4);

    return arkime_packet_run_ethernet_cb(batch, packet, data + 4, len - 4, type, "FrameRelay");
}
/******************************************************************************/
LOCAL ArkimePacketRC arkime_packet_ieee802(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len)
{
#ifdef DEBUG_PACKET
    LOG("enter %p %p %d", packet, data, len);
#endif

    if (len < 6 || memcmp(data + 2, "\xfe\xfe\x03", 3) != 0)
        return ARKIME_PACKET_CORRUPT;

    int etherlen = data[0] << 8 | data[1];
    int ethertype = data[5];

    if (etherlen > len - 2)
        return ARKIME_PACKET_CORRUPT;

    return arkime_packet_run_ethernet_cb(batch, packet, data + 6, len - 6, ethertype, "ieee802");
}
/******************************************************************************/
LOCAL ArkimePacketRC arkime_packet_ether(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len)
{
#ifdef DEBUG_PACKET
    LOG("enter %p %p %d", packet, data, len);
#endif

    if (len < 14) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: Too short %d", len);
#endif
        return ARKIME_PACKET_CORRUPT;
    }
    packet->outerEtherOffset = packet->etherOffset; //we need to keep track of the current and the previous mac offset, we don't know if this is the last etherframe here
    packet->etherOffset = (uint8_t *)data - packet->pkt;
#ifdef DEBUG_PACKET
    char str[20];
    snprintf(str, sizeof(str), "%02x:%02x:%02x:%02x:%02x:%02x",
             data[0],
             data[1],
             data[2],
             data[3],
             data[4],
             data[5]);

    LOG("arkime_packet_ether MAC A: %s, %u", str, packet->etherOffset);
    snprintf(str, sizeof(str), "%02x:%02x:%02x:%02x:%02x:%02x",
             data[6],
             data[7],
             data[8],
             data[9],
             data[10],
             data[11]);
    LOG("arkime_packet_ether MAC B: %s, %u", str, packet->etherOffset);
#endif


    int n = 12;
    while (n + 2 < len) {
        int ethertype = data[n] << 8 | data[n + 1];
        if (ethertype <= 1500) {
            return arkime_packet_ieee802(batch, packet, data + n, len - n);
        }
        n += 2;
        switch (ethertype) {
        case ETHERTYPE_VLAN:
        case ARKIME_ETHERTYPE_QINQ:
            n += 2;
            break;
        default:
            return arkime_packet_run_ethernet_cb(batch, packet, data + n, len - n, ethertype, "Ether");
        } // switch
    }
#ifdef DEBUG_PACKET
    LOG("BAD PACKET: bad len %d < %d", n + 2, len);
#endif
    return ARKIME_PACKET_CORRUPT;
}
/******************************************************************************/
LOCAL ArkimePacketRC arkime_packet_sll(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    if (len < 16) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: Too short %d", len);
#endif
        return ARKIME_PACKET_CORRUPT;
    }

    int ethertype = data[14] << 8 | data[15];
    switch (ethertype) {
    case ETHERTYPE_VLAN:
        if ((data[20] & 0xf0) == 0x60)
            return arkime_packet_ip6(batch, packet, data + 20, len - 20);
        else
            return arkime_packet_ip4(batch, packet, data + 20, len - 20);
    default:
        return arkime_packet_run_ethernet_cb(batch, packet, data + 16, len - 16, ethertype, "SLL");
    } // switch
    return ARKIME_PACKET_CORRUPT;
}
/******************************************************************************/
LOCAL ArkimePacketRC arkime_packet_sll2(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    if (len < 20) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: Too short %d", len);
#endif
        return ARKIME_PACKET_CORRUPT;
    }

    int ethertype = data[0] << 8 | data[1];
    return arkime_packet_run_ethernet_cb(batch, packet, data + 20, len - 20, ethertype, "SLL2");
}
/******************************************************************************/
LOCAL ArkimePacketRC arkime_packet_nflog(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    if (len < 14 ||
        (data[0] != AF_INET && data[0] != AF_INET6) ||
        data[1] != 0) {
#ifdef DEBUG_PACKET
        LOG("BAD PACKET: Wrong type %d", data[0]);
#endif
        return ARKIME_PACKET_CORRUPT;
    }
    int n = 4;
    while (n + 4 < len) {
        int length = data[n + 1] << 8 | data[n];

        // Make sure length is at least header and not bigger then remaining packet
        if (length < 4 || length > len - n) {
#ifdef DEBUG_PACKET
            LOG("BAD PACKET: Wrong len %d", length);
#endif
            return ARKIME_PACKET_CORRUPT;
        }

        if (data[n + 3] == 0 && data[n + 2] == 9) {
            if (data[0] == AF_INET) {
                return arkime_packet_ip4(batch, packet, data + n + 4, length - 4);
            } else {
                return arkime_packet_ip6(batch, packet, data + n + 4, length - 4);
            }
        } else {
            n += ((length + 3) & 0xfffffc);
        }
    }
#ifdef DEBUG_PACKET
    LOG("BAD PACKET: Not sure");
#endif
    return ARKIME_PACKET_CORRUPT;
}
/******************************************************************************/
LOCAL ArkimePacketRC arkime_packet_radiotap(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    if (data[0] != 0 || len < 36)
        return ARKIME_PACKET_UNKNOWN;

    int hl = packet->pkt[2];
    if (hl + 24 + 8 >= len)
        return ARKIME_PACKET_UNKNOWN;

    if (data[hl] != 8)
        return ARKIME_PACKET_UNKNOWN;

    hl += 24 + 3;

    if (data[hl] != 0 || data[hl + 1] != 0 || data[hl + 2] != 0)
        return ARKIME_PACKET_UNKNOWN;

    hl += 3;

    uint16_t ethertype = (data[hl] << 8) | data[hl + 1];
    hl += 2;

    return arkime_packet_run_ethernet_cb(batch, packet, data + hl, len - hl, ethertype, "RadioTap");
}
/******************************************************************************/
void arkime_packet_batch_init(ArkimePacketBatch_t *batch)
{
    int t;

    for (t = 0; t < config.packetThreads; t++) {
        DLL_INIT(packet_, &batch->packetQ[t]);
    }
    batch->count = 0;
}
/******************************************************************************/
void arkime_packet_batch_flush(ArkimePacketBatch_t *batch)
{
    int t;

    for (t = 0; t < config.packetThreads; t++) {
        if (DLL_COUNT(packet_, &batch->packetQ[t]) > 0) {
            ARKIME_LOCK(packetQ[t].lock);
            DLL_PUSH_TAIL_DLL(packet_, &packetQ[t], &batch->packetQ[t]);
            ARKIME_COND_SIGNAL(packetQ[t].lock);
            ARKIME_UNLOCK(packetQ[t].lock);
        }
    }
    batch->count = 0;
}
/******************************************************************************/
void arkime_packet_batch(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet)
{
    ArkimePacketRC rc;

#ifdef DEBUG_PACKET
    LOG("enter %p %u %d", packet, pcapFileHeader.dlt, packet->pktlen);
    arkime_print_hex_string(packet->pkt, packet->pktlen);
#endif

    switch(pcapFileHeader.dlt) {
    case DLT_NULL: // NULL
        if (packet->pktlen > 4) {
            if (packet->pkt[0] == 30)
                rc = arkime_packet_ip6(batch, packet, packet->pkt + 4, packet->pktlen - 4);
            else
                rc = arkime_packet_ip4(batch, packet, packet->pkt + 4, packet->pktlen - 4);
        } else {
#ifdef DEBUG_PACKET
            LOG("BAD PACKET: Too short %d", packet->pktlen);
#endif
            rc = ARKIME_PACKET_CORRUPT;
        }
        break;
    case DLT_EN10MB: // Ether
        rc = arkime_packet_ether(batch, packet, packet->pkt, packet->pktlen);
        break;
    case DLT_RAW: // RAW
        if ((packet->pkt[0] & 0xF0) == 0x60)
            rc = arkime_packet_ip6(batch, packet, packet->pkt, packet->pktlen);
        else
            rc = arkime_packet_ip4(batch, packet, packet->pkt, packet->pktlen);
        break;
    case DLT_FRELAY: // Frame Relay
        rc = arkime_packet_frame_relay(batch, packet, packet->pkt, packet->pktlen);
        break;
    case DLT_LINUX_SLL: // SLL
        if (packet->pkt[0] == 0 && packet->pkt[1] <= 4)
            rc = arkime_packet_sll(batch, packet, packet->pkt, packet->pktlen);
        else
            rc = arkime_packet_ip4(batch, packet, packet->pkt, packet->pktlen);
        break;
    case DLT_LINUX_SLL2: // SLL2
        rc = arkime_packet_sll2(batch, packet, packet->pkt, packet->pktlen);
        break;
    case DLT_IEEE802_11_RADIO: // radiotap
        rc = arkime_packet_radiotap(batch, packet, packet->pkt, packet->pktlen);
        break;
    case DLT_IPV4: //RAW IPv4
        rc = arkime_packet_ip4(batch, packet, packet->pkt, packet->pktlen);
        break;
    case DLT_IPV6: //RAW IPv6
        rc = arkime_packet_ip6(batch, packet, packet->pkt, packet->pktlen);
        break;
    case DLT_NFLOG: // NFLOG
        rc = arkime_packet_nflog(batch, packet, packet->pkt, packet->pktlen);
        break;
    default:
        if (config.ignoreErrors)
            rc = ARKIME_PACKET_CORRUPT;
        else
            LOGEXIT("ERROR - Unsupported pcap link type %u", pcapFileHeader.dlt);
    }

    if (likely(rc == ARKIME_PACKET_DO_PROCESS) && unlikely(packet->mProtocol == 0)) {
        if (config.debug)
            LOG("Packet was market as do process but no mProtocol was set");
        rc = ARKIME_PACKET_UNKNOWN;
    }

    ARKIME_THREAD_INCR(packetStats[rc]);

    if (unlikely(rc)) {
        if (rc == ARKIME_PACKET_CORRUPT) {
            if (config.corruptSavePcap) {
                arkime_packet_save_unknown_packet(2, packet);
            }

            // A CORRUPT callback is expected to free the packet.
            if (ipCbs[ARKIME_IPPROTO_CORRUPT]) {
                ipCbs[ARKIME_IPPROTO_CORRUPT](batch, packet, packet->pkt, packet->pktlen);
            } else {
                arkime_packet_free(packet);
            }
        } else if (rc != ARKIME_PACKET_DONT_PROCESS_OR_FREE) {
            arkime_packet_free(packet);
        }
        return;
    }

    /* This packet we are going to process */

    if (unlikely(totalPackets == 0)) {
        ArkimeReaderStats_t stats;
        if (!arkime_reader_stats(&stats)) {
            initialDropped = stats.dropped;
        }
        initialPacket = packet->ts;
        if (!config.pcapReadOffline)
            LOG("Initial Packet = %ld Initial Dropped = %" PRIu64, initialPacket.tv_sec, initialDropped);
    }

    ARKIME_THREAD_INCR(totalPackets);
    if (totalPackets % config.logEveryXPackets == 0) {
        arkime_packet_log(mProtocols[packet->mProtocol].ses);
    }

    uint32_t thread = packet->hash % config.packetThreads;

    totalBytes[thread] += packet->pktlen;

    if (DLL_COUNT(packet_, &packetQ[thread]) >= config.maxPacketsInQueue) {
        ARKIME_LOCK(packetQ[thread].lock);
        overloadDrops[thread]++;
        if ((overloadDrops[thread] % 10000) == 1 && (overloadDropTimes[thread] + 60) < packet->ts.tv_sec) {
            overloadDropTimes[thread] = packet->ts.tv_sec;
            LOG("WARNING - Packet Q %u is overflowing, total dropped so far %u.  See https://arkime.com/faq#why-am-i-dropping-packets and modify %s", thread, overloadDrops[thread], config.configFile);
        }
        ARKIME_COND_SIGNAL(packetQ[thread].lock);
        ARKIME_UNLOCK(packetQ[thread].lock);
        ARKIME_THREAD_INCR(packetStats[rc]);
        arkime_packet_free(packet);
        return;
    }

    if (!packet->copied) {
        uint8_t *pkt = malloc(packet->pktlen);
        memcpy(pkt, packet->pkt, packet->pktlen);
        packet->pkt = pkt;
        packet->copied = 1;
    }

#ifdef FUZZLOCH
    arkime_session_process_commands(thread);
    arkime_packet_process(packet, thread);
#else
    DLL_PUSH_TAIL(packet_, &batch->packetQ[thread], packet);
#endif
    batch->count++;
}
/******************************************************************************/
int arkime_packet_outstanding()
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
SUPPRESS_UNSIGNED_INTEGER_OVERFLOW
LOCAL uint32_t arkime_packet_frag_hash(const void *key)
{
    int i;
    uint32_t n = 0;
    for (i = 0; i < 10; i++) {
        n = (n << 5) - n + ((uint8_t *)key)[i];
    }
    return n;
}
/******************************************************************************/
LOCAL int arkime_packet_frag_cmp(const void *keyv, const ArkimeFrags_t *element)
{
    return memcmp(keyv, element->key, 10) == 0;
}
/******************************************************************************/
LOCAL gboolean arkime_packet_save_drophash(gpointer UNUSED(user_data))
{
    if (packetDrop4.changed)
        arkime_drophash_save(&packetDrop4);

    if (packetDrop6.changed)
        arkime_drophash_save(&packetDrop6);

    if (packetDrop4S.changed)
        arkime_drophash_save(&packetDrop4S);

    if (packetDrop6S.changed)
        arkime_drophash_save(&packetDrop6S);

    return G_SOURCE_CONTINUE;
}
/******************************************************************************/
void arkime_packet_save_ethernet( ArkimePacket_t *const packet, uint16_t type)
{
    if (BIT_ISSET(type, config.etherSavePcap))
        arkime_packet_save_unknown_packet(0, packet);
}
/******************************************************************************/
ArkimePacketRC arkime_packet_run_ethernet_cb(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len, uint16_t type, const char *str)
{
#ifdef DEBUG_PACKET
    LOG("enter %p type:%d (0x%x) %s %p %d", packet, type, type, str, data, len);
#endif

    if (type == ARKIME_ETHERTYPE_DETECT) {
        if (len < 2)
            return ARKIME_PACKET_CORRUPT;
        type = data[0] << 8 | data[1];
        data += 2;
        len -= 2;
    }

    if (ethernetCbs[type]) {
        return ethernetCbs[type](batch, packet, data, len);
    }

    if (ethernetCbs[ARKIME_ETHERTYPE_UNKNOWN]) {
        return ethernetCbs[ARKIME_ETHERTYPE_UNKNOWN](batch, packet, data, len);
    }

    if (config.logUnknownProtocols)
        LOG("Unknown %s ethernet protocol 0x%04x(%d)", str, type, type);
    arkime_packet_save_ethernet(packet, type);
    return ARKIME_PACKET_UNKNOWN;
}
/******************************************************************************/
void arkime_packet_set_ethernet_cb(uint16_t type, ArkimePacketEnqueue_cb enqueueCb)
{
    if (ethernetCbs[type])
        LOG ("redining existing callback type %d", type);

    ethernetCbs[type] = enqueueCb;
}
/******************************************************************************/
ArkimePacketRC arkime_packet_run_ip_cb(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len, uint16_t type, const char *str)
{
#ifdef DEBUG_PACKET
    LOG("enter %p %d %s %p %d", packet, type, str, data, len);
#endif

    if (type >= ARKIME_IPPROTO_MAX) {
        return ARKIME_PACKET_CORRUPT;
    }

    if (ipCbs[type]) {
        return ipCbs[type](batch, packet, data, len);
    }

    if (ipCbs[ARKIME_IPPROTO_UNKNOWN]) {
        return ipCbs[ARKIME_IPPROTO_UNKNOWN](batch, packet, data, len);
    }

    if (config.logUnknownProtocols)
        LOG("Unknown %s protocol %d", str, type);
    if (BIT_ISSET(type, config.ipSavePcap))
        arkime_packet_save_unknown_packet(1, packet);
    return ARKIME_PACKET_UNKNOWN;
}
/******************************************************************************/
void arkime_packet_set_ip_cb(uint16_t type, ArkimePacketEnqueue_cb enqueueCb)
{
    if (type >= ARKIME_IPPROTO_MAX)
        LOGEXIT ("ERROR - type value too large %d", type);

    ipCbs[type] = enqueueCb;
}
/******************************************************************************/
void arkime_packet_set_udpport_enqueue_cb(uint16_t port, ArkimePacketEnqueue_cb enqueueCb)
{
    udpPortCbs[htons(port)] = enqueueCb;
}
/******************************************************************************/
void arkime_packet_init()
{
    pcapFileHeader.magic = 0xa1b2c3d4;
    pcapFileHeader.version_major = 2;
    pcapFileHeader.version_minor = 4;

    pcapFileHeader.thiszone = 0;
    pcapFileHeader.sigfigs = 0;

    char filename[PATH_MAX];
    snprintf(filename, sizeof(filename), "/tmp/%s.tcp.drops.4", config.nodeName);
    arkime_drophash_init(&packetDrop4, filename, 4);

    snprintf(filename, sizeof(filename), "/tmp/%s.tcp.drops.6", config.nodeName);
    arkime_drophash_init(&packetDrop6, filename, 16);

    snprintf(filename, sizeof(filename), "/tmp/%s.tcp.drops.4S", config.nodeName);
    arkime_drophash_init(&packetDrop4S, filename, 12);

    snprintf(filename, sizeof(filename), "/tmp/%s.tcp.drops.6S", config.nodeName);
    arkime_drophash_init(&packetDrop6S, filename, 36);

    g_timeout_add_seconds(10, arkime_packet_save_drophash, 0);

    mac1Field = arkime_field_define("general", "lotermfield",
                                    "mac.src", "Src MAC", "source.mac",
                                    "Source ethernet mac addresses set for session",
                                    ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_ECS_CNT | ARKIME_FIELD_FLAG_LINKED_SESSIONS | ARKIME_FIELD_FLAG_NOSAVE,
                                    "transform", "dash2Colon",
                                    "fieldECS", "source.mac",
                                    (char *)NULL);

    mac2Field = arkime_field_define("general", "lotermfield",
                                    "mac.dst", "Dst MAC", "destination.mac",
                                    "Destination ethernet mac addresses set for session",
                                    ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_ECS_CNT | ARKIME_FIELD_FLAG_LINKED_SESSIONS | ARKIME_FIELD_FLAG_NOSAVE,
                                    "transform", "dash2Colon",
                                    "fieldECS", "destination.mac",
                                    (char *)NULL);

    outermac1Field = arkime_field_define("general", "lotermfield",
                                         "outermac.src", "Src Outer MAC", "srcOuterMac",
                                         "Source ethernet outer mac addresses set for session",
                                         ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_ECS_CNT | ARKIME_FIELD_FLAG_LINKED_SESSIONS,
                                         "transform", "dash2Colon",
                                         (char *)NULL);

    outermac2Field = arkime_field_define("general", "lotermfield",
                                         "outermac.dst", "Dst Outer MAC", "dstOuterMac",
                                         "Destination ethernet outer mac addresses set for session",
                                         ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_ECS_CNT | ARKIME_FIELD_FLAG_LINKED_SESSIONS,
                                         "transform", "dash2Colon",
                                         (char *)NULL);

    dscpField[0] = arkime_field_define("general", "integer",
                                       "dscp.src", "Src DSCP", "srcDscp",
                                       "Source non zero differentiated services class selector set for session",
                                       ARKIME_FIELD_TYPE_INT_GHASH,  ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    dscpField[1] = arkime_field_define("general", "integer",
                                       "dscp.dst", "Dst DSCP", "dstDscp",
                                       "Destination non zero differentiated services class selector set for session",
                                       ARKIME_FIELD_TYPE_INT_GHASH,  ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    arkime_field_define("general", "lotermfield",
                        "mac", "Src or Dst MAC", "macall",
                        "Shorthand for mac.src or mac.dst",
                        0,  ARKIME_FIELD_FLAG_FAKE,
                        "regex", "^mac\\\\.(?:(?!\\\\.cnt$).)*$",
                        "transform", "dash2Colon",
                        (char *)NULL);

    arkime_field_define("general", "lotermfield",
                        "outermac", "Src or Dst Outer MAC", "outermacall",
                        "Shorthand for outermac.src or outermac.dst",
                        0,  ARKIME_FIELD_FLAG_FAKE,
                        "regex", "^outermac\\\\.(?:(?!\\\\.cnt$).)*$",
                        "transform", "dash2Colon",
                        (char *)NULL);

    oui1Field = arkime_field_define("general", "termfield",
                                    "oui.src", "Src OUI", "srcOui",
                                    "Source ethernet oui for session",
                                    ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_LINKED_SESSIONS,
                                    (char *)NULL);

    oui2Field = arkime_field_define("general", "termfield",
                                    "oui.dst", "Dst OUI", "dstOui",
                                    "Destination ethernet oui for session",
                                    ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_LINKED_SESSIONS,
                                    (char *)NULL);

    outeroui1Field = arkime_field_define("general", "termfield",
                                         "outeroui.src", "Src Outer OUI", "srcOuterOui",
                                         "Source ethernet outer oui for session",
                                         ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_LINKED_SESSIONS,
                                         (char *)NULL);

    outeroui2Field = arkime_field_define("general", "termfield",
                                         "outeroui.dst", "Dst Outer OUI", "dstOuterOui",
                                         "Destination ethernet outer oui for session",
                                         ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_LINKED_SESSIONS,
                                         (char *)NULL);

    vlanField = arkime_field_define("general", "integer",
                                    "vlan", "VLan", "network.vlan.id",
                                    "vlan value",
                                    ARKIME_FIELD_TYPE_INT_GHASH,  ARKIME_FIELD_FLAG_ECS_CNT | ARKIME_FIELD_FLAG_LINKED_SESSIONS | ARKIME_FIELD_FLAG_NOSAVE,
                                    (char *)NULL);

    vniField = arkime_field_define("general", "integer",
                                   "vni", "VNI", "vni",
                                   "vni value",
                                   ARKIME_FIELD_TYPE_INT_GHASH,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_LINKED_SESSIONS,
                                   (char *)NULL);


    outerip1Field = arkime_field_define("general", "ip",
                                        "outerip.src", "Src Outer IP", "srcOuterIp",
                                        "Source ethernet outer ip for session",
                                        ARKIME_FIELD_TYPE_IP_GHASH,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_LINKED_SESSIONS,
                                        (char *)NULL);

    outerip2Field = arkime_field_define("general", "ip",
                                        "outerip.dst", "Dst Outer IP", "dstOuterIp",
                                        "Destination outer ip for session",
                                        ARKIME_FIELD_TYPE_IP_GHASH,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_LINKED_SESSIONS,
                                        (char *)NULL);

    arkime_field_define("general", "lotermfield",
                        "outerip", "Src or Dst Outer IP", "outeripall",
                        "Shorthand for outerip.src or outerip.dst",
                        0,  ARKIME_FIELD_FLAG_FAKE,
                        "regex", "^outerip\\\\.(?:(?!\\\\.cnt$).)*$",
                        (char *)NULL);

    arkime_field_define("general", "integer",
                        "tcpflags.syn", "TCP Flag SYN", "tcpflags.syn",
                        "Count of packets with SYN and no ACK flag set",
                        0,  ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("general", "integer",
                        "tcpflags.syn-ack", "TCP Flag SYN-ACK", "tcpflags.syn-ack",
                        "Count of packets with SYN and ACK flag set",
                        0,  ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("general", "integer",
                        "tcpflags.ack", "TCP Flag ACK", "tcpflags.ack",
                        "Count of packets with only the ACK flag set",
                        0,  ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("general", "integer",
                        "tcpflags.psh", "TCP Flag PSH", "tcpflags.psh",
                        "Count of packets with PSH flag set",
                        0,  ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("general", "integer",
                        "tcpflags.fin", "TCP Flag FIN", "tcpflags.fin",
                        "Count of packets with FIN flag set",
                        0,  ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("general", "integer",
                        "tcpflags.rst", "TCP Flag RST", "tcpflags.rst",
                        "Count of packets with RST flag set",
                        0,  ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("general", "integer",
                        "tcpflags.urg", "TCP Flag URG", "tcpflags.urg",
                        "Count of packets with URG flag set",
                        0,  ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("general", "integer",
                        "packets.src", "Src Packets", "srcPackets",
                        "Total number of packets sent by source in a session",
                        0,  ARKIME_FIELD_FLAG_FAKE,
                        "fieldECS", "source.packets",
                        (char *)NULL);

    arkime_field_define("general", "integer",
                        "packets.dst", "Dst Packets", "dstPackets",
                        "Total number of packets sent by destination in a session",
                        0,  ARKIME_FIELD_FLAG_FAKE,
                        "fieldECS", "destination.packets",
                        (char *)NULL);

    arkime_field_define("general", "integer",
                        "initRTT", "Initial RTT", "initRTT",
                        "Initial round trip time, difference between SYN and ACK timestamp divided by 2 in ms",
                        0,  ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("general", "termfield",
                        "communityId", "Community Id", "communityId",
                        "Community id flow hash",
                        0,  ARKIME_FIELD_FLAG_FAKE,
                        "fieldECS", "network.community_id",
                        (char *)NULL);

    int t;
    for (t = 0; t < config.packetThreads; t++) {
        char name[100];
        DLL_INIT(packet_, &packetQ[t]);
        ARKIME_LOCK_INIT(packetQ[t].lock);
        ARKIME_COND_INIT(packetQ[t].lock);
        snprintf(name, sizeof(name), "arkime-pkt%d", t);
#ifndef FUZZLOCH
        g_thread_unref(g_thread_new(name, &arkime_packet_thread, (gpointer)(long)t));
#endif
    }

    HASH_INIT(fragh_, fragsHash, arkime_packet_frag_hash, (HASH_CMP_FUNC)arkime_packet_frag_cmp);
    DLL_INIT(fragl_, &fragsList);

    arkime_add_can_quit(arkime_packet_outstanding, "packet outstanding");
    arkime_add_can_quit(arkime_packet_frags_outstanding, "packet frags outstanding");


    arkime_packet_set_ethernet_cb(ARKIME_ETHERTYPE_ETHER, arkime_packet_ether);
    arkime_packet_set_ethernet_cb(ARKIME_ETHERTYPE_TEB, arkime_packet_ether); // ETH_P_TEB - Trans Ether Bridging
    arkime_packet_set_ethernet_cb(ARKIME_ETHERTYPE_RAWFR, arkime_packet_frame_relay);
    arkime_packet_set_ethernet_cb(ETHERTYPE_IP, arkime_packet_ip4);
    arkime_packet_set_ethernet_cb(ETHERTYPE_IPV6, arkime_packet_ip6);
}
/******************************************************************************/
uint64_t arkime_packet_dropped_packets()
{
    ArkimeReaderStats_t stats;
    if (arkime_reader_stats(&stats)) {
        return 0;
    }
    return stats.dropped - initialDropped;
}
/******************************************************************************/
uint64_t arkime_packet_dropped_frags()
{
    return droppedFrags;
}
/******************************************************************************/
uint64_t arkime_packet_dropped_overload()
{
    uint64_t count = 0;

    int t;

    for (t = 0; t < config.packetThreads; t++) {
        count += overloadDrops[t];
    }
    return count;
}
/******************************************************************************/
uint64_t arkime_packet_total_bytes()
{
    uint64_t count = 0;

    int t;

    for (t = 0; t < config.packetThreads; t++) {
        count += totalBytes[t];
    }
    return count;
}
/******************************************************************************/
void arkime_packet_add_packet_ip(char *ipstr, int mode)
{
    patricia_node_t *node;
    if (strchr(ipstr, '.') != 0) {
        if (!ipTree4)
            newipTree4 = New_Patricia(32);
        node = make_and_lookup(newipTree4, ipstr);
    } else {
        if (!ipTree6)
            newipTree6 = New_Patricia(128);
        node = make_and_lookup(newipTree6, ipstr);
    }
    node->data = (void *)(long)mode;
}
/******************************************************************************/
LOCAL void arkime_packet_free_packet_ips(patricia_tree_t *tree)
{
    Destroy_Patricia(tree, NULL);
}
/******************************************************************************/
void arkime_packet_install_packet_ip()
{
    arkime_free_later(ipTree4, (GDestroyNotify) arkime_packet_free_packet_ips);
    ipTree4 = newipTree4;
    newipTree4 = 0;

    arkime_free_later(ipTree6, (GDestroyNotify) arkime_packet_free_packet_ips);
    ipTree6 = newipTree6;
    newipTree6 = 0;
}
/******************************************************************************/
void arkime_packet_set_dltsnap(int dlt, int snaplen)
{
    pcapFileHeader.dlt = dlt;
    pcapFileHeader.snaplen = snaplen;
    arkime_rules_recompile();
}
/******************************************************************************/
// PCAP Header needs linktype when written
// Code based on https://github.com/arkime/arkime/issues/1303#issuecomment-554684749
// Values from libpcap pcap-common.c
uint32_t arkime_packet_dlt_to_linktype(int dlt)
{
    if (dlt <= 10 || dlt >= 104)
        return dlt;

    switch (dlt)
    {
#ifdef DLT_FR
    case DLT_FR:
        return 107; // LINKTYPE_FRELAY;
#endif
    case DLT_ATM_RFC1483:
        return 100; // LINKTYPE_ATM_RFC1483;
    case DLT_RAW:
        return 101; // LINKTYPE_RAW;
    case DLT_SLIP_BSDOS:
        return 102; // LINKTYPE_SLIP_BSDOS;
    case DLT_PPP_BSDOS:
        return 103; // LINKTYPE_PPP_BSDOS;
    case DLT_C_HDLC:
        return 104; // LINKTYPE_C_HDLC;
    case DLT_ATM_CLIP:
        return 106; // LINKTYPE_ATM_CLIP;
    case DLT_PPP_SERIAL:
        return 50; // LINKTYPE_PPP_HDLC
    case DLT_PPP_ETHER:
        return 51; // LINKTYPE_PPP_ETHER;
    case DLT_PFSYNC:
        return 246; // LINKTYPE_PFSYNC;
    case DLT_PKTAP:
        return 258; // LINKTYPE_PKTAP
    }
    return dlt;
}
/******************************************************************************/
void arkime_packet_drophash_add(ArkimeSession_t *session, int which, int min)
{
    if (session->ses != SESSION_TCP)
        return;

    if (which == -1) {
        const int port = (htons(session->port1) * htons(session->port2)) & 0xffff;
        if (ARKIME_SESSION_v6(session)) {
            arkime_drophash_add(&packetDrop6S, port, session->sessionId + 1, session->lastPacket.tv_sec, min * 60);
        } else {
            arkime_drophash_add(&packetDrop4S, port, session->sessionId + 1, session->lastPacket.tv_sec, min * 60);
        }
    } else {
        // packetDrop is kept in network byte order
        const int port = (which == 0) ? htons(session->port1) : htons(session->port2);

        if (ARKIME_SESSION_v6(session)) {
            if (which == 0) {
                arkime_drophash_add(&packetDrop6, port, (void *)&session->addr1, session->lastPacket.tv_sec, min * 60);
            } else {
                arkime_drophash_add(&packetDrop6, port, (void *)&session->addr2, session->lastPacket.tv_sec, min * 60);
            }
        } else {
            if (which == 0) {
                arkime_drophash_add(&packetDrop4, port, &((uint32_t *)session->addr1.s6_addr)[3], session->lastPacket.tv_sec, min * 60);
            } else {
                arkime_drophash_add(&packetDrop4, port, &((uint32_t *)session->addr2.s6_addr)[3], session->lastPacket.tv_sec, min * 60);
            }
        }
    }
}
/******************************************************************************/
void arkime_packet_exit()
{
    if (ipTree4) {
        Destroy_Patricia(ipTree4, NULL);
        ipTree4 = 0;
    }

    if (ipTree6) {
        Destroy_Patricia(ipTree6, NULL);
        ipTree6 = 0;
    }
    arkime_packet_log(SESSION_TCP);
    if (unknownPacketFile[0])
        fclose(unknownPacketFile[0]);
    if (unknownPacketFile[1])
        fclose(unknownPacketFile[1]);
    if (unknownPacketFile[2])
        fclose(unknownPacketFile[2]);
}
/******************************************************************************/
int arkime_mprotocol_register_internal(char                            *name,
                                       int                              ses,
                                       ArkimeProtocolCreateSessionId_cb createSessionId,
                                       ArkimeProtocolPreProcess_cb      preProcess,
                                       ArkimeProtocolProcess_cb         process,
                                       ArkimeProtocolSessionFree_cb     sFree,
                                       size_t                           sessionsize,
                                       int                              apiversion)
{
    if (sizeof(ArkimeSession_t) != sessionsize) {
        CONFIGEXIT("Parser '%s' built with different version of arkime.h\n %u != %u", name, (unsigned int)sizeof(ArkimeSession_t),  (unsigned int)sessionsize);
    }

    if (ARKIME_API_VERSION != apiversion) {
        CONFIGEXIT("Parser '%s' built with different version of arkime.h\n %d %d", name, ARKIME_API_VERSION, apiversion);
    }

    int num = ++mProtocolCnt; // Leave 0 empty so we know if not set in code
    mProtocols[num].name = name;
    mProtocols[num].ses = ses;
    mProtocols[num].createSessionId = createSessionId;
    mProtocols[num].preProcess = preProcess;
    mProtocols[num].process = process;
    mProtocols[num].sFree = sFree;
    return num;
}
