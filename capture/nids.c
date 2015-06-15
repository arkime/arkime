/* nids.c  -- Functions for dealing with libnids
 *
 * Copyright 2012-2015 AOL Inc. All rights reserved.
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

#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#define __FAVOR_BSD
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "pcap.h"
#include "moloch.h"
#include <gio/gio.h>

/******************************************************************************/
extern MolochConfig_t        config;
extern GMainLoop            *mainLoop;
extern uint32_t              pluginsCbs;
extern void                 *esServer;

static MolochSessionHead_t   tcpSessionQ;
static MolochSessionHead_t   udpSessionQ;
static MolochSessionHead_t   icmpSessionQ;
static MolochSessionHead_t   tcpWriteQ;

static MolochStringHead_t    monitorQ;

static uint32_t              initialDropped = 0;
struct timeval               initialPacket;

static FILE                 *offlineFile = 0;
static char                  offlinePcapFilename[PATH_MAX+1];

static int                   tagsField;
static int                   protocolField;
static int                   mac1Field;
static int                   mac2Field;
static int                   vlanField;

uint64_t                     totalPackets = 0;
uint64_t                     totalBytes = 0;
uint64_t                     totalSessions = 0;

static struct bpf_program   *bpf_programs = 0;

extern MolochWriterQueueLength moloch_writer_queue_length;
extern MolochWriterWrite moloch_writer_write;
extern MolochWriterFlush moloch_writer_flush;
extern MolochWriterExit moloch_writer_exit;
extern MolochWriterNextInput moloch_writer_next_input;
extern MolochWriterName moloch_writer_name;

/******************************************************************************/

typedef HASH_VAR(h_, MolochSessionHash_t, MolochSessionHead_t, 199337);

#define SESSION_TCP  0
#define SESSION_UDP  1
#define SESSION_ICMP 2
#define SESSION_MAX  3
static MolochSessionHash_t sessions[SESSION_MAX];

/******************************************************************************/
void moloch_nids_session_free (MolochSession_t *session);
void moloch_nids_process_udp(MolochSession_t *session, struct udphdr   *udphdr, unsigned char *data, int len, int which);
int  moloch_nids_next_file();
void moloch_nids_init_nids();

/******************************************************************************/
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#define int_ntoa(x)     inet_ntoa(*((struct in_addr *)(int*)&x))

