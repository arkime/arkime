/******************************************************************************/
/* moloch.h -- General Moloch include file
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#define __FAVOR_BSD
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <sys/time.h>
#include "http_parser.h"
#include "dll.h"
#include "hash.h"
#include "bsb.h"
#include "glib.h"

#define UNUSED(x) x __attribute((unused))


#define MOLOCH_API_VERSION 101

#define MOLOCH_SESSIONID_LEN 37

#define MOLOCH_V6_TO_V4(_addr) (((uint32_t *)(_addr).s6_addr)[3])

#define MOLOCH_PACKET_MAX_LEN 0x10000

#define MOLOCH_SESSION_v6(s) ((s)->sessionId[0] == 37)

/******************************************************************************/
/*
 * Base Hash Table Types
 */
typedef struct moloch_int {
    struct moloch_int    *i_next, *i_prev;
    uint32_t              i_hash;
    short                 i_bucket;
} MolochInt_t;

typedef struct {
    struct moloch_int *i_next, *i_prev;
    int i_count;
} MolochIntHead_t;

typedef HASH_VAR(s_, MolochIntHash_t, MolochIntHead_t, 1);
typedef HASH_VAR(s_, MolochIntHashStd_t, MolochIntHead_t, 13);

typedef struct moloch_string {
    struct moloch_string *s_next, *s_prev;
    char                 *str;
    uint32_t              s_hash;
    gpointer              uw;
    short                 s_bucket;
    short                 len:15;
    short                 utf8:1;
} MolochString_t;

typedef struct {
    struct moloch_string *s_next, *s_prev;
    int s_count;
} MolochStringHead_t;
typedef HASH_VAR(s_, MolochStringHash_t, MolochStringHead_t, 1);
typedef HASH_VAR(s_, MolochStringHashStd_t, MolochStringHead_t, 13);

/******************************************************************************/
/*
 * TRIE
 */
typedef struct moloch_trie_node {
    void                     *data;
    struct moloch_trie_node **children;
    uint8_t                   value, first, last;
} MolochTrieNode_t;

typedef struct moloch_trie {
    int size;
    MolochTrieNode_t root;
} MolochTrie_t;

/******************************************************************************/
/*
 * Certs Info
 */

typedef struct {
    MolochStringHead_t  commonName; // 2.5.4.3
    char               *orgName;    // 2.5.4.10
    char                orgUtf8;
} MolochCertInfo_t;

typedef struct moloch_certsinfo {
    struct moloch_certsinfo *t_next, *t_prev;
    uint32_t                 t_hash;
    uint64_t                 notBefore;
    uint64_t                 notAfter;
    MolochCertInfo_t         issuer;
    MolochCertInfo_t         subject;
    MolochStringHead_t       alt;
    unsigned char           *serialNumber;
    short                    serialNumberLen;
    short                    t_bucket;
    unsigned char            hash[60];
    char                     isCA;
} MolochCertsInfo_t;

typedef struct {
    struct moloch_certsinfo *t_next, *t_prev;
    int                      t_count;
} MolochCertsInfoHead_t;

typedef HASH_VAR(s_, MolochCertsInfoHash_t, MolochCertsInfoHead_t, 1);
typedef HASH_VAR(s_, MolochCertsInfoHashStd_t, MolochCertsInfoHead_t, 5);


/******************************************************************************/
/*
 * Information about the various fields that we capture
 */

#define MOLOCH_FIELD_TYPE_INT        0
#define MOLOCH_FIELD_TYPE_INT_ARRAY  1
#define MOLOCH_FIELD_TYPE_INT_HASH   2
#define MOLOCH_FIELD_TYPE_INT_GHASH  3
#define MOLOCH_FIELD_TYPE_STR        4
#define MOLOCH_FIELD_TYPE_STR_ARRAY  5
#define MOLOCH_FIELD_TYPE_STR_HASH   6
#define MOLOCH_FIELD_TYPE_STR_GHASH  7
#define MOLOCH_FIELD_TYPE_IP         8
#define MOLOCH_FIELD_TYPE_IP_GHASH   9
#define MOLOCH_FIELD_TYPE_CERTSINFO 10

/* These are ones you should set */
/* Field should be set on all linked sessions */
#define MOLOCH_FIELD_FLAG_LINKED_SESSIONS    0x0001
/* Force the field to be utf8 */
#define MOLOCH_FIELD_FLAG_FORCE_UTF8         0x0004
/* Don't create in fields db table */
#define MOLOCH_FIELD_FLAG_NODB               0x0008
/* Don't create in capture list */
#define MOLOCH_FIELD_FLAG_FAKE               0x0010
/* Don't create in capture list */
#define MOLOCH_FIELD_FLAG_DISABLED           0x0020
/* Added Cnt */
#define MOLOCH_FIELD_FLAG_CNT                0x1000
/* prepend ip stuff - dont use*/
#define MOLOCH_FIELD_FLAG_IPPRE              0x4000



typedef struct moloch_field_info {
    struct moloch_field_info *d_next, *d_prev; /* Must be first */
    char                     *dbFieldFull;     /* Must be second - this is the full version example:mysql.user-term */
    char                     *dbField;         /* - this is the version used in db writing example:user-term */
    uint32_t                  d_hash;
    uint32_t                  d_bucket;
    uint32_t                  d_count;

    struct moloch_field_info *e_next, *e_prev;
    char                     *expression;
    uint32_t                  e_hash;
    uint32_t                  e_bucket;
    uint32_t                  e_count;

    int                       dbFieldLen;
    int                       dbGroupNum;
    char                     *dbGroup;
    int                       dbGroupLen;
    char                     *group;
    char                     *kind;
    char                     *category;
    int                       pos;
    uint16_t                  type;
    uint16_t                  flags;
    char                      ruleEnabled;
} MolochFieldInfo_t;

typedef struct {
    union {
        char                     *str;
        GPtrArray                *sarray;
        MolochStringHashStd_t    *shash;
        int                       i;
        GArray                   *iarray;
        MolochIntHashStd_t       *ihash;
        MolochCertsInfoHashStd_t *cihash;
        GHashTable               *ghash;
        struct in6_addr          *ip;
    };
    uint32_t                   jsonSize;
} MolochField_t;

