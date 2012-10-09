/* nids.c  -- Functions for dealing with libnids and detection
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
extern gchar                *nodeName;
extern gchar                *extraTag;
extern gboolean              debug;
static gchar                 nodeTag[100];
static gchar                 classTag[100];

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
static http_parser_settings  parserSettings;
static magic_t               cookie;
static uint32_t              initialDropped = 0;

uint64_t                     totalPackets = 0;
uint64_t                     totalBytes = 0;
uint64_t                     totalSessions = 0;

/******************************************************************************/
extern gchar   *interface;
extern gchar   *pcapFile;
extern gboolean fakePcap;
extern gboolean dryRun;

HASH_VAR(h_, sessions, MolochSessionHead_t, 199337);

/******************************************************************************/
void moloch_nids_session_free (MolochSession_t *session);
void moloch_nids_process_udp(MolochSession_t *session, struct udphdr   *udphdr, unsigned char *data, int len);

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
    if (session->outstandingTags > 0) {
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

    moloch_db_save_session(session);

    HASH_REMOVE(h_, sessions, session);
    moloch_nids_session_free(session);
}
/******************************************************************************/
void moloch_nids_initial_tag(MolochSession_t *session)
{
    if (session->tags[0])
        g_hash_table_destroy(session->tags[0]);
    if (session->tags[1])
        g_hash_table_destroy(session->tags[1]);

    session->tags[0] = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
    session->tags[1] = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);

    moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, nodeTag);
    if (config.nodeClass)
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, classTag);

    if (extraTag)
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, extraTag);

    switch(session->protocol) {
    case IPPROTO_TCP:
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "tcp");
        break;
    case IPPROTO_UDP:
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "udp");
        break;
    case IPPROTO_ICMP:
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "ICMP");
        break;
    }
}
/******************************************************************************/
void moloch_nids_mid_save_session(MolochSession_t *session) 
{
    if (!session->rootId) {
        session->rootId = "ROOT";
    }

    moloch_db_save_session(session);
    g_array_free(session->filePosArray, TRUE);
    session->filePosArray = g_array_sized_new(FALSE, FALSE, 8, 1024);

    g_array_free(session->fileNumArray, TRUE);
    session->fileNumArray = g_array_new(FALSE, FALSE, 4);

    g_ptr_array_free(session->urlArray, TRUE);
    session->urlArray = g_ptr_array_new_with_free_func(g_free);

    MolochString_t *string;
    HASH_FORALL_POP_HEAD(s_, session->hosts, string, 
        free(string->str);
        free(string);
    );

    HASH_FORALL_POP_HEAD(s_, session->userAgents, string, 
        free(string->str);
        free(string);
    );

    MolochInt_t *mi;
    HASH_FORALL_POP_HEAD(i_, session->xffs, mi, 
        free(mi);
    );

    if (session->tcp_next) {
        DLL_REMOVE(tcp_, &tcpWriteQ, session);
        DLL_PUSH_TAIL(tcp_, &tcpWriteQ, session);
    }

    session->lastSave = nids_last_pcap_header->ts.tv_sec;
    session->bytes = 0;
    session->databytes = 0;

    moloch_nids_initial_tag(session);
}
/******************************************************************************/
char *pcapFilename;