char *moloch_friendly_session_id (int protocol, uint32_t addr1, int port1, uint32_t addr2, int port2)
{
    static char buf[1000];
    int         len;

    if (addr1 < addr2) {
        len = snprintf(buf, sizeof(buf), "%d;%s:%i,", protocol, int_ntoa(addr1), port1);
        snprintf(buf+len, sizeof(buf) - len, "%s:%i", int_ntoa(addr2), port2);
    } else if (addr1 > addr2) {
        len = snprintf(buf, sizeof(buf), "%d;%s:%i,", protocol, int_ntoa(addr2), port2);
        snprintf(buf+len, sizeof(buf) - len, "%s:%i", int_ntoa(addr1), port1);
    } else if (port1 < port2) {
        len = snprintf(buf, sizeof(buf), "%d;%s:%i,", protocol, int_ntoa(addr1), port1);
        snprintf(buf+len, sizeof(buf) - len, "%s:%i", int_ntoa(addr2), port2);
    } else {
        len = snprintf(buf, sizeof(buf), "%d;%s:%i,", protocol, int_ntoa(addr2), port2);
        snprintf(buf+len, sizeof(buf) - len, "%s:%i", int_ntoa(addr1), port1);
    }

    return buf;
}
/******************************************************************************/
void moloch_nids_save_session(MolochSession_t *session)
{
    if (session->parserInfo) {
        int i;
        for (i = 0; i < session->parserNum; i++) {
            if (session->parserInfo[i].parserSaveFunc)
                session->parserInfo[i].parserSaveFunc(session, session->parserInfo[i].uw, TRUE);
        }
    }

    if (pluginsCbs & MOLOCH_PLUGIN_PRE_SAVE)
        moloch_plugins_cb_pre_save(session, TRUE);

    if (session->outstandingQueries > 0) {
        session->needSave = 1;

        if (session->tcp_next) {
            DLL_REMOVE(tcp_, &tcpWriteQ, session);
        }

        switch (session->protocol) {
        case IPPROTO_TCP:
            DLL_REMOVE(q_, &tcpSessionQ, session);
            HASH_REMOVE(h_, sessions[SESSION_TCP], session);
            break;
        case IPPROTO_UDP:
            DLL_REMOVE(q_, &udpSessionQ, session);
            HASH_REMOVE(h_, sessions[SESSION_UDP], session);
            break;
        case IPPROTO_ICMP:
            DLL_REMOVE(q_, &icmpSessionQ, session);
            HASH_REMOVE(h_, sessions[SESSION_ICMP], session);
            break;
        }

        return;
    }

    moloch_db_save_session(session, TRUE);

    switch (session->protocol) {
    case IPPROTO_TCP:
        HASH_REMOVE(h_, sessions[SESSION_TCP], session);
        break;
    case IPPROTO_UDP:
        HASH_REMOVE(h_, sessions[SESSION_UDP], session);
        break;
    case IPPROTO_ICMP:
        HASH_REMOVE(h_, sessions[SESSION_ICMP], session);
        break;
    }
    moloch_nids_session_free(session);
}
/******************************************************************************/
void moloch_nids_mid_save_session(MolochSession_t *session)
{
    if (session->parserInfo) {
        int i;
        for (i = 0; i < session->parserNum; i++) {
            if (session->parserInfo[i].parserSaveFunc)
                session->parserInfo[i].parserSaveFunc(session, session->parserInfo[i].uw, FALSE);
        }
    }

    if (pluginsCbs & MOLOCH_PLUGIN_PRE_SAVE)
        moloch_plugins_cb_pre_save(session, FALSE);

    /* If we are parsing pcap its ok to pause and make sure all tags are loaded */
    while (session->outstandingQueries > 0 && config.pcapReadOffline) {
        g_main_context_iteration (g_main_context_default(), TRUE);
    }

    if (!session->rootId) {
        session->rootId = "ROOT";
    }

    moloch_db_save_session(session, FALSE);
    g_array_set_size(session->filePosArray, 0);
    g_array_set_size(session->fileLenArray, 0);
    g_array_set_size(session->fileNumArray, 0);
    session->lastFileNum = 0;

    if (session->tcp_next) {
        DLL_MOVE_TAIL(tcp_, &tcpWriteQ, session);
    }

    session->lastSave = nids_last_pcap_header->ts.tv_sec;
    session->bytes[0] = 0;
    session->bytes[1] = 0;
    session->databytes[0] = 0;
    session->databytes[1] = 0;
    session->packets[0] = 0;
    session->packets[1] = 0;
}
/******************************************************************************/
struct pcap_file_header pcapFileHeader;
int dlt_to_linktype(int dlt);
/******************************************************************************/
void moloch_nids_cb_ip(struct ip *packet, int len)
{
    char             sessionId[MOLOCH_SESSIONID_LEN];
    MolochSession_t *headSession;
    struct tcphdr   *tcphdr = 0;
    struct udphdr   *udphdr = 0;
    MolochSessionHead_t *sessionsQ;
    uint32_t         sessionTimeout;
    int              ses;

    switch (packet->ip_p) {
    case IPPROTO_TCP:
        sessionsQ = &tcpSessionQ;
        sessionTimeout = config.tcpTimeout;

        tcphdr = (struct tcphdr *)((char*)packet + 4 * packet->ip_hl);

        //LOG("ip (%d) %x %d", len, tcphdr->th_flags, tcphdr->th_flags & TH_RST);
        //fflush(stdout);

        moloch_session_id(sessionId, packet->ip_src.s_addr, tcphdr->th_sport,
                          packet->ip_dst.s_addr, tcphdr->th_dport);
        ses = SESSION_TCP;
        break;
    case IPPROTO_UDP:
        sessionsQ = &udpSessionQ;
        sessionTimeout = config.udpTimeout;

        udphdr = (struct udphdr *)((char*)packet + 4 * packet->ip_hl);

        moloch_session_id(sessionId, packet->ip_src.s_addr, udphdr->uh_sport,
                          packet->ip_dst.s_addr, udphdr->uh_dport);
        ses = SESSION_UDP;
        break;
    case IPPROTO_ICMP:
        sessionsQ = &icmpSessionQ;
        sessionTimeout = config.icmpTimeout;

        moloch_session_id(sessionId, packet->ip_src.s_addr, 0,
                          packet->ip_dst.s_addr, 0);
        ses = SESSION_ICMP;
        break;
    case IPPROTO_IPV6:
        return;
    default:
        if (pluginsCbs & MOLOCH_PLUGIN_IP)
            moloch_plugins_cb_ip(NULL, packet, len);
        if (config.logUnknownProtocols)
            LOG("Unknown protocol %d", packet->ip_p);
        return;
    }

    totalBytes += nids_last_pcap_header->caplen;

    if (totalPackets == 0) {
        struct pcap_stat ps;
        if (!pcap_stats(nids_params.pcap_desc, &ps)) {
            initialDropped = ps.ps_drop;
        }
        initialPacket = nids_last_pcap_header->ts;
        LOG("Initial Packet = %ld", initialPacket.tv_sec);
        LOG("%" PRIu64 " Initial Dropped = %d", totalPackets, initialDropped);
    }

    if ((++totalPackets) % config.logEveryXPackets == 0) {
        struct pcap_stat ps;
        if (pcap_stats(nids_params.pcap_desc, &ps)) {
            ps.ps_drop = 0;
            ps.ps_recv = totalPackets;
            ps.ps_ifdrop = 0;
        }
        headSession = DLL_PEEK_HEAD(q_, sessionsQ);

        LOG("packets: %" PRIu64 " current sessions: %u/%u oldest: %d - recv: %u drop: %u (%0.2f) ifdrop: %u queue: %d disk: %d",
          totalPackets,
          sessionsQ->q_count,
          moloch_nids_monitoring_sessions(),
          (headSession?(int)(nids_last_pcap_header->ts.tv_sec - (headSession->lastPacket.tv_sec + sessionTimeout)):0),
          ps.ps_recv,
          ps.ps_drop - initialDropped, (ps.ps_drop - initialDropped)*(double)100.0/ps.ps_recv,
          ps.ps_ifdrop,
          moloch_http_queue_length(esServer),
          moloch_writer_queue_length());
    }

    /* Get or Create Session */
    MolochSession_t *session;
    uint32_t hash = HASH_HASH(sessions[ses], sessionId);

    HASH_FIND_HASH(h_, sessions[ses], hash, sessionId, session);

    if (!session) {
        session = MOLOCH_TYPE_ALLOC0(MolochSession_t);
        session->protocol = packet->ip_p;
        session->filePosArray = g_array_sized_new(FALSE, FALSE, sizeof(uint64_t), 100);
        session->fileLenArray = g_array_sized_new(FALSE, FALSE, sizeof(uint16_t), 100);
        session->fileNumArray = g_array_new(FALSE, FALSE, 4);
        HASH_ADD_HASH(h_, sessions[ses], hash, sessionId, session);
        session->lastSave = nids_last_pcap_header->ts.tv_sec;
        session->firstPacket = nids_last_pcap_header->ts;
        session->addr1 = packet->ip_src.s_addr;
        session->addr2 = packet->ip_dst.s_addr;
        session->ip_tos = packet->ip_tos;
        session->fields = MOLOCH_SIZE_ALLOC0(fields, sizeof(MolochField_t *)*config.maxField);
        session->maxFields = config.maxField;
        if (config.numPlugins > 0)
            session->pluginData = MOLOCH_SIZE_ALLOC0(pluginData, sizeof(void *)*config.numPlugins);

        moloch_parsers_initial_tag(session);
        memcpy(&session->sessionIda, sessionId, 8);
        memcpy(&session->sessionIdb, sessionId+8, 4);

        switch (packet->ip_p) {
        case IPPROTO_TCP:
           /* If antiSynDrop option is set to true, capture will assume that
            *if the syn-ack packet was captured first then the syn probably got dropped.*/
            if ((tcphdr->th_flags & TH_SYN) && (tcphdr->th_flags & TH_ACK) && (config.antiSynDrop)) {
                session->addr1 = packet->ip_dst.s_addr;
                session->addr2 = packet->ip_src.s_addr;
                session->port1 = ntohs(tcphdr->th_dport);
                session->port2 = ntohs(tcphdr->th_sport);
            } else {
                session->port1 = ntohs(tcphdr->th_sport);
                session->port2 = ntohs(tcphdr->th_dport);
            }
            if (moloch_http_is_moloch(hash, sessionId)) {
                if (config.debug)
                    LOG("Ignoring connection %s", moloch_friendly_session_id(session->protocol, session->addr1, session->port1, session->addr2, session->port2));
                session->stopSPI = 1;
                session->stopSaving = 1;
            }
            break;
        case IPPROTO_UDP:
            session->port1 = ntohs(udphdr->uh_sport);
            session->port2 = ntohs(udphdr->uh_dport);
            break;
        case IPPROTO_ICMP:
            session->port1 = 0;
            session->port2 = 0;
            break;
        }

        DLL_PUSH_TAIL(q_, sessionsQ, session);
        if (pluginsCbs & MOLOCH_PLUGIN_NEW)
            moloch_plugins_cb_new(session);
    } else {
        DLL_MOVE_TAIL(q_, sessionsQ, session);
    }

    int which = 0;
    switch (packet->ip_p) {
    case IPPROTO_UDP:
        which = (session->addr1 == packet->ip_src.s_addr &&
                 session->addr2 == packet->ip_dst.s_addr &&
                 session->port1 == ntohs(udphdr->uh_sport) &&
                 session->port2 == ntohs(udphdr->uh_dport))?0:1;
        session->databytes[which] += (nids_last_pcap_header->caplen - 8);
        moloch_nids_process_udp(session, udphdr, (unsigned char*)udphdr+8, nids_last_pcap_header->caplen - 8 - 4 * packet->ip_hl, which);
        break;
    case IPPROTO_TCP:
        which = (session->addr1 == packet->ip_src.s_addr &&
                 session->addr2 == packet->ip_dst.s_addr &&
                 session->port1 == ntohs(tcphdr->th_sport) &&
                 session->port2 == ntohs(tcphdr->th_dport))?0:1;
        session->tcp_flags |= tcphdr->th_flags;
        break;
    case IPPROTO_ICMP:
        which = (session->addr1 == packet->ip_src.s_addr &&
                 session->addr2 == packet->ip_dst.s_addr)?0:1;
        break;
    }

    /* Handle MACs and vlans on first few packets in each direction */
    if (pcapFileHeader.linktype == 1 && session->packets[which] <= 1) {
        char str1[20];
        char str2[20];
        snprintf(str1, sizeof(str1), "%02x:%02x:%02x:%02x:%02x:%02x",
                nids_last_pcap_data[0],
                nids_last_pcap_data[1],
                nids_last_pcap_data[2],
                nids_last_pcap_data[3],
                nids_last_pcap_data[4],
                nids_last_pcap_data[5]);


        snprintf(str2, sizeof(str2), "%02x:%02x:%02x:%02x:%02x:%02x",
                nids_last_pcap_data[6],
                nids_last_pcap_data[7],
                nids_last_pcap_data[8],
                nids_last_pcap_data[9],
                nids_last_pcap_data[10],
                nids_last_pcap_data[11]);

        if (which == 1) {
            moloch_field_string_add(mac1Field, session, str1, 17, TRUE);
            moloch_field_string_add(mac2Field, session, str2, 17, TRUE);
        } else {
            moloch_field_string_add(mac1Field, session, str2, 17, TRUE);
            moloch_field_string_add(mac2Field, session, str1, 17, TRUE);
        }

        int n = 12;
        while (nids_last_pcap_data[n] == 0x81 && nids_last_pcap_data[n+1] == 0x00) {
            uint16_t vlan = ((uint16_t)(nids_last_pcap_data[n+2] << 8 | nids_last_pcap_data[n+3])) & 0xfff;
            moloch_field_int_add(vlanField, session, vlan);
            n += 4;
        }
    }

    /* Check if the stop saving bpf filters match */
    if (bpf_programs && session->packets[which] == 0 && session->stopSaving == 0) {
        int i;
        for (i = 0; i < config.dontSaveBPFsNum; i++) {
            if (bpf_filter(bpf_programs[i].bf_insns, nids_last_pcap_data, nids_last_pcap_header->len, nids_last_pcap_header->caplen)) {
                session->stopSaving = config.dontSaveBPFsStop[i];
                break;
            }
        }
    }

    session->bytes[which] += nids_last_pcap_header->caplen;
    session->lastPacket = nids_last_pcap_header->ts;

    if (pluginsCbs & MOLOCH_PLUGIN_IP)
        moloch_plugins_cb_ip(session, packet, len);

    session->packets[which]++;
    uint32_t packets = session->packets[0] + session->packets[1];

    if (session->stopSaving == 0 || packets < session->stopSaving) {
        uint32_t fileNum;
        uint64_t filePos;
        uint16_t fileLen = 16 + nids_last_pcap_header->caplen;

        moloch_writer_write(nids_last_pcap_header, nids_last_pcap_data, &fileNum, &filePos);

        if (session->lastFileNum != fileNum) {
            session->lastFileNum = fileNum;
            g_array_append_val(session->fileNumArray, fileNum);
            int64_t pos = -1LL * fileNum;
            g_array_append_val(session->filePosArray, pos);
            int16_t len = 0;
            g_array_append_val(session->fileLenArray, len);
        }

        g_array_append_val(session->filePosArray, filePos);
        g_array_append_val(session->fileLenArray, fileLen);

        if (packets >= config.maxPackets) {
            moloch_nids_mid_save_session(session);
        }
    }

    /* Clean up the Q, only 1 per incoming packet so we don't fall behind */
    if ((headSession = DLL_PEEK_HEAD(q_, sessionsQ)) &&
           ((uint64_t)headSession->lastPacket.tv_sec + sessionTimeout < (uint64_t)nids_last_pcap_header->ts.tv_sec)) {

        if (packet->ip_p == IPPROTO_TCP && headSession->haveNidsTcp) {
            //LOG("Saving because of at head %s", moloch_friendly_session_id(headSession->protocol, headSession->addr1, headSession->port1, headSession->addr2, headSession->port2));
            headSession->lastPacket.tv_sec = nids_last_pcap_header->ts.tv_sec;

            DLL_MOVE_TAIL(q_, sessionsQ, headSession);

            moloch_nids_mid_save_session(headSession);
        } else {
            moloch_nids_save_session(headSession);
        }
    }

    if ((headSession = DLL_PEEK_HEAD(tcp_, &tcpWriteQ)) &&
           (headSession->lastSave + config.tcpSaveTimeout < (uint64_t)nids_last_pcap_header->ts.tv_sec)) {

            //LOG("Saving because of timeout %s", moloch_friendly_session_id(headSession->protocol, headSession->addr1, headSession->port1, headSession->addr2, headSession->port2));
            moloch_nids_mid_save_session(headSession);
    }
}