#define MOLOCH_FIELD_OPS_FLAGS_COPY 0x0001

typedef struct {
    char                 *str;
    int                   strLenOrInt;
    int16_t               fieldPos;
} MolochFieldOp_t;

typedef struct {
    MolochFieldOp_t     *ops;
    uint16_t              size;
    uint16_t              num;
    uint16_t              flags;
} MolochFieldOps_t;

#define MOLOCH_LOCK_DEFINE(var)         pthread_mutex_t var##_mutex = PTHREAD_MUTEX_INITIALIZER
#define MOLOCH_LOCK_EXTERN(var)         pthread_mutex_t var##_mutex
#define MOLOCH_LOCK_INIT(var)           pthread_mutex_init(&var##_mutex, NULL)
#define MOLOCH_LOCK(var)                pthread_mutex_lock(&var##_mutex)
#define MOLOCH_UNLOCK(var)              pthread_mutex_unlock(&var##_mutex)

#define MOLOCH_COND_DEFINE(var)         pthread_cond_t var##_cond = PTHREAD_COND_INITIALIZER
#define MOLOCH_COND_EXTERN(var)         pthread_cond_t var##_cond
#define MOLOCH_COND_INIT(var)           pthread_cond_init(&var##_cond, NULL)
#define MOLOCH_COND_WAIT(var)           pthread_cond_wait(&var##_cond, &var##_mutex)
#define MOLOCH_COND_TIMEDWAIT(var, _ts) pthread_cond_timedwait(&var##_cond, &var##_mutex, &_ts)
#define MOLOCH_COND_BROADCAST(var)      pthread_cond_broadcast(&var##_cond)
#define MOLOCH_COND_SIGNAL(var)         pthread_cond_signal(&var##_cond)

#define MOLOCH_THREAD_INCR(var)          __sync_add_and_fetch(&var, 1);
#define MOLOCH_THREAD_INCRNEW(var)       __sync_add_and_fetch(&var, 1);
#define MOLOCH_THREAD_INCROLD(var)       __sync_fetch_and_add(&var, 1);
#define MOLOCH_THREAD_INCR_NUM(var, num) __sync_fetch_and_add(&var, num);

/* You are probably looking here because you think 24 is too low, really it isn't.
 * Instead, increase the number of threads used for reading packets.
 * https://github.com/aol/moloch/wiki/FAQ#why-am-i-dropping-packets
 */
#define MOLOCH_MAX_PACKET_THREADS 24

#define MAX_INTERFACES 32

#ifndef LOCAL
#define LOCAL static
#endif

#ifndef CLOCK_REALTIME_COARSE
#define CLOCK_REALTIME_COARSE CLOCK_REALTIME
#endif

/******************************************************************************/

#define SESSION_TCP   0
#define SESSION_UDP   1
#define SESSION_ICMP  2
#define SESSION_SCTP  3
#define SESSION_ESP   4
#define SESSION_MAX   5

/******************************************************************************/
/*
 * Configuration Information
 */
enum MolochRotate { MOLOCH_ROTATE_HOURLY, MOLOCH_ROTATE_HOURLY6, MOLOCH_ROTATE_DAILY, MOLOCH_ROTATE_WEEKLY, MOLOCH_ROTATE_MONTHLY };

#define MOLOCH_FIELDS_MAX 256
#define MOLOCH_FIELD_EXSPECIAL_SRC_IP       (MOLOCH_FIELDS_MAX-1)
#define MOLOCH_FIELD_EXSPECIAL_SRC_PORT     (MOLOCH_FIELDS_MAX-2)
#define MOLOCH_FIELD_EXSPECIAL_DST_IP       (MOLOCH_FIELDS_MAX-3)
#define MOLOCH_FIELD_EXSPECIAL_DST_PORT     (MOLOCH_FIELDS_MAX-4)
#define MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_SYN (MOLOCH_FIELDS_MAX-5)
#define MOLOCH_FIELD_EXSPECIAL_PACKETS_SRC  (MOLOCH_FIELDS_MAX-6)
#define MOLOCH_FIELD_EXSPECIAL_PACKETS_DST  (MOLOCH_FIELDS_MAX-7)
#define MOLOCH_FIELD_EXSPECIAL_START        (MOLOCH_FIELDS_MAX-7)

typedef struct moloch_config {
    gboolean  quitting;
    char     *configFile;
    char     *nodeName;
    char     *hostName;
    char    **pcapReadFiles;
    char    **pcapReadDirs;
    gboolean  pcapReadOffline;
    gchar   **extraTags;
    gchar   **extraOps;
    MolochFieldOps_t ops;
    gchar     debug;
    gchar     insecure;
    gboolean  quiet;
    gboolean  dryRun;
    gboolean  noSPI;
    gboolean  copyPcap;
    gboolean  pcapRecursive;
    gboolean  tests;
    gboolean  pcapMonitor;
    gboolean  pcapDelete;
    gboolean  pcapSkip;
    gboolean  flushBetween;
    gboolean  noLoadTags;
    gboolean  trackESP;
    gint      pktsToRead;

    uint64_t  ipSavePcap[4];
    uint64_t  etherSavePcap[1024];

    enum MolochRotate rotate;

    int       writeMethod;

    HASH_VAR(s_, dontSaveTags, MolochStringHead_t, 11);
    MolochFieldInfo_t *fields[MOLOCH_FIELDS_MAX];
    int                maxField;
    int                tagsStringField;

    int                numPlugins;

    GRegex  *offlineRegex;
    char     *prefix;
    char     *nodeClass;
    char     *elasticsearch;
    char    **interface;
    int       pcapDirPos;
    char    **pcapDir;
    char     *pcapDirTemplate;
    char     *bpf;
    char     *yara;
    char     *emailYara;
    char     *geoLite2ASN;
    char     *geoLite2Country;
    char     *rirFile;
    char     *ouiFile;
    char     *dropUser;
    char     *dropGroup;
    char    **pluginsDir;
    char    **parsersDir;

    char    **rootPlugins;
    char    **plugins;
    char    **smtpIpHeaders;

    double    maxFileSizeG;
    uint64_t  maxFileSizeB;
    uint32_t  maxFileTimeM;
    uint32_t  timeouts[SESSION_MAX];
    uint32_t  tcpSaveTimeout;
    uint32_t  maxStreams[SESSION_MAX];
    uint32_t  maxPackets;
    uint32_t  maxPacketsInQueue;
    uint32_t  dbBulkSize;
    uint32_t  dbFlushTimeout;
    uint32_t  maxESConns;
    uint32_t  maxESRequests;
    uint32_t  logEveryXPackets;
    uint32_t  pcapBufferSize;
    uint32_t  pcapWriteSize;
    uint32_t  maxWriteBuffers;
    uint32_t  maxFreeOutputBuffers;
    uint32_t  fragsTimeout;
    uint32_t  maxFrags;
    uint32_t  snapLen;
    uint32_t  maxMemPercentage;
    uint32_t  maxReqBody;

    int       packetThreads;

    char      logUnknownProtocols;
    char      logESRequests;
    char      logFileCreation;
    char      logHTTPConnections;
    char      parseSMTP;
    char      parseSMB;
    char      parseQSValue;
    char      parseCookieValue;
    char      supportSha256;
    char      reqBodyOnlyUtf8;
    char      compressES;
    char      antiSynDrop;
    char      readTruncatedPackets;
    char     *pcapDirAlgorithm;
} MolochConfig_t;

