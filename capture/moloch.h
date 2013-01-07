/******************************************************************************/
/* moloch.h -- General Moloch include file
 *
 */

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include "http_parser.h"
#include "dll.h"
#include "hash.h"
#include "nids.h"
#include "glib.h"

#define UNUSED(x) x __attribute((unused))

typedef struct moloch_string {
    struct moloch_string *s_next, *s_prev;
    char                 *str;
    short                 s_bucket;
} MolochString_t;

typedef struct {
    struct moloch_string *s_next, *s_prev;
    int s_count;
} MolochStringHead_t;

typedef struct moloch_config {
    char     *configFile;
    char     *nodeName;
    char     *hostName;
    char     *pcapReadFile;
    char     *pcapReadDir;
    gboolean  debug;
    gboolean  dryRun;
    gboolean  fakePcap;

    HASH_VAR(s_, dontSaveTags, MolochStringHead_t, 11);

    char     *nodeClass;
    char     *elasticsearch;
    char     *interface;
    char     *pcapDir;
    char     *bpf;
    char     *yara;
    char     *geoipFile;
    char     *geoipASNFile;
    char     *dropUser;
    char     *dropGroup;
    char     *pluginsDir;

    char     **plugins;

    uint32_t  maxFileSizeG;
    uint32_t  minFreeSpaceG;
    uint32_t  icmpTimeout;
    uint32_t  udpTimeout;
    uint32_t  tcpTimeout;
    uint32_t  tcpSaveTimeout;
    uint32_t  maxStreams;
    uint32_t  maxPackets;
    uint32_t  dbBulkSize;
    uint32_t  maxESConns;
    uint32_t  maxESRequests;
    uint32_t  logEveryXPackets;
    uint32_t  packetsPerPoll;
    uint32_t  pcapBufferSize;
    uint32_t  pcapWriteSize;

    char      logUnknownProtocols;
    char      logESRequests;
    char      logFileCreation;
} MolochConfig_t;

typedef struct moloch_int {
    struct moloch_int    *i_next, *i_prev;
    int                   i;
    short                 i_bucket;
} MolochInt_t;

typedef struct {
    struct moloch_int *i_next, *i_prev;
    int i_count;
} MolochIntHead_t;

typedef struct {
    MolochStringHead_t  commonName; //2.5.4.3
    char               *orgName; // 2.5.4.10
} MolochCertInfo_t;

