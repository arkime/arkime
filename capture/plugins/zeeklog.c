/* zeeklog.c  -- Write Zeek style log files (conn.log, dns.log, http.log, ...)
 *
 * Every Arkime session save produces rows in the Zeek logs whose trigger
 * protocol the session carries.  Columns are mapped from Arkime fields and a
 * handful of computed "specials" (@ts, @uid, @conn_state, ...).  The default
 * schemas mirror the Zeek base scripts, and [zeeklog-<path>] config sections
 * add, replace, include or exclude columns like a Zeek Log::Filter.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"
#include "../parsers/dns_info.h"
#include "../parsers/certs_info.h"
#include <sys/stat.h>
#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>

extern ArkimeConfig_t        config;

#define ZEEKLOG_MAX_SOURCES   8
#define ZEEKLOG_MAX_ITEMS     1024
#define ZEEKLOG_ITEM_BUF_SIZE 0x10000
#define ZEEKLOG_ROW_BUF_SIZE  0x20000
#define ZEEKLOG_UID_LEN       18

/******************************************************************************/
typedef enum {
    ZB_STRING, ZB_COUNT, ZB_INT, ZB_DOUBLE, ZB_TIME, ZB_INTERVAL, ZB_BOOL, ZB_ADDR, ZB_PORT, ZB_ENUM
} ZeekBase_t;

LOCAL const char *zeekBaseNames[] = {"string", "count", "int", "double", "time", "interval", "bool", "addr", "port", "enum"};

typedef enum { ZC_SCALAR, ZC_VECTOR, ZC_SET } ZeekContainer_t;

typedef enum { ZSRC_NONE, ZSRC_SPECIAL, ZSRC_FIELD, ZSRC_CONTAINS } ZeekSourceKind_t;

typedef struct {
    ZeekSourceKind_t kind;
    int              special;
    int              pos;
    char            *value;
} ZeekSource_t;

typedef struct {
    char            *name;
    char            *typeName;
    char            *spec;
    ZeekBase_t       base;
    ZeekContainer_t  container;
    ZeekSource_t     sources[ZEEKLOG_MAX_SOURCES];
    int              numSources;
} ZeekColumn_t;

typedef enum { ZITER_SESSION, ZITER_DNS, ZITER_CERT, ZITER_FILE } ZeekIter_t;

typedef struct {
    char            *path;
    char           **protocols;
    ZeekIter_t       iter;
    GPtrArray       *columns;
    FILE            *fp;
    char            *filename;
    time_t           openTime;
    uint64_t         rows;
    uint64_t         dropped;
    pthread_mutex_t  lock;
    gboolean         enabled;
    gboolean         builtin;
} ZeekLog_t;

typedef struct {
    const char *source;
    const char *md5;
    const char *sha256;
    const char *mime;
    const char *filename;
    int         isOrig;
} ZeekFile_t;

typedef struct {
    ArkimeSession_t   *session;
    DNS_t             *dns;
    ArkimeCertsInfo_t *cert;
    ZeekFile_t         file;
    char               uid[ZEEKLOG_UID_LEN + 1];
} ZeekRow_t;

typedef struct {
    char        buf[ZEEKLOG_ITEM_BUF_SIZE];
    BSB         bsb;
    const char *items[ZEEKLOG_MAX_ITEMS];
    int         lens[ZEEKLOG_MAX_ITEMS];
    int         num;
    gboolean    present;
} ZeekItems_t;

typedef struct {
    ZeekItems_t items;
    ZeekItems_t scratch;
    char        row[ZEEKLOG_ROW_BUF_SIZE];
} ZeekThread_t;

typedef struct {
    const char     *name;
    ZeekBase_t      base;
    ZeekContainer_t container;
} ZeekSpecialDef_t;

typedef struct {
    const char *path;
    const char *protocols;
    ZeekIter_t  iter;
    const char *spec;
} ZeekBuiltin_t;

typedef struct {
    const char *source;
    const char *md5;
    const char *sha256;
    const char *mime;
    const char *filename;
    int         isOrig;
    int         md5Pos;
    int         sha256Pos;
    int         mimePos;
    int         filenamePos;
} ZeekFileSource_t;

/******************************************************************************/
enum {
    ZS_TS, ZS_UID, ZS_ORIG_H, ZS_ORIG_P, ZS_RESP_H, ZS_RESP_P, ZS_PROTO, ZS_SERVICE,
    ZS_DURATION, ZS_ORIG_BYTES, ZS_RESP_BYTES, ZS_ORIG_PKTS, ZS_RESP_PKTS, ZS_ORIG_IP_BYTES, ZS_RESP_IP_BYTES,
    ZS_CONN_STATE, ZS_HISTORY, ZS_MISSED_BYTES, ZS_IP_PROTO, ZS_TRANS_DEPTH, ZS_NODE, ZS_SESSION_ID,
    ZS_TRUE, ZS_FALSE, ZS_ZERO, ZS_EMPTY,
    ZS_TLS_RESUMED, ZS_TLS_ESTABLISHED, ZS_TLS_VERSION, ZS_TLS_SNI_MATCHES_CERT, ZS_SSL_CERT_CHAIN_FPS, ZS_SSL_SUBJECT, ZS_SSL_ISSUER,
    ZS_SSH_VERSION, ZS_SSH_HOST_KEY_FINGERPRINT,
    ZS_TUNNEL_TYPE, ZS_TUNNEL_ACTION, ZS_KRB_CLIENT,
    ZS_DNS_TRANS_ID, ZS_DNS_RTT, ZS_DNS_QUERY, ZS_DNS_QCLASS, ZS_DNS_QCLASS_NAME, ZS_DNS_QTYPE, ZS_DNS_QTYPE_NAME,
    ZS_DNS_RCODE, ZS_DNS_RCODE_NAME, ZS_DNS_AA, ZS_DNS_TC, ZS_DNS_RD, ZS_DNS_RA, ZS_DNS_Z,
    ZS_DNS_ANSWERS, ZS_DNS_TTLS, ZS_DNS_REJECTED, ZS_DNS_OPCODE, ZS_DNS_OPCODE_NAME,
    ZS_X509_FINGERPRINT, ZS_X509_SERIAL, ZS_X509_SUBJECT, ZS_X509_ISSUER, ZS_X509_NOT_VALID_BEFORE, ZS_X509_NOT_VALID_AFTER,
    ZS_X509_KEY_ALG, ZS_X509_KEY_TYPE, ZS_X509_CURVE, ZS_X509_SAN_DNS, ZS_X509_CA, ZS_X509_HOST_CERT,
    ZS_FILE_FUID, ZS_FILE_SOURCE, ZS_FILE_MIME_TYPE, ZS_FILE_MD5, ZS_FILE_SHA256, ZS_FILE_FILENAME, ZS_FILE_IS_ORIG,
    ZS_MAX
};

LOCAL const ZeekSpecialDef_t zeekSpecials[ZS_MAX] = {
    [ZS_TS]                       = {"ts", ZB_TIME, ZC_SCALAR},
    [ZS_UID]                      = {"uid", ZB_STRING, ZC_SCALAR},
    [ZS_ORIG_H]                   = {"orig_h", ZB_ADDR, ZC_SCALAR},
    [ZS_ORIG_P]                   = {"orig_p", ZB_PORT, ZC_SCALAR},
    [ZS_RESP_H]                   = {"resp_h", ZB_ADDR, ZC_SCALAR},
    [ZS_RESP_P]                   = {"resp_p", ZB_PORT, ZC_SCALAR},
    [ZS_PROTO]                    = {"proto", ZB_ENUM, ZC_SCALAR},
    [ZS_SERVICE]                  = {"service", ZB_STRING, ZC_SCALAR},
    [ZS_DURATION]                 = {"duration", ZB_INTERVAL, ZC_SCALAR},
    [ZS_ORIG_BYTES]               = {"orig_bytes", ZB_COUNT, ZC_SCALAR},
    [ZS_RESP_BYTES]               = {"resp_bytes", ZB_COUNT, ZC_SCALAR},
    [ZS_ORIG_PKTS]                = {"orig_pkts", ZB_COUNT, ZC_SCALAR},
    [ZS_RESP_PKTS]                = {"resp_pkts", ZB_COUNT, ZC_SCALAR},
    [ZS_ORIG_IP_BYTES]            = {"orig_ip_bytes", ZB_COUNT, ZC_SCALAR},
    [ZS_RESP_IP_BYTES]            = {"resp_ip_bytes", ZB_COUNT, ZC_SCALAR},
    [ZS_CONN_STATE]               = {"conn_state", ZB_STRING, ZC_SCALAR},
    [ZS_HISTORY]                  = {"history", ZB_STRING, ZC_SCALAR},
    [ZS_MISSED_BYTES]             = {"missed_bytes", ZB_COUNT, ZC_SCALAR},
    [ZS_IP_PROTO]                 = {"ip_proto", ZB_COUNT, ZC_SCALAR},
    [ZS_TRANS_DEPTH]              = {"trans_depth", ZB_COUNT, ZC_SCALAR},
    [ZS_NODE]                     = {"node", ZB_STRING, ZC_SCALAR},
    [ZS_SESSION_ID]               = {"session_id", ZB_STRING, ZC_SCALAR},
    [ZS_TRUE]                     = {"true", ZB_BOOL, ZC_SCALAR},
    [ZS_FALSE]                    = {"false", ZB_BOOL, ZC_SCALAR},
    [ZS_ZERO]                     = {"zero", ZB_COUNT, ZC_SCALAR},
    [ZS_EMPTY]                    = {"empty", ZB_STRING, ZC_SET},
    [ZS_TLS_RESUMED]              = {"tls.resumed", ZB_BOOL, ZC_SCALAR},
    [ZS_TLS_ESTABLISHED]          = {"tls.established", ZB_BOOL, ZC_SCALAR},
    [ZS_TLS_VERSION]              = {"tls.version", ZB_STRING, ZC_SCALAR},
    [ZS_TLS_SNI_MATCHES_CERT]     = {"tls.sni_matches_cert", ZB_BOOL, ZC_SCALAR},
    [ZS_SSL_CERT_CHAIN_FPS]       = {"ssl.cert_chain_fps", ZB_STRING, ZC_VECTOR},
    [ZS_SSL_SUBJECT]              = {"ssl.subject", ZB_STRING, ZC_SCALAR},
    [ZS_SSL_ISSUER]               = {"ssl.issuer", ZB_STRING, ZC_SCALAR},
    [ZS_SSH_VERSION]              = {"ssh.version", ZB_COUNT, ZC_SCALAR},
    [ZS_SSH_HOST_KEY_FINGERPRINT] = {"ssh.host_key_fingerprint", ZB_STRING, ZC_SCALAR},
    [ZS_TUNNEL_TYPE]              = {"tunnel.type", ZB_ENUM, ZC_SCALAR},
    [ZS_TUNNEL_ACTION]            = {"tunnel.action", ZB_ENUM, ZC_SCALAR},
    [ZS_KRB_CLIENT]               = {"krb.client", ZB_STRING, ZC_SCALAR},
    [ZS_DNS_TRANS_ID]             = {"dns.trans_id", ZB_COUNT, ZC_SCALAR},
    [ZS_DNS_RTT]                  = {"dns.rtt", ZB_INTERVAL, ZC_SCALAR},
    [ZS_DNS_QUERY]                = {"dns.query", ZB_STRING, ZC_SCALAR},
    [ZS_DNS_QCLASS]               = {"dns.qclass", ZB_COUNT, ZC_SCALAR},
    [ZS_DNS_QCLASS_NAME]          = {"dns.qclass_name", ZB_STRING, ZC_SCALAR},
    [ZS_DNS_QTYPE]                = {"dns.qtype", ZB_COUNT, ZC_SCALAR},
    [ZS_DNS_QTYPE_NAME]           = {"dns.qtype_name", ZB_STRING, ZC_SCALAR},
    [ZS_DNS_RCODE]                = {"dns.rcode", ZB_COUNT, ZC_SCALAR},
    [ZS_DNS_RCODE_NAME]           = {"dns.rcode_name", ZB_STRING, ZC_SCALAR},
    [ZS_DNS_AA]                   = {"dns.AA", ZB_BOOL, ZC_SCALAR},
    [ZS_DNS_TC]                   = {"dns.TC", ZB_BOOL, ZC_SCALAR},
    [ZS_DNS_RD]                   = {"dns.RD", ZB_BOOL, ZC_SCALAR},
    [ZS_DNS_RA]                   = {"dns.RA", ZB_BOOL, ZC_SCALAR},
    [ZS_DNS_Z]                    = {"dns.Z", ZB_COUNT, ZC_SCALAR},
    [ZS_DNS_ANSWERS]              = {"dns.answers", ZB_STRING, ZC_VECTOR},
    [ZS_DNS_TTLS]                 = {"dns.TTLs", ZB_INTERVAL, ZC_VECTOR},
    [ZS_DNS_REJECTED]             = {"dns.rejected", ZB_BOOL, ZC_SCALAR},
    [ZS_DNS_OPCODE]               = {"dns.opcode", ZB_COUNT, ZC_SCALAR},
    [ZS_DNS_OPCODE_NAME]          = {"dns.opcode_name", ZB_STRING, ZC_SCALAR},
    [ZS_X509_FINGERPRINT]         = {"x509.fingerprint", ZB_STRING, ZC_SCALAR},
    [ZS_X509_SERIAL]              = {"x509.serial", ZB_STRING, ZC_SCALAR},
    [ZS_X509_SUBJECT]             = {"x509.subject", ZB_STRING, ZC_SCALAR},
    [ZS_X509_ISSUER]              = {"x509.issuer", ZB_STRING, ZC_SCALAR},
    [ZS_X509_NOT_VALID_BEFORE]    = {"x509.not_valid_before", ZB_TIME, ZC_SCALAR},
    [ZS_X509_NOT_VALID_AFTER]     = {"x509.not_valid_after", ZB_TIME, ZC_SCALAR},
    [ZS_X509_KEY_ALG]             = {"x509.key_alg", ZB_STRING, ZC_SCALAR},
    [ZS_X509_KEY_TYPE]            = {"x509.key_type", ZB_STRING, ZC_SCALAR},
    [ZS_X509_CURVE]               = {"x509.curve", ZB_STRING, ZC_SCALAR},
    [ZS_X509_SAN_DNS]             = {"x509.san_dns", ZB_STRING, ZC_VECTOR},
    [ZS_X509_CA]                  = {"x509.ca", ZB_BOOL, ZC_SCALAR},
    [ZS_X509_HOST_CERT]           = {"x509.host_cert", ZB_BOOL, ZC_SCALAR},
    [ZS_FILE_FUID]                = {"file.fuid", ZB_STRING, ZC_SCALAR},
    [ZS_FILE_SOURCE]              = {"file.source", ZB_STRING, ZC_SCALAR},
    [ZS_FILE_MIME_TYPE]           = {"file.mime_type", ZB_STRING, ZC_SCALAR},
    [ZS_FILE_MD5]                 = {"file.md5", ZB_STRING, ZC_SCALAR},
    [ZS_FILE_SHA256]              = {"file.sha256", ZB_STRING, ZC_SCALAR},
    [ZS_FILE_FILENAME]            = {"file.filename", ZB_STRING, ZC_SCALAR},
    [ZS_FILE_IS_ORIG]             = {"file.is_orig", ZB_BOOL, ZC_SCALAR},
};

