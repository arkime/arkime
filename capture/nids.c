/* nids.c  -- Functions for dealing with libnids
 *
 * Copyright 2012-2014 AOL Inc. All rights reserved.
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
#include <inttypes.h>
#include <pthread.h>
#include <sys/stat.h>
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

typedef struct moloch_output {
    struct moloch_output *mo_next, *mo_prev;
    uint16_t   mo_count;

    char      *buf;
    uint64_t   max;
    uint64_t   pos;
    int        fd;
    char       close;
} MolochOutput_t;


static MolochOutput_t        outputQ;
static MolochOutput_t       *output;
static pthread_mutex_t       outputQMutex;

static uint64_t              dumperFilePos = 0;
static char                 *dumperFileName;
static struct timeval        dumperFileTime;
static int                   dumperFd = -1;
static uint32_t              dumperId;
static FILE                 *offlineFile = 0;
static uint32_t              initialDropped = 0;
struct timeval               initialPacket;
static char                  offlinePcapFilename[PATH_MAX+1];

uint64_t                     totalPackets = 0;
uint64_t                     totalBytes = 0;
uint64_t                     totalSessions = 0;

/******************************************************************************/

typedef HASH_VAR(h_, MolochSessionHash_t, MolochSessionHead_t, 199337);
static MolochSessionHash_t *sessions[256];

/******************************************************************************/
void moloch_nids_session_free (MolochSession_t *session);
void moloch_nids_process_udp(MolochSession_t *session, struct udphdr   *udphdr, unsigned char *data, int len);
int  moloch_nids_next_file();
void moloch_nids_init_nids();

/******************************************************************************/
/* Must match moloch_nids_session_cmp
 * a1 0-3
 * p1 4-5
 * a2 6-9
 * p2 10-11
 */
uint32_t moloch_nids_session_hash(const void *key)
{
    unsigned char *p = (unsigned char *)key;
    //return ((p[2] << 16 | p[3] << 8 | p[4]) * 59) ^ (p[8] << 16 | p[9] << 8 |  p[10]);
    return (((p[1]<<24) ^ (p[2]<<18) ^ (p[3]<<12) ^ (p[4]<<6) ^ p[5]) * 13) ^ (p[8]<<24|p[9]<<16 | p[10]<<8 | p[11]);
}