/******************************************************************************/
void moloch_nids_decr_outstanding(MolochSession_t *session)
{
    session->outstandingQueries--;
    if (session->needSave && session->outstandingQueries == 0) {
        session->needSave = 0; /* Stop endless loop if plugins add tags */
        moloch_db_save_session(session, TRUE);
        moloch_nids_session_free(session);
    }
}
/******************************************************************************/
void moloch_nids_get_tag_cb(void *session, int tagType, const char *tagName, uint32_t tag)
{
    if (tag == 0) {
        LOG("ERROR - Not adding tag %s type %d couldn't get tag num", tagName, tagType);
    } else {
        moloch_field_int_add(tagType, session, tag);
    }
    moloch_nids_decr_outstanding(session);
}
/******************************************************************************/
gboolean moloch_nids_has_tag(MolochSession_t *session, const char *tagName)
{
    uint32_t tagValue;

    if (!session->fields[tagsField])
        return FALSE;

    if ((tagValue = moloch_db_peek_tag(tagName)) == 0)
        return FALSE;

    MolochInt_t          *hint;
    HASH_FIND_INT(i_, *(session->fields[tagsField]->ihash), tagValue, hint);
    return hint != 0;
}
/******************************************************************************/
void moloch_nids_add_protocol(MolochSession_t *session, const char *protocol)
{
    moloch_field_string_add(protocolField, session, protocol, -1, TRUE);
}
/******************************************************************************/
gboolean moloch_nids_has_protocol(MolochSession_t *session, const char *protocol)
{
    if (!session->fields[protocolField])
        return FALSE;

    MolochString_t          *hstring;
    HASH_FIND(s_, *(session->fields[protocolField]->shash), protocol, hstring);
    return hstring != 0;
}
/******************************************************************************/
void moloch_nids_add_tag(MolochSession_t *session, const char *tag) {
    moloch_nids_incr_outstanding(session);
    moloch_db_get_tag(session, tagsField, tag, moloch_nids_get_tag_cb);

    if (session->stopSaving == 0 && HASH_COUNT(s_, config.dontSaveTags)) {
        MolochString_t *tstring;

        HASH_FIND(s_, config.dontSaveTags, tag, tstring);
        if (tstring) {
            session->stopSaving = (int)(long)tstring->uw;
        }
    }
}