/******************************************************************************/
#define CONN_ID "ts=@ts uid=@uid id.orig_h=@orig_h id.orig_p=@orig_p id.resp_h=@resp_h id.resp_p=@resp_p "

LOCAL const ZeekBuiltin_t zeekBuiltins[] = {
    {
        "conn", NULL, ZITER_SESSION,
        CONN_ID "proto=@proto service=@service duration=@duration orig_bytes=@orig_bytes resp_bytes=@resp_bytes "
                "conn_state=@conn_state local_orig=@none:bool local_resp=@none:bool missed_bytes=@missed_bytes history=@history "
                "orig_pkts=@orig_pkts orig_ip_bytes=@orig_ip_bytes resp_pkts=@resp_pkts resp_ip_bytes=@resp_ip_bytes "
                "tunnel_parents=@none:set[string] ip_proto=@ip_proto"
    },
    {
        "dns", "dns", ZITER_DNS,
        CONN_ID "proto=@proto trans_id=@dns.trans_id rtt=@dns.rtt query=@dns.query qclass=@dns.qclass "
                "qclass_name=@dns.qclass_name qtype=@dns.qtype qtype_name=@dns.qtype_name rcode=@dns.rcode rcode_name=@dns.rcode_name "
                "AA=@dns.AA TC=@dns.TC RD=@dns.RD RA=@dns.RA Z=@dns.Z answers=@dns.answers TTLs=@dns.TTLs rejected=@dns.rejected "
                "opcode=@dns.opcode opcode_name=@dns.opcode_name"
    },
    {
        "http", "http", ZITER_SESSION,
        CONN_ID "trans_depth=@trans_depth method=http.method host=host.http uri=http.uri.path referrer=http.request.referer "
                "version=http.version.src user_agent=http.user-agent origin=@none:string request_body_len=@none:count "
                "response_body_len=@none:count status_code=http.statuscode status_msg=@none:string info_code=@none:count "
                "info_msg=@none:string tags=@empty:set[enum] username=http.user password=@none:string proxied=@none:set[string] "
                "orig_fuids=@none:vector[string] orig_filenames=@none:vector[string] orig_mime_types=@none:vector[string] "
                "resp_fuids=@none:vector[string] resp_filenames=@none:vector[string] resp_mime_types=http.bodymagic:vector[string]"
    },
    {
        "ssl", "tls,dtls", ZITER_SESSION,
        CONN_ID "version=@tls.version cipher=tls.cipher curve=@none:string server_name=host.http resumed=@tls.resumed "
                "last_alert=@none:string next_protocol=@none:string established=@tls.established ssl_history=@none:string "
                "cert_chain_fps=@ssl.cert_chain_fps client_cert_chain_fps=@empty:vector[string] sni_matches_cert=@tls.sni_matches_cert"
    },
    {
        "x509", "tls,dtls", ZITER_CERT,
        "ts=@ts fingerprint=@x509.fingerprint certificate.version=@none:count certificate.serial=@x509.serial "
        "certificate.subject=@x509.subject certificate.issuer=@x509.issuer certificate.not_valid_before=@x509.not_valid_before "
        "certificate.not_valid_after=@x509.not_valid_after certificate.key_alg=@x509.key_alg certificate.sig_alg=@none:string "
        "certificate.key_type=@x509.key_type certificate.key_length=@none:count certificate.exponent=@none:string "
        "certificate.curve=@x509.curve san.dns=@x509.san_dns san.uri=@none:vector[string] san.email=@none:vector[string] "
        "san.ip=@none:vector[addr] basic_constraints.ca=@x509.ca basic_constraints.path_len=@none:count host_cert=@x509.host_cert client_cert=@false"
    },
    {
        "files", "http,smtp", ZITER_FILE,
        "ts=@ts fuid=@file.fuid uid=@uid id.orig_h=@orig_h id.orig_p=@orig_p id.resp_h=@resp_h id.resp_p=@resp_p "
        "source=@file.source depth=@zero analyzers=@none:set[string] mime_type=@file.mime_type filename=@file.filename "
        "duration=@none:interval local_orig=@none:bool is_orig=@file.is_orig seen_bytes=@none:count total_bytes=@none:count "
        "missing_bytes=@none:count overflow_bytes=@none:count timedout=@false parent_fuid=@none:string md5=@file.md5 "
        "sha1=@none:string sha256=@file.sha256 extracted=@none:string extracted_cutoff=@none:bool extracted_size=@none:count"
    },
    {
        "ssh", "ssh", ZITER_SESSION,
        CONN_ID "version=@ssh.version auth_success=@none:bool auth_attempts=@none:count direction=@none:enum client=ssh.ver "
                "server=@none:string cipher_alg=@none:string mac_alg=@none:string compression_alg=@none:string kex_alg=@none:string "
                "host_key_alg=@none:string host_key_fingerprint=@ssh.host_key_fingerprint"
    },
    {
        "smtp", "smtp", ZITER_SESSION,
        CONN_ID "trans_depth=@trans_depth helo=email.smtp-hello mailfrom=email.src rcptto=email.dst date=@none:string "
                "from=email.src to=email.dst cc=@none:set[string] reply_to=@none:string msg_id=email.message-id in_reply_to=@none:string "
                "subject=email.subject x_originating_ip=ip.email first_received=@none:string second_received=@none:string "
                "last_reply=@none:string path=@none:vector[addr] user_agent=email.x-mailer tls=protocols?tls fuids=@empty:vector[string]"
    },
    {
        "ftp", "ftp", ZITER_SESSION,
        CONN_ID "user=user password=@none:string command=ftp.command arg=ftp.filename mime_type=@none:string "
                "file_size=@none:count reply_code=ftp.responseCode reply_msg=@none:string data_channel.passive=@none:bool "
                "data_channel.orig_h=@none:addr data_channel.resp_h=@none:addr data_channel.resp_p=@none:port fuid=@none:string"
    },
    {
        "dhcp", "dhcp", ZITER_SESSION,
        "ts=@ts uids=@uid:set[string] client_addr=dhcp.yiaddr server_addr=@resp_h mac=dhcp.mac host_name=dhcp.host "
        "client_fqdn=@none:string domain=@none:string requested_addr=dhcp.requestIp assigned_addr=dhcp.yiaddr "
        "lease_time=@none:interval client_message=@none:string server_message=@none:string msg_types=dhcp.type:vector[string] "
        "duration=@duration"
    },
    {
        "kerberos", "krb5", ZITER_SESSION,
        CONN_ID "request_type=@none:string client=@krb.client service=krb5.sname success=@none:bool error_msg=krb5.error "
                "from=@none:time till=@none:time cipher=krb5.etype forwardable=@none:bool renewable=@none:bool "
                "client_cert_subject=@none:string client_cert_fuid=@none:string server_cert_subject=@none:string server_cert_fuid=@none:string"
    },
    {
        "ntlm", "ntlm", ZITER_SESSION,
        CONN_ID "username=user hostname=ntlm.host domainname=ntlm.domain server_nb_computer_name=@none:string "
                "server_dns_computer_name=@none:string server_tree_name=@none:string success=@none:bool"
    },
    {
        "smb_mapping", "smb", ZITER_SESSION,
        CONN_ID "path=smb.share service=@none:string native_file_system=@none:string share_type=@none:string"
    },
    {
        "smb_files", "smb", ZITER_SESSION,
        CONN_ID "fuid=@none:string action=@none:enum path=smb.share name=smb.fn size=@none:count prev_name=@none:string "
                "times.modified=@none:time times.accessed=@none:time times.created=@none:time times.changed=@none:time"
    },
    {
        "dce_rpc", "dcerpc", ZITER_SESSION,
        CONN_ID "rtt=@none:interval named_pipe=@none:string endpoint=dcerpc.interface operation=@none:string"
    },
    {
        "rdp", "rdp", ZITER_SESSION,
        CONN_ID "cookie=@none:string result=@none:string security_protocol=rdp.selected-protocol client_channels=@none:vector[string] "
                "keyboard_layout=rdp.keyboard-layout client_build=rdp.client-build:string client_name=rdp.client-name "
                "client_dig_product_id=@none:string desktop_width=@none:count desktop_height=@none:count requested_color_depth=@none:string "
                "cert_type=@none:string cert_count=@none:count cert_permanent=@none:bool encryption_level=@none:string encryption_method=@none:string"
    },
    {
        "sip", "sip", ZITER_SESSION,
        CONN_ID "trans_depth=@trans_depth method=sip.method uri=@none:string date=@none:string request_from=sip.from "
                "request_to=sip.to response_from=@none:string response_to=@none:string reply_to=@none:string call_id=sip.callid "
                "seq=@none:string subject=@none:string request_path=sip.via:vector[string] response_path=@none:vector[string] "
                "user_agent=sip.user-agent status_code=sip.statuscode status_msg=@none:string warning=@none:string "
                "request_body_len=@none:count response_body_len=@none:count content_type=@none:string"
    },
    {
        "snmp", "snmp", ZITER_SESSION,
        CONN_ID "duration=@duration version=snmp.version:string community=snmp.community get_requests=@none:count "
                "get_bulk_requests=@none:count get_responses=@none:count set_requests=@none:count display_string=@none:string up_since=@none:time"
    },
    {
        "socks", "socks", ZITER_SESSION,
        CONN_ID "version=@none:count user=socks.user password=@none:string status=@none:string request.host=ip.socks "
                "request.name=host.socks request_p=port.socks bound.host=@none:addr bound.name=@none:string bound_p=@none:port"
    },
    {
        "postgresql", "postgresql", ZITER_SESSION,
        CONN_ID "user=postgresql.user database=postgresql.db application_name=postgresql.app frontend=@none:string "
                "frontend_arg=@none:string backend=@none:string backend_arg=@none:string success=@none:bool rows=@none:count"
    },
    {
        "irc", "irc", ZITER_SESSION,
        CONN_ID "nick=irc.nick user=@none:string command=@none:string value=irc.channel addl=@none:string"
    },
    {
        "modbus", "modbus", ZITER_SESSION,
        CONN_ID "tid=modbus.transactionid unit=modbus.unitid func=modbus.funccode:string pdu_type=@none:string exception=modbus.exccode:string"
    },
    {
        "dnp3", "dnp3", ZITER_SESSION,
        CONN_ID "fc_request=dnp3.funcName fc_reply=@none:string iin=@none:count"
    },
    {
        "ntp", "ntp", ZITER_SESSION,
        CONN_ID "version=ntp.version mode=ntp.mode:count stratum=ntp.stratum:count poll=@none:interval precision=@none:interval "
                "root_delay=@none:interval root_disp=@none:interval ref_id=ntp.ref-id ref_time=@none:time org_time=@none:time "
                "rec_time=@none:time xmt_time=@none:time num_exts=@none:count"
    },
    {
        "radius", "radius", ZITER_SESSION,
        CONN_ID "username=radius.user mac=radius.mac framed_addr=radius.framed-ip tunnel_client=@none:string "
                "connect_info=@none:string reply_msg=@none:string result=@none:string ttl=@none:interval"
    },
    {
        "quic", "quic", ZITER_SESSION,
        CONN_ID "version=quic.version client_initial_dcid=@none:string client_scid=@none:string server_scid=@none:string "
                "server_name=host.quic client_protocol=@none:string history=@none:string"
    },
    {
        "ldap", "ldap", ZITER_SESSION,
        CONN_ID "message_id=@none:int version=@none:int opcode=@none:string result=@none:string diagnostic_message=@none:string "
                "object=ldap.bindname argument=@none:string"
    },
    {
        "mqtt_connect", "mqtt", ZITER_SESSION,
        CONN_ID "proto_name=@none:string proto_version=mqtt.version client_id=mqtt.clientId connect_status=mqtt.connackCode:string "
                "will_topic=mqtt.willTopic will_payload=@none:string"
    },
    {
        "mqtt_publish", "mqtt", ZITER_SESSION,
        CONN_ID "from_client=@none:bool retain=@none:bool qos=mqtt.qos:string status=@none:string topic=mqtt.topic "
                "payload=@none:string payload_len=@none:count"
    },
    {
        "websocket", "websocket", ZITER_SESSION,
        CONN_ID "host=host.http uri=http.uri.path user_agent=http.user-agent subprotocol=@none:string "
                "client_protocols=@none:vector[string] server_extensions=@none:vector[string] client_extensions=@none:vector[string]"
    },
    {
        "mysql", "mysql", ZITER_SESSION,
        CONN_ID "cmd=@none:string arg=mysql.user success=@none:bool rows=@none:count response=@none:string"
    },
    {
        "tunnel", "gre,vxlan,gtp,geneve", ZITER_SESSION,
        "ts=@ts uid=@uid id.orig_h=outerip.src id.orig_p=@zero:port id.resp_h=outerip.dst id.resp_p=@zero:port "
        "tunnel_type=@tunnel.type action=@tunnel.action"
    },
};