typedef struct {
    char     *country;
    char     *asn;
    char     *rir;
    int       numtags;
    char     *tagsStr[10];
} MolochIpInfo_t;

/******************************************************************************/
/*
 * Parser
 */

struct moloch_session;

typedef int  (* MolochParserFunc) (struct moloch_session *session, void *uw, const unsigned char *data, int remaining, int which);
typedef void (* MolochParserFreeFunc) (struct moloch_session *session, void *uw);
typedef void (* MolochParserSaveFunc) (struct moloch_session *session, void *uw, int final);

typedef struct {
    MolochParserFunc      parserFunc;
    void                 *uw;
    MolochParserFreeFunc  parserFreeFunc;
    MolochParserSaveFunc  parserSaveFunc;

} MolochParserInfo_t;

/******************************************************************************/
struct moloch_pcap_timeval {
    int32_t tv_sec;		   /* seconds */
    int32_t tv_usec;	   	   /* microseconds */
};
struct moloch_pcap_sf_pkthdr {
    struct moloch_pcap_timeval ts; /* time stamp */
    uint32_t caplen;		   /* length of portion present */
    uint32_t pktlen;		   /* length this packet (off wire) */
};

/******************************************************************************/
#define MOLOCH_PACKET_TUNNEL_GRE    0x1
#define MOLOCH_PACKET_TUNNEL_PPPOE  0x2
#define MOLOCH_PACKET_TUNNEL_MPLS   0x4
#define MOLOCH_PACKET_TUNNEL_PPP    0x8

typedef struct molochpacket_t
{
    struct molochpacket_t   *packet_next, *packet_prev;
    struct timeval ts;             // timestamp
    uint8_t       *pkt;            // full packet
    uint64_t       writerFilePos;  // where in output file
    uint64_t       readerFilePos;  // where in input file
    uint32_t       writerFileNum;  // file number in db
    uint32_t       hash;           // Saved hash
    uint16_t       pktlen;         // length of packet
    uint16_t       payloadLen;     // length of ip payload
    uint16_t       payloadOffset;  // offset to ip payload from start
    uint8_t        ipOffset;       // offset to ip header from start
    uint8_t        vpnIpOffset;    // offset to vpn ip header from start
    uint8_t        protocol;       // ip protocol
    uint8_t        readerPos;      // position for filename/ops
    uint8_t        direction:1;    // direction of packet
    uint8_t        ses:3;          // type of session
    uint8_t        v6:1;           // v6 or not
    uint8_t        copied:1;       // don't need to copy
    uint8_t        wasfrag:1;      // was a fragment
    uint8_t        tunnel:4;       // tunnel type
} MolochPacket_t;

typedef struct
{
    struct molochpacket_t   *packet_next, *packet_prev;
    uint32_t                 packet_count;
    MOLOCH_LOCK_EXTERN(lock);
    MOLOCH_COND_EXTERN(lock);
} MolochPacketHead_t;

typedef struct
{
    MolochPacketHead_t    packetQ[MOLOCH_MAX_PACKET_THREADS];
    int                   count;
    uint8_t               readerPos;
} MolochPacketBatch_t;
/******************************************************************************/
typedef struct moloch_tcp_data {
    struct moloch_tcp_data *td_next, *td_prev;

    MolochPacket_t *packet;
    uint32_t        seq;
    uint32_t        ack;
    uint16_t        len;
    uint16_t        dataOffset;
} MolochTcpData_t;

typedef struct {
    struct moloch_tcp_data *td_next, *td_prev;
    int td_count;
} MolochTcpDataHead_t;

#define MOLOCH_TCP_STATE_FIN     1
#define MOLOCH_TCP_STATE_FIN_ACK 2

/******************************************************************************/
typedef enum {
    MOLOCH_TCPFLAG_SYN = 0,
    MOLOCH_TCPFLAG_SYN_ACK,
    MOLOCH_TCPFLAG_ACK,
    MOLOCH_TCPFLAG_PSH,
    MOLOCH_TCPFLAG_FIN,
    MOLOCH_TCPFLAG_RST,
    MOLOCH_TCPFLAG_URG,
    MOLOCH_TCPFLAG_SRC_ZERO,
    MOLOCH_TCPFLAG_DST_ZERO,
    MOLOCH_TCPFLAG_MAX
} MolochSesTcpFlags;
/******************************************************************************/
/*
 * SPI Data Storage
 */