/******************************************************************************/
void moloch_nids_add_tag_type(MolochSession_t *session, int tagtype, const char *tag) {
    moloch_nids_incr_outstanding(session);
    moloch_db_get_tag(session, tagtype, tag, moloch_nids_get_tag_cb);

    if (session->stopSaving == 0 && HASH_COUNT(s_, config.dontSaveTags)) {
        MolochString_t *tstring;

        HASH_FIND(s_, config.dontSaveTags, tag, tstring);
        if (tstring) {
            session->stopSaving = (long)tstring->uw;
        }
    }
}

/******************************************************************************/
void moloch_nids_cb_tcp(struct tcp_stream *a_tcp, void *UNUSED(params))
{
    MolochSession_t *session;
    char             sessionId[1024];

    //LOG("TCP (%d) - %s", a_tcp->nids_state, moloch_friendly_session_id(IPPROTO_TCP, a_tcp->addr.saddr, a_tcp->addr.source, a_tcp->addr.daddr, a_tcp->addr.dest));
    //fflush(stdout);

    switch (a_tcp->nids_state) {
    case NIDS_JUST_EST:

        moloch_session_id(sessionId, a_tcp->addr.saddr, htons(a_tcp->addr.source), a_tcp->addr.daddr, htons(a_tcp->addr.dest));

        HASH_FIND(h_, sessions[SESSION_TCP], sessionId, session);

        if (session) {
            if (session->stopSPI) {
                a_tcp->client.collect = 0;
                a_tcp->server.collect = 0;
                return;
            }
            session->haveNidsTcp = 1;
            DLL_PUSH_TAIL(tcp_, &tcpWriteQ, session);
            a_tcp->user = session;
            a_tcp->client.collect++;
            a_tcp->server.collect++;
        } else {
            LOG("ERROR - no session for %s", moloch_friendly_session_id(IPPROTO_TCP, a_tcp->addr.saddr, a_tcp->addr.source, a_tcp->addr.daddr, a_tcp->addr.dest));
        }
        if (pluginsCbs & MOLOCH_PLUGIN_TCP)
            moloch_plugins_cb_tcp(session, a_tcp);
        return;
    case NIDS_DATA: {

        // libcurl requires we check on first data also since no connect callback
        if (a_tcp->client.offset == 0 || a_tcp->server.offset == 0) {
            moloch_session_id(sessionId, a_tcp->addr.saddr, htons(a_tcp->addr.source), a_tcp->addr.daddr, htons(a_tcp->addr.dest));
            uint32_t hash = HASH_HASH(sessions[SESSION_TCP], sessionId);
            HASH_FIND_HASH(h_, sessions[SESSION_TCP], hash, sessionId, session);

            if (moloch_http_is_moloch(hash, sessionId)) {
                if (config.debug)
                    LOG("Ignoring connection %s", moloch_friendly_session_id(session->protocol, session->addr1, session->port1, session->addr2, session->port2));
                session->stopSPI = 1;
                session->stopSaving = 1;
                a_tcp->client.collect = 0;
                a_tcp->server.collect = 0;
            }
        }
            
        session = a_tcp->user;
        if (!session) {
            LOG("ERROR - data - a_tcp->user not set for %s", moloch_friendly_session_id(IPPROTO_TCP, a_tcp->addr.saddr, a_tcp->addr.source, a_tcp->addr.daddr, a_tcp->addr.dest));
            a_tcp->client.collect = 0;
            a_tcp->server.collect = 0;
            return;
        }

        if (a_tcp->client.count > 0x7fff0000 || a_tcp->server.count > 0x7fff0000) {
            LOG("ERROR - Almost 2G in tcp session, preventing libnids crash for %s", moloch_friendly_session_id(IPPROTO_TCP, a_tcp->addr.saddr, a_tcp->addr.source, a_tcp->addr.daddr, a_tcp->addr.dest));
            a_tcp->client.collect = 0;
            a_tcp->server.collect = 0;
            return;
        }

        if (a_tcp->client.count_new) {
            int countNew      = a_tcp->client.count_new;
            int count         = a_tcp->client.count - a_tcp->client.offset;
            unsigned char *dataNew = (unsigned char*)(a_tcp->client.data + (a_tcp->client.count - a_tcp->client.offset - countNew));
            unsigned char *data    = (unsigned char*)a_tcp->client.data;

            if (a_tcp->client.offset == session->consumed[1])
                moloch_parsers_classify_tcp(session, data, count, 1);

            int i;
            int totConsumed = 0;
            int consumed = 0;

            for (i = 0; i < session->parserNum; i++) {
                if (session->parserInfo[i].parserFunc) {
                    consumed = session->parserInfo[i].parserFunc(session, session->parserInfo[i].uw, dataNew + totConsumed, countNew - totConsumed, 1);
                    if (consumed) {
                        totConsumed += consumed;
                        session->consumed[1] += consumed;
                    }

                    if (consumed >= countNew)
                        break;
                }
            }

            if (config.yara) {
                moloch_yara_execute(session, data, count, a_tcp->client.count-countNew == session->consumed[1]);

                nids_discard(a_tcp, session->offsets[1]);
                session->offsets[1] = countNew;
            } else if (session->offsets[1] == 0) {
                session->offsets[1] = countNew;
                nids_discard(a_tcp, 0);
            }

            if (a_tcp->client.offset == 0) {
                session->firstBytesLen[1] = MIN(8, count);
                memcpy(session->firstBytes[1], data, session->firstBytesLen[1]);
            }

            session->databytes[1] += countNew;
        }

        if (a_tcp->server.count_new) {
            int countNew      = a_tcp->server.count_new;
            int count         = a_tcp->server.count - a_tcp->server.offset;
            unsigned char *dataNew = (unsigned char*)(a_tcp->server.data + (a_tcp->server.count - a_tcp->server.offset - countNew));
            unsigned char *data    = (unsigned char*)a_tcp->server.data;

            if (a_tcp->server.offset == session->consumed[0])
                moloch_parsers_classify_tcp(session, data, count, 0);

            int i;
            int totConsumed = 0;
            int consumed = 0;
            for (i = 0; i < session->parserNum; i++) {
                if (session->parserInfo[i].parserFunc) {
                    consumed = session->parserInfo[i].parserFunc(session, session->parserInfo[i].uw, dataNew + totConsumed, countNew - totConsumed, 0);

                    if (consumed) {
                        totConsumed += consumed;
                        session->consumed[0] += consumed;
                    }

                    if (consumed >= countNew)
                        break;
                }
            }

            if (config.yara) {
                moloch_yara_execute(session, data, count, a_tcp->server.count-countNew == session->consumed[0]);

                nids_discard(a_tcp, session->offsets[0]);
                session->offsets[0] = countNew;
            } else if (session->offsets[0] == 0) {
                session->offsets[0] = countNew;
                nids_discard(a_tcp, 0);
            }

            if (a_tcp->server.offset == 0) {
                session->firstBytesLen[0] = MIN(8, count);
                memcpy(session->firstBytes[0], data, session->firstBytesLen[0]);
            }

            session->databytes[0] += countNew;
        }


        if (pluginsCbs & MOLOCH_PLUGIN_TCP)
            moloch_plugins_cb_tcp(session, a_tcp);
        return;
    }
    default:
        session = a_tcp->user;

        if (!session) {
            LOG("ERROR - default (%d) - a_tcp->user not set for %s", a_tcp->nids_state, moloch_friendly_session_id(IPPROTO_TCP, a_tcp->addr.saddr, a_tcp->addr.source, a_tcp->addr.daddr, a_tcp->addr.dest));
            return;
        }

        if (config.yara) {
            if (a_tcp->client.count != a_tcp->client.offset) {
                moloch_yara_execute(session, (unsigned char*)a_tcp->client.data, a_tcp->client.count - a_tcp->client.offset, a_tcp->client.offset == 0);
            }

            if (a_tcp->server.count != a_tcp->server.offset) {
                moloch_yara_execute(session, (unsigned char*)a_tcp->server.data, a_tcp->server.count - a_tcp->server.offset, a_tcp->server.offset == 0);
            }
        }

        if (pluginsCbs & MOLOCH_PLUGIN_TCP)
            moloch_plugins_cb_tcp(session, a_tcp);
        //LOG("TCP %d ", a_tcp->nids_state);
        moloch_nids_save_session(session);
        a_tcp->user = 0;
        return;
    }
}