LOCAL ZeekFileSource_t zeekFileSources[] = {
    {"HTTP", "http.md5", "http.sha256", "http.bodymagic", NULL, -1, -1, -1, -1, -1},
    {"SMTP", "email.md5", "email.sha256", "email.bodymagic", "email.fn", 1, -1, -1, -1, -1},
};

/******************************************************************************/
LOCAL GPtrArray     *zeekLogs;
LOCAL char          *zeekLogDir;
LOCAL gboolean       zeekJson;
LOCAL gboolean       zeekUtf8;
LOCAL uint32_t       zeekRotateSeconds;
LOCAL ZeekThread_t  *zeekThreads[ARKIME_MAX_PACKET_THREADS];

LOCAL int protocolField;
LOCAL int certsField;
LOCAL int dnsField;
LOCAL int tlsSrcSessionIdField;
LOCAL int tlsDstSessionIdField;
LOCAL int tlsCipherField;
LOCAL int tlsVersionField;
LOCAL int httpHostField;
LOCAL int sshVerField;
LOCAL int sshKeyField;
LOCAL int krbCnameField;
LOCAL int krbRealmField;
LOCAL int vlanField;

/******************************************************************************/
LOCAL const ArkimeField_t *zeeklog_session_field(const ArkimeSession_t *session, int pos)
{
    if (pos < 0 || pos >= session->maxFields)
        return NULL;
    return session->fields[pos];
}
/******************************************************************************/
LOCAL void zeeklog_make_uid(char prefix, const uint8_t *data, int len, char *out)
{
    static const char b62[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    uint64_t h1 = 0xcbf29ce484222325ULL;
    uint64_t h2 = 0x84222325cbf29ce4ULL;

    for (int i = 0; i < len; i++) {
        h1 = (h1 ^ data[i]) * 0x100000001b3ULL;
        h2 = (h2 ^ data[len - 1 - i]) * 0x100000001b3ULL;
    }
    h2 ^= h1 >> 29;

    out[0] = prefix;
    int i;
    for (i = 1; i <= 11; i++) {
        out[i] = b62[h1 % 62];
        h1 /= 62;
    }
    for (; i < ZEEKLOG_UID_LEN; i++) {
        out[i] = b62[h2 % 62];
        h2 /= 62;
    }
    out[ZEEKLOG_UID_LEN] = 0;
}
/******************************************************************************/
/* Items: the list of values a column produced for one row */
LOCAL void zeeklog_items_reset(ZeekItems_t *items)
{
    BSB_INIT(items->bsb, items->buf, sizeof(items->buf));
    items->num = 0;
    items->present = FALSE;
}
/******************************************************************************/
LOCAL void zeeklog_items_add(ZeekItems_t *items, const char *str, int len)
{
    items->present = TRUE;
    if (!str || len < 0 || items->num >= ZEEKLOG_MAX_ITEMS || BSB_REMAINING(items->bsb) < (uint32_t)len + 1)
        return;
    items->items[items->num] = (char *)BSB_WORK_PTR(items->bsb);
    items->lens[items->num] = len;
    BSB_EXPORT_ptr(items->bsb, str, len);
    BSB_EXPORT_u08(items->bsb, 0);
    items->num++;
}
/******************************************************************************/
LOCAL void zeeklog_items_add_str(ZeekItems_t *items, const char *str)
{
    if (!str) {
        items->present = TRUE;
        return;
    }
    zeeklog_items_add(items, str, strlen(str));
}
/******************************************************************************/
LOCAL void zeeklog_items_addf(ZeekItems_t *items, const char *fmt, ...)
{
    char    tmp[512];
    va_list args;

    va_start(args, fmt);
    int len = vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);
    if (len >= (int)sizeof(tmp))
        len = sizeof(tmp) - 1;
    zeeklog_items_add(items, tmp, len);
}
/******************************************************************************/
LOCAL void zeeklog_items_add_bool(ZeekItems_t *items, gboolean value)
{
    zeeklog_items_add(items, value ? "T" : "F", 1);
}
/******************************************************************************/
LOCAL void zeeklog_items_add_int(ZeekItems_t *items, ZeekBase_t base, int64_t value)
{
    switch (base) {
    case ZB_BOOL:
        zeeklog_items_add_bool(items, value != 0);
        break;
    case ZB_TIME:
    case ZB_INTERVAL:
        zeeklog_items_addf(items, "%" PRId64 ".000000", value);
        break;
    default:
        zeeklog_items_addf(items, "%" PRId64, value);
        break;
    }
}
/******************************************************************************/
LOCAL void zeeklog_items_add_timeval(ZeekItems_t *items, const struct timeval *tv)
{
    zeeklog_items_addf(items, "%ld.%06ld", (long)tv->tv_sec, (long)tv->tv_usec);
}
/******************************************************************************/
LOCAL void zeeklog_items_add_ip(ZeekItems_t *items, const struct in6_addr *ip)
{
    char buf[INET6_ADDRSTRLEN];

    if (IN6_IS_ADDR_V4MAPPED(ip)) {
        arkime_ip4tostr(ARKIME_V6_TO_V4(*ip), buf, sizeof(buf));
    } else {
        inet_ntop(AF_INET6, ip, buf, sizeof(buf));
    }
    zeeklog_items_add_str(items, buf);
}
/******************************************************************************/
LOCAL void zeeklog_items_add_string_head(ZeekItems_t *items, const ArkimeStringHead_t *head)
{
    ArkimeString_t *string;
    DLL_FOREACH(s_, head, string) {
        zeeklog_items_add(items, string->str, string->len);
    }
}
/******************************************************************************/
LOCAL gboolean zeeklog_items_contains(const ZeekItems_t *items, const char *value)
{
    for (int i = 0; i < items->num; i++) {
        if (strcasecmp(items->items[i], value) == 0)
            return TRUE;
    }
    return FALSE;
}
/******************************************************************************/
/* Collect the values of an Arkime field */
LOCAL gboolean zeeklog_field_items(ZeekItems_t *items, const ArkimeSession_t *session, int pos, ZeekBase_t base)
{
    const ArkimeField_t *field = zeeklog_session_field(session, pos);
    if (!field)
        return FALSE;

    const ArkimeFieldInfo_t *info = config.fields[pos];
    GHashTableIter           iter;
    gpointer                 ikey;
    ArkimeString_t          *hstring;
    ArkimeInt_t             *hint;

    switch (info->type) {
    case ARKIME_FIELD_TYPE_INT:
        zeeklog_items_add_int(items, base, field->i);
        break;
    case ARKIME_FIELD_TYPE_INT_ARRAY:
    case ARKIME_FIELD_TYPE_INT_ARRAY_UNIQUE:
        items->present = TRUE;
        for (uint32_t i = 0; i < field->iarray->len; i++) {
            zeeklog_items_add_int(items, base, g_array_index(field->iarray, uint32_t, i));
        }
        break;
    case ARKIME_FIELD_TYPE_INT_HASH:
        items->present = TRUE;
        HASH_FORALL2(i_, *field->ihash, hint) {
            zeeklog_items_add_int(items, base, hint->i_hash);
        }
        break;
    case ARKIME_FIELD_TYPE_INT_GHASH:
        items->present = TRUE;
        g_hash_table_iter_init(&iter, field->ghash);
        while (g_hash_table_iter_next(&iter, &ikey, NULL)) {
            zeeklog_items_add_int(items, base, (int64_t)(long)ikey);
        }
        break;
    case ARKIME_FIELD_TYPE_STR:
        zeeklog_items_add_str(items, field->str);
        break;
    case ARKIME_FIELD_TYPE_STR_ARRAY:
        items->present = TRUE;
        for (uint32_t i = 0; i < field->sarray->len; i++) {
            zeeklog_items_add_str(items, g_ptr_array_index(field->sarray, i));
        }
        break;
    case ARKIME_FIELD_TYPE_STR_HASH:
        items->present = TRUE;
        HASH_FORALL2(s_, *field->shash, hstring) {
            zeeklog_items_add(items, hstring->str, hstring->len);
        }
        break;
    case ARKIME_FIELD_TYPE_STR_GHASH:
        items->present = TRUE;
        g_hash_table_iter_init(&iter, field->ghash);
        while (g_hash_table_iter_next(&iter, &ikey, NULL)) {
            zeeklog_items_add_str(items, ikey);
        }
        break;
    case ARKIME_FIELD_TYPE_IP:
        zeeklog_items_add_ip(items, field->ip);
        break;
    case ARKIME_FIELD_TYPE_IP_GHASH:
        items->present = TRUE;
        g_hash_table_iter_init(&iter, field->ghash);
        while (g_hash_table_iter_next(&iter, &ikey, NULL)) {
            zeeklog_items_add_ip(items, ikey);
        }
        break;
    case ARKIME_FIELD_TYPE_FLOAT:
        zeeklog_items_addf(items, "%f", field->f);
        break;
    case ARKIME_FIELD_TYPE_FLOAT_ARRAY:
        items->present = TRUE;
        for (uint32_t i = 0; i < field->farray->len; i++) {
            zeeklog_items_addf(items, "%f", g_array_index(field->farray, float, i));
        }
        break;
    case ARKIME_FIELD_TYPE_FLOAT_GHASH:
        items->present = TRUE;
        g_hash_table_iter_init(&iter, field->ghash);
        while (g_hash_table_iter_next(&iter, &ikey, NULL)) {
            zeeklog_items_addf(items, "%f", POINTER_TO_FLOAT(ikey));
        }
        break;
    case ARKIME_FIELD_TYPE_OBJECT:
        break;
    }
    return TRUE;
}
/******************************************************************************/
LOCAL const char *zeeklog_service_name(const char *protocol)
{
    static const char *skip[] = {"tcp", "udp", "icmp", "sctp", "gre", "ppp", "pppoe", "vxlan", "vxlan-gpe", "gtp", "geneve", "mpls", "erspan", "esp", "ah", NULL};
    for (int i = 0; skip[i]; i++) {
        if (strcmp(protocol, skip[i]) == 0)
            return NULL;
    }
    if (strcmp(protocol, "tls") == 0)
        return "ssl";
    if (strcmp(protocol, "krb5") == 0)
        return "krb";
    if (strcmp(protocol, "dcerpc") == 0)
        return "dce_rpc";
    return protocol;
}
/******************************************************************************/
// Arkime counts wire bytes, Zeek counts IP bytes, so strip the Ethernet header
LOCAL uint64_t zeeklog_ip_bytes(const ArkimeSession_t *session, int which)
{
    uint64_t header = 14;
    if (zeeklog_session_field(session, vlanField))
        header += 4;
    uint64_t overhead = header * session->packets[which];
    return session->bytes[which] > overhead ? session->bytes[which] - overhead : 0;
}
/******************************************************************************/
LOCAL void zeeklog_conn_state(ZeekItems_t *items, const ArkimeSession_t *session)
{
    const char *state;

    if (session->ipProtocol == IPPROTO_TCP) {
        int syn    = session->tcpData.tcpFlagCnt[ARKIME_TCPFLAG_SYN];
        int synack = session->tcpData.tcpFlagCnt[ARKIME_TCPFLAG_SYN_ACK];
        int fin    = session->tcpData.tcpFlagCnt[ARKIME_TCPFLAG_FIN];
        int rst    = session->tcpData.tcpFlagCnt[ARKIME_TCPFLAG_RST];

        if (syn && synack) {
            if (fin >= 2)
                state = "SF";
            else if (rst)
                state = session->packets[1] > 1 ? "RSTR" : "RSTO";
            else if (fin == 1)
                state = "S2";
            else
                state = "S1";
        } else if (syn) {
            if (rst)
                state = session->packets[1] ? "REJ" : "RSTOS0";
            else if (fin)
                state = "SH";
            else
                state = "S0";
        } else if (synack) {
            if (rst)
                state = "RSTRH";
            else if (fin)
                state = "SHR";
            else
                state = "OTH";
        } else {
            state = "OTH";
        }
    } else if (session->ipProtocol == IPPROTO_UDP) {
        state = session->packets[1] ? "SF" : "S0";
    } else {
        state = "OTH";
    }
    zeeklog_items_add_str(items, state);
}
/******************************************************************************/
LOCAL void zeeklog_history(ZeekItems_t *items, const ArkimeSession_t *session)
{
    char history[16];
    int  len = 0;

    if (session->ipProtocol == IPPROTO_TCP) {
        int syn    = session->tcpData.tcpFlagCnt[ARKIME_TCPFLAG_SYN];
        int synack = session->tcpData.tcpFlagCnt[ARKIME_TCPFLAG_SYN_ACK];
        int ack    = session->tcpData.tcpFlagCnt[ARKIME_TCPFLAG_ACK];
        int fin    = session->tcpData.tcpFlagCnt[ARKIME_TCPFLAG_FIN];
        int rst    = session->tcpData.tcpFlagCnt[ARKIME_TCPFLAG_RST];

        if (syn)
            history[len++] = 'S';
        if (synack)
            history[len++] = 'h';
        if (ack && session->packets[0])
            history[len++] = 'A';
        if (session->databytes[0])
            history[len++] = 'D';
        if (ack && session->packets[1])
            history[len++] = 'a';
        if (session->databytes[1])
            history[len++] = 'd';
        if (fin)
            history[len++] = 'F';
        if (fin > 1)
            history[len++] = 'f';
        if (rst)
            history[len++] = 'R';
    } else {
        if (session->databytes[0] || session->packets[0])
            history[len++] = 'D';
        if (session->databytes[1] || session->packets[1])
            history[len++] = 'd';
    }
    zeeklog_items_add(items, history, len);
}
/******************************************************************************/
LOCAL void zeeklog_dn(ZeekItems_t *items, const ArkimeCertInfo_t *info)
{
    char  buf[4096];
    BSB   bsb;
    ArkimeString_t *string;

    BSB_INIT(bsb, buf, sizeof(buf));
    DLL_FOREACH(s_, &info->commonName, string) {
        BSB_EXPORT_sprintf(bsb, "CN=%s,", string->str);
    }
    DLL_FOREACH(s_, &info->orgName, string) {
        BSB_EXPORT_sprintf(bsb, "O=%s,", string->str);
    }
    DLL_FOREACH(s_, &info->orgUnit, string) {
        BSB_EXPORT_sprintf(bsb, "OU=%s,", string->str);
    }
    if (BSB_IS_ERROR(bsb) || BSB_LENGTH(bsb) == 0)
        return;
    zeeklog_items_add(items, buf, BSB_LENGTH(bsb) - 1);
}
/******************************************************************************/
LOCAL ArkimeCertsInfo_t *zeeklog_leaf_cert(const ArkimeSession_t *session)
{
    const ArkimeField_t *field = zeeklog_session_field(session, certsField);
    if (!field)
        return NULL;

    ArkimeFieldObject_t *object;
    ArkimeCertsInfo_t   *first = NULL;
    HASH_FORALL2(o_, *field->ohash, object) {
        ArkimeCertsInfo_t *ci = object->object;
        if (!ci)
            continue;
        if (!ci->isCA)
            return ci;
        if (!first)
            first = ci;
    }
    return first;
}
/******************************************************************************/
LOCAL const char *zeeklog_dns_class_name(const char *name)
{
    if (!name)
        return NULL;
    if (strcmp(name, "IN") == 0)
        return "C_INTERNET";
    if (strcmp(name, "CS") == 0)
        return "C_CSNET";
    if (strcmp(name, "CH") == 0)
        return "C_CHAOS";
    if (strcmp(name, "HS") == 0)
        return "C_HESIOD";
    if (strcmp(name, "NONE") == 0)
        return "C_NONE";
    if (strcmp(name, "ANY") == 0)
        return "C_ANY";
    return name;
}
/******************************************************************************/
LOCAL const char *zeeklog_dns_opcode_name(int opcode)
{
    switch (opcode) {
    case 0:
        return "query";
    case 1:
        return "iquery";
    case 2:
        return "server-status";
    case 4:
        return "notify";
    case 5:
        return "dynamic-update";
    case 6:
        return "dso";
    default:
        return NULL;
    }
}
/******************************************************************************/
LOCAL void zeeklog_dns_query(ZeekItems_t *items, const DNS_t *dns)
{
    // Zeek logs the wire name, Arkime decodes punycode and uses <root> for an empty name
    if (dns->punyHosts && HASH_COUNT(s_, *dns->punyHosts) > 0) {
        ArkimeString_t *hstring;
        HASH_FORALL2(s_, *dns->punyHosts, hstring) {
            zeeklog_items_add(items, hstring->str, hstring->len);
            return;
        }
    }
    if (!dns->query.hostname || strcmp(dns->query.hostname, "<root>") == 0)
        zeeklog_items_add(items, "", 0);
    else
        zeeklog_items_add_str(items, dns->query.hostname);
}
/******************************************************************************/
LOCAL void zeeklog_tls_version(ZeekItems_t *items, const ArkimeSession_t *session, ZeekItems_t *scratch)
{
    zeeklog_items_reset(scratch);
    zeeklog_field_items(scratch, session, tlsVersionField, ZB_STRING);
    for (int i = 0; i < scratch->num; i++) {
        // TLSv1.2 -> TLSv12, TLSv1.3-draft-18 -> TLSv13-draft18
        char  buf[64];
        int   len = 0;
        int   dashes = 0;
        for (const char *p = scratch->items[i]; *p && len < (int)sizeof(buf) - 1; p++) {
            if (*p == '.')
                continue;
            if (*p == '-' && ++dashes == 2)
                continue;
            buf[len++] = *p;
        }
        // TLSv1 is TLSv10 in Zeek
        if (len > 2 && buf[len - 2] == 'v' && buf[len - 1] == '1' && len < (int)sizeof(buf) - 1)
            buf[len++] = '0';
        zeeklog_items_add(items, buf, len);
    }
}
/******************************************************************************/
LOCAL gboolean zeeklog_name_matches(const char *pattern, const char *name)
{
    if (strncmp(pattern, "*.", 2) == 0) {
        const char *dot = strchr(name, '.');
        return dot && strcasecmp(pattern + 2, dot + 1) == 0;
    }
    return strcasecmp(pattern, name) == 0;
}
/******************************************************************************/
LOCAL void zeeklog_dns_answers(ZeekItems_t *items, const DNS_t *dns, gboolean ttls)
{
    DNSAnswer_t *answer;
    char         buf[INET6_ADDRSTRLEN];

    items->present = TRUE;
    DLL_FOREACH(t_, &dns->answers, answer) {
        if (answer->section != RESULT_RECORD_ANSWER)
            continue;
        if (ttls) {
            zeeklog_items_addf(items, "%u.000000", answer->ttl);
            continue;
        }
        switch (answer->type_id) {
        case DNS_RR_A: {
            uint32_t ip = answer->ipA;
            zeeklog_items_addf(items, "%u.%u.%u.%u", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff);
            break;
        }
        case DNS_RR_AAAA:
            zeeklog_items_add_ip(items, answer->ipAAAA);
            break;
        case DNS_RR_NS:
            zeeklog_items_add_str(items, answer->nsdname);
            break;
        case DNS_RR_CNAME:
            zeeklog_items_add_str(items, answer->cname);
            break;
        case DNS_RR_PTR:
            zeeklog_items_add_str(items, answer->ptrdname);
            break;
        case DNS_RR_MX:
            zeeklog_items_add_str(items, answer->mx ? answer->mx->exchange : NULL);
            break;
        case DNS_RR_SRV:
            zeeklog_items_add_str(items, answer->srv ? answer->srv->target : NULL);
            break;
        case DNS_RR_HTTPS:
            zeeklog_items_add_str(items, answer->svcb ? answer->svcb->dname : NULL);
            break;
        case DNS_RR_TXT:
            if (answer->txts && answer->txts->len > 0) {
                const char *txt = g_ptr_array_index(answer->txts, 0);
                zeeklog_items_addf(items, "TXT %d %s", (int)strlen(txt), txt);
            } else {
                zeeklog_items_add_str(items, "TXT 0 ");
            }
            break;
        case DNS_RR_RRSIG:
            if (answer->rrsig)
                zeeklog_items_addf(items, "RRSIG %u %s", answer->rrsig->typeCovered, answer->rrsig->signerName && *answer->rrsig->signerName ? answer->rrsig->signerName : "<Root>");
            break;
        case DNS_RR_DS:
            if (answer->ds)
                zeeklog_items_addf(items, "DS %u %u", answer->ds->algorithm, answer->ds->digestType);
            break;
        case DNS_RR_CAA:
            if (answer->caa) {
                snprintf(buf, sizeof(buf), "%u %s", answer->caa->flags, answer->caa->tag ? answer->caa->tag : "");
                zeeklog_items_add_str(items, buf);
            }
            break;
        default:
            zeeklog_items_add_str(items, answer->type ? answer->type : "?");
            break;
        }
    }
}
/******************************************************************************/
LOCAL void zeeklog_ssh_version(ZeekItems_t *items, const ArkimeSession_t *session, ZeekItems_t *scratch)
{
    zeeklog_items_reset(scratch);
    if (!zeeklog_field_items(scratch, session, sshVerField, ZB_STRING))
        return;

    int version = 0;
    for (int i = 0; i < scratch->num; i++) {
        const char *v = scratch->items[i];
        if (strncasecmp(v, "ssh-2", 5) == 0 || strncasecmp(v, "ssh-1.99", 8) == 0)
            version = 2;
        else if (version == 0 && strncasecmp(v, "ssh-1", 5) == 0)
            version = 1;
    }
    if (version)
        zeeklog_items_addf(items, "%d", version);
}
/******************************************************************************/
LOCAL void zeeklog_ssh_fingerprint(ZeekItems_t *items, const ArkimeSession_t *session, ZeekItems_t *scratch)
{
    zeeklog_items_reset(scratch);
    if (!zeeklog_field_items(scratch, session, sshKeyField, ZB_STRING) || scratch->num == 0)
        return;

    gsize    len = 0;
    guchar  *raw = g_base64_decode(scratch->items[0], &len);
    if (!raw)
        return;

    guint8 digest[16];
    gsize  dlen = sizeof(digest);
    GChecksum *sum = g_checksum_new(G_CHECKSUM_MD5);
    g_checksum_update(sum, raw, len);
    g_checksum_get_digest(sum, digest, &dlen);
    g_checksum_free(sum);
    g_free(raw);

    char fp[48];
    for (int i = 0; i < 16; i++) {
        snprintf(fp + i * 3, 4, "%02x:", digest[i]);
    }
    zeeklog_items_add(items, fp, 47);
}
/******************************************************************************/
LOCAL void zeeklog_tunnel_type(ZeekItems_t *items, const ArkimeSession_t *session)
{
    if (arkime_session_has_protocol((ArkimeSession_t *)session, "gre"))
        zeeklog_items_add_str(items, "Tunnel::GRE");
    else if (arkime_session_has_protocol((ArkimeSession_t *)session, "vxlan"))
        zeeklog_items_add_str(items, "Tunnel::VXLAN");
    else if (arkime_session_has_protocol((ArkimeSession_t *)session, "gtp"))
        zeeklog_items_add_str(items, "Tunnel::GTPv1");
    else if (arkime_session_has_protocol((ArkimeSession_t *)session, "geneve"))
        zeeklog_items_add_str(items, "Tunnel::GENEVE");
}
/******************************************************************************/
LOCAL void zeeklog_special_items(ZeekItems_t *items, int special, const ZeekRow_t *row, ZeekItems_t *scratch)
{
    const ArkimeSession_t *session = row->session;
    const DNS_t           *dns = row->dns;
    ArkimeCertsInfo_t     *cert = row->cert;
    ArkimeString_t        *hstring;
    const ArkimeField_t   *field;

    switch (special) {
    case ZS_TS:
        if (dns && dns->queryTime.tv_sec)
            zeeklog_items_add_timeval(items, &dns->queryTime);
        else if (dns && dns->responseTime.tv_sec)
            zeeklog_items_add_timeval(items, &dns->responseTime);
        else
            zeeklog_items_add_timeval(items, &session->firstPacket);
        break;
    case ZS_EMPTY:
        items->present = TRUE;
        break;
    case ZS_UID:
        zeeklog_items_add(items, row->uid, ZEEKLOG_UID_LEN);
        break;
    case ZS_ORIG_H:
        zeeklog_items_add_ip(items, &session->addr1);
        break;
    case ZS_RESP_H:
        zeeklog_items_add_ip(items, &session->addr2);
        break;
    case ZS_ORIG_P:
        zeeklog_items_addf(items, "%u", session->port1);
        break;
    case ZS_RESP_P:
        zeeklog_items_addf(items, "%u", session->port2);
        break;
    case ZS_PROTO:
        switch (session->ipProtocol) {
        case IPPROTO_TCP:
            zeeklog_items_add_str(items, "tcp");
            break;
        case IPPROTO_UDP:
            zeeklog_items_add_str(items, "udp");
            break;
        case IPPROTO_ICMP:
        case IPPROTO_ICMPV6:
            zeeklog_items_add_str(items, "icmp");
            break;
        default:
            zeeklog_items_add_str(items, "unknown_transport");
            break;
        }
        break;
    case ZS_SERVICE: {
        char buf[1024];
        BSB  bsb;
        BSB_INIT(bsb, buf, sizeof(buf));
        field = zeeklog_session_field(session, protocolField);
        if (field) {
            HASH_FORALL2(s_, *field->shash, hstring) {
                const char *name = zeeklog_service_name(hstring->str);
                if (name)
                    BSB_EXPORT_sprintf(bsb, "%s,", name);
            }
        }
        if (!BSB_IS_ERROR(bsb) && BSB_LENGTH(bsb) > 0)
            zeeklog_items_add(items, buf, BSB_LENGTH(bsb) - 1);
        break;
    }
    case ZS_DURATION: {
        struct timeval d;
        timersub(&session->lastPacket, &session->firstPacket, &d);
        zeeklog_items_add_timeval(items, &d);
        break;
    }
    case ZS_ORIG_BYTES:
        zeeklog_items_addf(items, "%" PRIu64, session->databytes[0]);
        break;
    case ZS_RESP_BYTES:
        zeeklog_items_addf(items, "%" PRIu64, session->databytes[1]);
        break;
    case ZS_ORIG_PKTS:
        zeeklog_items_addf(items, "%u", session->packets[0]);
        break;
    case ZS_RESP_PKTS:
        zeeklog_items_addf(items, "%u", session->packets[1]);
        break;
    case ZS_ORIG_IP_BYTES:
        zeeklog_items_addf(items, "%" PRIu64, zeeklog_ip_bytes(session, 0));
        break;
    case ZS_RESP_IP_BYTES:
        zeeklog_items_addf(items, "%" PRIu64, zeeklog_ip_bytes(session, 1));
        break;
    case ZS_CONN_STATE:
        zeeklog_conn_state(items, session);
        break;
    case ZS_HISTORY:
        zeeklog_history(items, session);
        break;
    case ZS_MISSED_BYTES:
    case ZS_ZERO:
        zeeklog_items_add_str(items, "0");
        break;
    case ZS_IP_PROTO:
        zeeklog_items_addf(items, "%u", session->ipProtocol);
        break;
    case ZS_TRANS_DEPTH:
        zeeklog_items_add_str(items, "1");
        break;
    case ZS_NODE:
        zeeklog_items_add_str(items, config.nodeName);
        break;
    case ZS_SESSION_ID: {
        char buf[ARKIME_SESSIONID_LEN * 2 + 8];
        arkime_session_id_string(session->sessionId, buf);
        zeeklog_items_add_str(items, buf);
        break;
    }
    case ZS_TRUE:
        zeeklog_items_add_bool(items, TRUE);
        break;
    case ZS_FALSE:
        zeeklog_items_add_bool(items, FALSE);
        break;
    case ZS_TLS_RESUMED: {
        gboolean resumed = FALSE;
        zeeklog_items_reset(scratch);
        if (zeeklog_field_items(scratch, session, tlsSrcSessionIdField, ZB_STRING) && scratch->num > 0) {
            int srcNum = scratch->num;
            zeeklog_field_items(scratch, session, tlsDstSessionIdField, ZB_STRING);
            for (int i = srcNum; i < scratch->num && !resumed; i++) {
                for (int j = 0; j < srcNum; j++) {
                    if (strcmp(scratch->items[i], scratch->items[j]) == 0) {
                        resumed = TRUE;
                        break;
                    }
                }
            }
        }
        zeeklog_items_add_bool(items, resumed);
        break;
    }
    case ZS_TLS_ESTABLISHED:
        zeeklog_items_add_bool(items, zeeklog_session_field(session, tlsCipherField) != NULL);
        break;
    case ZS_TLS_VERSION:
        zeeklog_tls_version(items, session, scratch);
        break;
    case ZS_TLS_SNI_MATCHES_CERT: {
        cert = zeeklog_leaf_cert(session);
        zeeklog_items_reset(scratch);
        zeeklog_field_items(scratch, session, httpHostField, ZB_STRING);
        if (!cert || scratch->num == 0)
            break;
        gboolean        match = FALSE;
        ArkimeString_t *string;
        for (int i = 0; i < scratch->num && !match; i++) {
            DLL_FOREACH(s_, &cert->subject.commonName, string) {
                if (zeeklog_name_matches(string->str, scratch->items[i]))
                    match = TRUE;
            }
            DLL_FOREACH(s_, &cert->alt, string) {
                if (zeeklog_name_matches(string->str, scratch->items[i]))
                    match = TRUE;
            }
        }
        zeeklog_items_add_bool(items, match);
        break;
    }
    case ZS_SSL_CERT_CHAIN_FPS:
        field = zeeklog_session_field(session, certsField);
        if (field) {
            ArkimeFieldObject_t *object;
            items->present = TRUE;
            HASH_FORALL2(o_, *field->ohash, object) {
                if (object->object)
                    zeeklog_items_add_str(items, (char *)((ArkimeCertsInfo_t *)object->object)->hash);
            }
        }
        break;
    case ZS_SSL_SUBJECT:
        cert = zeeklog_leaf_cert(session);
        if (cert)
            zeeklog_dn(items, &cert->subject);
        break;
    case ZS_SSL_ISSUER:
        cert = zeeklog_leaf_cert(session);
        if (cert)
            zeeklog_dn(items, &cert->issuer);
        break;
    case ZS_SSH_VERSION:
        zeeklog_ssh_version(items, session, scratch);
        break;
    case ZS_SSH_HOST_KEY_FINGERPRINT:
        zeeklog_ssh_fingerprint(items, session, scratch);
        break;
    case ZS_TUNNEL_TYPE:
        zeeklog_tunnel_type(items, session);
        break;
    case ZS_TUNNEL_ACTION:
        zeeklog_items_add_str(items, "Tunnel::DISCOVER");
        break;
    case ZS_KRB_CLIENT: {
        zeeklog_items_reset(scratch);
        zeeklog_field_items(scratch, session, krbCnameField, ZB_STRING);
        if (scratch->num == 0)
            break;
        int cnames = scratch->num;
        zeeklog_field_items(scratch, session, krbRealmField, ZB_STRING);
        if (scratch->num > cnames)
            zeeklog_items_addf(items, "%s/%s", scratch->items[0], scratch->items[cnames]);
        else
            zeeklog_items_add_str(items, scratch->items[0]);
        break;
    }

    case ZS_DNS_TRANS_ID:
        if (dns)
            zeeklog_items_addf(items, "%u", dns->query.packet_uid);
        break;
    case ZS_DNS_RTT:
        if (dns && dns->queryTime.tv_sec && dns->responseTime.tv_sec && timercmp(&dns->responseTime, &dns->queryTime, >=)) {
            struct timeval d;
            timersub(&dns->responseTime, &dns->queryTime, &d);
            zeeklog_items_add_timeval(items, &d);
        }
        break;
    case ZS_DNS_QUERY:
        if (dns)
            zeeklog_dns_query(items, dns);
        break;
    case ZS_DNS_QCLASS:
        if (dns)
            zeeklog_items_addf(items, "%u", dns->query.class_id);
        break;
    case ZS_DNS_QCLASS_NAME:
        if (dns)
            zeeklog_items_add_str(items, zeeklog_dns_class_name(dns->query.class));
        break;
    case ZS_DNS_QTYPE:
        if (dns)
            zeeklog_items_addf(items, "%u", dns->query.type_id);
        break;
    case ZS_DNS_QTYPE_NAME:
        if (dns && dns->query.type)
            zeeklog_items_add_str(items, strcmp(dns->query.type, "ANY") == 0 ? "*" : dns->query.type);
        break;
    case ZS_DNS_RCODE:
        if (dns && dns->rcode_id != -1)
            zeeklog_items_addf(items, "%d", dns->rcode_id);
        break;
    case ZS_DNS_RCODE_NAME:
        if (dns && dns->rcode_id != -1)
            zeeklog_items_add_str(items, dns->rcode_id == 4 ? "NOTIMP" : dns->rcode);
        break;
    // Zeek takes RD from the query and AA/TC/RA/Z from the reply
    case ZS_DNS_AA:
        zeeklog_items_add_bool(items, dns && (dns->responseFlags & 0x40));
        break;
    case ZS_DNS_TC:
        zeeklog_items_add_bool(items, dns && (dns->responseFlags & 0x20));
        break;
    case ZS_DNS_RD:
        zeeklog_items_add_bool(items, dns && ((dns->queryTime.tv_sec ? dns->queryFlags : dns->responseFlags) & 0x10));
        break;
    case ZS_DNS_RA:
        zeeklog_items_add_bool(items, dns && (dns->responseFlags & 0x08));
        break;
    case ZS_DNS_Z:
        zeeklog_items_addf(items, "%u", dns ? (dns->responseFlags & 0x07) : 0);
        break;
    case ZS_DNS_ANSWERS:
        if (dns && dns->rcode_id != -1)
            zeeklog_dns_answers(items, dns, FALSE);
        break;
    case ZS_DNS_TTLS:
        if (dns && dns->rcode_id != -1)
            zeeklog_dns_answers(items, dns, TRUE);
        break;
    case ZS_DNS_REJECTED:
        // Zeek: a reply that carries no resource records at all
        zeeklog_items_add_bool(items, dns && dns->responseTime.tv_sec && DLL_COUNT(t_, &dns->answers) == 0);
        break;
    case ZS_DNS_OPCODE:
        if (dns)
            zeeklog_items_addf(items, "%u", dns->query.opcode_id);
        break;
    case ZS_DNS_OPCODE_NAME:
        if (dns) {
            const char *name = zeeklog_dns_opcode_name(dns->query.opcode_id);
            if (name)
                zeeklog_items_add_str(items, name);
            else
                zeeklog_items_addf(items, "opcode-%u", dns->query.opcode_id);
        }
        break;

    case ZS_X509_FINGERPRINT:
        if (cert)
            zeeklog_items_add_str(items, (char *)cert->hash);
        break;
    case ZS_X509_SERIAL:
        if (cert && cert->serialNumber) {
            char buf[1024];
            BSB  bsb;
            BSB_INIT(bsb, buf, sizeof(buf));
            for (uint32_t k = 0; k < cert->serialNumberLen; k++) {
                BSB_EXPORT_sprintf(bsb, "%02X", cert->serialNumber[k]);
            }
            if (!BSB_IS_ERROR(bsb))
                zeeklog_items_add(items, buf, BSB_LENGTH(bsb));
        }
        break;
    case ZS_X509_SUBJECT:
        if (cert)
            zeeklog_dn(items, &cert->subject);
        break;
    case ZS_X509_ISSUER:
        if (cert)
            zeeklog_dn(items, &cert->issuer);
        break;
    case ZS_X509_NOT_VALID_BEFORE:
        if (cert)
            zeeklog_items_addf(items, "%" PRIu64 ".000000", cert->notBefore);
        break;
    case ZS_X509_NOT_VALID_AFTER:
        if (cert)
            zeeklog_items_addf(items, "%" PRIu64 ".000000", cert->notAfter);
        break;
    case ZS_X509_KEY_ALG:
        if (cert)
            zeeklog_items_add_str(items, cert->publicAlgorithm);
        break;
    case ZS_X509_KEY_TYPE:
        if (cert && cert->publicAlgorithm) {
            if (strcasestr(cert->publicAlgorithm, "rsa"))
                zeeklog_items_add_str(items, "rsa");
            else if (strcasestr(cert->publicAlgorithm, "ec"))
                zeeklog_items_add_str(items, "ecdsa");
            else if (strcasestr(cert->publicAlgorithm, "dsa"))
                zeeklog_items_add_str(items, "dsa");
        }
        break;
    case ZS_X509_CURVE:
        if (cert)
            zeeklog_items_add_str(items, cert->curve);
        break;
    case ZS_X509_SAN_DNS:
        if (cert) {
            items->present = TRUE;
            zeeklog_items_add_string_head(items, &cert->alt);
        }
        break;
    case ZS_X509_CA:
        if (cert)
            zeeklog_items_add_bool(items, cert->isCA);
        break;
    case ZS_X509_HOST_CERT:
        if (cert)
            zeeklog_items_add_bool(items, !cert->isCA);
        break;

    case ZS_FILE_FUID:
        if (row->file.md5) {
            char fuid[ZEEKLOG_UID_LEN + 1];
            zeeklog_make_uid('F', (const uint8_t *)row->file.md5, strlen(row->file.md5), fuid);
            zeeklog_items_add(items, fuid, ZEEKLOG_UID_LEN);
        }
        break;
    case ZS_FILE_SOURCE:
        zeeklog_items_add_str(items, row->file.source);
        break;
    case ZS_FILE_MIME_TYPE:
        zeeklog_items_add_str(items, row->file.mime);
        break;
    case ZS_FILE_MD5:
        zeeklog_items_add_str(items, row->file.md5);
        break;
    case ZS_FILE_SHA256:
        zeeklog_items_add_str(items, row->file.sha256);
        break;
    case ZS_FILE_FILENAME:
        zeeklog_items_add_str(items, row->file.filename);
        break;
    case ZS_FILE_IS_ORIG:
        if (row->file.isOrig != -1)
            zeeklog_items_add_bool(items, row->file.isOrig);
        break;
    }
}
/******************************************************************************/
LOCAL void zeeklog_column_items(ZeekItems_t *items, const ZeekColumn_t *col, const ZeekRow_t *row, ZeekItems_t *scratch)
{
    zeeklog_items_reset(items);

    for (int s = 0; s < col->numSources; s++) {
        const ZeekSource_t *src = &col->sources[s];
        switch (src->kind) {
        case ZSRC_NONE:
            break;
        case ZSRC_SPECIAL:
            zeeklog_special_items(items, src->special, row, scratch);
            break;
        case ZSRC_FIELD:
            zeeklog_field_items(items, row->session, src->pos, col->base);
            break;
        case ZSRC_CONTAINS:
            zeeklog_items_reset(scratch);
            zeeklog_field_items(scratch, row->session, src->pos, ZB_STRING);
            zeeklog_items_add_bool(items, zeeklog_items_contains(scratch, src->value));
            break;
        }
    }
}
/******************************************************************************/
/* Output formatting */
LOCAL void zeeklog_ascii_escape(BSB *bsb, const char *str, int len, gboolean inSet)
{
    if (len == 0) {
        BSB_EXPORT_cstr(*bsb, "(empty)");
        return;
    }
    if (len == 1 && str[0] == '-') {
        BSB_EXPORT_cstr(*bsb, "\\x2d");
        return;
    }
    if (len == 7 && memcmp(str, "(empty)", 7) == 0) {
        BSB_EXPORT_cstr(*bsb, "\\x28empty)");
        return;
    }

    for (int i = 0; i < len; i++) {
        uint8_t c = str[i];
        if (c == '\\') {
            BSB_EXPORT_cstr(*bsb, "\\\\");
        } else if (c < 0x20 || c == 0x7f || (!zeekUtf8 && c >= 0x80)) {
            BSB_EXPORT_sprintf(*bsb, "\\x%02x", c);
        } else if (inSet && c == ',') {
            BSB_EXPORT_cstr(*bsb, "\\x2c");
        } else {
            BSB_EXPORT_u08(*bsb, c);
        }
    }
}
/******************************************************************************/
LOCAL void zeeklog_ascii_cell(BSB *bsb, const ZeekColumn_t *col, const ZeekItems_t *items)
{
    if (items->num == 0) {
        if (items->present && col->container != ZC_SCALAR)
            BSB_EXPORT_cstr(*bsb, "(empty)");
        else
            BSB_EXPORT_u08(*bsb, '-');
        return;
    }

    gboolean inSet = col->container != ZC_SCALAR || items->num > 1;
    for (int i = 0; i < items->num; i++) {
        if (i > 0)
            BSB_EXPORT_u08(*bsb, ',');
        zeeklog_ascii_escape(bsb, items->items[i], items->lens[i], inSet);
    }
}
/******************************************************************************/
LOCAL gboolean zeeklog_is_number(const char *str)
{
    if (*str == '-')
        str++;
    if (!*str)
        return FALSE;
    for (; *str; str++) {
        if (!isdigit((unsigned char) * str) && *str != '.')
            return FALSE;
    }
    return TRUE;
}
/******************************************************************************/
LOCAL void zeeklog_json_value(BSB *bsb, const ZeekColumn_t *col, const char *item)
{
    switch (col->base) {
    case ZB_BOOL:
        if (item[0] == 'T' || item[0] == 't' || item[0] == '1')
            BSB_EXPORT_cstr(*bsb, "true");
        else
            BSB_EXPORT_cstr(*bsb, "false");
        break;
    case ZB_COUNT:
    case ZB_INT:
    case ZB_DOUBLE:
    case ZB_TIME:
    case ZB_INTERVAL:
    case ZB_PORT:
        if (zeeklog_is_number(item)) {
            BSB_EXPORT_ptr(*bsb, item, strlen(item));
            break;
        }
    // fall through
    default:
        arkime_db_js0n_str(bsb, (uint8_t *)item, zeekUtf8);
        break;
    }
}
/******************************************************************************/
LOCAL void zeeklog_json_cell(BSB *bsb, const ZeekColumn_t *col, const ZeekItems_t *items)
{
    if (items->num == 0 && !(items->present && col->container != ZC_SCALAR))
        return;

    BSB_EXPORT_sprintf(*bsb, "\"%s\":", col->name);
    if (col->container == ZC_SCALAR) {
        if (items->num == 1 || col->base != ZB_STRING) {
            zeeklog_json_value(bsb, col, items->items[0]);
        } else {
            char   joined[ZEEKLOG_ITEM_BUF_SIZE];
            BSB    jbsb;
            BSB_INIT(jbsb, joined, sizeof(joined));
            for (int i = 0; i < items->num; i++) {
                if (i > 0)
                    BSB_EXPORT_u08(jbsb, ',');
                BSB_EXPORT_ptr(jbsb, items->items[i], items->lens[i]);
            }
            BSB_EXPORT_u08(jbsb, 0);
            if (!BSB_IS_ERROR(jbsb))
                arkime_db_js0n_str(bsb, (uint8_t *)joined, zeekUtf8);
            else
                BSB_EXPORT_cstr(*bsb, "null");
        }
    } else {
        BSB_EXPORT_u08(*bsb, '[');
        for (int i = 0; i < items->num; i++) {
            if (i > 0)
                BSB_EXPORT_u08(*bsb, ',');
            zeeklog_json_value(bsb, col, items->items[i]);
        }
        BSB_EXPORT_u08(*bsb, ']');
    }
    BSB_EXPORT_u08(*bsb, ',');
}
/******************************************************************************/
/* File handling, always called with log->lock held */
LOCAL void zeeklog_timestamp(time_t t, char *buf, int len)
{
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, len, "%Y-%m-%d-%H-%M-%S", &tm);
}
/******************************************************************************/
LOCAL void zeeklog_write_header(ZeekLog_t *log)
{
    if (zeekJson)
        return;

    char stamp[32];
    zeeklog_timestamp(log->openTime, stamp, sizeof(stamp));

    fprintf(log->fp, "#separator \\x09\n#set_separator\t,\n#empty_field\t(empty)\n#unset_field\t-\n#path\t%s\n#open\t%s\n", log->path, stamp);

    fputs("#fields", log->fp);
    for (guint i = 0; i < log->columns->len; i++) {
        ZeekColumn_t *col = g_ptr_array_index(log->columns, i);
        fprintf(log->fp, "\t%s", col->name);
    }
    fputs("\n#types", log->fp);
    for (guint i = 0; i < log->columns->len; i++) {
        ZeekColumn_t *col = g_ptr_array_index(log->columns, i);
        fprintf(log->fp, "\t%s", col->typeName);
    }
    fputs("\n", log->fp);
}
/******************************************************************************/
LOCAL void zeeklog_close(ZeekLog_t *log, gboolean rotate)
{
    if (!log->fp)
        return;

    if (!zeekJson) {
        char stamp[32];
        zeeklog_timestamp(time(NULL), stamp, sizeof(stamp));
        fprintf(log->fp, "#close\t%s\n", stamp);
    }
    fclose(log->fp);
    log->fp = NULL;

    if (rotate) {
        char stamp[32];
        zeeklog_timestamp(log->openTime, stamp, sizeof(stamp));
        char *rotated = g_strdup_printf("%s/%s.%s.log", zeekLogDir, log->path, stamp);
        if (rename(log->filename, rotated) != 0) {
            LOG("ERROR - Couldn't rotate %s to %s: %s", log->filename, rotated, strerror(errno));
        }
        g_free(rotated);
    }
}
/******************************************************************************/
LOCAL gboolean zeeklog_open(ZeekLog_t *log)
{
    if (log->fp)
        return TRUE;

    struct stat st;
    if (stat(log->filename, &st) == 0 && st.st_size > 0) {
        char stamp[32];
        zeeklog_timestamp(st.st_mtime, stamp, sizeof(stamp));
        char *leftover = g_strdup_printf("%s/%s.%s.log", zeekLogDir, log->path, stamp);
        if (rename(log->filename, leftover) != 0) {
            LOG("ERROR - Couldn't move leftover %s to %s: %s", log->filename, leftover, strerror(errno));
        }
        g_free(leftover);
    }

    log->fp = fopen(log->filename, "w");
    if (!log->fp) {
        LOG("ERROR - Couldn't open %s: %s", log->filename, strerror(errno));
        return FALSE;
    }
    setvbuf(log->fp, NULL, _IOFBF, 0x10000);
    log->openTime = time(NULL);
    zeeklog_write_header(log);
    return TRUE;
}
/******************************************************************************/
LOCAL void zeeklog_write_row(ZeekLog_t *log, ZeekRow_t *row)
{
    ZeekThread_t *td = zeekThreads[row->session->thread];
    BSB           bsb;

    BSB_INIT(bsb, td->row, sizeof(td->row));

    if (zeekJson)
        BSB_EXPORT_u08(bsb, '{');

    for (guint i = 0; i < log->columns->len; i++) {
        ZeekColumn_t *col = g_ptr_array_index(log->columns, i);
        zeeklog_column_items(&td->items, col, row, &td->scratch);
        if (zeekJson) {
            zeeklog_json_cell(&bsb, col, &td->items);
        } else {
            if (i > 0)
                BSB_EXPORT_u08(bsb, '\t');
            zeeklog_ascii_cell(&bsb, col, &td->items);
        }
    }

    if (zeekJson) {
        if (BSB_LENGTH(bsb) > 1)
            BSB_EXPORT_rewind(bsb, 1);
        BSB_EXPORT_u08(bsb, '}');
    }
    BSB_EXPORT_u08(bsb, '\n');

    if (BSB_IS_ERROR(bsb)) {
        if (log->dropped++ == 0)
            LOG("WARNING - zeeklog row too long for %s.log, dropping", log->path);
        return;
    }

    pthread_mutex_lock(&log->lock);
    if (zeeklog_open(log)) {
        fwrite(td->row, 1, BSB_LENGTH(bsb), log->fp);
        log->rows++;
    }
    pthread_mutex_unlock(&log->lock);
}
/******************************************************************************/
LOCAL gboolean zeeklog_log_matches(const ZeekLog_t *log, ArkimeSession_t *session)
{
    if (!log->protocols)
        return TRUE;
    for (int i = 0; log->protocols[i]; i++) {
        if (arkime_session_has_protocol(session, log->protocols[i]))
            return TRUE;
    }
    return FALSE;
}
/******************************************************************************/
LOCAL const char *zeeklog_single_item(ZeekItems_t *scratch, const ArkimeSession_t *session, int pos)
{
    zeeklog_items_reset(scratch);
    zeeklog_field_items(scratch, session, pos, ZB_STRING);
    return scratch->num == 1 ? scratch->items[0] : NULL;
}
/******************************************************************************/
LOCAL void zeeklog_plugin_save(ArkimeSession_t *session, int UNUSED(final))
{
    ZeekRow_t row;

    memset(&row, 0, sizeof(row));
    row.session = session;
    row.file.isOrig = -1;
    zeeklog_make_uid('C', session->sessionId, session->sessionId[0], row.uid);

    for (guint l = 0; l < zeekLogs->len; l++) {
        ZeekLog_t *log = g_ptr_array_index(zeekLogs, l);
        if (!log->enabled || !zeeklog_log_matches(log, session))
            continue;

        switch (log->iter) {
        case ZITER_SESSION:
            zeeklog_write_row(log, &row);
            break;
        case ZITER_DNS: {
            const ArkimeField_t *field = zeeklog_session_field(session, dnsField);
            if (!field)
                break;
            ArkimeFieldObject_t *object;
            HASH_FORALL2(o_, *field->ohash, object) {
                row.dns = object->object;
                if (row.dns)
                    zeeklog_write_row(log, &row);
            }
            row.dns = NULL;
            break;
        }
        case ZITER_CERT: {
            const ArkimeField_t *field = zeeklog_session_field(session, certsField);
            if (!field)
                break;
            ArkimeFieldObject_t *object;
            HASH_FORALL2(o_, *field->ohash, object) {
                row.cert = object->object;
                if (row.cert)
                    zeeklog_write_row(log, &row);
            }
            row.cert = NULL;
            break;
        }
        case ZITER_FILE: {
            ZeekThread_t *td = zeekThreads[session->thread];
            for (int f = 0; f < ARRAY_LEN(zeekFileSources); f++) {
                ZeekFileSource_t *fs = &zeekFileSources[f];
                zeeklog_items_reset(&td->scratch);
                if (!zeeklog_field_items(&td->scratch, session, fs->md5Pos, ZB_STRING) || td->scratch.num == 0)
                    continue;

                // Snapshot, the scratch buffer is reused while writing rows
                int    num = td->scratch.num;
                char **md5s = g_new0(char *, num + 1);
                for (int i = 0; i < num; i++) {
                    md5s[i] = g_strdup(td->scratch.items[i]);
                }

                row.file.source   = fs->source;
                row.file.isOrig   = fs->isOrig;
                row.file.sha256   = num == 1 ? g_strdup(zeeklog_single_item(&td->scratch, session, fs->sha256Pos)) : NULL;
                row.file.mime     = num == 1 ? g_strdup(zeeklog_single_item(&td->scratch, session, fs->mimePos)) : NULL;
                row.file.filename = num == 1 ? g_strdup(zeeklog_single_item(&td->scratch, session, fs->filenamePos)) : NULL;

                for (int i = 0; i < num; i++) {
                    row.file.md5 = md5s[i];
                    zeeklog_write_row(log, &row);
                }
                g_strfreev(md5s);
                g_free((char *)row.file.sha256);
                g_free((char *)row.file.mime);
                g_free((char *)row.file.filename);
                memset(&row.file, 0, sizeof(row.file));
                row.file.isOrig = -1;
            }
            break;
        }
        }
    }
}
/******************************************************************************/
LOCAL gboolean zeeklog_timer(gpointer UNUSED(user_data))
{
    time_t now = time(NULL);

    for (guint l = 0; l < zeekLogs->len; l++) {
        ZeekLog_t *log = g_ptr_array_index(zeekLogs, l);
        pthread_mutex_lock(&log->lock);
        if (log->fp) {
            if (zeekRotateSeconds && now - log->openTime >= zeekRotateSeconds)
                zeeklog_close(log, TRUE);
            else
                fflush(log->fp);
        }
        pthread_mutex_unlock(&log->lock);
    }
    return G_SOURCE_CONTINUE;
}
/******************************************************************************/
LOCAL void zeeklog_plugin_exit()
{
    for (guint l = 0; l < zeekLogs->len; l++) {
        ZeekLog_t *log = g_ptr_array_index(zeekLogs, l);
        pthread_mutex_lock(&log->lock);
        zeeklog_close(log, FALSE);
        pthread_mutex_unlock(&log->lock);
        if (config.debug && log->rows)
            LOG("zeeklog %s.log rows=%" PRIu64 " dropped=%" PRIu64, log->path, log->rows, log->dropped);
    }
}
/******************************************************************************/
/* Schema construction */
LOCAL gboolean zeeklog_parse_type(const char *str, ZeekBase_t *base, ZeekContainer_t *container)
{
    char  buf[64];
    char *inner = NULL;

    g_strlcpy(buf, str, sizeof(buf));
    g_strstrip(buf);

    if (g_str_has_prefix(buf, "vector[") || g_str_has_prefix(buf, "set[")) {
        char *end = strchr(buf, ']');
        if (!end)
            return FALSE;
        *end = 0;
        *container = buf[0] == 'v' ? ZC_VECTOR : ZC_SET;
        inner = strchr(buf, '[') + 1;
    } else {
        *container = ZC_SCALAR;
        inner = buf;
    }

    for (int i = 0; i < ARRAY_LEN(zeekBaseNames); i++) {
        if (strcmp(inner, zeekBaseNames[i]) == 0) {
            *base = i;
            return TRUE;
        }
    }
    return FALSE;
}
/******************************************************************************/
LOCAL char *zeeklog_type_name(ZeekBase_t base, ZeekContainer_t container, const char *explicitName)
{
    if (explicitName) {
        char *name = g_strdup(explicitName);
        g_strstrip(name);
        return name;
    }
    switch (container) {
    case ZC_VECTOR:
        return g_strdup_printf("vector[%s]", zeekBaseNames[base]);
    case ZC_SET:
        return g_strdup_printf("set[%s]", zeekBaseNames[base]);
    default:
        return g_strdup(zeekBaseNames[base]);
    }
}
/******************************************************************************/
LOCAL int zeeklog_special_find(const char *name)
{
    for (int i = 0; i < ZS_MAX; i++) {
        if (zeekSpecials[i].name && strcmp(zeekSpecials[i].name, name) == 0)
            return i;
    }
    return -1;
}
/******************************************************************************/
LOCAL void zeeklog_infer_field_type(int pos, ZeekBase_t *base, ZeekContainer_t *container)
{
    *base = ZB_STRING;
    *container = ZC_SCALAR;
    if (pos < 0)
        return;

    switch (config.fields[pos]->type) {
    case ARKIME_FIELD_TYPE_INT:
        *base = ZB_COUNT;
        break;
    case ARKIME_FIELD_TYPE_INT_ARRAY:
    case ARKIME_FIELD_TYPE_INT_ARRAY_UNIQUE:
        *base = ZB_COUNT;
        *container = ZC_VECTOR;
        break;
    case ARKIME_FIELD_TYPE_INT_HASH:
    case ARKIME_FIELD_TYPE_INT_GHASH:
        *base = ZB_COUNT;
        *container = ZC_SET;
        break;
    case ARKIME_FIELD_TYPE_STR:
        break;
    case ARKIME_FIELD_TYPE_STR_ARRAY:
        *container = ZC_VECTOR;
        break;
    case ARKIME_FIELD_TYPE_STR_HASH:
    case ARKIME_FIELD_TYPE_STR_GHASH:
        *container = ZC_SET;
        break;
    case ARKIME_FIELD_TYPE_IP:
        *base = ZB_ADDR;
        break;
    case ARKIME_FIELD_TYPE_IP_GHASH:
        *base = ZB_ADDR;
        *container = ZC_SET;
        break;
    case ARKIME_FIELD_TYPE_FLOAT:
        *base = ZB_DOUBLE;
        break;
    case ARKIME_FIELD_TYPE_FLOAT_ARRAY:
        *base = ZB_DOUBLE;
        *container = ZC_VECTOR;
        break;
    case ARKIME_FIELD_TYPE_FLOAT_GHASH:
        *base = ZB_DOUBLE;
        *container = ZC_SET;
        break;
    case ARKIME_FIELD_TYPE_OBJECT:
        break;
    }
}
/******************************************************************************/
LOCAL int zeeklog_field_lookup(const char *expression, const char *where)
{
    int pos = arkime_field_by_exp_ignore_error(expression);
    if (pos < 0) {
        if (config.debug)
            LOG("zeeklog %s: unknown Arkime field '%s', column will always be unset", where, expression);
        return -1;
    }
    if (pos >= config.minInternalField || config.fields[pos]->getCb || (config.fields[pos]->flags & ARKIME_FIELD_FLAG_FAKE)) {
        LOG("WARNING - zeeklog %s: field '%s' is not stored on sessions, column will always be unset", where, expression);
        return -1;
    }
    return pos;
}
/******************************************************************************/
LOCAL void zeeklog_column_free(ZeekColumn_t *col)
{
    for (int i = 0; i < col->numSources; i++) {
        g_free(col->sources[i].value);
    }
    g_free(col->name);
    g_free(col->typeName);
    g_free(col->spec);
    ARKIME_TYPE_FREE(ZeekColumn_t, col);
}
/******************************************************************************/
LOCAL ZeekColumn_t *zeeklog_column_find(const ZeekLog_t *log, const char *name, guint *index)
{
    for (guint i = 0; i < log->columns->len; i++) {
        ZeekColumn_t *col = g_ptr_array_index(log->columns, i);
        if (strcmp(col->name, name) == 0) {
            if (index)
                *index = i;
            return col;
        }
    }
    return NULL;
}
/******************************************************************************/
/* spec is "source[+source...][:type]" where source is @special, @none,
 * an Arkime field expression, or expression?VALUE for a boolean contains test */