typedef struct moloch_session {
    struct moloch_session *tcp_next, *tcp_prev;
    struct moloch_session *q_next, *q_prev;
    struct moloch_session *h_next, *h_prev;
    int                    h_bucket;
    uint32_t               h_hash;

    char                   sessionId[MOLOCH_SESSIONID_LEN];

    MolochField_t        **fields;

    void                  **pluginData;

    MolochParserInfo_t    *parserInfo;

    MolochTcpDataHead_t   tcpData;
    uint32_t              tcpSeq[2];
    char                  tcpState[2];

    GArray                *filePosArray;
    GArray                *fileLenArray;
    GArray                *fileNumArray;
    char                  *rootId;

    struct timeval         firstPacket;
    struct timeval         lastPacket;
    struct in6_addr        addr1;
    struct in6_addr        addr2;
    char                   firstBytes[2][8];

    uint64_t               bytes[2];
    uint64_t               databytes[2];
    uint64_t               totalDatabytes[2];

    uint32_t               lastFileNum;
    uint32_t               saveTime;
    uint32_t               packets[2];

    uint16_t               port1;
    uint16_t               port2;
    uint16_t               outstandingQueries;
    uint16_t               segments;
    uint16_t               stopSaving;
    uint16_t               tcpFlagCnt[MOLOCH_TCPFLAG_MAX];

    uint8_t                consumed[2];
    uint8_t                protocol;
    uint8_t                firstBytesLen[2];
    uint8_t                ip_tos;
    uint8_t                tcp_flags;
    uint8_t                parserLen;
    uint8_t                parserNum;
    uint8_t                minSaving;
    uint8_t                maxFields;
    uint8_t                thread;

    uint16_t               haveTcpSession:1;
    uint16_t               needSave:1;
    uint16_t               stopSPI:1;
    uint16_t               closingQ:1;
    uint16_t               stopTCP:1;
    uint16_t               ses:3;
    uint16_t               midSave:1;
    uint16_t               outOfOrder:2;
    uint16_t               ackedUnseenSegment:2;
} MolochSession_t;

typedef struct moloch_session_head {
    struct moloch_session *tcp_next, *tcp_prev;
    struct moloch_session *q_next, *q_prev;
    struct moloch_session *h_next, *h_prev;
    int                    h_bucket;
    int                    tcp_count;
    int                    q_count;
    int                    h_count;
} MolochSessionHead_t;


//#define MOLOCH_USE_MALLOC

#ifdef MOLOCH_USE_MALLOC
#define MOLOCH_TYPE_ALLOC(type) (type *)(malloc(sizeof(type)))
#define MOLOCH_TYPE_ALLOC0(type) (type *)(calloc(1, sizeof(type)))
#define MOLOCH_TYPE_FREE(type,mem) free(mem)

#define MOLOCH_SIZE_ALLOC(name, s)  malloc(s)
#define MOLOCH_SIZE_ALLOC0(name, s) calloc(s, 1)
#define MOLOCH_SIZE_FREE(name, mem) free(mem)
#else
#define MOLOCH_TYPE_ALLOC(type) (type *)(g_slice_alloc(sizeof(type)))
#define MOLOCH_TYPE_ALLOC0(type) (type *)(g_slice_alloc0(sizeof(type)))
#define MOLOCH_TYPE_FREE(type,mem) g_slice_free1(sizeof(type),mem)

void *moloch_size_alloc(int size, int zero);
int   moloch_size_free(void *mem);
#define MOLOCH_SIZE_ALLOC(name, s)  moloch_size_alloc(s, 0)
#define MOLOCH_SIZE_ALLOC0(name, s) moloch_size_alloc(s, 1)
#define MOLOCH_SIZE_FREE(name, mem) moloch_size_free(mem)
#endif

// pcap_file_header
typedef struct {
	uint32_t magic;
	uint16_t version_major;
	uint16_t version_minor;
	int32_t  thiszone;	/* gmt to local correction */
	uint32_t sigfigs;	/* accuracy of timestamps */
	uint32_t snaplen;	/* max length saved portion of each pkt */
	uint32_t linktype;	/* data link type (LINKTYPE_*) */
} MolochPcapFileHdr_t;

#ifndef likely
#define likely(x)       __builtin_expect((x),1)
#endif

#ifndef unlikely
#define unlikely(x)     __builtin_expect((x),0)
#endif

/******************************************************************************/
/*
 * Callback function definitions
 */
typedef int (*MolochWatchFd_func)(gint fd, GIOCondition cond, gpointer data);

typedef void (*MolochHttpResponse_cb)(int code, unsigned char *data, int len, gpointer uw);

typedef void (*MolochTag_cb)(void *uw, int tagType, const char *tagName, uint32_t tagValue, gboolean async);

typedef void (*MolochSeqNum_cb)(uint32_t seq, gpointer uw);

/******************************************************************************/
extern MOLOCH_LOCK_EXTERN(LOG);
#define LOG(...) do { \
    if(config.quiet == FALSE) { \
        MOLOCH_LOCK(LOG); \
        time_t _t = time(NULL); \
        char   _b[26]; \
        printf("%15.15s %s:%d %s(): ",\
            ctime_r(&_t, _b)+4, __FILE__,\
            __LINE__, __FUNCTION__); \
        printf(__VA_ARGS__); \
        printf("\n"); \
        fflush(stdout); \
        MOLOCH_UNLOCK(LOG); \
    } \
} while(0) /* no trailing ; */

#define LOGEXIT(...) do { LOG(__VA_ARGS__); exit(1); } while(0) /* no trailing ; */


/******************************************************************************/
/* Simple bit macros, assuming uint64_t */

#define BIT_ISSET(bit, bits) ((bits[bit/64] & (1 << (bit & 0x3f))) != 0)
#define BIT_SET(bit, bits) bits[bit/64] |= (1 << (bit & 0x3f))
#define BIT_CLR(bit, bits) bits[bit/64] &= ~(1 << (bit & 0x3f))

/******************************************************************************/
/*
 * main.c
 */

// Return 0 if ready to quit
typedef int  (* MolochCanQuitFunc) ();

#define MOLOCH_GIO_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL)
#define MOLOCH_GIO_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

gint moloch_watch_fd(gint fd, GIOCondition cond, MolochWatchFd_func func, gpointer data);
unsigned char *moloch_js0n_get(unsigned char *data, uint32_t len, char *key, uint32_t *olen);
char *moloch_js0n_get_str(unsigned char *data, uint32_t len, char *key);

gboolean moloch_string_add(void *hash, char *string, gpointer uw, gboolean copy);