/******************************************************************************/
void moloch_nids_session_free (MolochSession_t *session)
{
    if (session->q_next) {
        switch (session->protocol) {
        case IPPROTO_TCP:
            DLL_REMOVE(q_, &tcpSessionQ, session);
            break;
        case IPPROTO_UDP:
            DLL_REMOVE(q_, &udpSessionQ, session);
            break;
        case IPPROTO_ICMP:
            DLL_REMOVE(q_, &icmpSessionQ, session);
            break;
        }
    }

    if (session->tcp_next) {
        DLL_REMOVE(tcp_, &tcpWriteQ, session);
    }

    g_array_free(session->filePosArray, TRUE);
    g_array_free(session->fileLenArray, TRUE);
    g_array_free(session->fileNumArray, TRUE);

    if (session->rootId)
        g_free(session->rootId);

    if (session->parserInfo) {
        int i;
        for (i = 0; i < session->parserNum; i++) {
            if (session->parserInfo[i].parserFreeFunc)
                session->parserInfo[i].parserFreeFunc(session, session->parserInfo[i].uw);
        }
        free(session->parserInfo);
    }

    if (session->pluginData)
        MOLOCH_SIZE_FREE(pluginData, session->pluginData);
    moloch_field_free(session);
    MOLOCH_TYPE_FREE(MolochSession_t, session);
}
/******************************************************************************/
void moloch_nids_syslog(int type, int errnum, struct ip *iph, void *data)
{
    static int count[100];

    if (count[errnum] == 100)
        return;

    char saddr[20], daddr[20];

    switch (type) {
    case NIDS_WARN_IP:
	if (errnum != NIDS_WARN_IP_HDR) {
	    strcpy(saddr, int_ntoa(iph->ip_src.s_addr));
	    strcpy(daddr, int_ntoa(iph->ip_dst.s_addr));
	    LOG("NIDSIP:%s %s, packet (apparently) from %s to %s",
		   moloch_writer_name(), nids_warnings[errnum], saddr, daddr);
	} else {
            if (iph->ip_hl < 5) {
                LOG("NIDSIP:%s %s - %d (iph->ip_hl) < 5", moloch_writer_name(), nids_warnings[errnum], iph->ip_hl);
            } else if (iph->ip_v != 4) {
                LOG("NIDSIP:%s %s - %d (iph->ip_v) != 4", moloch_writer_name(), nids_warnings[errnum], iph->ip_v);
            } else if (ntohs(iph->ip_len) < iph->ip_hl << 2) {
                LOG("NIDSIP:%s %s - %d < %d (iph->ip_len < iph->ip_hl)", moloch_writer_name(), nids_warnings[errnum], ntohs(iph->ip_len), iph->ip_hl << 2);
            } else {
                LOG("NIDSIP:%s %s - Length issue, was packet truncated?", moloch_writer_name(), nids_warnings[errnum]);
            }
        }
	break;

    case NIDS_WARN_TCP:
	strcpy(saddr, int_ntoa(iph->ip_src.s_addr));
	strcpy(daddr, int_ntoa(iph->ip_dst.s_addr));
        if (errnum == NIDS_WARN_TCP_TOOMUCH || errnum == NIDS_WARN_TCP_BIGQUEUE) {
            /* ALW - Should do something with it */
	} else if (errnum != NIDS_WARN_TCP_HDR)
	    LOG("NIDSTCP:%s %s,from %s:%hu to  %s:%hu", moloch_writer_name(), nids_warnings[errnum],
		saddr, ntohs(((struct tcphdr *) data)->th_sport), daddr,
		ntohs(((struct tcphdr *) data)->th_dport));
	else
	    LOG("NIDSTCP:%s %s,from %s to %s", moloch_writer_name(),
		nids_warnings[errnum], saddr, daddr);
	break;

    default:
	LOG("NIDS: Unknown error type:%d errnum:%d", type, errnum);
    }

    count[errnum]++;
    if (count[errnum] == 100) {
        LOG("NIDS: No longer displaying above error");
    }
}

/******************************************************************************/
/* When processing many pcap files we don't start the next file
 * until there are less then 20 outstanding es requests
 */