LOCAL gboolean zeeklog_column_set(ZeekLog_t *log, const char *name, const char *spec)
{
    char *where = g_strdup_printf("%s.log column %s", log->path, name);
    char *s = g_strdup(spec);
    g_strstrip(s);

    ZeekColumn_t *col = ARKIME_TYPE_ALLOC0(ZeekColumn_t);
    col->name = g_strdup(name);
    col->spec = g_strdup(s);

    gboolean    haveType = FALSE;
    const char *explicitType = NULL;
    char       *colon = strrchr(s, ':');
    if (colon && zeeklog_parse_type(colon + 1, &col->base, &col->container)) {
        *colon = 0;
        explicitType = colon + 1;
        haveType = TRUE;
    }

    char **parts = g_strsplit(s, "+", ZEEKLOG_MAX_SOURCES);
    for (int p = 0; parts[p]; p++) {
        char *part = g_strstrip(parts[p]);
        if (!*part)
            continue;

        ZeekSource_t    *src = &col->sources[col->numSources];
        ZeekBase_t       base = ZB_STRING;
        ZeekContainer_t  container = ZC_SCALAR;

        if (strcmp(part, "@none") == 0) {
            src->kind = ZSRC_NONE;
        } else if (part[0] == '@') {
            int special = zeeklog_special_find(part + 1);
            if (special < 0) {
                LOG("ERROR - zeeklog %s: unknown special '%s'", where, part);
                g_strfreev(parts);
                g_free(s);
                g_free(where);
                zeeklog_column_free(col);
                return FALSE;
            }
            src->kind = ZSRC_SPECIAL;
            src->special = special;
            base = zeekSpecials[special].base;
            container = zeekSpecials[special].container;
        } else {
            char *q = strchr(part, '?');
            if (q) {
                *q = 0;
                src->kind = ZSRC_CONTAINS;
                src->value = g_strdup(q + 1);
                base = ZB_BOOL;
            } else {
                src->kind = ZSRC_FIELD;
            }
            src->pos = zeeklog_field_lookup(part, where);
            if (src->pos < 0) {
                g_free(src->value);
                src->value = NULL;
                src->kind = ZSRC_NONE;
            } else if (!q) {
                zeeklog_infer_field_type(src->pos, &base, &container);
            }
        }

        if (!haveType && src->kind != ZSRC_NONE) {
            col->base = base;
            col->container = container;
            haveType = TRUE;
        }
        col->numSources++;
    }
    g_strfreev(parts);

    if (!haveType) {
        col->base = ZB_STRING;
        col->container = ZC_SCALAR;
    }
    col->typeName = zeeklog_type_name(col->base, col->container, explicitType);

    guint         index;
    ZeekColumn_t *old = zeeklog_column_find(log, name, &index);
    if (old) {
        zeeklog_column_free(old);
        g_ptr_array_index(log->columns, index) = col;
    } else {
        g_ptr_array_add(log->columns, col);
    }

    g_free(s);
    g_free(where);
    return TRUE;
}
/******************************************************************************/
LOCAL void zeeklog_apply_spec(ZeekLog_t *log, const char *spec)
{
    char **items = g_strsplit_set(spec, " \t\n", 0);
    for (int i = 0; items[i]; i++) {
        if (!*items[i])
            continue;
        char *eq = strchr(items[i], '=');
        if (!eq)
            CONFIGEXIT("zeeklog %s.log: bad column spec '%s'", log->path, items[i]);
        *eq = 0;
        if (!zeeklog_column_set(log, items[i], eq + 1))
            CONFIGEXIT("zeeklog %s.log: bad column spec '%s=%s'", log->path, items[i], eq + 1);
    }
    g_strfreev(items);
}
/******************************************************************************/
LOCAL void zeeklog_set_protocols(ZeekLog_t *log, const char *protocols)
{
    g_strfreev(log->protocols);
    log->protocols = NULL;
    if (!protocols || !*protocols || strcmp(protocols, "*") == 0)
        return;

    log->protocols = g_strsplit_set(protocols, ",; ", 0);
    for (int i = 0; log->protocols[i]; i++) {
        g_strstrip(log->protocols[i]);
    }
}
/******************************************************************************/
LOCAL ZeekLog_t *zeeklog_log_find(const char *path)
{
    for (guint i = 0; i < zeekLogs->len; i++) {
        ZeekLog_t *log = g_ptr_array_index(zeekLogs, i);
        if (strcmp(log->path, path) == 0)
            return log;
    }
    return NULL;
}
/******************************************************************************/
LOCAL ZeekLog_t *zeeklog_log_create(const char *path, const ZeekBuiltin_t *builtin)
{
    ZeekLog_t *log = ARKIME_TYPE_ALLOC0(ZeekLog_t);
    log->path     = g_strdup(path);
    log->filename = g_strdup_printf("%s/%s.log", zeekLogDir, path);
    log->columns  = g_ptr_array_new();
    log->enabled  = TRUE;
    log->builtin  = builtin != NULL;
    pthread_mutex_init(&log->lock, NULL);

    if (builtin) {
        log->iter = builtin->iter;
        zeeklog_set_protocols(log, builtin->protocols);
        zeeklog_apply_spec(log, builtin->spec);
    }
    g_ptr_array_add(zeekLogs, log);
    return log;
}
/******************************************************************************/
LOCAL void zeeklog_filter_columns(ZeekLog_t *log, const char *list, gboolean include)
{
    char **names = g_strsplit_set(list, ", ", 0);
    GPtrArray *keep = g_ptr_array_new();

    if (include) {
        for (int i = 0; names[i]; i++) {
            g_strstrip(names[i]);
            if (!*names[i])
                continue;
            guint         index;
            ZeekColumn_t *col = zeeklog_column_find(log, names[i], &index);
            if (!col)
                CONFIGEXIT("zeeklog %s.log include: unknown column '%s'", log->path, names[i]);
            g_ptr_array_add(keep, col);
            g_ptr_array_remove_index(log->columns, index);
        }
        for (guint i = 0; i < log->columns->len; i++) {
            zeeklog_column_free(g_ptr_array_index(log->columns, i));
        }
    } else {
        for (guint i = 0; i < log->columns->len; i++) {
            ZeekColumn_t *col = g_ptr_array_index(log->columns, i);
            gboolean      drop = FALSE;
            for (int n = 0; names[n] && !drop; n++) {
                drop = strcmp(g_strstrip(names[n]), col->name) == 0;
            }
            if (drop)
                zeeklog_column_free(col);
            else
                g_ptr_array_add(keep, col);
        }
    }
    g_ptr_array_free(log->columns, TRUE);
    log->columns = keep;
    g_strfreev(names);
}
/******************************************************************************/
LOCAL void zeeklog_apply_section(ZeekLog_t *log, const char *section)
{
    gsize   keysLen = 0;
    gchar **keys = arkime_config_section_keys(NULL, section, &keysLen);
    if (!keys)
        return;

    for (gsize k = 0; k < keysLen; k++) {
        gchar *value = arkime_config_section_str(NULL, section, keys[k], NULL);
        if (!value)
            continue;
        g_strstrip(value);

        if (strcmp(keys[k], "enabled") == 0) {
            log->enabled = strcasecmp(value, "false") != 0 && strcmp(value, "0") != 0;
        } else if (strcmp(keys[k], "protocols") == 0) {
            zeeklog_set_protocols(log, value);
        } else if (strcmp(keys[k], "iterate") == 0) {
            if (strcmp(value, "session") == 0)
                log->iter = ZITER_SESSION;
            else if (strcmp(value, "dns") == 0)
                log->iter = ZITER_DNS;
            else if (strcmp(value, "cert") == 0)
                log->iter = ZITER_CERT;
            else if (strcmp(value, "file") == 0)
                log->iter = ZITER_FILE;
            else
                CONFIGEXIT("[%s] iterate must be session, dns, cert or file", section);
        } else if (strcmp(keys[k], "include") == 0) {
            zeeklog_filter_columns(log, value, TRUE);
        } else if (strcmp(keys[k], "exclude") == 0) {
            zeeklog_filter_columns(log, value, FALSE);
        } else if (!zeeklog_column_set(log, keys[k], value)) {
            CONFIGEXIT("[%s] bad column spec '%s=%s'", section, keys[k], value);
        }
        g_free(value);
    }
    g_strfreev(keys);
}
/******************************************************************************/
LOCAL void zeeklog_dump_schema()
{
    for (guint l = 0; l < zeekLogs->len; l++) {
        ZeekLog_t *log = g_ptr_array_index(zeekLogs, l);
        LOG("zeeklog %s.log enabled=%d iterate=%d", log->path, log->enabled, log->iter);
        for (guint i = 0; i < log->columns->len; i++) {
            ZeekColumn_t *col = g_ptr_array_index(log->columns, i);
            LOG("    %-28s %-18s %s", col->name, col->typeName, col->spec);
        }
    }
}
/******************************************************************************/
void arkime_plugin_init()
{
    arkime_plugins_register("zeeklog", FALSE);

    zeekLogs = g_ptr_array_new();

    zeekLogDir = arkime_config_str(NULL, "zeekLogDir", ".");
    if (g_mkdir_with_parents(zeekLogDir, 0755) != 0)
        CONFIGEXIT("zeekLogDir %s couldn't be created: %s", zeekLogDir, strerror(errno));

    char *format = arkime_config_str(NULL, "zeekLogFormat", "ascii");
    if (strcasecmp(format, "json") == 0)
        zeekJson = TRUE;
    else if (strcasecmp(format, "ascii") != 0)
        CONFIGEXIT("zeekLogFormat must be ascii or json");
    g_free(format);

    zeekUtf8 = arkime_config_boolean(NULL, "zeekLogUtf8", TRUE);
    zeekRotateSeconds = arkime_config_int(NULL, "zeekLogRotateSeconds", 0, 0, 0x7fffffff);

    protocolField        = arkime_field_by_exp("protocols");
    certsField           = arkime_field_by_db("cert");
    dnsField             = arkime_field_by_db("dns");
    tlsSrcSessionIdField = arkime_field_by_exp_ignore_error("tls.sessionid.src");
    tlsDstSessionIdField = arkime_field_by_exp_ignore_error("tls.sessionid.dst");
    tlsCipherField       = arkime_field_by_exp_ignore_error("tls.cipher");
    tlsVersionField      = arkime_field_by_exp_ignore_error("tls.version");
    httpHostField        = arkime_field_by_exp_ignore_error("host.http");
    sshVerField          = arkime_field_by_exp_ignore_error("ssh.ver");
    sshKeyField          = arkime_field_by_exp_ignore_error("ssh.key");
    vlanField            = arkime_field_by_exp_ignore_error("vlan");
    krbCnameField        = arkime_field_by_exp_ignore_error("krb5.cname");
    krbRealmField        = arkime_field_by_exp_ignore_error("krb5.realm");

    for (int f = 0; f < ARRAY_LEN(zeekFileSources); f++) {
        ZeekFileSource_t *fs = &zeekFileSources[f];
        fs->md5Pos      = arkime_field_by_exp_ignore_error(fs->md5);
        fs->sha256Pos   = fs->sha256 ? arkime_field_by_exp_ignore_error(fs->sha256) : -1;
        fs->mimePos     = fs->mime ? arkime_field_by_exp_ignore_error(fs->mime) : -1;
        fs->filenamePos = fs->filename ? arkime_field_by_exp_ignore_error(fs->filename) : -1;
    }

    char **names = arkime_config_str_list(NULL, "zeekLogs", "*");
    for (int i = 0; names[i]; i++) {
        g_strstrip(names[i]);
        if (!*names[i])
            continue;
        if (strcmp(names[i], "*") == 0) {
            for (int b = 0; b < ARRAY_LEN(zeekBuiltins); b++) {
                if (!zeeklog_log_find(zeekBuiltins[b].path))
                    zeeklog_log_create(zeekBuiltins[b].path, &zeekBuiltins[b]);
            }
            continue;
        }
        if (zeeklog_log_find(names[i]))
            continue;
        const ZeekBuiltin_t *builtin = NULL;
        for (int b = 0; b < ARRAY_LEN(zeekBuiltins); b++) {
            if (strcmp(zeekBuiltins[b].path, names[i]) == 0)
                builtin = &zeekBuiltins[b];
        }
        zeeklog_log_create(names[i], builtin);
    }
    g_strfreev(names);

    for (guint l = 0; l < zeekLogs->len; l++) {
        ZeekLog_t *log = g_ptr_array_index(zeekLogs, l);
        char *section = g_strdup_printf("zeeklog-%s", log->path);
        zeeklog_apply_section(log, section);
        g_free(section);
        if (log->columns->len == 0)
            CONFIGEXIT("zeeklog %s.log has no columns, define them in a [zeeklog-%s] section", log->path, log->path);
        if (!log->builtin && !log->protocols)
            LOG("WARNING - zeeklog %s.log has no protocols setting, every session will be logged", log->path);
    }

    for (int t = 0; t < config.packetThreads; t++) {
        zeekThreads[t] = g_malloc0(sizeof(ZeekThread_t));
    }

    if (config.debug)
        zeeklog_dump_schema();

    arkime_plugins_set_cb("zeeklog",
                          NULL,
                          NULL,
                          NULL,
                          NULL,
                          zeeklog_plugin_save,
                          NULL,
                          zeeklog_plugin_exit,
                          NULL
                         );

    g_timeout_add_seconds(1, zeeklog_timer, 0);
}