uint32_t moloch_string_hash(const void *key);
uint32_t moloch_string_hash_len(const void *key, int len);
int moloch_string_cmp(const void *keyv, const void *elementv);
int moloch_string_ncmp(const void *keyv, const void *elementv);


uint32_t moloch_int_hash(const void *key);
int moloch_int_cmp(const void *keyv, const void *elementv);

const char *moloch_memstr(const char *haystack, int haysize, const char *needle, int needlesize);
const char *moloch_memcasestr(const char *haystack, int haysize, const char *needle, int needlesize);

void moloch_add_can_quit(MolochCanQuitFunc func, const char *name);

void moloch_quit();


/******************************************************************************/
/*
 * config.c
 */

void moloch_config_init();
void moloch_config_load_local_ips();
void moloch_config_load_packet_ips();
void moloch_config_add_header(MolochStringHashStd_t *hash, char *key, int pos);
void moloch_config_load_header(char *section, char *group, char *helpBase, char *expBase, char *dbBase, MolochStringHashStd_t *hash, int flags);
void moloch_config_exit();

gchar *moloch_config_section_str(GKeyFile *keyfile, char *section, char *key, char *d);
gchar **moloch_config_section_keys(GKeyFile *keyfile, char *section, gsize *keys_len);

gchar *moloch_config_str(GKeyFile *keyfile, char *key, char *d);
gchar **moloch_config_str_list(GKeyFile *keyfile, char *key, char *d);
gchar **moloch_config_raw_str_list(GKeyFile *keyfile, char *key, char *d);
uint32_t moloch_config_int(GKeyFile *keyfile, char *key, uint32_t d, uint32_t min, uint32_t max);
char moloch_config_boolean(GKeyFile *keyfile, char *key, char d);

typedef void (*MolochFileChange_cb)(char *name);
typedef void (*MolochFilesChange_cb)(char **names);
void moloch_config_monitor_file(char *desc, char *name, MolochFileChange_cb cb);
void moloch_config_monitor_files(char *desc, char **names, MolochFilesChange_cb cb);

/******************************************************************************/
/*
 * db.c
 */

void     moloch_db_init();
char    *moloch_db_create_file(time_t firstPacket, const char *name, uint64_t size, int locked, uint32_t *id);
char    *moloch_db_create_file_full(time_t firstPacket, const char *name, uint64_t size, int locked, uint32_t *id, ...);
void     moloch_db_save_session(MolochSession_t *session, int final);
void     moloch_db_add_local_ip(char *str, MolochIpInfo_t *ii);
void     moloch_db_add_field(char *group, char *kind, char *expression, char *friendlyName, char *dbField, char *help, int haveap, va_list ap);
void     moloch_db_update_field(char *expression, char *name, char *value);
void     moloch_db_update_filesize(uint32_t fileid, uint64_t size);
gboolean moloch_db_file_exists(char *filename);
void     moloch_db_exit();
void     moloch_db_oui_lookup(int field, MolochSession_t *session, const uint8_t *mac);


// Replace how SPI data is sent to ES.
// The implementation must either call a moloch_http_free_buffer or another moloch_http routine that frees the buffer
typedef void (* MolochDbSendBulkFunc) (char *json, int len);
void     moloch_db_set_send_bulk(MolochDbSendBulkFunc func);

/******************************************************************************/
/*
 * drophash.c
 */

typedef struct molochdrophashitem_t  MolochDropHashItem_t;
typedef struct molochdrophash_t      MolochDropHash_t;
typedef struct molochdrophashgroup_t MolochDropHashGroup_t;
struct molochdrophashgroup_t {
    MolochDropHashItem_t *dhg_next, *dhg_prev;
    int                   dhg_count;
    int                   changed;
    char                 *file;
    char                  isIp4;
    MolochDropHash_t     *drops[0x10000];
    MOLOCH_LOCK_EXTERN(lock);
};


void moloch_drophash_init(MolochDropHashGroup_t *group, char *file, int isIp4);
int moloch_drophash_add (MolochDropHashGroup_t *group, int port, const void *key, uint32_t current, uint32_t seconds);
int moloch_drophash_should_drop (MolochDropHashGroup_t *group, int port, void *key, uint32_t current);
void moloch_drophash_delete (MolochDropHashGroup_t *group, int port, void *key);
void moloch_drophash_save(MolochDropHashGroup_t *group);

/******************************************************************************/
/*
 * parsers.c
 */
typedef struct {
    uint32_t pc, tag, len;
    const unsigned char *value;
} MolochASNSeq_t;

void moloch_parsers_init();
void moloch_parsers_initial_tag(MolochSession_t *session);
unsigned char *moloch_parsers_asn_get_tlv(BSB *bsb, uint32_t *apc, uint32_t *atag, uint32_t *alen);
int moloch_parsers_asn_get_sequence(MolochASNSeq_t *seqs, int maxSeq, const unsigned char *data, int len, gboolean wrapper);
const char *moloch_parsers_asn_sequence_to_string(MolochASNSeq_t *seq, int *len);
void moloch_parsers_asn_decode_oid(char *buf, int bufsz, unsigned char *oid, int len);
void moloch_parsers_classify_tcp(MolochSession_t *session, const unsigned char *data, int remaining, int which);
void moloch_parsers_classify_udp(MolochSession_t *session, const unsigned char *data, int remaining, int which);
void moloch_parsers_exit();

const char *moloch_parsers_magic(MolochSession_t *session, int field, const char *data, int len);

typedef void (* MolochClassifyFunc) (MolochSession_t *session, const unsigned char *data, int remaining, int which, void *uw);

void  moloch_parsers_unregister(MolochSession_t *session, void *uw);
void  moloch_parsers_register2(MolochSession_t *session, MolochParserFunc func, void *uw, MolochParserFreeFunc ffunc, MolochParserSaveFunc sfunc);
#define moloch_parsers_register(session, func, uw, ffunc) moloch_parsers_register2(session, func, uw, ffunc, NULL)

void  moloch_parsers_classifier_register_tcp_internal(const char *name, void *uw, int offset, const unsigned char *match, int matchlen, MolochClassifyFunc func, size_t sessionsize, int apiversion);
#define moloch_parsers_classifier_register_tcp(name, uw, offset, match, matchlen, func) moloch_parsers_classifier_register_tcp_internal(name, uw, offset, match, matchlen, func, sizeof(MolochSession_t), MOLOCH_API_VERSION)

