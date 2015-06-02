/******************************************************************************/
/* moloch.h -- General Moloch include file
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

#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include "http_parser.h"
#include "dll.h"
#include "hash.h"
#include "bsb.h"
#include "nids.h"
#include "glib.h"

#define UNUSED(x) x __attribute((unused))


#define MOLOCH_API_VERSION 13

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
    char                      value, first, last;
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
    MolochStringHead_t  commonName; //2.5.4.3
    char               *orgName; // 2.5.4.10
    char                orgUtf8;
} MolochCertInfo_t;

typedef struct moloch_tlsinfo{
    struct moloch_tlsinfo *t_next, *t_prev;
    uint32_t               t_hash;
    uint64_t               notBefore;
    uint64_t               notAfter;
    MolochCertInfo_t       issuer;
    MolochCertInfo_t       subject;
    MolochStringHead_t     alt;
    unsigned char         *serialNumber;
    short                  serialNumberLen;
    short                  t_bucket;
} MolochCertsInfo_t;

typedef struct {
    struct moloch_tlsinfo *t_next, *t_prev;
    int                    t_count;
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
#define MOLOCH_FIELD_TYPE_STR        3
#define MOLOCH_FIELD_TYPE_STR_ARRAY  4
#define MOLOCH_FIELD_TYPE_STR_HASH   5
#define MOLOCH_FIELD_TYPE_IP         6
#define MOLOCH_FIELD_TYPE_IP_HASH    7
#define MOLOCH_FIELD_TYPE_CERTSINFO  8

/* These are ones you should set */
/* Field should be set on all linked sessions */
#define MOLOCH_FIELD_FLAG_LINKED_SESSIONS    0x0001
/* Create a XXX-cnt field with unique count */
#define MOLOCH_FIELD_FLAG_COUNT              0x0002
/* Force the field to be utf8 */
#define MOLOCH_FIELD_FLAG_FORCE_UTF8         0x0004
/* Don't create in fields db table */
#define MOLOCH_FIELD_FLAG_NODB               0x0008
/* Don't create in capture list */ 
#define MOLOCH_FIELD_FLAG_FAKE               0x0010
/* Don't create in capture list */ 
#define MOLOCH_FIELD_FLAG_DISABLED           0x0020

/* These are ones you shouldn't set, for old cruf before we were smarter */
/* XXXcnt - dont use */
#define MOLOCH_FIELD_FLAG_CNT                0x1000
/* XXXscnt - dont use */
#define MOLOCH_FIELD_FLAG_SCNT               0x2000
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
    };
    uint32_t                   jsonSize;
} MolochField_t;

/******************************************************************************/
/*
 * Configuration Information
 */
enum MolochRotate { MOLOCH_ROTATE_HOURLY, MOLOCH_ROTATE_DAILY, MOLOCH_ROTATE_WEEKLY, MOLOCH_ROTATE_MONTHLY };

typedef struct moloch_config {
    gboolean  exiting;
    char     *configFile;
    char     *nodeName;
    char     *hostName;
    char    **pcapReadFiles;
    char    **pcapReadDirs;
    gboolean  pcapReadOffline;
    gchar   **extraTags;
    gchar     debug;
    gboolean  dryRun;
    gboolean  noSPI;
    gboolean  copyPcap;
    gboolean  pcapRecursive;
    gboolean  tests;
    gboolean  pcapMonitor;
    gboolean  pcapDelete;
    gboolean  pcapSkip;

    enum MolochRotate rotate;

    int       writeMethod;

    HASH_VAR(s_, dontSaveTags, MolochStringHead_t, 11);
    MolochFieldInfo_t *fields[200];
    int                maxField;

    int                numPlugins;

    GRegex  *offlineRegex;
    char     *prefix;
    char     *nodeClass;
    char     *elasticsearch;
    char     *interface;
    int       pcapDirPos;
    char    **pcapDir;
    char     *bpf;
    char     *yara;
    char     *emailYara;
    char     *geoipFile;
    char     *geoipASNFile;
    char     *rirFile;
    char     *dropUser;
    char     *dropGroup;
    char    **pluginsDir;
    char    **parsersDir;
    char    **dontSaveBPFs;
    int      *dontSaveBPFsStop;
    int       dontSaveBPFsNum;

    char    **plugins;
    char    **smtpIpHeaders;

    double    maxFileSizeG;
    uint64_t  maxFileSizeB;
    uint32_t  maxFileTimeM;
    uint32_t  minFreeSpaceG;
    uint32_t  icmpTimeout;
    uint32_t  udpTimeout;
    uint32_t  tcpTimeout;
    uint32_t  tcpSaveTimeout;
    uint32_t  maxStreams;
    uint32_t  maxPackets;
    uint32_t  dbBulkSize;
    uint32_t  dbFlushTimeout;
    uint32_t  maxESConns;
    uint32_t  maxESRequests;
    uint32_t  logEveryXPackets;
    uint32_t  packetsPerPoll;
    uint32_t  pcapBufferSize;
    uint32_t  pcapWriteSize;
    uint32_t  maxWriteBuffers;
    uint32_t  maxFreeOutputBuffers;


    char      logUnknownProtocols;
    char      logESRequests;
    char      logFileCreation;
    char      parseSMTP;
    char      parseSMB;
    char      parseQSValue;
    char      parseCookieValue;
    char      compressES;
    char      antiSynDrop;
} MolochConfig_t;