void moloch_nids_file_create()
{
    pcap_dumper_t        *dumper;

    if (dryRun) {
        pcapFilename = "dryrun.pcap";
        dumperFd = 1;
        return;
    }

    pcapFilename = moloch_db_create_file(nids_last_pcap_header->ts.tv_sec, &dumperId);
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
void moloch_nids_file_flush(gboolean all)
{
    int pos = 0;
    int amount;

    if (dryRun) {
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
          (int)(nids_last_pcap_header->ts.tv_sec - (headSession->lastPacket + sessionTimeout)),
          ps.ps_recv, 
          ps.ps_drop - initialDropped, (ps.ps_drop - initialDropped)*100.0/ps.ps_recv,
          ps.ps_ifdrop,
          moloch_es_queue_length());
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
        session->urlArray = g_ptr_array_new_with_free_func(g_free);
        HASH_INIT(s_, session->hosts, moloch_string_hash, moloch_string_cmp);
        HASH_INIT(s_, session->userAgents, moloch_string_hash, moloch_string_cmp);
        HASH_INIT(i_, session->xffs, moloch_int_hash, moloch_int_cmp);
        HASH_INIT(t_, session->certs, moloch_nids_certs_hash, moloch_nids_certs_cmp);
        HASH_ADD(h_, sessions, sessionId, session);
        session->lastSave = session->firstPacket = nids_last_pcap_header->ts.tv_sec;
        session->addr1 = packet->ip_src.s_addr;
        session->addr2 = packet->ip_dst.s_addr;

        moloch_nids_initial_tag(session);

        switch (packet->ip_p) {
        case IPPROTO_TCP:
            session->port1 = ntohs(tcphdr->source);
            session->port2 = ntohs(tcphdr->dest);
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
    } else {
        DLL_REMOVE(q_, sessionsQ, session);
        DLL_PUSH_TAIL(q_, sessionsQ, session);
    }

    session->bytes += len;
    session->lastPacket = nids_last_pcap_header->ts.tv_sec;

    if (packet->ip_p == IPPROTO_UDP) {
        session->databytes += (len - 8);
        moloch_nids_process_udp(session, udphdr, (unsigned char*)udphdr+8, len - 8 - 4 * packet->ip_hl);
    }

    if (!dryRun && !session->dontSave) {
        /* Save packet to file */
        if (dumperFd == -1 || dumperFilePos >= config.maxFileSizeG*1024LL*1024LL*1024LL) {
            if (dumperFd > 0 && !dryRun)  {
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

        if (fakePcap) {
            dumperBuf[dumperBufPos] = 'P';
            dumperBufPos++;
        } else {
            moloch_nids_pcap_dump(nids_last_pcap_header, nids_last_pcap_data);
            dumperFilePos += 16 + nids_last_pcap_header->caplen;
        }

        if (dumperBufPos > dumperBufMax) {
            moloch_nids_file_flush(FALSE);
        }

        if (session->filePosArray->len >= config.maxPackets) {
            moloch_nids_mid_save_session(session);
        }
    }

    /* Clean up the Q, only 1 per incoming packet so we don't fall behind */
    if ((headSession = DLL_PEEK_HEAD(q_, sessionsQ)) &&
           (headSession->lastPacket + sessionTimeout < nids_last_pcap_header->ts.tv_sec)) {

        if (packet->ip_p == IPPROTO_TCP && headSession->haveNidsTcp) {
            //LOG("Saving because of at head %s", moloch_friendly_session_id(headSession->protocol, headSession->addr1, headSession->port1, headSession->addr2, headSession->port2));
            headSession->lastPacket = nids_last_pcap_header->ts.tv_sec;

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
void moloch_nids_get_tag_cb(MolochSession_t *session, int tagtype, uint32_t tag)
{
    g_hash_table_insert(session->tags[tagtype], (void *)(long)tag, (void *)1);
    session->outstandingTags--;
    if (session->needSave && session->outstandingTags == 0) {
        moloch_db_save_session(session);
        moloch_nids_session_free(session);
    }
}
/******************************************************************************/
void moloch_nids_add_tag(MolochSession_t *session, int tagtype, const char *tag) {
    session->outstandingTags++;
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
unsigned char *
moloch_nids_asn_get_tlv(unsigned char **data, int *len, int *apc, int *atag, int *alen)
{
    if ((*len) < 2) {
        goto get_tlv_error;
    }

    int pos = 0;

    *apc = ((*data)[0] >> 5) & 0x1;

    if (((*data)[0] & 0x1f) ==  0x1f) {
        for (pos = 1; pos < *len; pos++) {
            (*atag) = ((*atag) << 7) | (*data)[pos];
            if (((*data)[pos] & 0x80) == 0)
                break;
        }
    } else {
        *atag = (*data)[pos] & 0x1f;
        pos++;
    }

    if (pos >= (*len) || (*data)[pos] == 0x80) {
        goto get_tlv_error;
    }

    if ((*data)[pos] & 0x80) {
        int cnt = (*data)[pos] & 0x7f;
        pos++;
        (*alen) = 0;
        while (cnt > 0 && pos < (*len)) {
            (*alen) = ((*alen) << 8) | (*data)[pos];
            cnt--;
            pos++;
        }
    } else {
        (*alen) = (*data)[pos];
        pos++;
    }

    (*len) -= pos;
    (*data) += pos;
    unsigned char *value = (*data);

    if ((*len) < 0 || (*alen) < 0) {
        goto get_tlv_error;
    }

    if ((*alen) > (*len)) {
        (*alen) = (*len);
    }
    (*data) += (*alen);
    (*len) -= (*alen);

    return value;

get_tlv_error:
    (*apc)  = 0;
    (*alen) = 0;
    (*atag) = 0;
    (*len)  = 0;
    return 0;
}
/******************************************************************************/
char *moloch_nids_asn_decode_oid(unsigned char *oid, int len) {
    static char buf[1000];
    int buflen = 0;
    int pos = 0;
    int first = TRUE;
    int value = 0;

    for (pos = 0; pos < len; pos++) {
        value = (value << 7) | (oid[pos] & 0x7f);
        if (oid[pos] & 0x80) {
            continue;
        } 

        if (first) {
            first = FALSE;
            if (value > 40) /* two values in first byte */
                buflen += sprintf(buf, "%d.%d", value/40, value % 40);
            else /* one value in first byte */
                buflen += sprintf(buf, "%d", value);
        } else {
            buflen += sprintf(buf+buflen, ".%d", value);
        }

        value = 0;
    }

    return buf;
}
/******************************************************************************/
void
moloch_nids_tls_certinfo_process(MolochCertInfo_t *ci, unsigned char *data, int len)
{
    int apc, atag, alen;
    char *lastOid = NULL;

    while (len > 0) {
        unsigned char *value = moloch_nids_asn_get_tlv(&data, &len, &apc, &atag, &alen);
        if (apc) {
            moloch_nids_tls_certinfo_process(ci, value, alen);
        } else if (atag  == 6)  {
            lastOid = moloch_nids_asn_decode_oid(value, alen);
        } else if (lastOid && (atag == 20 || atag == 19 || atag == 12))  {
            if (strcmp(lastOid, "2.5.4.3") == 0) {
                MolochString_t *element = malloc(sizeof(*element));
                element->str = g_ascii_strdown((char*)value, alen);
                DLL_PUSH_TAIL(s_, &ci->commonName, element);
            } else if (strcmp(lastOid, "2.5.4.10") == 0) {
                if (ci->orgName) {
                    LOG("Multiple orgName %s => %.*s", ci->orgName, alen, value);
                    free(ci->orgName);
                }
                ci->orgName = g_ascii_strdown((char*)value, alen);
            }
        }
    }
}
/******************************************************************************/
void
moloch_nids_tls_alt_names(MolochCertsInfo_t *certs, unsigned char *data, int len)
{
    int apc, atag, alen;
    static char *lastOid = NULL;

    while (len >= 2) {
        unsigned char *value = moloch_nids_asn_get_tlv(&data, &len, &apc, &atag, &alen);

        if (!value)
            return;

        if (apc) {
            moloch_nids_tls_alt_names(certs, value, alen);
            if (certs->alt.s_count > 0) {
                return;
            }
        } else if (atag == 6)  {
            lastOid = moloch_nids_asn_decode_oid(value, alen);
            if (strcmp(lastOid, "2.5.29.17") != 0)
                lastOid = NULL;
        } else if (lastOid && atag == 4) {
            moloch_nids_tls_alt_names(certs, value, alen);
            return;
        } else if (lastOid && atag == 2) {
            MolochString_t *element = malloc(sizeof(*element));
            element->str = g_ascii_strdown((char*)value, alen);
            DLL_PUSH_TAIL(s_, &certs->alt, element);
        }
    }
    return;
}
/******************************************************************************/
void
moloch_nids_tls_process(MolochSession_t *session, unsigned char *data, int len)
{
    unsigned char *ssldata;
    unsigned char *pdata;
    unsigned char *cdata;
    int            ssllen = 0;
    int            plen = 0;
    int            clen = 0;

    for (ssldata = data;
         ssldata < data + len; 
         ssldata += ssllen + 5) {

        ssllen = MIN(data+len-ssldata-5, ((ssldata[3]&0xff) << 8 | (ssldata[4]&0xff)));



        for (pdata = ssldata + 5;
             pdata < data + len && pdata < ssldata + ssllen; 
             pdata += plen + 4) {

            plen = MIN(data+len-pdata-7, ((pdata[2]&0xff) << 8 | (pdata[3]&0xff)));

            if (pdata[0] != 0x0b)
                continue;

            for (cdata = pdata+7;
                 cdata < data + len && cdata < pdata + plen;
                 cdata += clen + 3)
            {
                int badreason = 0;
                clen = MIN(data+len-cdata-3, cdata[0] << 16 | cdata[1] << 8 | cdata[2]);

                MolochCertsInfo_t *certs = malloc(sizeof(MolochCertsInfo_t));
                memset(certs, 0, sizeof(*certs));
                DLL_INIT(s_, &certs->alt);
                DLL_INIT(s_, &certs->subject.commonName);
                DLL_INIT(s_, &certs->issuer.commonName);

                unsigned char *asndata = cdata + 3;
                int            asnlen  = clen;
                int            atag, alen, apc;
                unsigned char *value;

                /* Certificate */
                if (!(value = moloch_nids_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                    {badreason = 1; goto bad_cert;}
                asndata = value;
                asnlen = alen;

                /* signedCertificate */
                if (!(value = moloch_nids_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                    {badreason = 2; goto bad_cert;}
                asndata = value;
                asnlen = alen;

                /* serialNumber or version*/
                if (!(value = moloch_nids_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                    {badreason = 3; goto bad_cert;}

                if (apc) {
                    if (!(value = moloch_nids_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                        {badreason = 4; goto bad_cert;}
                }
                certs->serialNumberLen = alen;
                certs->serialNumber = malloc(alen);
                memcpy(certs->serialNumber, value, alen);

                /* signature */
                if (!(value = moloch_nids_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                    {badreason = 5; goto bad_cert;}

                /* issuer */
                if (!(value = moloch_nids_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                    {badreason = 6; goto bad_cert;}
                moloch_nids_tls_certinfo_process(&certs->issuer, value, alen);

                /* validity */
                if (!(value = moloch_nids_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                    {badreason = 7; goto bad_cert;}

                /* subject */
                if (!(value = moloch_nids_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                    {badreason = 8; goto bad_cert;}
                moloch_nids_tls_certinfo_process(&certs->subject, value, alen);

                /* subjectPublicKeyInfo */
                if (!(value = moloch_nids_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                    {badreason = 9; goto bad_cert;}

                /* extensions */
                if (asnlen) {
                    if (!(value = moloch_nids_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                        {badreason = 10; goto bad_cert;}
                    moloch_nids_tls_alt_names(certs, value, alen);
                }

                MolochCertsInfo_t *element;
                HASH_FIND(t_, session->certs, certs, element);
                if (element) {
                    moloch_nids_certs_free(certs);
                } else {
                    HASH_ADD(t_, session->certs, certs, certs);
                }

                continue;

            bad_cert:
                if (debug)
                    LOG("bad cert %d - %d %.*s", badreason, clen, clen, cdata);
                moloch_nids_certs_free(certs);
                break;
            }
        }
    }
}
/******************************************************************************/
void moloch_nids_parse_classify(MolochSession_t *session, struct tcp_stream *UNUSED(a_tcp), struct half_stream *hlf)
{
    unsigned char *data = (unsigned char *)hlf->data;

    if (hlf->offset != 0)
        return;

    if (hlf->count < 3)
        return;

    if (memcmp("SSH", data, 3) == 0)
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:ssh");

    if (hlf->count < 4)
        return;

    if (memcmp("220 ", data, 4) == 0) {
        if (g_strstr_len((char *)data, hlf->count_new, "LMTP") != 0)
            moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:lmtp");
        else if (g_strstr_len((char *)data, hlf->count_new, "SMTP") != 0)
            moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:smtp");
        else
            moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:ftp");
    }

    if (hlf->count < 5)
        return;

    if (memcmp("HELO ", data, 5) == 0)
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:smtp");

    if (memcmp("EHLO ", data, 5) == 0)
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:smtp");



    if (hlf->count < 9)
        return;

    if (memcmp("+OK POP3 ", data, 9) == 0)
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:pop3");


    if (hlf->count < 14)
        return;


    if (data[13] == 0x78 &&  
        (((data[8] == 0) && (data[7] == 0) && (((data[6]&0xff) << 8 | (data[5]&0xff)) == hlf->count)) ||  // Windows
         ((data[5] == 0) && (data[6] == 0) && (((data[7]&0xff) << 8 | (data[8]&0xff)) == hlf->count)))) { // Mac
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:gh0st");
     }else if (data[7] == 0 && data[8] == 0 && data[11] == 0 && data[12] == 0 && data[13] == 0x78 && data[14] == 0x9c) {
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:gh0st-improved");
    }

    if (hlf->count < 19)
        return;

    if (memcmp("BitTorrent protocol", data, 19) == 0)
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:bittorrent");

    if (hlf->count < 30)
        return;

    if (hlf->count != hlf->count_new && data[0] == 0x16 && data[1] == 0x03 && data[2] <= 0x03 && data[5] == 2) {
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:tls");
        moloch_nids_tls_process(session, data, hlf->count);
    }
}
/******************************************************************************/
void moloch_nids_parse_yara(MolochSession_t *session, struct tcp_stream *UNUSED(a_tcp), struct half_stream *hlf)
{
    moloch_yara_execute(session, hlf->data, hlf->count - hlf->offset, (hlf->offset == 0));
}
/******************************************************************************/
void moloch_nids_parse_http(MolochSession_t *session, struct tcp_stream *UNUSED(a_tcp), struct half_stream *hlf, int which)
{
    int remaining = hlf->count_new;
    char *data    = hlf->data + (hlf->count - hlf->offset - hlf->count_new);

    while (remaining > 0) {
        if ((session->wParsers & (1 << which)) == 0) {
            if (hlf->offset == 0) {
                http_parser_init(&session->parsers[which], HTTP_BOTH);
                session->wParsers |= (1 << which);
                session->parsers[which].data = session;
            } else {
                break;
            }
        }

        int len = http_parser_execute(&session->parsers[which], &parserSettings, data, remaining);
        if (len <= 0) {
            session->wParsers &= ~(1 << which);
            break;
        }
        data += len;
        remaining -= len;
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
            //LOG("client: s: %s new: %d count: %d offset: %d len: %d oldoffset: %d", key, a_tcp->client.count_new, a_tcp->client.count, a_tcp->client.offset, (a_tcp->client.count - a_tcp->client.offset - a_tcp->client.count_new), session->offsets[0]);
            moloch_nids_parse_http(session, a_tcp, &a_tcp->client, 0);
            moloch_nids_parse_classify(session, a_tcp, &a_tcp->client);
            moloch_nids_parse_yara(session, a_tcp, &a_tcp->client);
            nids_discard(a_tcp, session->offsets[0]);
            session->offsets[0] = a_tcp->client.count_new;
        }

        if (a_tcp->server.count_new) {
            //LOG("server: s: %s new: %d count: %d offset: %d len: %d oldoffset: %d", key, a_tcp->server.count_new, a_tcp->server.count, a_tcp->server.offset, (a_tcp->server.count - a_tcp->server.offset - a_tcp->server.count_new), session->offsets[1]);
            moloch_nids_parse_http(session, a_tcp, &a_tcp->server, 1);
            moloch_nids_parse_classify(session, a_tcp, &a_tcp->server);
            moloch_nids_parse_yara(session, a_tcp, &a_tcp->server);
            nids_discard(a_tcp, session->offsets[1]);
            session->offsets[1] = a_tcp->server.count_new;
        }

        session->databytes += a_tcp->server.count_new + a_tcp->client.count_new;

        return;
    default:
        session = a_tcp->user;

        if (!session) {
            LOG("ERROR - default (%d) - a_tcp->user not set for %s", a_tcp->nids_state, moloch_friendly_session_id(IPPROTO_TCP, a_tcp->addr.saddr, a_tcp->addr.source, a_tcp->addr.daddr, a_tcp->addr.dest));
            return;
        }

        if (a_tcp->client.count != a_tcp->client.offset) {
            moloch_nids_parse_yara(session, a_tcp, &a_tcp->client);
        }

        if (a_tcp->server.count != a_tcp->server.offset) {
            moloch_nids_parse_yara(session, a_tcp, &a_tcp->server);
        }
    
        //LOG("TCP %d ", a_tcp->nids_state);
        moloch_nids_save_session(session);
        a_tcp->user = 0;
        return;
    }
}

/******************************************************************************/
int
moloch_hp_cb_on_message_begin (http_parser *parser)
{
    MolochSession_t *session = parser->data;

    session->inValue = 0;
    session->inBody = 0;

    return 0;
}
/******************************************************************************/
int
moloch_hp_cb_on_url (http_parser *parser, const char *at, size_t length)
{
    MolochSession_t *session = parser->data;

    if (!session->urlString)
        session->urlString = g_string_new_len(at, length);
    else
        g_string_append_len(session->urlString, at, length);
    return 0;
}

/******************************************************************************/
const char *moloch_memstr(const char *haystack, int haysize, const char *needle, int needlesize)
{
    const char *p;

    haysize -= needlesize;

    for (p = haystack; p <= (haystack+haysize); p++)
    {
        if (p[0] == needle[0] && memcmp(p+1, needle+1, needlesize-1) == 0)
            return p;
    }
    return NULL;
}
/******************************************************************************/
int
moloch_hp_cb_on_body (http_parser *parser, const char *at, size_t length)
{
    MolochSession_t *session = parser->data;

    if (!session->inBody) {
        if (moloch_memstr(at, length, "password=", 9)) {
            moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "http:password");
        }

        const char *m = magic_buffer(cookie, at, length);
        if (m) {
            char tmp[500];
            snprintf(tmp, sizeof(tmp), "http:content:%s", m);
            char *semi = strchr(tmp, ';');
            if (semi) {
                *semi = 0;
            } 
            moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, tmp);
        }
        session->inBody = 1;
    }

    return 0;
}

/******************************************************************************/
int
moloch_hp_cb_on_message_complete (http_parser *parser)
{
    MolochSession_t *session = parser->data;

    char tag[200];

    if (parser->status_code == 0) {
        snprintf(tag, sizeof(tag), "http:method:%s", http_method_str(parser->method));
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, tag);
    } else {
        snprintf(tag, sizeof(tag), "http:statuscode:%d", parser->status_code);
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, tag);
    }

    session->header[0] = 0;

    if (session->urlString) {
        char *ch = session->urlString->str;
        while (*ch) {
            if (*ch < 32) {
                moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "http:control-char");
                break;
            }
            ch++;
        }
    }

    if (session->hostString) {
        g_string_ascii_down(session->hostString);
    }

    if (session->urlString && session->hostString) {
        MolochString_t *hstring;

        HASH_FIND(s_, session->hosts, session->hostString->str+2, hstring);
        if (!hstring) {
            hstring = malloc(sizeof(*hstring));
            hstring->str = g_strdup(session->hostString->str+2);
            HASH_ADD(s_, session->hosts, hstring->str, hstring);
        }

        if (session->urlString->str[0] != '/') {
            char *result = strstr(session->urlString->str, session->hostString->str+2);

            /* If the host header is in the first 8 bytes of url then just use the url */
            if (result && result - session->urlString->str <= 8) {
                g_ptr_array_add(session->urlArray, g_string_free(session->urlString, FALSE));
                g_string_free(session->hostString, TRUE);
            } else {
                /* Host header doesn't match the url */
                g_string_append(session->hostString, ";");
                g_string_append(session->hostString, session->urlString->str);
                g_ptr_array_add(session->urlArray, g_string_free(session->hostString, FALSE));
                g_string_free(session->urlString, TRUE);
            }
        } else {
            /* Normal case, url starts with /, so no extra host in url */
            g_string_append(session->hostString, session->urlString->str);
            g_ptr_array_add(session->urlArray, g_string_free(session->hostString, FALSE));
            g_string_free(session->urlString, TRUE);
        }

        if (session->urlArray->len == 1)
            moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:http");

        session->urlString = NULL;
        session->hostString = NULL;
    } else if (session->urlString) {
        g_ptr_array_add(session->urlArray, g_string_free(session->urlString, FALSE));
        if (session->urlArray->len == 1)
            moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:http");

        session->urlString = NULL;
    } else if (session->hostString) {
        MolochString_t *hstring;

        HASH_FIND(s_, session->hosts, session->hostString->str+2, hstring);
        if (!hstring) {
            hstring = malloc(sizeof(*hstring));
            hstring->str = g_strdup(session->hostString->str+2);
            HASH_ADD(s_, session->hosts, hstring->str, hstring);
        }

        g_string_free(session->hostString, TRUE);
        session->hostString = NULL;
    }

    if (session->uaString) {
        MolochString_t *string;

        HASH_FIND(s_, session->userAgents, session->uaString->str, string);
        if (!string) {
            string = malloc(sizeof(*string));
            string->str = g_strdup(session->uaString->str);
            HASH_ADD(s_, session->userAgents, string->str, string);
        }

        g_string_free(session->uaString, TRUE);
        session->uaString = NULL;
    }

    if (session->xffString) {
        int i;
        gchar **parts = g_strsplit(session->xffString->str, ",", 0);

        for (i = 0; parts[i]; i++) {
            gchar *ip = parts[i];
            while (*ip == ' ')
                ip++;

            in_addr_t ia = inet_addr(ip);
            if (ia == 0 || ia == 0xffffffff) {
                moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "http:bad-xff");
                LOG("ERROR - Didn't understand ip: %s %s %d", session->xffString->str, ip, ia);
                continue;
            }

            MolochInt_t *mi;

            HASH_FIND(i_, session->xffs, (void*)(long)ia, mi);
            if (!mi) {
                mi = malloc(sizeof(*mi));
                mi->i = ia;
                HASH_ADD(i_, session->xffs, (void *)(long)mi->i, mi);
            }
        }

        g_strfreev(parts);
        g_string_free(session->xffString, TRUE);
        session->xffString = NULL;
    }
    return 0;
}

/******************************************************************************/
int
moloch_hp_cb_on_header_field (http_parser *parser, const char *at, size_t length)
{
    MolochSession_t *session = parser->data;

    if (session->inValue) {
        session->inValue = 0;
        session->header[0] = 0;
    }

    size_t remaining = sizeof(session->header) - strlen(session->header) - 1;
    if (remaining > 0)
        strncat(session->header, at, MIN(length, remaining));

    return 0;
}

/******************************************************************************/
int
moloch_hp_cb_on_header_value (http_parser *parser, const char *at, size_t length)
{
    MolochSession_t *session = parser->data;
    char header[200];

    if (!session->inValue) {
        session->inValue = 1;

        char *lower = g_ascii_strdown(session->header, -1);
        snprintf(header, sizeof(header), "http:header:%s", lower);
        g_free(lower);
        moloch_nids_add_tag(session, MOLOCH_TAG_HTTP_HEADERS, header);
    }

    if (parser->method && strcasecmp("host", session->header) == 0) {
        if (!session->hostString)
            session->hostString = g_string_new_len("//", 2);
        g_string_append_len(session->hostString, at, length);
    } 

    if (parser->method && strcasecmp("user-agent", session->header) == 0) {
        if (!session->uaString)
            session->uaString = g_string_new_len(at, length);
        else
            g_string_append_len(session->uaString, at, length);
    } 

    if (parser->method && strcasecmp("x-forwarded-for", session->header) == 0) {
        if (!session->xffString)
            session->xffString = g_string_new_len(at, length);
        else
            g_string_append_len(session->xffString, at, length);
    } 

    return 0;
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

    if (session->tcp_next)
        DLL_REMOVE(tcp_, &tcpWriteQ, session);

    g_array_free(session->filePosArray, TRUE);
    g_array_free(session->fileNumArray, TRUE);
    g_ptr_array_free(session->urlArray, TRUE);

    MolochString_t *string;
    HASH_FORALL_POP_HEAD(s_, session->hosts, string, 
        free(string->str);
        free(string);
    );

    HASH_FORALL_POP_HEAD(s_, session->hosts, string, 
        free(string->str);
        free(string);
    );

    MolochCertsInfo_t *certs;
    HASH_FORALL_POP_HEAD(t_, session->certs, certs, 
        moloch_nids_certs_free(certs);
    );

    if (session->urlString)
        g_string_free(session->urlString, TRUE);
    if (session->hostString)
        g_string_free(session->hostString, TRUE);
    if (session->uaString)
        g_string_free(session->uaString, TRUE);
    if (session->xffString)
        g_string_free(session->xffString, TRUE);

    if (session->rootId)
        g_free(session->rootId);
    g_hash_table_destroy(session->tags[0]);
    g_hash_table_destroy(session->tags[1]);
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
gboolean moloch_nids_watch_cb(gint UNUSED(fd), GIOCondition UNUSED(cond), gpointer UNUSED(data)) {
    int r = nids_dispatch(config.packetsPerPoll);
    if (r <= 0 && pcapFile)
        g_main_loop_quit(mainLoop);
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
    pcap_stats(nids_params.pcap_desc, &ps);
    return ps.ps_drop - initialDropped;
}
/******************************************************************************/
uint32_t moloch_nids_monitoring_sessions()
{
    return HASH_COUNT(h_, sessions);
}
/******************************************************************************/
void moloch_nids_detect_dns(MolochSession_t *session, unsigned char *data, int len) 
{

    if (len < 18)
        return;

    int qr      = (data[2] >> 7) & 0x1;

    if (qr != 0)
        return;
    
    int qdcount = (data[4] << 8) | data[5];

    unsigned char *ptr = data + 12;
    unsigned char  name[8000];

    if (qdcount > 10 || qdcount <= 0)
        return;

    int i;
    for (i = 0; (ptr < data + len) && i < qdcount; i++) {
        unsigned char *nptr = name;

        while (ptr < data + len) {
            int len = *ptr;
            ptr++;
            if (len == 0)
                break;
            if (nptr != name) {
                *nptr = '.';
                nptr++;
            }

            int j;
            for (j = 0; j < len; j++) {
                register u_char c = ptr[j];

                if (!isascii(c)) {
                    *(nptr++) = 'M';
                    *(nptr++) = '-';
                    c = toascii(c);
                }
                if (!isprint(c)) {
                    *(nptr++) = '^';
                    c ^= 0x40;
                } 

                *(nptr++) = c;
            }
            ptr  += len;
        }
        ptr += 4; // qtype && qclass
        *nptr = 0;

        if (name == nptr)
            break;

        MolochString_t *hstring;

        HASH_FIND(s_, session->hosts, name, hstring);
        if (!hstring) {
            hstring = malloc(sizeof(*hstring));
            hstring->str = (char *)g_strdup((char *)name);
            HASH_ADD(s_, session->hosts, hstring->str, hstring);
        }
    }
    moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:dns");
}
/******************************************************************************/
void moloch_nids_process_udp(MolochSession_t *session, struct udphdr *UNUSED(udphdr), unsigned char *data, int len)
{
    if (session->port1 == 53 || session->port2 == 53)
        moloch_nids_detect_dns(session, data, len);

}
/******************************************************************************/
void moloch_nids_root_init()
{
    char errbuf[1024];
    if (pcapFile) {
        nids_params.pcap_desc = pcap_open_offline(pcapFile, errbuf);
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
void moloch_nids_init()
{

    LOG("%s", pcap_lib_version());
    snprintf(nodeTag, sizeof(nodeTag), "node:%s", nodeName);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, nodeTag, NULL);

    if (config.nodeClass) {
        snprintf(classTag, sizeof(classTag), "node:%s", config.nodeClass);
        moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, classTag, NULL);
    }

    if (extraTag) {
        moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, extraTag, NULL);
    }

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

    memset(&parserSettings, 0, sizeof(parserSettings));
    parserSettings.on_message_begin = moloch_hp_cb_on_message_begin;
    parserSettings.on_url = moloch_hp_cb_on_url;
    parserSettings.on_body = moloch_hp_cb_on_body;
    parserSettings.on_message_complete = moloch_hp_cb_on_message_complete;
    parserSettings.on_header_field = moloch_hp_cb_on_header_field;
    parserSettings.on_header_value = moloch_hp_cb_on_header_value;

    HASH_INIT(h_, sessions, moloch_nids_session_hash, moloch_nids_session_cmp);
    DLL_INIT(tcp_, &tcpWriteQ);
    DLL_INIT(q_, &tcpSessionQ);
    DLL_INIT(q_, &udpSessionQ);
    DLL_INIT(q_, &icmpSessionQ);

    cookie = magic_open(MAGIC_MIME);
    if (!cookie) {
        LOG("Error with libmagic %s", magic_error(cookie));
    } else {
        magic_load(cookie, NULL);
    }

    nids_params.n_hosts = 1024;
    nids_params.tcp_workarounds = 1;
    nids_params.one_loop_less = 0;
    nids_params.scan_num_hosts = 0;
    nids_params.scan_num_ports = 0;
    nids_params.syslog = moloch_nids_syslog;
    nids_params.n_tcp_streams = config.maxStreams;

    if (config.bpf)
        nids_params.pcap_filter = config.bpf;


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
void moloch_nids_exit() {
    nids_exit();

    MolochSession_t *hsession;
    HASH_FORALL_POP_HEAD(h_, sessions, hsession, 
        moloch_db_save_session(hsession);
    );

    magic_close(cookie);
    if (!dryRun) {
        moloch_nids_file_flush(TRUE);
        close(dumperFd);
    }
}