void  moloch_parsers_classifier_register_udp_internal(const char *name, void *uw, int offset, const unsigned char *match, int matchlen, MolochClassifyFunc func, size_t sessionsize, int apiversion);
#define moloch_parsers_classifier_register_udp(name, uw, offset, match, matchlen, func) moloch_parsers_classifier_register_udp_internal(name, uw, offset, match, matchlen, func, sizeof(MolochSession_t), MOLOCH_API_VERSION)

#define  MOLOCH_PARSERS_PORT_UDP_SRC 0x01
#define  MOLOCH_PARSERS_PORT_UDP_DST 0x02
#define  MOLOCH_PARSERS_PORT_UDP     MOLOCH_PARSERS_PORT_UDP_SRC | MOLOCH_PARSERS_PORT_UDP_DST
#define  MOLOCH_PARSERS_PORT_TCP_SRC 0x04
#define  MOLOCH_PARSERS_PORT_TCP_DST 0x08
#define  MOLOCH_PARSERS_PORT_TCP     MOLOCH_PARSERS_PORT_TCP_SRC | MOLOCH_PARSERS_PORT_TCP_DST

void  moloch_parsers_classifier_register_port_internal(const char *name, void *uw, uint16_t port, uint32_t type, MolochClassifyFunc func, size_t sessionsize, int apiversion);
#define moloch_parsers_classifier_register_port(name, uw, port, type, func) moloch_parsers_classifier_register_port_internal(name, uw, port, type, func, sizeof(MolochSession_t), MOLOCH_API_VERSION)

void  moloch_print_hex_string(const unsigned char* data, unsigned int length);
char *moloch_sprint_hex_string(char *buf, const unsigned char* data, unsigned int length);

/******************************************************************************/
/*
 * http.c
 */

typedef void (*MolochHttpHeader_cb)(char *url, const char *field, const char *value, int valueLen, gpointer uw);


#define MOLOCH_HTTP_BUFFER_SIZE 10000

void moloch_http_init();

unsigned char *moloch_http_send_sync(void *serverV, const char *method, const char *key, uint32_t key_len, char *data, uint32_t data_len, char **headers, size_t *return_len);
gboolean moloch_http_send(void *serverV, const char *method, const char *key, uint32_t key_len, char *data, uint32_t data_len, char **headers, gboolean dropable, MolochHttpResponse_cb func, gpointer uw);


gboolean moloch_http_set(void *server, char *key, int key_len, char *data, uint32_t data_len, MolochHttpResponse_cb func, gpointer uw);
unsigned char *moloch_http_get(void *server, char *key, int key_len, size_t *mlen);
#define moloch_http_get_buffer(size) MOLOCH_SIZE_ALLOC(buffer, size)
#define moloch_http_free_buffer(b) MOLOCH_SIZE_FREE(buffer, b)
void moloch_http_exit();
int moloch_http_queue_length(void *server);
uint64_t moloch_http_dropped_count(void *server);

void *moloch_http_create_server(const char *hostnames, int maxConns, int maxOutstandingRequests, int compress);
void moloch_http_set_retries(void *server, uint16_t retries);
void moloch_http_set_print_errors(void *server);
void moloch_http_set_headers(void *server, char **headers);
void moloch_http_set_header_cb(void *server, MolochHttpHeader_cb cb);
void moloch_http_free_server(void *server);

gboolean moloch_http_is_moloch(uint32_t hash, char *key);

/******************************************************************************/
/*
 * session.c
 */


void     moloch_session_id (char *buf, uint32_t addr1, uint16_t port1, uint32_t addr2, uint16_t port2);
void     moloch_session_id6 (char *buf, uint8_t *addr1, uint16_t port1, uint8_t *addr2, uint16_t port2);
char    *moloch_session_id_string (char *sessionId, char *buf);

uint32_t moloch_session_hash(const void *key);
int      moloch_session_cmp(const void *keyv, const void *elementv);

MolochSession_t *moloch_session_find(int ses, char *sessionId);
MolochSession_t *moloch_session_find_or_create(int ses, uint32_t hash, char *sessionId, int *isNew);

void     moloch_session_init();
void     moloch_session_exit();
void     moloch_session_add_protocol(MolochSession_t *session, const char *protocol);
gboolean moloch_session_has_protocol(MolochSession_t *session, const char *protocol);
void     moloch_session_add_tag(MolochSession_t *session, const char *tag);

#define  moloch_session_incr_outstanding(session) (session)->outstandingQueries++
gboolean moloch_session_decr_outstanding(MolochSession_t *session);

void     moloch_session_mark_for_close (MolochSession_t *session, int ses);

void     moloch_session_mid_save(MolochSession_t *session, uint32_t tv_sec);

int      moloch_session_watch_count(int ses);
int      moloch_session_idle_seconds(int ses);
int      moloch_session_close_outstanding();

void     moloch_session_flush();
void     moloch_session_flush_internal(int thread);
uint32_t moloch_session_monitoring();
void     moloch_session_process_commands(int thread);

int      moloch_session_need_save_outstanding();
int      moloch_session_thread_outstanding(int thread);
int      moloch_session_cmd_outstanding();

typedef enum {
    MOLOCH_SES_CMD_FUNC
} MolochSesCmd;
typedef void (*MolochCmd_func)(MolochSession_t *session, gpointer uw1, gpointer uw2);

void moloch_session_add_cmd(MolochSession_t *session, MolochSesCmd cmd, gpointer uw1, gpointer uw2, MolochCmd_func func);
void moloch_session_add_cmd_thread(int thread, gpointer uw1, gpointer uw2, MolochCmd_func func);

/******************************************************************************/
/*
 * packet.c
 */

void     moloch_packet_init();
uint64_t moloch_packet_dropped_packets();
void     moloch_packet_exit();
void     moloch_packet_tcp_free(MolochSession_t *session);
int      moloch_packet_outstanding();
int      moloch_packet_frags_outstanding();
int      moloch_packet_frags_size();
uint64_t moloch_packet_dropped_frags();
uint64_t moloch_packet_dropped_overload();
uint64_t moloch_packet_total_bytes();
void     moloch_packet_thread_wake(int thread);
void     moloch_packet_flush();
void     moloch_packet_process_data(MolochSession_t *session, const uint8_t *data, int len, int which);
void     moloch_packet_add_packet_ip(char *ip, int mode);

