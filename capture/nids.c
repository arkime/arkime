/* nids.c  -- Functions for dealing with libnids
 *
 * Copyright 2012 AOL Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include "glib.h"
#include "nids.h"
#include "pcap.h"
#include "magic.h"
#include "moloch.h"

/******************************************************************************/
extern MolochConfig_t        config;
extern GMainLoop            *mainLoop;
extern uint32_t              pluginsCbs;
extern void                 *esServer;

static MolochSessionHead_t   tcpSessionQ;
static MolochSessionHead_t   udpSessionQ;
static MolochSessionHead_t   icmpSessionQ;
static MolochSessionHead_t   tcpWriteQ;


static int64_t               dumperFilePos = 0;
static int                   dumperBufPos = 0;
static int                   dumperBufMax = 0;
static int                   dumperFd = -1;
static char                  dumperBuf[0x1fffff + 4096];
static uint32_t              dumperId;
static uint32_t              initialDropped = 0;
static char                  offlinePcapFilename[PATH_MAX+1];

uint64_t                     totalPackets = 0;
uint64_t                     totalBytes = 0;
uint64_t                     totalSessions = 0;

/******************************************************************************/

HASH_VAR(h_, sessions, MolochSessionHead_t, 199337);

/******************************************************************************/
void moloch_nids_session_free (MolochSession_t *session);
void moloch_nids_process_udp(MolochSession_t *session, struct udphdr   *udphdr, unsigned char *data, int len);
int  moloch_nids_next_file();
void moloch_nids_init_nids();

/******************************************************************************/
uint32_t moloch_nids_session_hash(const void *key)
{
    char *p = (char *)key;
    return (p[4] & 0xff) << 24 | (p[6] & 0xff) << 16 | (p[10] & 0xff) << 8 | (p[12] & 0xff);
}

/******************************************************************************/
int moloch_nids_session_cmp(const void *keyv, const void *elementv)
{
    unsigned char *key = (unsigned char*)keyv;
    MolochSession_t *element = (MolochSession_t *)elementv;

    if (key[0] != element->protocol) return 0;

    if (element->addr1 < element->addr2) {
        return key[5] == ((element->port1 >> 8) & 0xff) && 
               key[6] == (element->port1 & 0xff) &&
               key[11] == ((element->port2 >> 8) & 0xff) &&
               key[12] == (element->port2 & 0xff) &&
               memcmp(key + 1, &element->addr1, 4) == 0 &&
               memcmp(key + 7, &element->addr2, 4) == 0;
    } else {
        return key[5] == ((element->port2 >> 8) & 0xff) && 
               key[6] == (element->port2 & 0xff) &&
               key[11] == ((element->port1 >> 8) & 0xff) &&
               key[12] == (element->port1 & 0xff) &&
               memcmp(key + 1, &element->addr2, 4) == 0 &&
               memcmp(key + 7, &element->addr1, 4) == 0;
    }
}

/******************************************************************************/
void moloch_nids_certs_free (MolochCertsInfo_t *certs) 
{
    MolochString_t *string;

    while (DLL_POP_HEAD(s_, &certs->alt, string)) {
        free(string->str);
        free(string);
    }

    while (DLL_POP_HEAD(s_, &certs->issuer.commonName, string)) {
        free(string->str);
        free(string);
    }

    while (DLL_POP_HEAD(s_, &certs->subject.commonName, string)) {
        free(string->str);
        free(string);
    }

    if (certs->issuer.orgName)
        free(certs->issuer.orgName);
    if (certs->subject.orgName)
        free(certs->subject.orgName);
    if (certs->serialNumber)
        free(certs->serialNumber);

    free(certs);
}

/******************************************************************************/
uint32_t moloch_nids_certs_hash(const void *key)
{
    MolochCertsInfo_t *ci = (MolochCertsInfo_t *)key;

    return ((ci->serialNumber[0] << 28) | 
            (ci->serialNumber[ci->serialNumberLen-1] << 24) |
            (ci->issuer.commonName.s_count << 18) |
            (ci->issuer.orgName?ci->issuer.orgName[0] << 12:0) |
            (ci->subject.commonName.s_count << 6) |
            (ci->subject.orgName?ci->subject.orgName[0]:0));
}

