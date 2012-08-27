/******************************************************************************/
/* moloch.h -- General Moloch include file
 *
 */

#include "http_parser.h"
#include "dll.h"
#include "hash.h"

#define UNUSED(x) x __attribute((unused))

typedef struct moloch_config {
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

    uint32_t  maxFileSizeG;
    uint32_t  minFreeSpaceG;
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

typedef struct moloch_string {
    struct moloch_string *s_next, *s_prev;
    char                 *str;
    short                 s_bucket;
} MolochString_t;

typedef struct {
    struct moloch_string *s_next, *s_prev;
    int s_count;
} MolochStringHead_t;

typedef struct moloch_int {
    struct moloch_int    *i_next, *i_prev;
    int                   i;
    short                 i_bucket;
} MolochInt_t;

typedef struct {
    struct moloch_int *i_next, *i_prev;
    int i_count;
} MolochIntHead_t;


#define MOLOCH_TAG_TAGS         0
#define MOLOCH_TAG_HTTP_HEADERS 1
typedef struct moloch_session {
    struct moloch_session *tcp_next, *tcp_prev;
    struct moloch_session *q_next, *q_prev;
    struct moloch_session *h_next, *h_prev;
    int                    h_bucket;

    HASH_VAR(s_, hosts, MolochStringHead_t, 11);
    HASH_VAR(s_, userAgents, MolochStringHead_t, 11);
    HASH_VAR(i_, xffs, MolochIntHead_t, 11);

    char        header[32];
    http_parser parsers[2];


    GArray     *filePosArray;
    GArray     *fileNumArray;
    GPtrArray  *urlArray;
    GString    *urlString;
    char       *rootId;
    GString    *hostString;
    GString    *uaString;
    GString    *xffString;
    GHashTable *tags[2];

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
    uint16_t    outstandingTags;

    uint8_t     wParsers;

    uint8_t     haveNidsTcp:1;
    uint8_t     inValue:1;
    uint8_t     inBody:1;
    uint8_t     needSave:1;
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

typedef gboolean (*MolochWatchFd_func)(gint fd, GIOCondition cond, gpointer data);

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

uint32_t moloch_string_hash(const void *key);
int moloch_string_cmp(const void *keyv, const void *elementv);

uint32_t moloch_int_hash(const void *key);
int moloch_int_cmp(const void *keyv, const void *elementv);


/******************************************************************************/
/*
 * config.c
 */
void moloch_config_init();
void moloch_config_exit();

/******************************************************************************/
/*
 * db.c
 */

void  moloch_db_init();
char *moloch_db_create_file(time_t firstPacket, uint32_t *id);
void  moloch_db_save_session(MolochSession_t *session);
void  moloch_db_get_tag(MolochSession_t *session, int tagtype, const char *tag, MolochTag_cb func);
void  moloch_db_exit();

/******************************************************************************/
/*
 * es.c
 */

#define MOLOCH_ES_BUFFER_SIZE_S 9999
#define MOLOCH_ES_BUFFER_SIZE_M 99999
#define MOLOCH_ES_BUFFER_SIZE_L 3000000

void moloch_es_init();
unsigned char *moloch_es_send(char *method, char *key, uint32_t key_len, char *data, size_t data_len, size_t *return_len, gboolean sync, MolochResponse_cb func, gpointer uw);
void moloch_es_set(char *key, int key_len, char *data, size_t data_len, MolochResponse_cb func, gpointer uw);
unsigned char *moloch_es_get(char *key, int key_len, size_t *mlen);
char *moloch_es_get_buffer(int size);
void moloch_es_exit();
int moloch_es_queue_length();

/******************************************************************************/
/*
 * nids.c
 */

void  moloch_nids_root_init();
void  moloch_nids_init();
void  moloch_nids_add_tag(MolochSession_t *session, int tagtype, const char *tag);
uint32_t moloch_nids_dropped_packets();
uint32_t moloch_nids_monitoring_sessions();
void  moloch_nids_exit();

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