typedef struct moloch_tlsinfo{
    struct moloch_tlsinfo *t_next, *t_prev;
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


#define MOLOCH_MAX_PLUGINS       10

#define MOLOCH_TAG_TAGS          0
#define MOLOCH_TAG_HTTP_REQUEST  1
#define MOLOCH_TAG_HTTP_RESPONSE 2
#define MOLOCH_TAG_MAX           3

typedef struct moloch_session_http {
    GPtrArray  *urlArray;
    GString    *urlString;
    GString    *hostString;
    GString    *uaString;
    GString    *xffString;

    HASH_VAR(s_, userAgents, MolochStringHead_t, 11);
    HASH_VAR(i_, xffs, MolochIntHead_t, 11);

    char        header[2][40];
    http_parser parsers[2];

    uint16_t    wParsers:1;
    uint16_t    inHeader:2;
    uint16_t    inValue:2;
    uint16_t    inBody:2;
} MolochSessionHttp_t;

typedef struct moloch_session {
    struct moloch_session *tcp_next, *tcp_prev;
    struct moloch_session *q_next, *q_prev;
    struct moloch_session *h_next, *h_prev;
    int                    h_bucket;


    HASH_VAR(s_, hosts, MolochStringHead_t, 11);
    HASH_VAR(i_, dnsips, MolochIntHead_t, 11);
    HASH_VAR(t_, certs, MolochCertsInfoHead_t, 5);
    HASH_VAR(s_, users, MolochStringHead_t, 1);
    HASH_VAR(s_, sshver, MolochStringHead_t, 1);
    HASH_VAR(s_, sshkey, MolochStringHead_t, 1);


    MolochSessionHttp_t   *http;

    void       *pluginData[MOLOCH_MAX_PLUGINS];


    GArray     *filePosArray;
    GArray     *fileNumArray;
    char       *rootId;
    GHashTable *tags[MOLOCH_TAG_MAX];

    uint64_t    bytes;
    uint64_t    databytes;


    uint32_t    firstPacket;
    uint32_t    lastPacket;
    uint32_t    lastSave;
    uint32_t    addr1;
    uint32_t    addr2;

    uint16_t    port1;
    uint16_t    port2;
    uint16_t    protocol;
    uint16_t    offsets[2];
    uint16_t    outstandingQueries;
    uint16_t    sshLen;

    uint8_t     sshCode;

    uint16_t    haveNidsTcp:1;
    uint16_t    needSave:1;
    uint16_t    dontSave:1;
    uint16_t    which:1;
    uint16_t    isSsh:1;
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


typedef int (*MolochWatchFd_func)(gint fd, GIOCondition cond, gpointer data);

typedef void (*MolochResponse_cb)(unsigned char *data, int len, gpointer uw);

typedef void (*MolochTag_cb)(MolochSession_t *session, int tagtype, uint32_t tag);

typedef void (*MolochSeqNum_cb)(uint32_t seq, gpointer uw);

/******************************************************************************/
#define LOG(...) do { \
    time_t t = time(NULL); \
    printf("%15.15s %s:%d %s(): ",\
        ctime(&t)+4, __FILE__,\
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

uint32_t moloch_string_hash(const void *key);
int moloch_string_cmp(const void *keyv, const void *elementv);

uint32_t moloch_int_hash(const void *key);
int moloch_int_cmp(const void *keyv, const void *elementv);

void moloch_quit();


/******************************************************************************/
/*
 * config.c
 */
void moloch_config_init();
void moloch_config_exit();

gchar *moloch_config_str(GKeyFile *keyfile, char *key, char *d);
gchar **moloch_config_str_list(GKeyFile *keyfile, char *key, char *d);
uint32_t moloch_config_int(GKeyFile *keyfile, char *key, uint32_t d, uint32_t min, uint32_t max);
char moloch_config_boolean(GKeyFile *keyfile, char *key, char d);



/******************************************************************************/
/*
 * db.c
 */

void  moloch_db_init();
int   moloch_db_tags_loading();
char *moloch_db_create_file(time_t firstPacket, uint32_t *id);
void  moloch_db_save_session(MolochSession_t *session, int final);
void  moloch_db_get_tag(MolochSession_t *session, int tagtype, const char *tag, MolochTag_cb func);
void  moloch_db_exit();

/******************************************************************************/
/*
 * detect.c
 */
void moloch_detect_init();
void moloch_detect_initial_tag(MolochSession_t *session);
void moloch_detect_parse_classify(MolochSession_t *session, struct tcp_stream *UNUSED(a_tcp), struct half_stream *hlf);
void moloch_detect_parse_http(MolochSession_t *session, struct tcp_stream *UNUSED(a_tcp), struct half_stream *hlf);
void moloch_detect_parse_ssh(MolochSession_t *session, struct tcp_stream *UNUSED(a_tcp), struct half_stream *hlf);
void moloch_detect_parse_yara(MolochSession_t *session, struct tcp_stream *UNUSED(a_tcp), struct half_stream *hlf);
void moloch_detect_dns(MolochSession_t *session, unsigned char *data, int len);
void moloch_detect_exit();

/******************************************************************************/
/*
 * http.c
 */

#define MOLOCH_HTTP_BUFFER_SIZE_S 9999
#define MOLOCH_HTTP_BUFFER_SIZE_M 99999
#define MOLOCH_HTTP_BUFFER_SIZE_L 3000000

void moloch_http_init();

unsigned char *moloch_http_send_sync(void *serverV, char *method, char *key, uint32_t key_len, char *data, size_t data_len, size_t *return_len);
gboolean moloch_http_send(void *serverV, char *method, char *key, uint32_t key_len, char *data, size_t data_len, gboolean dropable, MolochResponse_cb func, gpointer uw);


gboolean moloch_http_set(void *server, char *key, int key_len, char *data, size_t data_len, MolochResponse_cb func, gpointer uw);
unsigned char *moloch_http_get(void *server, char *key, int key_len, size_t *mlen);
char *moloch_http_get_buffer(int size);
void moloch_http_exit();
int moloch_http_queue_length(void *server);

void *moloch_http_create_server(char *hostname, int defaultPort, int maxConns, int maxOutstandingRequests);
void moloch_http_free_server(void *serverV);

/******************************************************************************/
/*
 * nids.c
 */

void  moloch_nids_root_init();
void  moloch_nids_init();
void  moloch_nids_add_tag(MolochSession_t *session, int tagtype, const char *tag);
void  moloch_nids_certs_free (MolochCertsInfo_t *certs);
uint32_t moloch_nids_dropped_packets();
uint32_t moloch_nids_monitoring_sessions();
void  moloch_nids_exit();

void moloch_nids_incr_outstanding(MolochSession_t *session);
void moloch_nids_decr_outstanding(MolochSession_t *session);

void moloch_nids_new_session_http(MolochSession_t *session);
void moloch_nids_free_session_http(MolochSession_t *session);

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

#define MOLOCH_PLUGIN_SAVE    0x00000001
#define MOLOCH_PLUGIN_IP      0x00000002
#define MOLOCH_PLUGIN_UDP     0x00000004
#define MOLOCH_PLUGIN_TCP     0x00000008
#define MOLOCH_PLUGIN_EXIT    0x00000010
#define MOLOCH_PLUGIN_NEW     0x00000020
#define MOLOCH_PLUGIN_RELOAD  0x00000040

#define MOLOCH_PLUGIN_HP_OMB  0x00001000
#define MOLOCH_PLUGIN_HP_OU   0x00002000
#define MOLOCH_PLUGIN_HP_OHF  0x00004000
#define MOLOCH_PLUGIN_HP_OHV  0x00008000
#define MOLOCH_PLUGIN_HP_OHC  0x00010000
#define MOLOCH_PLUGIN_HP_OB   0x00020000
#define MOLOCH_PLUGIN_HP_OMC  0x00040000

void moloch_plugins_init();
void moloch_plugins_reload();
int  moloch_plugins_register(const char *name, gboolean storeData);
void moloch_plugins_set_cb(const char *            name,
                           MolochPluginIpFunc      ipFunc,
                           MolochPluginUdpFunc     udpFunc,
                           MolochPluginTcpFunc     tcpFunc,
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

void moloch_plugins_exit();

/******************************************************************************/
/*
 * yara.c
 */
void  moloch_yara_init();
void  moloch_yara_execute(MolochSession_t *session, char *data, int len, int first);
void  moloch_yara_exit();

/******************************************************************************/
/*
 * viewer.c
 */

void  moloch_viewer_init();
void  moloch_viewer_exit();

/******************************************************************************/
/*
 * js0n.c
 */
int js0n(unsigned char *js, unsigned int len, unsigned int *out);