gboolean moloch_nids_next_file_gfunc (gpointer UNUSED(user_data))
{
    if (moloch_http_queue_length(esServer) > 20)
        return TRUE;

    moloch_nids_init_nids();
    return FALSE;
}
/******************************************************************************/
gboolean moloch_nids_monitor_gfunc (gpointer UNUSED(user_data))
{
    if (DLL_COUNT(s_, &monitorQ) == 0)
        return TRUE;

    if (moloch_nids_next_file()) {
        g_timeout_add(10, moloch_nids_next_file_gfunc, 0);
        return FALSE;
    }

    return TRUE;
}
/******************************************************************************/
/* Used when reading packets from an interface thru libnids/libpcap */
gboolean moloch_nids_interface_dispatch()
{
    nids_dispatch(config.packetsPerPoll);
    return TRUE;
}
/******************************************************************************/
/* Used when reading packets from a file thru libnids/libpcap */
gboolean moloch_nids_file_dispatch()
{
    // pause reading if too many waiting disk operations
    if (moloch_writer_queue_length() > 10) {
        return TRUE;
    }

    // pause reading if too many waiting ES operations
    if (moloch_http_queue_length(esServer) > 100) {
        return TRUE;
    }

    int r = nids_dispatch(config.packetsPerPoll);

    // Some kind of failure, move to the next file or quit
    if (r <= 0) {
        if (config.pcapDelete && r == 0) {
            if (config.debug)
                LOG("Deleting %s", offlinePcapFilename);
            int rc = unlink(offlinePcapFilename);
            if (rc != 0)
                LOG("Failed to delete file %s %s (%d)", offlinePcapFilename, strerror(errno), errno);
        }
        if (moloch_nids_next_file()) {
            g_timeout_add(10, moloch_nids_next_file_gfunc, 0);
            return FALSE;
        }

        if (config.pcapMonitor)
            g_timeout_add(100, moloch_nids_monitor_gfunc, 0);
        else
            moloch_quit();
        return FALSE;
    }

    return TRUE;
}

/******************************************************************************/
pcap_t *
moloch_pcap_open_live(const char *source, int snaplen, int promisc, int to_ms, char *errbuf)
{
    pcap_t *p;
    int status;

    p = pcap_create(source, errbuf);
    if (p == NULL)
        return (NULL);
    status = pcap_set_snaplen(p, snaplen);
    if (status < 0)
        goto fail;
    status = pcap_set_promisc(p, promisc);
    if (status < 0)
        goto fail;
    status = pcap_set_timeout(p, to_ms);
    if (status < 0)
        goto fail;
    status = pcap_set_buffer_size(p, config.pcapBufferSize);
    if (status < 0)
        goto fail;
    status = pcap_activate(p);
    if (status < 0)
        goto fail;
    status = pcap_setnonblock(p, TRUE, errbuf);
    if (status < 0) {
        pcap_close(p);
        return (NULL);
    }

    return (p);
fail:
    if (status == PCAP_ERROR)
        snprintf(errbuf, PCAP_ERRBUF_SIZE, "%s: %s", source,
            pcap_geterr(p));
    else if (status == PCAP_ERROR_NO_SUCH_DEVICE ||
        status == PCAP_ERROR_PERM_DENIED)
        snprintf(errbuf, PCAP_ERRBUF_SIZE, "%s: %s (%s)", source,
            pcap_statustostr(status), pcap_geterr(p));
    else
        snprintf(errbuf, PCAP_ERRBUF_SIZE, "%s: %s", source,
            pcap_statustostr(status));
    pcap_close(p);
    return (NULL);
}
/******************************************************************************/
uint32_t moloch_nids_dropped_packets()
{
    struct pcap_stat ps;
    if (nids_params.pcap_desc) {
        if (pcap_stats(nids_params.pcap_desc, &ps))
            return 0;

        return ps.ps_drop - initialDropped;
    }

    return 0;
}
/******************************************************************************/
uint32_t moloch_nids_monitoring_sessions()
{
    return HASH_COUNT(h_, sessions[SESSION_TCP]) + HASH_COUNT(h_, sessions[SESSION_UDP]) + HASH_COUNT(h_, sessions[SESSION_ICMP]);
}
/******************************************************************************/
void moloch_nids_process_udp(MolochSession_t *session, struct udphdr *udphdr, unsigned char *data, int len, int which)
{
    if (pluginsCbs & MOLOCH_PLUGIN_UDP)
        moloch_plugins_cb_udp(session, udphdr, data, len);

    if (session->firstBytesLen[which] == 0) {
        moloch_parsers_classify_udp(session, data, len, which);
        session->firstBytesLen[which] = MIN(8, len);
        memcpy(session->firstBytes[which], data, session->firstBytesLen[which]);
    }
}
/******************************************************************************/
void moloch_nids_pcap_opened() 
{
    pcapFileHeader.snaplen = pcap_snapshot(nids_params.pcap_desc);
    pcapFileHeader.linktype = dlt_to_linktype(pcap_datalink(nids_params.pcap_desc)) | pcap_datalink_ext(nids_params.pcap_desc);
    if (config.debug)
        LOG("linktype %x", pcapFileHeader.linktype);

    if (config.dontSaveBPFs) {
        int i;
        if (bpf_programs) {
            for (i = 0; i < config.dontSaveBPFsNum; i++) {
                pcap_freecode(&bpf_programs[i]);
            }
        } else {
            bpf_programs= malloc(config.dontSaveBPFsNum*sizeof(struct bpf_program));
        }
        for (i = 0; i < config.dontSaveBPFsNum; i++) {
            if (pcap_compile(nids_params.pcap_desc, &bpf_programs[i], config.dontSaveBPFs[i], 0, PCAP_NETMASK_UNKNOWN) == -1) {
                LOG("ERROR - Couldn't compile filter: '%s' with %s", config.dontSaveBPFs[i], pcap_geterr(nids_params.pcap_desc));
                exit(1);
            }
        }
    }

    if (offlineFile && moloch_writer_next_input)
        moloch_writer_next_input(offlineFile, offlinePcapFilename);
}