/******************************************************************************/
int moloch_nids_certs_cmp(const void *keyv, const void *elementv)
{
    MolochCertsInfo_t *key = (MolochCertsInfo_t *)keyv;
    MolochCertsInfo_t *element = (MolochCertsInfo_t *)elementv;

    if ( !((key->serialNumberLen == element->serialNumberLen) &&
           (memcmp(key->serialNumber, element->serialNumber, element->serialNumberLen) == 0) &&
           (key->issuer.commonName.s_count == element->issuer.commonName.s_count) &&
           (key->issuer.orgName == element->issuer.orgName || strcmp(key->issuer.orgName, element->issuer.orgName) == 0) &&
           (key->subject.commonName.s_count == element->subject.commonName.s_count) &&
           (key->subject.orgName == element->subject.orgName || strcmp(key->subject.orgName, element->subject.orgName) == 0)
          )
       ) {

        return 0;
    }

    MolochString_t *kstr, *estr;
    for (kstr = key->issuer.commonName.s_next, estr = element->issuer.commonName.s_next;
         kstr != (void *)&(key->issuer.commonName);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    for (kstr = key->subject.commonName.s_next, estr = element->subject.commonName.s_next;
         kstr != (void *)&(key->subject.commonName);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    return 1;
}

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
    } else {
        len = snprintf(buf, sizeof(buf), "%d;%s:%i,", protocol, int_ntoa(addr2), port2);
        snprintf(buf+len, sizeof(buf) - len, "%s:%i", int_ntoa(addr1), port1);
    }

    return buf;
}
#define MOLOCH_SESSIONID_LEN 13
/******************************************************************************/
void moloch_session_id (char *buf, int protocol, uint32_t addr1, uint16_t port1, uint32_t addr2, uint16_t port2)
{
    buf[0] = protocol;
    if (addr1 < addr2) {
        memcpy(buf + 1, &addr1, 4);
        buf[5] = (port1 >> 8) & 0xff;
        buf[6] = port1 & 0xff;
        memcpy(buf + 7, &addr2, 4);
        buf[11] = (port2 >> 8) & 0xff;
        buf[12] = port2 & 0xff;
    } else {
        memcpy(buf + 1, &addr2, 4);
        buf[5] = (port2 >> 8) & 0xff;
        buf[6] = port2 & 0xff;
        memcpy(buf + 7, &addr1, 4);
        buf[11] = (port1 >> 8) & 0xff;
        buf[12] = port1 & 0xff;
    }
}
/******************************************************************************/
void moloch_nids_save_session(MolochSession_t *session) 
{
    if (session->outstandingQueries > 0) {
        session->needSave = 1;

        if (session->tcp_next) {
            DLL_REMOVE(tcp_, &tcpWriteQ, session);
        }

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

        HASH_REMOVE(h_, sessions, session);
        return;
    }

    moloch_db_save_session(session, TRUE);

    HASH_REMOVE(h_, sessions, session);
    moloch_nids_session_free(session);
}
/******************************************************************************/
void moloch_nids_mid_save_session(MolochSession_t *session) 
{
    MolochString_t *string;
    MolochInt_t    *mi;

    /* If we are parsing pcap its ok to pause and make sure all tags are loaded */
    while (session->outstandingQueries > 0 && (config.pcapReadDir || config.pcapReadFile)) {
        g_main_context_iteration (g_main_context_default(), TRUE);
    }

    if (!session->rootId) {
        session->rootId = "ROOT";
    }

    moloch_db_save_session(session, FALSE);
    g_array_free(session->filePosArray, TRUE);
    session->filePosArray = g_array_sized_new(FALSE, FALSE, 8, 1024);

    g_array_free(session->fileNumArray, TRUE);
    session->fileNumArray = g_array_new(FALSE, FALSE, 4);

    if (session->http) {
        g_ptr_array_free(session->http->urlArray, TRUE);
        session->http->urlArray = g_ptr_array_new_with_free_func(g_free);

        HASH_FORALL_POP_HEAD(s_, session->http->userAgents, string, 
            free(string->str);
            free(string);
        );

        HASH_FORALL_POP_HEAD(i_, session->http->xffs, mi, 
            free(mi);
        );
    }

    HASH_FORALL_POP_HEAD(s_, session->hosts, string, 
        free(string->str);
        free(string);
    );

    HASH_FORALL_POP_HEAD(i_, session->dnsips, mi, 
        free(mi);
    );

    if (session->tcp_next) {
        DLL_REMOVE(tcp_, &tcpWriteQ, session);
        DLL_PUSH_TAIL(tcp_, &tcpWriteQ, session);
    }

    session->lastSave = nids_last_pcap_header->ts.tv_sec;
    session->bytes = 0;
    session->databytes = 0;

    moloch_detect_initial_tag(session);
}
/******************************************************************************/
char *pcapFilename;