void     moloch_packet_batch_init(MolochPacketBatch_t *batch);
void     moloch_packet_batch_flush(MolochPacketBatch_t *batch);
void     moloch_packet_batch(MolochPacketBatch_t * batch, MolochPacket_t * const packet);

void     moloch_packet_set_linksnap(int linktype, int snaplen);
void     moloch_packet_drophash_add(MolochSession_t *session, int which, int min);


/******************************************************************************/
/*
 * plugins.c
 */
typedef void (* MolochPluginInitFunc) ();
typedef void (* MolochPluginIpFunc) (MolochSession_t *session, struct ip *packet, int len);
typedef void (* MolochPluginUdpFunc) (MolochSession_t *session, struct udphdr *udphdr, unsigned char *data, int len);
typedef void (* MolochPluginTcpFunc) (MolochSession_t *session, unsigned char *data, int len);
typedef void (* MolochPluginSaveFunc) (MolochSession_t *session, int final);
typedef void (* MolochPluginNewFunc) (MolochSession_t *session);
typedef void (* MolochPluginExitFunc) ();
typedef void (* MolochPluginReloadFunc) ();

typedef void (* MolochPluginHttpDataFunc) (MolochSession_t *session, http_parser *hp, const char *at, size_t length);
typedef void (* MolochPluginHttpFunc) (MolochSession_t *session, http_parser *hp);

typedef void (* MolochPluginSMTPHeaderFunc) (MolochSession_t *session, const char *field, size_t field_len, const char *value, size_t value_len);
typedef void (* MolochPluginSMTPFunc) (MolochSession_t *session);
typedef uint32_t (* MolochPluginOutstandingFunc) ();

#define MOLOCH_PLUGIN_SAVE         0x00000001
#define MOLOCH_PLUGIN_IP           0x00000002
#define MOLOCH_PLUGIN_UDP          0x00000004
#define MOLOCH_PLUGIN_TCP          0x00000008
#define MOLOCH_PLUGIN_EXIT         0x00000010
#define MOLOCH_PLUGIN_NEW          0x00000020
#define MOLOCH_PLUGIN_RELOAD       0x00000040
#define MOLOCH_PLUGIN_PRE_SAVE     0x00000100

#define MOLOCH_PLUGIN_HP_OMB       0x00001000
#define MOLOCH_PLUGIN_HP_OU        0x00002000
#define MOLOCH_PLUGIN_HP_OHF       0x00004000
#define MOLOCH_PLUGIN_HP_OHV       0x00008000
#define MOLOCH_PLUGIN_HP_OHC       0x00010000
#define MOLOCH_PLUGIN_HP_OB        0x00020000
#define MOLOCH_PLUGIN_HP_OMC       0x00040000

#define MOLOCH_PLUGIN_SMTP_OH      0x00100000
#define MOLOCH_PLUGIN_SMTP_OHC     0x00200000

void moloch_plugins_init();
void moloch_plugins_load(char **plugins);
void moloch_plugins_reload();

int  moloch_plugins_register_internal(const char *name, gboolean storeData, size_t sessionsize, int apiversion);
#define moloch_plugins_register(name, storeData) moloch_plugins_register_internal(name, storeData, sizeof(MolochSession_t), MOLOCH_API_VERSION)

void moloch_plugins_set_cb(const char *            name,
                           MolochPluginIpFunc      ipFunc,
                           MolochPluginUdpFunc     udpFunc,
                           MolochPluginTcpFunc     tcpFunc,
                           MolochPluginSaveFunc    preSaveFunc,
                           MolochPluginSaveFunc    saveFunc,
                           MolochPluginNewFunc     newFunc,
                           MolochPluginExitFunc    exitFunc,
                           MolochPluginExitFunc    reloadFunc);

void moloch_plugins_set_http_cb(const char *             name,
                                MolochPluginHttpFunc     on_message_begin,
                                MolochPluginHttpDataFunc on_url,
                                MolochPluginHttpDataFunc on_header_field,
                                MolochPluginHttpDataFunc on_header_value,
                                MolochPluginHttpFunc     on_headers_complete,
                                MolochPluginHttpDataFunc on_body,
                                MolochPluginHttpFunc     on_message_complete);

void moloch_plugins_set_smtp_cb(const char *                name,
                                MolochPluginSMTPHeaderFunc  on_header,
                                MolochPluginSMTPFunc        on_header_complete);

void moloch_plugins_set_outstanding_cb(const char *                name,
                                       MolochPluginOutstandingFunc outstandingFunc);

uint32_t moloch_plugins_outstanding();

void moloch_plugins_cb_pre_save(MolochSession_t *session, int final);
void moloch_plugins_cb_save(MolochSession_t *session, int final);
void moloch_plugins_cb_new(MolochSession_t *session);
//void moloch_plugins_cb_ip(MolochSession_t *session, struct ip *packet, int len);
void moloch_plugins_cb_udp(MolochSession_t *session, struct udphdr *udphdr, unsigned char *data, int len);
//void moloch_plugins_cb_tcp(MolochSession_t *session, struct tcp_stream *a_tcp);

void moloch_plugins_cb_hp_omb(MolochSession_t *session, http_parser *parser);
void moloch_plugins_cb_hp_ou(MolochSession_t *session, http_parser *parser, const char *at, size_t length);
void moloch_plugins_cb_hp_ohf(MolochSession_t *session, http_parser *parser, const char *at, size_t length);
void moloch_plugins_cb_hp_ohv(MolochSession_t *session, http_parser *parser, const char *at, size_t length);
void moloch_plugins_cb_hp_ohc(MolochSession_t *session, http_parser *parser);
void moloch_plugins_cb_hp_ob(MolochSession_t *session, http_parser *parser, const char *at, size_t length);
void moloch_plugins_cb_hp_omc(MolochSession_t *session, http_parser *parser);