/******************************************************************************/
int moloch_nids_session_cmp(const void *keyv, const void *elementv)
{
    unsigned char *key = (unsigned char*)keyv;
    MolochSession_t *element = (MolochSession_t *)elementv;

    if (element->addr1 < element->addr2) {
        return key[5] == ((element->port1 >> 8) & 0xff) && 
               key[4] == (element->port1 & 0xff) &&
               key[11] == ((element->port2 >> 8) & 0xff) &&
               key[10] == (element->port2 & 0xff) &&
               memcmp(key, &element->addr1, 4) == 0 &&
               memcmp(key + 6, &element->addr2, 4) == 0;
    } else if (element->addr1 > element->addr2) {
        return key[5] == ((element->port2 >> 8) & 0xff) && 
               key[4] == (element->port2 & 0xff) &&
               key[11] == ((element->port1 >> 8) & 0xff) &&
               key[10] == (element->port1 & 0xff) &&
               memcmp(key, &element->addr2, 4) == 0 &&
               memcmp(key + 6, &element->addr1, 4) == 0;
    } else if (element->port1 > element->port2) {
        return key[5] == ((element->port1 >> 8) & 0xff) && 
               key[4] == (element->port1 & 0xff) &&
               key[11] == ((element->port2 >> 8) & 0xff) &&
               key[10] == (element->port2 & 0xff) &&
               memcmp(key, &element->addr1, 4) == 0 &&
               memcmp(key + 6, &element->addr1, 4) == 0;
    } else {
        return key[5] == ((element->port2 >> 8) & 0xff) && 
               key[4] == (element->port2 & 0xff) &&
               key[11] == ((element->port1 >> 8) & 0xff) &&
               key[10] == (element->port1 & 0xff) &&
               memcmp(key, &element->addr1, 4) == 0 &&
               memcmp(key + 6, &element->addr1, 4) == 0;
    }
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
#define MOLOCH_SESSIONID_LEN 12
/******************************************************************************/
void moloch_session_id (char *buf, uint32_t addr1, uint16_t port1, uint32_t addr2, uint16_t port2)
{
    if (addr1 < addr2) {
        memcpy(buf, &addr1, 4);
        buf[4] = (port1 >> 8) & 0xff;
        buf[5] = port1 & 0xff;
        memcpy(buf + 6, &addr2, 4);
        buf[10] = (port2 >> 8) & 0xff;
        buf[11] = port2 & 0xff;
    } else if (addr1 > addr2) {
        memcpy(buf, &addr2, 4);
        buf[4] = (port2 >> 8) & 0xff;
        buf[5] = port2 & 0xff;
        memcpy(buf + 6, &addr1, 4);
        buf[10] = (port1 >> 8) & 0xff;
        buf[11] = port1 & 0xff;
    } else if (port1 < port2) {
        memcpy(buf, &addr1, 4);
        buf[4] = (port1 >> 8) & 0xff;
        buf[5] = port1 & 0xff;
        memcpy(buf + 6, &addr1, 4);
        buf[10] = (port2 >> 8) & 0xff;
        buf[11] = port2 & 0xff;
    } else {
        memcpy(buf, &addr1, 4);
        buf[4] = (port2 >> 8) & 0xff;
        buf[5] = port2 & 0xff;
        memcpy(buf + 6, &addr1, 4);
        buf[10] = (port1 >> 8) & 0xff;
        buf[11] = port1 & 0xff;
    }

}
/******************************************************************************/
void moloch_nids_save_session(MolochSession_t *session) 
{
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
            break;
        case IPPROTO_UDP:
            DLL_REMOVE(q_, &udpSessionQ, session);
            break;
        case IPPROTO_ICMP:
            DLL_REMOVE(q_, &icmpSessionQ, session);
            break;
        }

        HASH_REMOVE(h_, *(sessions[session->protocol]), session);
        return;
    }

    moloch_db_save_session(session, TRUE);

    HASH_REMOVE(h_, *(sessions[session->protocol]), session);
    moloch_nids_session_free(session);
}
/******************************************************************************/
void moloch_nids_mid_save_session(MolochSession_t *session) 
{
    if (pluginsCbs & MOLOCH_PLUGIN_PRE_SAVE)
        moloch_plugins_cb_pre_save(session, FALSE);

    /* If we are parsing pcap its ok to pause and make sure all tags are loaded */
    while (session->outstandingQueries > 0 && (config.pcapReadDir || config.pcapReadFile)) {
        g_main_context_iteration (g_main_context_default(), TRUE);
    }

    if (!session->rootId) {
        session->rootId = "ROOT";
    }

    moloch_db_save_session(session, FALSE);
    g_array_set_size(session->filePosArray, 0);
    g_array_set_size(session->fileNumArray, 0);

    if (session->tcp_next) {
        DLL_REMOVE(tcp_, &tcpWriteQ, session);
        DLL_PUSH_TAIL(tcp_, &tcpWriteQ, session);
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
int dlt_to_linktype(int dlt);
void moloch_nids_file_create()
{
    if (config.dryRun) {
        dumperFileName = "dryrun.pcap";
        dumperFd = 1;
        return;
    }

    dumperFileName = moloch_db_create_file(nids_last_pcap_header->ts.tv_sec, NULL, 0, &dumperId);
    dumperFilePos = 24;
    output = MOLOCH_TYPE_ALLOC0(MolochOutput_t);
    output->max = config.pcapWriteSize;
    if (config.writeMethod == MOLOCH_WRITE_DIRECT)
        posix_memalign((void **)&output->buf, 512, config.pcapWriteSize + 8192);
    else
        output->buf = malloc(config.pcapWriteSize + 8192);
    output->pos = 24;
    gettimeofday(&dumperFileTime, 0);

    struct pcap_file_header hdr;
    hdr.magic = 0xa1b2c3d4;
    hdr.version_major = PCAP_VERSION_MAJOR;
    hdr.version_minor = PCAP_VERSION_MINOR;

    hdr.thiszone = 0;
    hdr.snaplen = pcap_snapshot(nids_params.pcap_desc);
    hdr.sigfigs = 0;
    hdr.linktype = dlt_to_linktype(pcap_datalink(nids_params.pcap_desc)) | pcap_datalink_ext(nids_params.pcap_desc);

    memcpy(output->buf, &hdr, 24);

    LOG("Opening %s", dumperFileName);
    int options = O_LARGEFILE | O_NOATIME | O_WRONLY | O_NONBLOCK | O_CREAT | O_TRUNC;
    if (config.writeMethod == MOLOCH_WRITE_DIRECT)
        options |= O_DIRECT;
    dumperFd = open(dumperFileName,  options, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    if (dumperFd < 0) {
        LOG("ERROR - pcap open failed - Couldn't open file: '%s' with %s  (%d)", dumperFileName, strerror(errno), errno);
        exit (2);
    }
}
/******************************************************************************/
void moloch_nids_file_locked(char *filename)
{
    struct stat st;

    fstat(fileno(offlineFile), &st);

    dumperFilePos = 24;
    dumperFd = 1;

    if (config.dryRun) {
        dumperFileName = "dryrun.pcap";
        return;
    }

    dumperFileName = moloch_db_create_file(nids_last_pcap_header->ts.tv_sec, filename, st.st_size, &dumperId);
}
/******************************************************************************/
gboolean moloch_nids_output_cb(gint UNUSED(fd), GIOCondition UNUSED(cond), gpointer UNUSED(data)) 
{
    if (config.exiting && fd)
        return FALSE;

    MolochOutput_t *out = DLL_PEEK_HEAD(mo_, &outputQ);
    if (!out)
        return DLL_COUNT(mo_, &outputQ) > 0;

    int len;
    if (config.writeMethod == MOLOCH_WRITE_NORMAL) {
        len = write(out->fd, out->buf+out->pos, (out->max - out->pos));
        if (len < 0) {
            LOG("ERROR - Write %d failed with %d %d\n", out->fd, len, errno);
            exit (0);
        }
    } else {
        int wlen = (out->max - out->pos);
        int filelen = 0;
        if (out->close && wlen % config.pagesize != 0) {
            filelen = lseek(out->fd, 0, SEEK_CUR) + wlen;
            wlen = (wlen - (wlen % config.pagesize) + config.pagesize);
        }
        len = write(out->fd, out->buf+out->pos, wlen);
        if (len < 0) {
            LOG("ERROR - Write %d failed with %d %d\n", out->fd, len, errno);
            exit (0);
        }
        if (out->close && filelen) {
            ftruncate(out->fd, filelen);
        }
    }

    out->pos += len;

    // Still more to write out
    if (out->pos < out->max) {
        return TRUE;
    }

    // The last write for this fd
    char needClose = out->close;
    if (out->close) {
        close(out->fd);
    }

    // Cleanup buffer
    free(out->buf);
    DLL_REMOVE(mo_, &outputQ, out);
    MOLOCH_TYPE_FREE(MolochOutput_t, out);

    // More waiting to write on different fd, setup a new watch
    if (fd && needClose && DLL_COUNT(mo_, &outputQ) > 0) {
        out = DLL_PEEK_HEAD(mo_, &outputQ);
        moloch_watch_fd(out->fd, MOLOCH_GIO_WRITE_COND, moloch_nids_output_cb, NULL);
        return FALSE;
    }

    return DLL_COUNT(mo_, &outputQ) > 0;
}
/******************************************************************************/
void *moloch_nids_output_thread(void *UNUSED(arg))
{
    MolochOutput_t *out;
    while (1) {
        pthread_mutex_lock(&outputQMutex);
        int count = DLL_COUNT(mo_, &outputQ);
        if (count == 0) {
            pthread_mutex_unlock(&outputQMutex);
            usleep(100000);
            continue;
        }
        DLL_POP_HEAD(mo_, &outputQ, out);
        pthread_mutex_unlock(&outputQMutex);

        while (out->pos < out->max) {
            int len = write(out->fd, out->buf+out->pos, (out->max - out->pos));
            out->pos += len;
            if (len < 0) {
                LOG("ERROR - Write %d failed with %d %d\n", out->fd, len, errno);
                exit (0);
            }
        }

        if (out->close) {
            close(out->fd);
        }
        free(out->buf);
        MOLOCH_TYPE_FREE(MolochOutput_t, out);
    }
}
/******************************************************************************/
void moloch_nids_file_flush(gboolean all)
{
    if (config.dryRun) {
        return;
    }

    output->close = all;
    output->fd    = dumperFd;

    MolochOutput_t *noutput = MOLOCH_TYPE_ALLOC0(MolochOutput_t);
    noutput->max = config.pcapWriteSize;
    if (config.writeMethod == MOLOCH_WRITE_DIRECT)
        posix_memalign((void **)&noutput->buf, 512, config.pcapWriteSize + 8192);
    else
        noutput->buf = malloc(config.pcapWriteSize + 8192);


    all |= (output->pos <= output->max);

    if (all) {
        output->max = output->pos;
    } else {
        noutput->pos = output->pos - output->max;
        memcpy(noutput->buf, output->buf + output->max, noutput->pos);
    }
    output->pos = 0;

    int count;
    if (config.writeMethod == MOLOCH_WRITE_THREAD) {
        pthread_mutex_lock(&outputQMutex);
        DLL_PUSH_TAIL(mo_, &outputQ, output);
        count = DLL_COUNT(mo_, &outputQ);
        pthread_mutex_unlock(&outputQMutex);
    } else {
        DLL_PUSH_TAIL(mo_, &outputQ, output);
        count = DLL_COUNT(mo_, &outputQ);

        if (count == 1) {
            moloch_watch_fd(dumperFd, MOLOCH_GIO_WRITE_COND, moloch_nids_output_cb, NULL);
        }

    }

    if (count >= 100 && count % 50 == 0) {
        LOG("WARNING - %d output buffers waiting, disk IO system too slow?", count);
    }

    output = noutput;
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

    memcpy(output->buf + output->pos, (char *)&hdr, sizeof(hdr));
    output->pos += sizeof(hdr);

    memcpy(output->buf + output->pos, sp, h->caplen);
    output->pos += h->caplen;
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

        tcphdr = (struct tcphdr *)((char*)packet + 4 * packet->ip_hl);

        moloch_session_id(sessionId, packet->ip_src.s_addr, tcphdr->source, 
                          packet->ip_dst.s_addr, tcphdr->dest);
        break;
    case IPPROTO_UDP:
        sessionsQ = &udpSessionQ;
        sessionTimeout = config.udpTimeout;

        udphdr = (struct udphdr *)((char*)packet + 4 * packet->ip_hl);

        moloch_session_id(sessionId, packet->ip_src.s_addr, udphdr->source, 
                          packet->ip_dst.s_addr, udphdr->dest);
        break;
    case IPPROTO_ICMP:
        sessionsQ = &icmpSessionQ;
        sessionTimeout = config.icmpTimeout;

        moloch_session_id(sessionId, packet->ip_src.s_addr, 0, 
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

    totalBytes += nids_last_pcap_header->caplen;

    if (totalPackets == 1) {
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
          moloch_nids_disk_queue());
    }
    
    /* Get or Create Session */
    MolochSession_t *session;
    
    HASH_FIND(h_, *(sessions[packet->ip_p]), sessionId, session);

    if (!session) {
        session = MOLOCH_TYPE_ALLOC0(MolochSession_t);
        session->protocol = packet->ip_p;
        session->filePosArray = g_array_sized_new(FALSE, FALSE, sizeof(uint64_t), 100);
        session->fileNumArray = g_array_new(FALSE, FALSE, 4);
        HASH_ADD(h_, *sessions[packet->ip_p], sessionId, session);
        session->lastSave = nids_last_pcap_header->ts.tv_sec;
        session->firstPacket = nids_last_pcap_header->ts;
        session->addr1 = packet->ip_src.s_addr;
        session->addr2 = packet->ip_dst.s_addr;
        session->ip_tos = packet->ip_tos;
        session->fields = MOLOCH_SIZE_ALLOC0(fields, sizeof(MolochField_t *)*config.maxField);
        if (config.numPlugins > 0)
            session->pluginData = MOLOCH_SIZE_ALLOC0(pluginData, sizeof(void *)*config.numPlugins);

        moloch_parsers_initial_tag(session);

        switch (packet->ip_p) {
        case IPPROTO_TCP:
            /* If antiSynDrop option is set to true, capture will assume that 
            *if the syn-ack packet was captured first then the syn probably got dropped.*/
            if ((tcphdr->syn) && (tcphdr->ack) && (config.antiSynDrop)) {
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

    switch (packet->ip_p) {
    case IPPROTO_UDP:
        session->which = (session->addr1 == packet->ip_src.s_addr &&
                          session->addr2 == packet->ip_dst.s_addr &&
                          session->port1 == ntohs(udphdr->source) &&
                          session->port2 == ntohs(udphdr->dest))?0:1;
        session->databytes[session->which] += (nids_last_pcap_header->caplen - 8);
        moloch_nids_process_udp(session, udphdr, (unsigned char*)udphdr+8, nids_last_pcap_header->caplen - 8 - 4 * packet->ip_hl);
        break;
    case IPPROTO_TCP:
        session->which = (session->addr1 == packet->ip_src.s_addr &&
                          session->addr2 == packet->ip_dst.s_addr &&
                          session->port1 == ntohs(tcphdr->source) &&
                          session->port2 == ntohs(tcphdr->dest))?0:1;
        session->tcp_flags |= *((char*)packet + 4 * packet->ip_hl+12);
        break;
    }

    session->bytes[session->which] += nids_last_pcap_header->caplen;
    session->lastPacket = nids_last_pcap_header->ts;

    if (pluginsCbs & MOLOCH_PLUGIN_IP)
        moloch_plugins_cb_ip(session, packet, len);

    session->packets[session->which]++;
    if (!config.dryRun && !session->dontSave) {
        if (config.copyPcap) {
            /* Save packet to file */
            if (dumperFd == -1 || dumperFilePos >= config.maxFileSizeB) {
                if (dumperFd > 0 && !config.dryRun)  {
                    moloch_nids_file_flush(TRUE);
                }
                moloch_nids_file_create();
            }

            //LOG("ALW POS %d %llx", dumperFilePos, val);
            if (session->fileNumArray->len == 0 || g_array_index(session->fileNumArray, uint32_t, session->fileNumArray->len-1) != dumperId) {
                g_array_append_val(session->fileNumArray, dumperId);
                int64_t val = -1LL * dumperId;
                g_array_append_val(session->filePosArray, val);
            }
            g_array_append_val(session->filePosArray, dumperFilePos);

            if (config.fakePcap) {
                output->buf[output->pos] = 'P';
                output->pos++;
            } else {
                moloch_nids_pcap_dump(nids_last_pcap_header, nids_last_pcap_data);
                dumperFilePos += 16 + nids_last_pcap_header->caplen;
            }

            if (output->pos > output->max) {
                moloch_nids_file_flush(FALSE);
            }
        } else {
            if (dumperFd == -1) {
                moloch_nids_file_locked(offlinePcapFilename);
            }

            dumperFilePos = ftell(offlineFile) - 16 - nids_last_pcap_header->caplen;
            if (session->fileNumArray->len == 0 || g_array_index(session->fileNumArray, uint32_t, session->fileNumArray->len-1) != dumperId) {
                g_array_append_val(session->fileNumArray, dumperId);
                int64_t val = -1LL * dumperId;
                g_array_append_val(session->filePosArray, val);
            }
            g_array_append_val(session->filePosArray, dumperFilePos);
        }

        if (session->packets[0] + session->packets[1] >= config.maxPackets) {
            moloch_nids_mid_save_session(session);
        }
    } else if (config.tests) {
        dumperFilePos = ftell(offlineFile) - 16 - nids_last_pcap_header->caplen;
        g_array_append_val(session->filePosArray, dumperFilePos);
    }

    /* Clean up the Q, only 1 per incoming packet so we don't fall behind */
    if ((headSession = DLL_PEEK_HEAD(q_, sessionsQ)) &&
           ((uint64_t)headSession->lastPacket.tv_sec + sessionTimeout < (uint64_t)nids_last_pcap_header->ts.tv_sec)) {

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
           (headSession->lastSave + config.tcpSaveTimeout < (uint64_t)nids_last_pcap_header->ts.tv_sec)) {

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
void moloch_nids_get_tag_cb(void *session, int tagtype, uint32_t tag)
{
    moloch_field_int_add(tagtype, session, tag);
    moloch_nids_decr_outstanding(session);
}
/******************************************************************************/
gboolean moloch_nids_has_tag(MolochSession_t *session, int tagType, const char *tagName) 
{
    uint32_t tagValue;

    if (!session->fields[tagType])
        return FALSE;

    if ((tagValue = moloch_db_peek_tag(tagName)) == 0)
        return FALSE;

    MolochInt_t          *hint;
    HASH_FIND_INT(i_, *(session->fields[tagType]->ihash), tagValue, hint);
    return hint != 0;
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

        moloch_session_id(key, a_tcp->addr.saddr, htons(a_tcp->addr.source), a_tcp->addr.daddr, htons(a_tcp->addr.dest));
        HASH_FIND(h_, *sessions[IPPROTO_TCP], key, session);
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
    case NIDS_DATA: {
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

            session->which = 1;

            if (a_tcp->client.offset == session->consumed[1])
                moloch_parsers_classify_tcp(session, data, count);

            int i;
            int totConsumed = 0;
            int consumed = 0;

            for (i = 0; i < session->parserNum; i++)
                if (session->parserInfo[i].parserFunc) {
                    consumed = session->parserInfo[i].parserFunc(session, session->parserInfo[i].uw, dataNew + totConsumed, countNew - totConsumed);
                    if (consumed) {
                        totConsumed += consumed;
                        session->consumed[1] += consumed;
                    }

                    if (consumed >= countNew)
                        break;
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
                session->firstBytesLen[1] = MIN(8, countNew);
                memcpy(session->firstBytes[1], data, session->firstBytesLen[1]);
            }
        }

        if (a_tcp->server.count_new) {
            int countNew      = a_tcp->server.count_new;
            int count         = a_tcp->server.count - a_tcp->server.offset;
            unsigned char *dataNew = (unsigned char*)(a_tcp->server.data + (a_tcp->server.count - a_tcp->server.offset - countNew));
            unsigned char *data    = (unsigned char*)a_tcp->server.data;

            session->which = 0;

            if (a_tcp->server.offset == session->consumed[0])
                moloch_parsers_classify_tcp(session, data, count);

            int i;
            int totConsumed = 0;
            int consumed = 0;
            for (i = 0; i < session->parserNum; i++)
                if (session->parserInfo[i].parserFunc) {
                    consumed = session->parserInfo[i].parserFunc(session, session->parserInfo[i].uw, dataNew + totConsumed, countNew - totConsumed);

                    if (consumed) {
                        totConsumed += consumed;
                        session->consumed[0] += consumed;
                    }

                    if (consumed >= countNew)
                        break;
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
                session->firstBytesLen[0] = MIN(8, countNew);
                memcpy(session->firstBytes[0], data, session->firstBytesLen[0]);
            }
        }

        session->databytes[session->which] += a_tcp->server.count_new + a_tcp->client.count_new;

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
                moloch_yara_execute(session, (unsigned char*)a_tcp->client.data, a_tcp->client.count, a_tcp->client.offset == 0);
            }

            if (a_tcp->server.count != a_tcp->server.offset) {
                moloch_yara_execute(session, (unsigned char*)a_tcp->server.data, a_tcp->server.count, a_tcp->server.offset == 0);
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

    if (session->tcp_next)
        DLL_REMOVE(tcp_, &tcpWriteQ, session);

    g_array_free(session->filePosArray, TRUE);
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
		   dumperFileName, nids_warnings[errnum], saddr, daddr);
	} else {
            if (iph->ip_hl < 5) {
                LOG("NIDSIP:%s %s - %d (iph->ip_hl) < 5", dumperFileName, nids_warnings[errnum], iph->ip_hl);
            } else if (iph->ip_v != 4) {
                LOG("NIDSIP:%s %s - %d (iph->ip_v) != 4", dumperFileName, nids_warnings[errnum], iph->ip_v);
            } else if (ntohs(iph->ip_len) < iph->ip_hl << 2) {
                LOG("NIDSIP:%s %s - %d < %d (iph->ip_len < iph->ip_hl)", dumperFileName, nids_warnings[errnum], ntohs(iph->ip_len), iph->ip_hl << 2);
            } else {
                LOG("NIDSIP:%s %s - Length issue, was packet truncated?", dumperFileName, nids_warnings[errnum]);
            }
        }
	break;

    case NIDS_WARN_TCP:
	strcpy(saddr, int_ntoa(iph->ip_src.s_addr));
	strcpy(daddr, int_ntoa(iph->ip_dst.s_addr));
        if (errnum == NIDS_WARN_TCP_TOOMUCH || errnum == NIDS_WARN_TCP_BIGQUEUE) {
            /* ALW - Should do something with it */
	} else if (errnum != NIDS_WARN_TCP_HDR)
	    LOG("NIDSTCP:%s %s,from %s:%hu to  %s:%hu", dumperFileName, nids_warnings[errnum],
		saddr, ntohs(((struct tcphdr *) data)->source), daddr,
		ntohs(((struct tcphdr *) data)->dest));
	else
	    LOG("NIDSTCP:%s %s,from %s to %s", dumperFileName,
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
    if (moloch_nids_disk_queue() > 10) {
        return TRUE;
    }

    // pause reading if too many waiting ES operations
    if (moloch_http_queue_length(esServer) > 100) {
        return TRUE;
    }

    int r = nids_dispatch(config.packetsPerPoll);

    // Some kind of failure, move to the next file or quit
    if (r <= 0) {
        if (config.pcapReadDir && moloch_nids_next_file()) {
            g_timeout_add(10, moloch_nids_next_file_gfunc, 0);
            return FALSE;
        }

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
    if (!sessions[IPPROTO_TCP])
        return 0;

    return HASH_COUNT(h_, *sessions[IPPROTO_TCP]) + HASH_COUNT(h_, *sessions[IPPROTO_UDP]) + HASH_COUNT(h_, *sessions[IPPROTO_ICMP]);
}
/******************************************************************************/
uint32_t moloch_nids_disk_queue()
{
    int count;
    if (config.writeMethod == MOLOCH_WRITE_THREAD) {
        pthread_mutex_lock(&outputQMutex);
        count = DLL_COUNT(mo_, &outputQ);
        pthread_mutex_unlock(&outputQMutex);
    } else {
        count = DLL_COUNT(mo_, &outputQ);
    }
    return count;
}
/******************************************************************************/
void moloch_nids_process_udp(MolochSession_t *session, struct udphdr *udphdr, unsigned char *data, int len)
{
    moloch_parsers_classify_udp(session, data, len);

    if (pluginsCbs & MOLOCH_PLUGIN_UDP)
        moloch_plugins_cb_udp(session, udphdr, data, len);

    if (session->firstBytesLen[session->which] == 0) {
        session->firstBytesLen[session->which] = MIN(8, len);
        memcpy(session->firstBytes[session->which], data, session->firstBytesLen[session->which]);
    }
}
/******************************************************************************/
static pcap_t *closeNextOpen = 0;
int moloch_nids_next_file()
{
    static GDir *pcapGDir[21];
    static char *pcapBase[21];
    static int   pcapGDirLevel = -1;
    GError      *error = 0;
    char         errbuf[1024];

    if (pcapGDirLevel == -1) {
        pcapGDirLevel = 0;
        pcapBase[0] = config.pcapReadDir;
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

        gchar *fullfilename;
        fullfilename = g_build_filename (pcapBase[pcapGDirLevel], filename, NULL);

        // If recursive option and a directory then process all the files in that dir
        if (config.pcapRecursive && g_file_test(fullfilename, G_FILE_TEST_IS_DIR)) {
            if (pcapGDirLevel >= 20)
                continue;
            pcapBase[pcapGDirLevel+1] = fullfilename;
            pcapGDirLevel++;
            return moloch_nids_next_file();
        }

        // If it doesn't end with pcap we ignore it
        if (strcasecmp(".pcap", filename + strlen(filename)-5) != 0) {
            g_free(fullfilename);
            continue;
        }

        if (!realpath(fullfilename, offlinePcapFilename)) {
            g_free(fullfilename);
            continue;
        }
        LOG ("Processing %s", fullfilename);


        if (!config.copyPcap) {
            dumperFd = -1;
        }

        errbuf[0] = 0;
        if (nids_params.pcap_desc)
            closeNextOpen = nids_params.pcap_desc;
        nids_params.pcap_desc = pcap_open_offline(fullfilename, errbuf);
        if (!nids_params.pcap_desc) {
            LOG("Couldn't process '%s' error '%s'", fullfilename, errbuf);
            g_free(fullfilename);
            continue;
        }
        offlineFile = pcap_file(nids_params.pcap_desc);
        g_free(fullfilename);
        return 1;
    }
    g_dir_close(pcapGDir[pcapGDirLevel]);
    pcapGDir[pcapGDirLevel] = 0;

    if (pcapGDirLevel > 0) {
        g_free(pcapBase[pcapGDirLevel]);
        pcapGDirLevel--;
        return moloch_nids_next_file();
    }

    return 0;
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
        if (nids_params.pcap_desc) {
            offlineFile = pcap_file(nids_params.pcap_desc);
            if (!realpath(config.pcapReadFile, offlinePcapFilename)) {
                LOG("ERROR - pcap open failed - Couldn't realpath file: '%s' with %d", config.pcapReadFile, errno);
                exit(1);
            }
        }
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

    config.maxWriteBuffers = (config.pcapReadDir || config.pcapReadFile)?10:2000;
}
/******************************************************************************/
gboolean moloch_nids_check_file_time_gfunc (gpointer UNUSED(user_data))
{
    static struct timeval tv;
    gettimeofday(&tv, 0);

    if (dumperFilePos > 24 && (tv.tv_sec - dumperFileTime.tv_sec) >= config.maxFileTimeM*60) {
        moloch_nids_file_flush(TRUE);
        moloch_nids_file_create();
    }

    return TRUE;
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
    if (config.pcapReadDir || config.pcapReadFile)
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
void moloch_nids_init()
{

    LOG("%s", pcap_lib_version());

    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "tcp", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "udp", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "protocol:http", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "protocol:ssh", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "protocol:smtp", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "protocol:ftp", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "protocol:pop3", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "protocol:gh0st", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "protocol:dns", NULL);

    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "http:statuscode:200", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "http:statuscode:204", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "http:statuscode:301", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "http:statuscode:302", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "http:statuscode:304", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "http:statuscode:400", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "http:statuscode:404", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "http:statuscode:500", NULL);

    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "http:method:GET", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "http:method:POST", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "http:method:HEAD", NULL);

    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "http:content:application/octet-stream", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "http:content:text/plain", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "http:content:text/html", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "http:content:application/x-gzip", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "http:content:application/x-shockwave-flash", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "http:content:image/gif", NULL);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, "http:content:image/jpg", NULL);

    memset(sessions, 0, sizeof(MolochSessionHash_t *) * 256);
    sessions[IPPROTO_UDP] = malloc(sizeof(MolochSessionHash_t));
    sessions[IPPROTO_TCP] = malloc(sizeof(MolochSessionHash_t));
    sessions[IPPROTO_ICMP] = malloc(sizeof(MolochSessionHash_t));
    HASH_INIT(h_, *(sessions[IPPROTO_UDP]), moloch_nids_session_hash, moloch_nids_session_cmp);
    HASH_INIT(h_, *(sessions[IPPROTO_TCP]), moloch_nids_session_hash, moloch_nids_session_cmp);
    HASH_INIT(h_, *(sessions[IPPROTO_ICMP]), moloch_nids_session_hash, moloch_nids_session_cmp);
    DLL_INIT(tcp_, &tcpWriteQ);
    DLL_INIT(q_, &tcpSessionQ);
    DLL_INIT(q_, &udpSessionQ);
    DLL_INIT(q_, &icmpSessionQ);

    DLL_INIT(mo_, &outputQ);

    nids_params.n_hosts = 1024;
    nids_params.tcp_workarounds = 1;
    nids_params.one_loop_less = 0;
    nids_params.scan_num_hosts = 0;
    nids_params.scan_num_ports = 0;
    nids_params.syslog = moloch_nids_syslog;
    nids_params.n_tcp_streams = config.maxStreams;

    if (config.bpf)
        nids_params.pcap_filter = config.bpf;

    if (config.maxFileTimeM > 0 && !config.dryRun && config.copyPcap) {
        g_timeout_add_seconds( 30, moloch_nids_check_file_time_gfunc, 0);
    }

    moloch_nids_init_nids();

    if (config.writeMethod == MOLOCH_WRITE_THREAD) {
        pthread_t thread;
        int rc = pthread_create(&thread, NULL, &moloch_nids_output_thread, NULL);
        if (rc) {
            LOG("ERROR - thread create failed - %d %s (%d)", rc, strerror(errno), errno);
            exit (2);
        }
    }
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
    for (i = 0; i < 256; i++) {
        if (!sessions[i])
            continue;

#ifdef PRINT_BUCKETS
        // Print out the histogram for buckets, see how we are doing
        printf("\nBuckets for %d:\n", i);
        int buckets[51];
        int total[51];
        memset(buckets, 0, sizeof(buckets));
        memset(total, 0, sizeof(total));
        int b;
        for ( b = 0;  b < sessions[i]->size;  b++) {
            if (sessions[i]->buckets[b].h_count >= 50) {
                buckets[50]++; 
                total[50] += sessions[i]->buckets[b].h_count;
            } else {
                buckets[(sessions[i]->buckets[b].h_count)]++;
                total[(sessions[i]->buckets[b].h_count)] += sessions[i]->buckets[b].h_count;
            }
        }
        for ( b = 0;  b <= 50;  b++) {
            if (buckets[b])
                printf(" %2d: %7d %7d\n", b, buckets[b], total[b]);
        } 
#endif

        MolochSession_t *hsession;
        HASH_FORALL_POP_HEAD(h_, *sessions[i], hsession, 
            moloch_db_save_session(hsession, TRUE);
        );
    }

    if (!config.dryRun && config.copyPcap) {
        moloch_nids_file_flush(TRUE);
        if (config.writeMethod == MOLOCH_WRITE_THREAD) {
            while (moloch_nids_disk_queue() >0) {
                usleep(10000);
            }
        } else {
            // Write out all the buffers
            while (DLL_COUNT(mo_, &outputQ) > 0) {
                moloch_nids_output_cb(0, 0, 0);
            }
        }
    }
}