/******************************************************************************/
static pcap_t *closeNextOpen = 0;
int moloch_nids_next_file()
{
    char         errbuf[1024];
    gchar       *fullfilename;

    if (config.pcapReadFiles) {
        static int pcapFilePos = 0;

        fullfilename = config.pcapReadFiles[pcapFilePos];

        errbuf[0] = 0;
        closeNextOpen = nids_params.pcap_desc;
        if (!fullfilename) {
            goto filesDone;
        }
        pcapFilePos++;

        LOG ("Processing %s", fullfilename);
        nids_params.pcap_desc = pcap_open_offline(fullfilename, errbuf);

        if (!nids_params.pcap_desc) {
            LOG("Couldn't process '%s' error '%s'", fullfilename, errbuf);
            return moloch_nids_next_file();
        }
        offlineFile = pcap_file(nids_params.pcap_desc);

        if (!realpath(fullfilename, offlinePcapFilename)) {
            LOG("ERROR - pcap open failed - Couldn't realpath file: '%s' with %d", fullfilename, errno);
            exit(1);
        }

        moloch_nids_pcap_opened();
        return 1;
    }

filesDone:

    if (config.pcapReadDirs) {
        static int   pcapDirPos = 0;
        static GDir *pcapGDir[21];
        static char *pcapBase[21];
        static int   pcapGDirLevel = -1;
        GError      *error = 0;

        if (pcapGDirLevel == -2) {
            goto dirsDone;
        }

        if (pcapGDirLevel == -1) {
            pcapGDirLevel = 0;
            pcapBase[0] = config.pcapReadDirs[pcapDirPos];
            if (!pcapBase[0]) {
                pcapGDirLevel = -2;
                goto dirsDone;
            }
        }

        if (!pcapGDir[pcapGDirLevel]) {
            pcapGDir[pcapGDirLevel] = g_dir_open(pcapBase[pcapGDirLevel], 0, &error);
            if (error) {
                LOG("ERROR: Couldn't open pcap directory: Receive Error: %s", error->message);
                exit(0);
            }
        }
        const gchar *filename;
        while (1) {
            filename = g_dir_read_name(pcapGDir[pcapGDirLevel]);

            // No more files, stop processing this directory
            if (!filename) {
                break;
            }

            // Skip hidden files/directories
            if (filename[0] == '.')
                continue;

            fullfilename = g_build_filename (pcapBase[pcapGDirLevel], filename, NULL);

            // If recursive option and a directory then process all the files in that dir
            if (config.pcapRecursive && g_file_test(fullfilename, G_FILE_TEST_IS_DIR)) {
                if (pcapGDirLevel >= 20)
                    continue;
                pcapBase[pcapGDirLevel+1] = fullfilename;
                pcapGDirLevel++;
                return moloch_nids_next_file();
            }

            if (!g_regex_match(config.offlineRegex, filename, 0, NULL)) {
                g_free(fullfilename);
                continue;
            }

            if (!realpath(fullfilename, offlinePcapFilename)) {
                g_free(fullfilename);
                continue;
            }

            if (config.pcapSkip && moloch_db_file_exists(offlinePcapFilename)) {
                if (config.debug)
                    LOG("Skipping %s", fullfilename);
                g_free(fullfilename);
                continue;
            }

            LOG ("Processing %s", fullfilename);
            errbuf[0] = 0;
            closeNextOpen = nids_params.pcap_desc;
            nids_params.pcap_desc = pcap_open_offline(fullfilename, errbuf);
            if (!nids_params.pcap_desc) {
                LOG("Couldn't process '%s' error '%s'", fullfilename, errbuf);
                g_free(fullfilename);
                continue;
            }
            offlineFile = pcap_file(nids_params.pcap_desc);
            moloch_nids_pcap_opened();
            g_free(fullfilename);
            return 1;
        }
        g_dir_close(pcapGDir[pcapGDirLevel]);
        pcapGDir[pcapGDirLevel] = 0;

        if (pcapGDirLevel > 0) {
            g_free(pcapBase[pcapGDirLevel]);
            pcapGDirLevel--;
            return moloch_nids_next_file();
        } else {
            pcapDirPos++;
            pcapGDirLevel = -1;
            return moloch_nids_next_file();
        }

    }

dirsDone:
    while (DLL_COUNT(s_, &monitorQ) > 0) {
        MolochString_t *string;
        DLL_POP_HEAD(s_, &monitorQ, string);
        fullfilename = string->str;
        MOLOCH_TYPE_FREE(MolochString_t, string);

        if (!realpath(fullfilename, offlinePcapFilename)) {
            g_free(fullfilename);
            continue;
        }

        if (config.pcapSkip && moloch_db_file_exists(offlinePcapFilename)) {
            if (config.debug)
                LOG("Skipping %s", fullfilename);
            g_free(fullfilename);
            continue;
        }

        LOG ("Processing %s", fullfilename);
        errbuf[0] = 0;
        closeNextOpen = nids_params.pcap_desc;
        nids_params.pcap_desc = pcap_open_offline(fullfilename, errbuf);
        if (!nids_params.pcap_desc) {
            LOG("Couldn't process '%s' error '%s'", fullfilename, errbuf);
            g_free(fullfilename);
            continue;
        }
        offlineFile = pcap_file(nids_params.pcap_desc);
        moloch_nids_pcap_opened();
        g_free(fullfilename);
        return 1;
    }
    return 0;
}
/******************************************************************************/
void moloch_nids_root_init()
{
    char errbuf[1024];
    if (!config.pcapReadOffline) {
#ifdef SNF
        nids_params.pcap_desc = pcap_open_live(config.interface, 8096, 1, 500, errbuf);
#else
        nids_params.pcap_desc = moloch_pcap_open_live(config.interface, 8096, 1, 500, errbuf);
#endif


        if (!nids_params.pcap_desc) {
            LOG("pcap open live failed! %s", errbuf);
            exit(1);
        }
        moloch_nids_pcap_opened();
    }
}
/******************************************************************************/
void moloch_nids_init_nids()
{
    nids_unregister_tcp(moloch_nids_cb_tcp);
    nids_unregister_ip(moloch_nids_cb_ip);
    int rc = nids_init();
    if (rc == 0) {
        LOG("libnids error: %s", nids_errbuf);
        if (nids_params.pcap_desc) {
            LOG("libpcap error: %s", pcap_geterr(nids_params.pcap_desc));
        }
        exit(1);
    }

    /* Have to do this after nids_init, libnids has issues */
    if (closeNextOpen) {
        pcap_close(closeNextOpen);
        closeNextOpen = 0;
    }

    GVoidFunc dispatch;
    if (config.pcapReadOffline)
        dispatch = (GVoidFunc)&moloch_nids_file_dispatch;
    else
        dispatch = (GVoidFunc)&moloch_nids_interface_dispatch;

    if (nids_getfd() == -1) {
        g_timeout_add(0, (GSourceFunc)dispatch, NULL);
    } else {
        moloch_watch_fd(nids_getfd(), MOLOCH_GIO_READ_COND, (MolochWatchFd_func)dispatch, NULL);
    }


    static struct nids_chksum_ctl ctl;
    ctl.netaddr = 0;
    ctl.mask = 0;
    ctl.action = NIDS_DONT_CHKSUM;
    nids_register_chksum_ctl(&ctl, 1);

    nids_register_tcp(moloch_nids_cb_tcp);
    nids_register_ip(moloch_nids_cb_ip);
}
/******************************************************************************/
void moloch_nids_monitor_dir(char *dirname);
static void
moloch_nids_monitor_changed (GFileMonitor      *UNUSED(monitor),
                             GFile             *file,
                             GFile             *UNUSED(other_file),
                             GFileMonitorEvent  event_type,
                             gpointer           UNUSED(user_data))
{
    // Monitor new directories?
    if (config.pcapRecursive &&
        event_type == G_FILE_MONITOR_EVENT_CREATED &&
        g_file_query_file_type(file, G_FILE_QUERY_INFO_NONE, NULL) == G_FILE_TYPE_DIRECTORY) {

        gchar *path = g_file_get_path(file);
        moloch_nids_monitor_dir(path);
        g_free(path);

        return;
    }

    if (event_type != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
        return;

    gchar *basename = g_file_get_path(file);
    if (!g_regex_match(config.offlineRegex, basename, 0, NULL)) {
        g_free(basename);
        return;
    }
    g_free(basename);

    gchar *path = g_file_get_path(file);
    MolochString_t *string = MOLOCH_TYPE_ALLOC0(MolochString_t);
    string->str = path;

    if (config.debug) 
        LOG("Monitor enqueing %s", string->str);
    DLL_PUSH_TAIL(s_, &monitorQ, string);
}
/******************************************************************************/
void moloch_nids_monitor_dir(char *dirname)
{
    GError      *error = 0;
    if (config.debug)
        LOG("Monitoring %s", dirname);
    if (error) {
        LOG("ERROR: Couldn't open pcap directory %s: Receive Error: %s", dirname, error->message);
        exit(0);
    }

    GFile *filedir = g_file_new_for_path(dirname);
    GFileMonitor *monitor = g_file_monitor_directory (filedir, 0, NULL, &error);
    g_file_monitor_set_rate_limit(monitor, 0);
    g_signal_connect (monitor, "changed", G_CALLBACK (moloch_nids_monitor_changed), 0);

    if (!config.pcapRecursive)
        return;
    GDir *dir = g_dir_open(dirname, 0, &error);
    while (1) {
        const gchar *filename = g_dir_read_name(dir);

        // No more files, stop processing this directory
        if (!filename) {
            break;
        }

        // Skip hidden files/directories
        if (filename[0] == '.')
            continue;

        gchar *fullfilename = g_build_filename (dirname, filename, NULL);

        if (g_file_test(fullfilename, G_FILE_TEST_IS_DIR)) {
            moloch_nids_monitor_dir(fullfilename);
        }
        g_free(fullfilename);
    }
    g_dir_close(dir);
}
/******************************************************************************/
void moloch_nids_init_monitor()
{
    int          dir;

    for (dir = 0; config.pcapReadDirs[dir] && config.pcapReadDirs[dir][0]; dir++) {
        moloch_nids_monitor_dir(config.pcapReadDirs[dir]);
    }
}
/******************************************************************************/
void moloch_nids_init()
{

    LOG("%s", pcap_lib_version());

    pcapFileHeader.magic = 0xa1b2c3d4;
    pcapFileHeader.version_major = PCAP_VERSION_MAJOR;
    pcapFileHeader.version_minor = PCAP_VERSION_MINOR;

    pcapFileHeader.thiszone = 0;
    pcapFileHeader.sigfigs = 0;

    config.maxWriteBuffers = config.pcapReadOffline?10:2000;

    protocolField = moloch_field_define("general", "termfield",
        "protocols", "Protocols", "prot-term",
        "Protocols set for session",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_COUNT | MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);

    mac1Field = moloch_field_define("general", "lotermfield",
        "mac.src", "Src MAC", "mac1-term",
        "Source ethernet mac addresses set for session",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_COUNT | MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);

    mac2Field = moloch_field_define("general", "lotermfield",
        "mac.dst", "Dst MAC", "mac2-term",
        "Destination ethernet mac addresses set for session",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_COUNT | MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);

    moloch_field_define("general", "lotermfield",
        "mac", "Src or Dst MAC", "macall",
        "Shorthand for mac.src or mac.dst",
        0,  MOLOCH_FIELD_FLAG_FAKE,
        "regex", "^mac\\\\.(?:(?!\\\\.cnt$).)*$",
        NULL);

    vlanField = moloch_field_define("general", "integer",
        "vlan", "VLan", "vlan",
        "vlan value",
        MOLOCH_FIELD_TYPE_INT_HASH,  MOLOCH_FIELD_FLAG_COUNT | MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);

    tagsField = moloch_field_by_db("ta");

    HASH_INIT(h_, sessions[SESSION_UDP], moloch_session_hash, moloch_session_cmp);
    HASH_INIT(h_, sessions[SESSION_TCP], moloch_session_hash, moloch_session_cmp);
    HASH_INIT(h_, sessions[SESSION_ICMP], moloch_session_hash, moloch_session_cmp);
    DLL_INIT(tcp_, &tcpWriteQ);
    DLL_INIT(q_, &tcpSessionQ);
    DLL_INIT(q_, &udpSessionQ);
    DLL_INIT(q_, &icmpSessionQ);
    DLL_INIT(s_, &monitorQ);

    nids_params.n_hosts = 1024;
    nids_params.tcp_workarounds = 1;
    nids_params.one_loop_less = 0;
    nids_params.scan_num_hosts = 0;
    nids_params.scan_num_ports = 0;
    nids_params.syslog = moloch_nids_syslog;
    nids_params.n_tcp_streams = config.maxStreams;

    if (config.pcapReadOffline) {
        if (config.dryRun || !config.copyPcap) {
            moloch_writers_start("inplace");
        } else {
            moloch_writers_start(NULL);
        }

    } else {
        if (config.dryRun) {
            moloch_writers_start("null");
        } else {
            moloch_writers_start(NULL);
        }
    }

    if (config.bpf)
        nids_params.pcap_filter = config.bpf;

    if (config.pcapReadOffline) {
        moloch_nids_next_file();
        if (!nids_params.pcap_desc) {
            if (config.pcapMonitor) {
                g_timeout_add(100, moloch_nids_monitor_gfunc, 0);
            } else {
                LOG("No files to process.");
                exit(0);
            }
        }
    }

    if (nids_params.pcap_desc)
        moloch_nids_init_nids();

    if (config.pcapMonitor)
        moloch_nids_init_monitor();
}
/******************************************************************************/
void moloch_nids_exit() {

    config.exiting = 1;
    LOG("sessions: %d tcp: %d udp: %d icmp: %d",
            moloch_nids_monitoring_sessions(),
            tcpSessionQ.q_count,
            udpSessionQ.q_count,
            icmpSessionQ.q_count);

    nids_unregister_tcp(moloch_nids_cb_tcp);
    nids_unregister_ip(moloch_nids_cb_ip);
    nids_exit();

    int i;
    for (i = 0; i < SESSION_MAX; i++) {

#ifdef PRINT_BUCKETS
        // Print out the histogram for buckets, see how we are doing
        printf("\nBuckets for %d:\n", i);
        int buckets[51];
        int total[51];
        memset(buckets, 0, sizeof(buckets));
        memset(total, 0, sizeof(total));
        int b;
        for ( b = 0;  b < sessions[i].size;  b++) {
            if (sessions[i].buckets[b].h_count >= 50) {
                buckets[50]++;
                total[50] += sessions[i].buckets[b].h_count;
            } else {
                buckets[(sessions[i].buckets[b].h_count)]++;
                total[(sessions[i].buckets[b].h_count)] += sessions[i].buckets[b].h_count;
            }
        }
        for ( b = 0;  b <= 50;  b++) {
            if (buckets[b])
                printf(" %2d: %7d %7d\n", b, buckets[b], total[b]);
        }
#endif

        MolochSession_t *hsession;
        HASH_FORALL_POP_HEAD(h_, sessions[i], hsession,
            moloch_db_save_session(hsession, TRUE);
        );
    }

    if (!config.dryRun && config.copyPcap) {
        moloch_writer_exit();
    }
}