void moloch_plugins_cb_smtp_oh(MolochSession_t *session, const char *field, size_t field_len, const char *value, size_t value_len);
void moloch_plugins_cb_smtp_ohc(MolochSession_t *session);

void moloch_plugins_exit();

/******************************************************************************/
/*
 * yara.c
 */
void  moloch_yara_init();
void  moloch_yara_execute(MolochSession_t *session, const uint8_t *data, int len, int first);
void  moloch_yara_email_execute(MolochSession_t *session, const uint8_t *data, int len, int first);
void  moloch_yara_exit();
char *moloch_yara_version();

/******************************************************************************/
/*
 * field.c
 */

void moloch_field_init();
void moloch_field_define_json(unsigned char *expression, int expression_len, unsigned char *data, int data_len);
int  moloch_field_define_text(char *text, int *shortcut);
int  moloch_field_define(char *group, char *kind, char *expression, char *friendlyName, char *dbField, char *help, int type, int flags, ...);
int  moloch_field_by_db(const char *dbField);
int  moloch_field_by_exp(const char *exp);
const char *moloch_field_string_add(int pos, MolochSession_t *session, const char *string, int len, gboolean copy);
gboolean moloch_field_string_add_lower(int pos, MolochSession_t *session, const char *string, int len);
const char *moloch_field_string_uw_add(int pos, MolochSession_t *session, const char *string, int len, gpointer uw, gboolean copy);
gboolean moloch_field_int_add(int pos, MolochSession_t *session, int i);
gboolean moloch_field_ip4_add(int pos, MolochSession_t *session, int i);
gboolean moloch_field_ip6_add(int pos, MolochSession_t *session, const uint8_t *val);
gboolean moloch_field_ip_add_str(int pos, MolochSession_t *session, char *str);
gboolean moloch_field_certsinfo_add(int pos, MolochSession_t *session, MolochCertsInfo_t *info, int len);
void moloch_field_macoui_add(MolochSession_t *session, int macField, int ouiField, const uint8_t *mac);

int  moloch_field_count(int pos, MolochSession_t *session);
void moloch_field_certsinfo_free (MolochCertsInfo_t *certs);
void moloch_field_free(MolochSession_t *session);
void moloch_field_exit();

void moloch_field_ops_init(MolochFieldOps_t *ops, int numOps, uint16_t flags);
void moloch_field_ops_free(MolochFieldOps_t *ops);
void moloch_field_ops_add(MolochFieldOps_t *ops, int fieldPos, char *value, int valuelen);
void moloch_field_ops_run(MolochSession_t *session, MolochFieldOps_t *ops);

void *moloch_field_parse_ip(const char *str);
gboolean moloch_field_ip_equal (gconstpointer v1, gconstpointer v2);
guint moloch_field_ip_hash (gconstpointer v);


/******************************************************************************/
/*
 * writers.c
 */

typedef void (*MolochWriterInit)(char *name);
typedef uint32_t (*MolochWriterQueueLength)();
typedef void (*MolochWriterWrite)(const MolochSession_t * const session, MolochPacket_t * const packet);
typedef void (*MolochWriterExit)();

extern MolochWriterQueueLength moloch_writer_queue_length;
extern MolochWriterWrite moloch_writer_write;
extern MolochWriterExit moloch_writer_exit;


void moloch_writers_init();
void moloch_writers_start(char *name);
void moloch_writers_add(char *name, MolochWriterInit func);

/******************************************************************************/
/*
 * readers.c
 */

typedef struct {
    uint64_t total;
    uint64_t dropped;
} MolochReaderStats_t;

typedef void (*MolochReaderInit)(char *name);
typedef int  (*MolochReaderStats)(MolochReaderStats_t *stats);
typedef void (*MolochReaderStart)();
typedef void (*MolochReaderStop)();

extern MolochReaderStart moloch_reader_start;
extern MolochReaderStats moloch_reader_stats;
extern MolochReaderStop moloch_reader_stop;


void moloch_readers_init();
void moloch_readers_set(char *name);
void moloch_readers_start();
void moloch_readers_add(char *name, MolochReaderInit func);
void moloch_readers_exit();

/******************************************************************************/
/*
 * rules.c
 */
typedef enum {
    MOLOCH_RULE_TYPE_EVERY_PACKET,
    MOLOCH_RULE_TYPE_SESSION_SETUP,
    MOLOCH_RULE_TYPE_AFTER_CLASSIFY,
    MOLOCH_RULE_TYPE_FIELD_SET,
    MOLOCH_RULE_TYPE_BEFORE_SAVE,
    MOLOCH_RULE_TYPE_NUM

} MolochRuleType;

void moloch_rules_init();
void moloch_rules_recompile();
void moloch_rules_run_field_set(MolochSession_t *session, int pos, const gpointer value);
int moloch_rules_run_every_packet(MolochPacket_t *packet);
void moloch_rules_session_create(MolochSession_t *session);
void moloch_rules_run_session_setup(MolochSession_t *session, MolochPacket_t *packet);
void moloch_rules_run_after_classify(MolochSession_t *session);
void moloch_rules_run_before_save(MolochSession_t *session, int final);
void moloch_rules_exit();

/******************************************************************************/
/*
 * trie.c
 */
void moloch_trie_init(MolochTrie_t *trie);
MolochTrieNode_t *moloch_trie_add_node(MolochTrieNode_t *node, const char key);
void moloch_trie_add_forward(MolochTrie_t *trie, const char *key, const int len, void *data);
void moloch_trie_add_reverse(MolochTrie_t *trie, const char *key, const int len, void *data);
void *moloch_trie_get_forward(MolochTrie_t *trie, const char *key, const int len);
void *moloch_trie_get_reverse(MolochTrie_t *trie, const char *key, const int len);
void *moloch_trie_best_forward(MolochTrie_t *trie, const char *key, const int len);
void *moloch_trie_best_reverse(MolochTrie_t *trie, const char *key, const int len);
void *moloch_trie_del_forward(MolochTrie_t *trie, const char *key, const int len);
void *moloch_trie_del_reverse(MolochTrie_t *trie, const char *key, const int len);


/******************************************************************************/
/*
 * js0n.c
 */
int js0n(unsigned char *js, unsigned int len, unsigned int *out);