void moloch_nids_file_create()
{
    pcap_dumper_t        *dumper;

    if (config.dryRun) {
        pcapFilename = "dryrun.pcap";
        dumperFd = 1;
        return;
    }

    pcapFilename = moloch_db_create_file(nids_last_pcap_header->ts.tv_sec, NULL, &dumperId);
    dumper = pcap_dump_open(nids_params.pcap_desc, pcapFilename);

    if (!dumper) {
        LOG("ERROR - pcap open failed - Couldn't open file: '%s'", pcapFilename);
        exit (1);
    }
    pcap_dump_close(dumper);

    dumperFilePos = 24;
    dumperBufPos = 0;
    dumperBufMax = config.pcapWriteSize - 24;

    dumperFd = open(pcapFilename,  O_LARGEFILE | O_NOATIME | O_WRONLY );
    if (dumperFd < 0) {
        LOG("ERROR - pcap open failed - Couldn't open file: '%s' with %d", pcapFilename, errno);
        exit (2);
    }
    lseek(dumperFd, 24, SEEK_SET);
}
/******************************************************************************/
void moloch_nids_file_locked(char *filename)
{
    dumperFilePos = 24;
    dumperFd = 1;

    if (config.dryRun) {
        pcapFilename = "dryrun.pcap";
        return;
    }

    pcapFilename = moloch_db_create_file(nids_last_pcap_header->ts.tv_sec, filename, &dumperId);
}
/******************************************************************************/
void moloch_nids_file_flush(gboolean all)
{
    int pos = 0;
    int amount;

    if (config.dryRun) {
        return;
    }

    all |= (dumperBufPos <= dumperBufMax);

    if (all) {
        amount = dumperBufPos;
    } else {
        amount = dumperBufMax;
    }

    while (pos < amount) {
        int len = write(dumperFd, dumperBuf+pos, amount-pos);
        if (len < 0) {
            LOG("ERROR - Write failed with %d %d\n", len, errno);
            exit (0);
        }
        pos += len;
    }

    if (all) {
        dumperBufPos = 0;
    } else {
        dumperBufPos = dumperBufPos - dumperBufMax;
        memmove(dumperBuf, dumperBuf+dumperBufMax, dumperBufPos);
    }
    dumperBufMax = config.pcapWriteSize;

}
/******************************************************************************/
struct pcap_timeval {
    int32_t tv_sec;		/* seconds */
    int32_t tv_usec;		/* microseconds */
};
struct pcap_sf_pkthdr {
    struct pcap_timeval ts;	/* time stamp */
    uint32_t caplen;		/* length of portion present */
    uint32_t len;		/* length this packet (off wire) */
};
void
moloch_nids_pcap_dump(const struct pcap_pkthdr *h, const u_char *sp)
{
    struct pcap_sf_pkthdr hdr;

    hdr.ts.tv_sec  = h->ts.tv_sec;
    hdr.ts.tv_usec = h->ts.tv_usec;
    hdr.caplen     = h->caplen;
    hdr.len        = h->len;

    memcpy(dumperBuf+dumperBufPos, (char *)&hdr, sizeof(hdr));
    dumperBufPos += sizeof(hdr);

    memcpy(dumperBuf+dumperBufPos, sp, h->caplen);
    dumperBufPos += h->caplen;
}
/******************************************************************************/
void moloch_nids_new_session_http(MolochSession_t *session)
{
    session->http = malloc(sizeof(MolochSessionHttp_t));
    memset(session->http, 0, sizeof(MolochSessionHttp_t));

    HASH_INIT(s_, session->http->userAgents, moloch_string_hash, moloch_string_cmp);
    HASH_INIT(i_, session->http->xffs, moloch_int_hash, moloch_int_cmp);
    session->http->urlArray = g_ptr_array_new_with_free_func(g_free);
}
/******************************************************************************/
void moloch_nids_free_session_http(MolochSession_t *session, gboolean conditionally)
{
    MolochString_t *string;
    MolochInt_t    *mi;

    if (conditionally && (session->http->urlArray->len > 0 || HASH_COUNT(s_, session->http->userAgents) > 0 || HASH_COUNT(i_, session->http->xffs) > 0)) {
        return;
    }

    g_ptr_array_free(session->http->urlArray, TRUE);

    HASH_FORALL_POP_HEAD(s_, session->http->userAgents, string, 
        free(string->str);
        free(string);
    );

    HASH_FORALL_POP_HEAD(i_, session->http->xffs, mi, 
        free(mi);
    );

    if (session->http->urlString)
        g_string_free(session->http->urlString, TRUE);
    if (session->http->hostString)
        g_string_free(session->http->hostString, TRUE);
    if (session->http->uaString)
        g_string_free(session->http->uaString, TRUE);
    if (session->http->xffString)
        g_string_free(session->http->xffString, TRUE);

    free(session->http);

    session->http = 0;
}
/******************************************************************************/
void moloch_nids_cb_ip(struct ip *packet, int len)
{
    char             sessionId[MOLOCH_SESSIONID_LEN];
    MolochSession_t *headSession;
    struct tcphdr   *tcphdr = 0;
    struct udphdr   *udphdr = 0;
    MolochSessionHead_t *sessionsQ;
    uint32_t         sessionTimeout;

    switch (packet->ip_p) {
    case IPPROTO_TCP:
        sessionsQ = &tcpSessionQ;
        sessionTimeout = config.tcpTimeout;

        tcphdr = (struct tcphdr *)((void*)packet + 4 * packet->ip_hl);

        moloch_session_id(sessionId, packet->ip_p, packet->ip_src.s_addr, ntohs(tcphdr->source), 
                          packet->ip_dst.s_addr, ntohs(tcphdr->dest));
        break;
    case IPPROTO_UDP:
        sessionsQ = &udpSessionQ;
        sessionTimeout = config.udpTimeout;

        udphdr = (struct udphdr *)((void*)packet + 4 * packet->ip_hl);

        moloch_session_id(sessionId, packet->ip_p, packet->ip_src.s_addr, ntohs(udphdr->source), 
                          packet->ip_dst.s_addr, ntohs(udphdr->dest));
        break;
    case IPPROTO_ICMP:
        sessionsQ = &icmpSessionQ;
        sessionTimeout = config.icmpTimeout;

        moloch_session_id(sessionId, packet->ip_p, packet->ip_src.s_addr, 0, 
                          packet->ip_dst.s_addr, 0);
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

    totalBytes += len;

    if (totalPackets == 1) {
        struct pcap_stat ps;
        if (!pcap_stats(nids_params.pcap_desc, &ps)) {
            initialDropped = ps.ps_drop;
        }
        LOG("%ld Initial Dropped = %d", totalPackets, initialDropped);
    }

    if ((++totalPackets) % config.logEveryXPackets == 0) {
        struct pcap_stat ps;
        if (pcap_stats(nids_params.pcap_desc, &ps)) {
            ps.ps_drop = 0;
            ps.ps_recv = totalPackets;
            ps.ps_ifdrop = 0;
        }
        headSession = DLL_PEEK_HEAD(q_, sessionsQ);

        LOG("packets: %lu current sessions: %u/%u oldest: %d - recv: %u drop: %u (%0.2f) ifdrop: %u queue: %d", 
          totalPackets, 
          sessionsQ->q_count, 
          HASH_COUNT(h_, sessions),
          (int)(nids_last_pcap_header->ts.tv_sec - (headSession->lastPacket.tv_sec + sessionTimeout)),
          ps.ps_recv, 
          ps.ps_drop - initialDropped, (ps.ps_drop - initialDropped)*100.0/ps.ps_recv,
          ps.ps_ifdrop,
          moloch_http_queue_length(esServer));
    }
    
    /* Get or Create Session */
    MolochSession_t *session;
    
    HASH_FIND(h_, sessions, sessionId, session);

    if (!session) {
        //LOG ("New session: %s", sessionId);
        session = malloc(sizeof(MolochSession_t));
        memset(session, 0, sizeof(MolochSession_t));
        session->protocol = packet->ip_p;
        session->filePosArray = g_array_sized_new(FALSE, FALSE, 8, 100);
        session->fileNumArray = g_array_new(FALSE, FALSE, 4);
        HASH_INIT(s_, session->hosts, moloch_string_hash, moloch_string_cmp);
        HASH_INIT(s_, session->users, moloch_string_hash, moloch_string_cmp);
        HASH_INIT(s_, session->sshver, moloch_string_hash, moloch_string_cmp);
        HASH_INIT(s_, session->sshkey, moloch_string_hash, moloch_string_cmp);
        HASH_INIT(i_, session->dnsips, moloch_int_hash, moloch_int_cmp);
        HASH_INIT(t_, session->certs, moloch_nids_certs_hash, moloch_nids_certs_cmp);
        HASH_ADD(h_, sessions, sessionId, session);
        session->lastSave = nids_last_pcap_header->ts.tv_sec;
        session->firstPacket = nids_last_pcap_header->ts;
        session->addr1 = packet->ip_src.s_addr;
        session->addr2 = packet->ip_dst.s_addr;

        moloch_detect_initial_tag(session);

        switch (packet->ip_p) {
        case IPPROTO_TCP:
            /* If a syn & ack that means the first packet is actually the syn-ack 
             * reply, the syn probably got dropped */
            if ((tcphdr->syn) && (tcphdr->ack)) {
                session->addr1 = packet->ip_dst.s_addr;
                session->addr2 = packet->ip_src.s_addr;
                session->port1 = ntohs(tcphdr->dest);
                session->port2 = ntohs(tcphdr->source);
            } else {
                session->port1 = ntohs(tcphdr->source);
                session->port2 = ntohs(tcphdr->dest);
            }
            break;
        case IPPROTO_UDP:
            session->port1 = ntohs(udphdr->source);
            session->port2 = ntohs(udphdr->dest);
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
        DLL_REMOVE(q_, sessionsQ, session);
        DLL_PUSH_TAIL(q_, sessionsQ, session);
    }

    session->bytes += len;
    session->lastPacket = nids_last_pcap_header->ts;

    if (pluginsCbs & MOLOCH_PLUGIN_IP)
        moloch_plugins_cb_ip(session, packet, len);

    if (packet->ip_p == IPPROTO_UDP) {
        session->databytes += (len - 8);
        moloch_nids_process_udp(session, udphdr, (unsigned char*)udphdr+8, len - 8 - 4 * packet->ip_hl);
    }

    if (!config.dryRun && !session->dontSave) {
        if (config.copyPcap) {
            /* Save packet to file */
            if (dumperFd == -1 || dumperFilePos >= config.maxFileSizeG*1024LL*1024LL*1024LL) {
                if (dumperFd > 0 && !config.dryRun)  {
                    moloch_nids_file_flush(TRUE);
                    close(dumperFd);
                }
                moloch_nids_file_create();
            }

            uint64_t val = dumperFilePos | ((uint64_t)dumperId << 36);
            g_array_append_val(session->filePosArray, val);
            if (session->fileNumArray->len == 0 || g_array_index(session->fileNumArray, uint32_t, session->fileNumArray->len-1) != dumperId) {
                g_array_append_val(session->fileNumArray, dumperId);
            }

            if (config.fakePcap) {
                dumperBuf[dumperBufPos] = 'P';
                dumperBufPos++;
            } else {
                moloch_nids_pcap_dump(nids_last_pcap_header, nids_last_pcap_data);
                dumperFilePos += 16 + nids_last_pcap_header->caplen;
            }

            if (dumperBufPos > dumperBufMax) {
                moloch_nids_file_flush(FALSE);
            }
        } else {
            if (dumperFd == -1) {
                moloch_nids_file_locked(offlinePcapFilename);
            }

            uint64_t val = dumperFilePos | ((uint64_t)dumperId << 36);
            g_array_append_val(session->filePosArray, val);
            if (session->fileNumArray->len == 0 || g_array_index(session->fileNumArray, uint32_t, session->fileNumArray->len-1) != dumperId) {
                g_array_append_val(session->fileNumArray, dumperId);
            }
            dumperFilePos += 16 + nids_last_pcap_header->caplen;
        }

        if (session->filePosArray->len >= config.maxPackets) {
            moloch_nids_mid_save_session(session);
        }
    }

    /* Clean up the Q, only 1 per incoming packet so we don't fall behind */
    if ((headSession = DLL_PEEK_HEAD(q_, sessionsQ)) &&
           (headSession->lastPacket.tv_sec + sessionTimeout < nids_last_pcap_header->ts.tv_sec)) {

        if (packet->ip_p == IPPROTO_TCP && headSession->haveNidsTcp) {
            //LOG("Saving because of at head %s", moloch_friendly_session_id(headSession->protocol, headSession->addr1, headSession->port1, headSession->addr2, headSession->port2));
            headSession->lastPacket.tv_sec = nids_last_pcap_header->ts.tv_sec;

            DLL_REMOVE(q_, sessionsQ, headSession);
            DLL_PUSH_TAIL(q_, sessionsQ, headSession);

            moloch_nids_mid_save_session(headSession);
        } else
            moloch_nids_save_session(headSession);
    }

    if ((headSession = DLL_PEEK_HEAD(tcp_, &tcpWriteQ)) &&
           (headSession->lastSave + config.tcpSaveTimeout < nids_last_pcap_header->ts.tv_sec)) {

            //LOG("Saving because of timeout %s", moloch_friendly_session_id(headSession->protocol, headSession->addr1, headSession->port1, headSession->addr2, headSession->port2));
            moloch_nids_mid_save_session(headSession);
    }
}


/******************************************************************************/
void moloch_nids_incr_outstanding(MolochSession_t *session)
{
    session->outstandingQueries++;
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
void moloch_nids_get_tag_cb(MolochSession_t *session, int tagtype, uint32_t tag)
{
    g_hash_table_insert(session->tags[tagtype], (void *)(long)tag, (void *)1);
    moloch_nids_decr_outstanding(session);
}
/******************************************************************************/
void moloch_nids_add_tag(MolochSession_t *session, int tagtype, const char *tag) {
    moloch_nids_incr_outstanding(session);
    moloch_db_get_tag(session, tagtype, tag, moloch_nids_get_tag_cb);

    if (!session->dontSave && HASH_COUNT(s_, config.dontSaveTags)) {
        MolochString_t *tstring;

        HASH_FIND(s_, config.dontSaveTags, tag, tstring);
        if (tstring) {
            session->dontSave = 1;
        }
    }
}

/******************************************************************************/
void moloch_nids_cb_tcp(struct tcp_stream *a_tcp, void *UNUSED(params))
{
    MolochSession_t *session;
    char             key[1024];

    //LOG("TCP (%d) - %s", a_tcp->nids_state, moloch_friendly_session_id(IPPROTO_TCP, a_tcp->addr.saddr, a_tcp->addr.source, a_tcp->addr.daddr, a_tcp->addr.dest));
    //fflush(stdout);

    switch (a_tcp->nids_state) {
    case NIDS_JUST_EST:

        moloch_session_id(key, IPPROTO_TCP, a_tcp->addr.saddr, a_tcp->addr.source, a_tcp->addr.daddr, a_tcp->addr.dest);
        HASH_FIND(h_, sessions, key, session);
        if (session) {
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
    case NIDS_DATA:
        session = a_tcp->user;
        if (!session) {
            LOG("ERROR - data - a_tcp->user not set for %s", moloch_friendly_session_id(IPPROTO_TCP, a_tcp->addr.saddr, a_tcp->addr.source, a_tcp->addr.daddr, a_tcp->addr.dest));
            a_tcp->client.collect = 0;
            a_tcp->server.collect = 0;
            return;
        }
        if (a_tcp->client.count_new) {
            session->which = 1;
            moloch_detect_parse_http(session, a_tcp, &a_tcp->client);
            moloch_detect_parse_classify(session, a_tcp, &a_tcp->client);
            moloch_detect_parse_yara(session, a_tcp, &a_tcp->client);
            nids_discard(a_tcp, session->offsets[1]);
            if (session->isSsh)
                moloch_detect_parse_ssh(session, a_tcp, &a_tcp->client);
            session->offsets[1] = a_tcp->client.count_new;
        }

        if (a_tcp->server.count_new) {
            session->which = 0;
            moloch_detect_parse_http(session, a_tcp, &a_tcp->server);
            moloch_detect_parse_classify(session, a_tcp, &a_tcp->server);
            moloch_detect_parse_yara(session, a_tcp, &a_tcp->server);
            nids_discard(a_tcp, session->offsets[0]);
            session->offsets[0] = a_tcp->server.count_new;
        }

        session->databytes += a_tcp->server.count_new + a_tcp->client.count_new;

        if (pluginsCbs & MOLOCH_PLUGIN_TCP)
            moloch_plugins_cb_tcp(session, a_tcp);
        return;
    default:
        session = a_tcp->user;

        if (!session) {
            LOG("ERROR - default (%d) - a_tcp->user not set for %s", a_tcp->nids_state, moloch_friendly_session_id(IPPROTO_TCP, a_tcp->addr.saddr, a_tcp->addr.source, a_tcp->addr.daddr, a_tcp->addr.dest));
            return;
        }

        if (a_tcp->client.count != a_tcp->client.offset) {
            moloch_detect_parse_yara(session, a_tcp, &a_tcp->client);
        }

        if (a_tcp->server.count != a_tcp->server.offset) {
            moloch_detect_parse_yara(session, a_tcp, &a_tcp->server);
        }
    
        //LOG("TCP %d ", a_tcp->nids_state);
        moloch_nids_save_session(session);
        a_tcp->user = 0;
        if (pluginsCbs & MOLOCH_PLUGIN_TCP)
            moloch_plugins_cb_tcp(session, a_tcp);
        return;
    }
}

/******************************************************************************/
void moloch_nids_session_free (MolochSession_t *session)
{
    MolochString_t *string;

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

    if (session->tcp_next)
        DLL_REMOVE(tcp_, &tcpWriteQ, session);

    g_array_free(session->filePosArray, TRUE);
    g_array_free(session->fileNumArray, TRUE);

    if (session->http) {
        moloch_nids_free_session_http(session, FALSE);
    }

    HASH_FORALL_POP_HEAD(s_, session->hosts, string, 
        free(string->str);
        free(string);
    );

    HASH_FORALL_POP_HEAD(s_, session->users, string, 
        free(string->str);
        free(string);
    );

    HASH_FORALL_POP_HEAD(s_, session->sshver, string, 
        free(string->str);
        free(string);
    );

    HASH_FORALL_POP_HEAD(s_, session->sshkey, string, 
        free(string->str);
        free(string);
    );

    MolochCertsInfo_t *certs;
    HASH_FORALL_POP_HEAD(t_, session->certs, certs, 
        moloch_nids_certs_free(certs);
    );

    if (session->rootId)
        g_free(session->rootId);

    int i;
    for (i = 0; i < MOLOCH_TAG_MAX; i++) {
        g_hash_table_destroy(session->tags[i]);
    }
    free(session);
}
/******************************************************************************/
void moloch_nids_syslog(int type, int errnum, struct ip *iph, void *data)
{
    char saddr[20], daddr[20];

    switch (type) {

    case NIDS_WARN_IP:
	if (errnum != NIDS_WARN_IP_HDR) {
	    strcpy(saddr, int_ntoa(iph->ip_src.s_addr));
	    strcpy(daddr, int_ntoa(iph->ip_dst.s_addr));
	    LOG("NIDSIP:%s %s, packet (apparently) from %s to %s",
		   pcapFilename, nids_warnings[errnum], saddr, daddr);
	} else
	    LOG("NIDSIP:%s %s",
		   pcapFilename, nids_warnings[errnum]);
	break;

    case NIDS_WARN_TCP:
	strcpy(saddr, int_ntoa(iph->ip_src.s_addr));
	strcpy(daddr, int_ntoa(iph->ip_dst.s_addr));
        if (errnum == NIDS_WARN_TCP_TOOMUCH || errnum == NIDS_WARN_TCP_BIGQUEUE) {
            /* ALW - Should do something with it */
	} else if (errnum != NIDS_WARN_TCP_HDR)
	    LOG("NIDSTCP:%s %s,from %s:%hu to  %s:%hu", pcapFilename, nids_warnings[errnum],
		saddr, ntohs(((struct tcphdr *) data)->source), daddr,
		ntohs(((struct tcphdr *) data)->dest));
	else
	    LOG("NIDSTCP:%s %s,from %s to %s", pcapFilename,
		nids_warnings[errnum], saddr, daddr);
	break;

    default:
	LOG("NIDS: Unknown warning number %d", type);
    }
}

/******************************************************************************/
gboolean moloch_nids_watch_cb(gint fd, GIOCondition cond, gpointer data);
/******************************************************************************/
/* When processing many pcap files we don't start the next file
 * until there are less then 20 outstanding es requests
 */
gboolean moloch_nids_next_file_gfunc (gpointer UNUSED(user_data))
{
    if (moloch_http_queue_length(esServer) > 20)
        return TRUE;

    moloch_nids_init_nids();
    /*nids_init();
    moloch_watch_fd(nids_getfd(), MOLOCH_GIO_READ_COND, moloch_nids_watch_cb, NULL);
    nids_register_tcp(moloch_nids_cb_tcp);
    nids_register_ip(moloch_nids_cb_ip);*/
    return FALSE;
}
/******************************************************************************/
gboolean moloch_nids_watch_cb(gint UNUSED(fd), GIOCondition UNUSED(cond), gpointer UNUSED(data)) {
    int r = nids_dispatch(config.packetsPerPoll);
    if (r <= 0 && (config.pcapReadFile || config.pcapReadDir)) {
        if (config.pcapReadDir && moloch_nids_next_file()) {
            g_timeout_add(100, moloch_nids_next_file_gfunc, 0);
            return FALSE;
        }

        moloch_quit();
        return FALSE;
    }
    return TRUE;
}
/******************************************************************************/
gboolean moloch_nids_poll_cb(gpointer UNUSED(uw)) {
    nids_dispatch(config.packetsPerPoll);
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
    if (!nids_params.pcap_desc)
        pcap_stats(nids_params.pcap_desc, &ps);
    return ps.ps_drop - initialDropped;
}
/******************************************************************************/
uint32_t moloch_nids_monitoring_sessions()
{
    return HASH_COUNT(h_, sessions);
}
/******************************************************************************/
void moloch_nids_process_udp(MolochSession_t *session, struct udphdr *udphdr, unsigned char *data, int len)
{
    if (session->port1 == 53 || session->port2 == 53)
        moloch_detect_dns(session, data, len);

    if (pluginsCbs & MOLOCH_PLUGIN_UDP)
        moloch_plugins_cb_udp(session, udphdr, data, len);
}
/******************************************************************************/
int moloch_nids_next_file()
{
    static GDir *pcapGDir = NULL;
    GError      *error = 0;
    char         errbuf[1024];

    if (!pcapGDir) {
        pcapGDir = g_dir_open(config.pcapReadDir, 0, &error);
        if (error) {
            LOG("ERROR: Couldn't open pcap directory: Receive Error: %s", error->message);
            exit(0);
        }
    }
    if (nids_params.pcap_desc) {
        pcap_close(nids_params.pcap_desc);
    }
    const gchar *filename;
    while (1) {
        filename = g_dir_read_name(pcapGDir);
        if (!filename) {
            return 0;
        }

        if (filename[0] == '.' || strcmp(".pcap", filename + strlen(filename)-5) != 0) {
            continue;
        }

        gchar *fullfilename;
        fullfilename = g_build_filename (config.pcapReadDir, filename, NULL);
        LOG ("Processing %s", fullfilename);
        realpath(fullfilename, offlinePcapFilename);
        if (!config.copyPcap) {
            dumperFd = -1;
        }

        errbuf[0] = 0;
        nids_params.pcap_desc = pcap_open_offline(fullfilename, errbuf);
        if (!nids_params.pcap_desc) {
            LOG("Couldn't process '%s' error '%s'", fullfilename, errbuf);
            nids_params.pcap_desc = (void *)0x01; /* Stop lib nids from freeing desc, crazy */
            return 0;
        }
        g_free(fullfilename);
        return 1;
    }
}
/******************************************************************************/
void moloch_nids_root_init()
{
    char errbuf[1024];
    if (config.pcapReadDir) {
        moloch_nids_next_file();
    }
    else if (config.pcapReadFile) {
        nids_params.pcap_desc = pcap_open_offline(config.pcapReadFile, errbuf);
        realpath(config.pcapReadFile, offlinePcapFilename);
    } else {
#ifdef SNF
        nids_params.pcap_desc = pcap_open_live(config.interface, 1600, 1, 500, errbuf);
#else
        nids_params.pcap_desc = moloch_pcap_open_live(config.interface, 1600, 1, 500, errbuf);
#endif

    }

    if (!nids_params.pcap_desc) {
        LOG("pcap open live failed! %s", errbuf);
        exit(1);
    }
}
/******************************************************************************/
void moloch_nids_init_nids()
{
    nids_init();

    if (nids_getfd() == -1) {
        g_timeout_add(0, moloch_nids_poll_cb, NULL);
    } else {
        moloch_watch_fd(nids_getfd(), MOLOCH_GIO_READ_COND, moloch_nids_watch_cb, NULL);
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
void moloch_nids_init()
{

    LOG("%s", pcap_lib_version());

    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "tcp", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "udp", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "protocol:http", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "protocol:ssh", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "protocol:smtp", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "protocol:ftp", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "protocol:pop3", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "protocol:gh0st", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "protocol:dns", NULL);

    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "http:statuscode:200", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "http:statuscode:204", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "http:statuscode:301", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "http:statuscode:302", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "http:statuscode:304", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "http:statuscode:400", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "http:statuscode:404", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "http:statuscode:500", NULL);

    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "http:method:GET", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "http:method:POST", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "http:method:HEAD", NULL);

    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "http:content:application/octet-stream", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "http:content:text/plain", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "http:content:text/html", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "http:content:application/x-gzip", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "http:content:application/x-shockwave-flash", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "http:content:image/gif", NULL);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, "http:content:image/jpg", NULL);

    HASH_INIT(h_, sessions, moloch_nids_session_hash, moloch_nids_session_cmp);
    DLL_INIT(tcp_, &tcpWriteQ);
    DLL_INIT(q_, &tcpSessionQ);
    DLL_INIT(q_, &udpSessionQ);
    DLL_INIT(q_, &icmpSessionQ);

    nids_params.n_hosts = 1024;
    nids_params.tcp_workarounds = 1;
    nids_params.one_loop_less = 0;
    nids_params.scan_num_hosts = 0;
    nids_params.scan_num_ports = 0;
    nids_params.syslog = moloch_nids_syslog;
    nids_params.n_tcp_streams = config.maxStreams;

    if (config.bpf)
        nids_params.pcap_filter = config.bpf;


    moloch_nids_init_nids();
}
/******************************************************************************/
void moloch_nids_exit() {
    LOG("exit");
    nids_exit();

    MolochSession_t *hsession;
    HASH_FORALL_POP_HEAD(h_, sessions, hsession, 
        moloch_db_save_session(hsession, TRUE);
    );

    if (!config.dryRun && config.copyPcap) {
        moloch_nids_file_flush(TRUE);
        close(dumperFd);
    }
}