typedef struct {
    char     *country;
    char     *asn;
    char     *rir;
    int       numtags;
    int       tags[10];
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
/*
 * SPI Data Storage
 */
#define MOLOCH_SESSIONID_LEN 12
typedef struct moloch_session {
    struct moloch_session *tcp_next, *tcp_prev;
    struct moloch_session *q_next, *q_prev;
    struct moloch_session *h_next, *h_prev;
    int                    h_bucket;
    uint32_t               h_hash;

    uint64_t               sessionIda;
    uint32_t               sessionIdb;
    MolochField_t        **fields;

    void                  **pluginData;

    MolochParserInfo_t    *parserInfo;

    GArray                *filePosArray;
    GArray                *fileLenArray;
    GArray                *fileNumArray;
    char                  *rootId;

    struct timeval         firstPacket;
    struct timeval         lastPacket;
    char                   firstBytes[2][8];

    uint64_t               bytes[2];
    uint64_t               databytes[2];


    uint32_t               lastFileNum;
    uint32_t               lastSave;
    uint32_t               addr1;
    uint32_t               addr2;
    uint32_t               packets[2];

    uint16_t               port1;
    uint16_t               port2;
    uint16_t               offsets[2];
    uint16_t               outstandingQueries;
    uint16_t               segments;
    uint16_t               stopSaving;

    uint8_t                consumed[2];
    uint8_t                protocol;
    uint8_t                firstBytesLen[2];
    uint8_t                ip_tos;
    uint8_t                tcp_flags;
    uint8_t                parserLen;
    uint8_t                parserNum;

    uint8_t                maxFields;
    uint16_t               haveNidsTcp:1;
    uint16_t               needSave:1;
    uint16_t               stopSPI:1;
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


#define MOLOCH_TYPE_ALLOC(type) (type *)(g_slice_alloc(sizeof(type)))
#define MOLOCH_TYPE_ALLOC0(type) (type *)(g_slice_alloc0(sizeof(type)))
#define MOLOCH_TYPE_FREE(type,mem) g_slice_free1(sizeof(type),mem)

void *moloch_size_alloc(int size, int zero);
int   moloch_size_free(void *mem);
#define MOLOCH_SIZE_ALLOC(name, s)  moloch_size_alloc(s, 0)
#define MOLOCH_SIZE_ALLOC0(name, s) moloch_size_alloc(s, 1)
#define MOLOCH_SIZE_FREE(name, mem) moloch_size_free(mem)


/******************************************************************************/
/*
 * Callback function definitions
 */
typedef int (*MolochWatchFd_func)(gint fd, GIOCondition cond, gpointer data);

typedef void (*MolochHttpResponse_cb)(int code, unsigned char *data, int len, gpointer uw);

typedef void (*MolochTag_cb)(void *uw, int tagType, const char *tagName, uint32_t tagValue);

typedef void (*MolochSeqNum_cb)(uint32_t seq, gpointer uw);

/******************************************************************************/
#define LOG(...) do { \
    time_t _t = time(NULL); \
    printf("%15.15s %s:%d %s(): ",\
        ctime(&_t)+4, __FILE__,\
        __LINE__, __FUNCTION__); \
    printf(__VA_ARGS__); \
    printf("\n"); \
} while(0) /* no trailing ; */


/******************************************************************************/
/*
 * main.c
 */

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

void moloch_session_id (char *buf, uint32_t addr1, uint16_t port1, uint32_t addr2, uint16_t port2);
uint32_t moloch_session_hash(const void *key);
int moloch_session_cmp(const void *keyv, const void *elementv);

const char *moloch_memstr(const char *haystack, int haysize, const char *needle, int needlesize);
const char *moloch_memcasestr(const char *haystack, int haysize, const char *needle, int needlesize);

void moloch_quit();


/******************************************************************************/
/*
 * config.c
 */
void moloch_config_init();
void moloch_config_load_local_ips();
void moloch_config_add_header(MolochStringHashStd_t *hash, char *key, int pos);
void moloch_config_load_header(char *section, char *group, char *helpBase, char *expBase, char *dbBase, MolochStringHashStd_t *hash, int flags);
void moloch_config_exit();

gchar *moloch_config_str(GKeyFile *keyfile, char *key, char *d);
gchar **moloch_config_str_list(GKeyFile *keyfile, char *key, char *d);
uint32_t moloch_config_int(GKeyFile *keyfile, char *key, uint32_t d, uint32_t min, uint32_t max);
char moloch_config_boolean(GKeyFile *keyfile, char *key, char d);



/******************************************************************************/
/*
 * db.c
 */

void     moloch_db_init();
int      moloch_db_tags_loading();
char    *moloch_db_create_file(time_t firstPacket, char *name, uint64_t size, int locked, uint32_t *id);
void     moloch_db_save_session(MolochSession_t *session, int final);
void     moloch_db_get_tag(void *uw, int tagtype, const char *tag, MolochTag_cb func);
uint32_t moloch_db_peek_tag(const char *tagname);
void     moloch_db_add_local_ip(char *str, MolochIpInfo_t *ii);
void     moloch_db_add_field(char *group, char *kind, char *expression, char *friendlyName, char *dbField, char *help, va_list ap);
void     moloch_db_update_field(char *expression, char *name, char *value);
MolochIpInfo_t *moloch_db_get_local_ip(MolochSession_t *session, uint32_t ip);
gboolean moloch_db_file_exists(char *filename);
void     moloch_db_exit();

/******************************************************************************/
/*
 * parsers.c
 */

void moloch_parsers_init();
void moloch_parsers_initial_tag(MolochSession_t *session);
unsigned char *moloch_parsers_asn_get_tlv(BSB *bsb, int *apc, int *atag, int *alen);
void moloch_parsers_asn_decode_oid(char *buf, int bufsz, unsigned char *oid, int len);
void moloch_parsers_classify_tcp(MolochSession_t *session, const unsigned char *data, int remaining, int which);
void moloch_parsers_classify_udp(MolochSession_t *session, const unsigned char *data, int remaining, int which);
void moloch_parsers_exit();

void moloch_parsers_magic_tag(MolochSession_t *session, int field, const char *base, const char *data, int len);

typedef void (* MolochClassifyFunc) (MolochSession_t *session, const unsigned char *data, int remaining, int which);

void  moloch_parsers_unregister(MolochSession_t *session, void *uw);
void  moloch_parsers_register2(MolochSession_t *session, MolochParserFunc func, void *uw, MolochParserFreeFunc ffunc, MolochParserSaveFunc sfunc);
#define moloch_parsers_register(session, func, uw, ffunc) moloch_parsers_register2(session, func, uw, ffunc, NULL)

void  moloch_parsers_classifier_register_tcp_internal(const char *name, int offset, unsigned char *match, int matchlen, MolochClassifyFunc func, size_t sessionsize, int apiversion);
#define moloch_parsers_classifier_register_tcp(name, offset, match, matchlen, func) moloch_parsers_classifier_register_tcp_internal(name, offset, match, matchlen, func, sizeof(MolochSession_t), MOLOCH_API_VERSION)

void  moloch_parsers_classifier_register_udp_internal(const char *name, int offset, unsigned char *match, int matchlen, MolochClassifyFunc func, size_t sessionsize, int apiversion);
#define moloch_parsers_classifier_register_udp(name, offset, match, matchlen, func) moloch_parsers_classifier_register_udp_internal(name, offset, match, matchlen, func, sizeof(MolochSession_t), MOLOCH_API_VERSION)
void moloch_print_hex_string(unsigned char* data, unsigned int length);

/******************************************************************************/
/*
 * http.c
 */

typedef void (*MolochHttpHeader_cb)(char *url, const char *field, const char *value, int valueLen, gpointer uw);


#define MOLOCH_HTTP_BUFFER_SIZE 10000

void moloch_http_init();

unsigned char *moloch_http_send_sync(void *serverV, char *method, char *key, uint32_t key_len, char *data, uint32_t data_len, char **headers, size_t *return_len);
gboolean moloch_http_send(void *serverV, char *method, char *key, uint32_t key_len, char *data, uint32_t data_len, char **headers, gboolean dropable, MolochHttpResponse_cb func, gpointer uw);


gboolean moloch_http_set(void *server, char *key, int key_len, char *data, uint32_t data_len, MolochHttpResponse_cb func, gpointer uw);
unsigned char *moloch_http_get(void *server, char *key, int key_len, size_t *mlen);
#define moloch_http_get_buffer(size) MOLOCH_SIZE_ALLOC(buffer, size)
#define moloch_http_free_buffer(b) MOLOCH_SIZE_FREE(buffer, b)
void moloch_http_exit();
int moloch_http_queue_length(void *server);

void *moloch_http_create_server(char *hostname, int defaultPort, int maxConns, int maxOutstandingRequests, int compress);
void moloch_http_set_header_cb(void *server, MolochHttpHeader_cb cb);
void moloch_http_free_server(void *server);

gboolean moloch_http_is_moloch(uint32_t hash, char *key);

/******************************************************************************/
/*
 * nids.c
 */

void     moloch_nids_root_init();
void     moloch_nids_init();
void     moloch_nids_add_protocol(MolochSession_t *session, const char *protocol);
gboolean moloch_nids_has_protocol(MolochSession_t *session, const char *protocol);
void     moloch_nids_add_tag(MolochSession_t *session, const char *tag);
void     moloch_nids_add_tag_type(MolochSession_t *session, int field, const char *tag);
gboolean moloch_nids_has_tag(MolochSession_t *session, const char *tag);
uint32_t moloch_nids_dropped_packets();
uint32_t moloch_nids_monitoring_sessions();
uint32_t moloch_nids_disk_queue();
void     moloch_nids_exit();

#define  moloch_nids_incr_outstanding(session) (session)->outstandingQueries++
void     moloch_nids_decr_outstanding(MolochSession_t *session);

char    *moloch_friendly_session_id (int protocol, uint32_t addr1, int port1, uint32_t addr2, int port2);

/******************************************************************************/
/*
 * plugins.c
 */
typedef void (* MolochPluginInitFunc) ();
typedef void (* MolochPluginIpFunc) (MolochSession_t *session, struct ip *packet, int len);
typedef void (* MolochPluginUdpFunc) (MolochSession_t *session, struct udphdr *udphdr, unsigned char *data, int len);
typedef void (* MolochPluginTcpFunc) (MolochSession_t *session, struct tcp_stream *a_tcp);
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
void moloch_plugins_cb_ip(MolochSession_t *session, struct ip *packet, int len);
void moloch_plugins_cb_udp(MolochSession_t *session, struct udphdr *udphdr, unsigned char *data, int len);
void moloch_plugins_cb_tcp(MolochSession_t *session, struct tcp_stream *a_tcp);

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
void moloch_yara_init();
void moloch_yara_execute(MolochSession_t *session, unsigned char *data, int len, int first);
void moloch_yara_email_execute(MolochSession_t *session, unsigned char *data, int len, int first);
void moloch_yara_exit();

/******************************************************************************/
/*
 * field.c
 */

void moloch_field_init();
void moloch_field_define_json(unsigned char *expression, int expression_len, unsigned char *data, int data_len);
int  moloch_field_define_text(char *text, int *shortcut);
int  moloch_field_define(char *group, char *kind, char *expression, char *friendlyName, char *dbField, char *help, int type, int flags, ...);
int  moloch_field_by_db(char *dbField);
int  moloch_field_by_exp(char *exp);
gboolean moloch_field_string_add(int pos, MolochSession_t *session, const char *string, int len, gboolean copy);
gboolean moloch_field_int_add(int pos, MolochSession_t *session, int i);
gboolean moloch_field_certsinfo_add(int pos, MolochSession_t *session, MolochCertsInfo_t *info, int len);
int  moloch_field_count(int pos, MolochSession_t *session);
void moloch_field_certsinfo_free (MolochCertsInfo_t *certs);
void moloch_field_free(MolochSession_t *session);
void moloch_field_exit();

/******************************************************************************/
/*
 * writers.c
 */

typedef void (*MolochWriterInit)(char *name);
typedef uint32_t (*MolochWriterQueueLength)();
typedef void (*MolochWriterWrite)(const struct pcap_pkthdr *h, const u_char *sp, uint32_t *fileNum, uint64_t *filePos);
typedef void (*MolochWriterFlush)(gboolean all);
typedef void (*MolochWriterNextInput)(FILE *file, char *filename);
typedef void (*MolochWriterExit)();
typedef char * (*MolochWriterName)();

extern MolochWriterQueueLength moloch_writer_queue_length;
extern MolochWriterWrite moloch_writer_write;
extern MolochWriterFlush moloch_writer_flush;
extern MolochWriterExit moloch_writer_exit;
extern MolochWriterNextInput moloch_writer_next_input;
extern MolochWriterName moloch_writer_name;


void moloch_writers_init();
void moloch_writers_start(char *name);
void moloch_writers_add(char *name, MolochWriterInit func);

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
