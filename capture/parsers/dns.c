/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <arpa/inet.h>

//#define DNSDEBUG 1

#define MAX_QTYPES   512
#define MAX_QCLASSES 256
#define MAX_IPS 2000

#define DEFAULT_JSON_LEN 200
#define HOST_IP_JSON_LEN 250
#define ANSWER_JSON_LEN 200

#define FNV_OFFSET ((uint32_t)0x811c9dc5)
#define FNV_PRIME ((uint32_t)0x01000193)

LOCAL  char                 *qclasses[MAX_QCLASSES];
LOCAL  char                 *qtypes[MAX_QTYPES];
LOCAL  char                 *rcodes[24] = {"NOERROR", "FORMERR", "SERVFAIL", "NXDOMAIN", "NOTIMPL", "REFUSED", "YXDOMAIN", "YXRRSET", "NXRRSET", "NOTAUTH", "NOTZONE", "DSOTYPENI", "12", "13", "14", "15", "BADSIG_VERS", "BADKEY", "BADTIME", "BADMODE", "BADNAME", "BADALG", "BADTRUNC", "BADCOOKIE"};
LOCAL  char                 *opcodes[16] = {"QUERY", "IQUERY", "STATUS", "3", "NOTIFY", "UPDATE", "DSO Message", "7", "8", "9", "10", "11", "12", "13", "14", "15"};
LOCAL  char                 *flags[7] = {"AA", "TC", "RD", "RA", "AD", "CD", "DO"};

typedef enum dns_type {
    DNS_RR_A          =   1,
    DNS_RR_NS         =   2,
    DNS_RR_CNAME      =   5,
    DNS_RR_MX         =  15,
    DNS_RR_TXT        =  16,
    DNS_RR_AAAA       =  28,
    DNS_RR_HTTPS      =  65,
    DNS_RR_CAA        = 257
} DNSType_t;

typedef enum dns_class {
    CLASS_IN      =     1,
    CLASS_CS      =     2,
    CLASS_CH      =     3,
    CLASS_HS      =     4,
    CLASS_NONE    =   254,
    CLASS_ANY     =   255,
    CLASS_UNKNOWN = 65280
} DNSClass_t;

typedef enum dns_svcb_param_key {
    SVCB_PARAM_KEY_ALPN      = 1,
    SVCB_PARAM_KEY_PORT      = 3,
    SVCB_PARAM_KEY_IPV4_HINT = 4,
    SVCB_PARAM_KEY_IPV6_HINT = 6
} DNSSVCBParamKey_t;

typedef enum dns_result_record_type {
    RESULT_RECORD_ANSWER          =     1,    /* Answer or Prerequisites Record */
    RESULT_RECORD_AUTHORITATIVE   =     2,    /* Authoritative or Update Record */
    RESULT_RECORD_ADDITIONAL      =     3,    /* Additional Record */
    RESULT_RECORD_UNKNOWN         =     4,    /* Unknown Record*/
} DNSResultRecordType_t;

typedef struct dns_answer_mxrdata {
    uint16_t preference;
    char    *exchange;
} DNSMXRData_t;

typedef struct dns_answer_svcbrdata_field_value {
    struct dns_answer_svcbrdata_field_value  *t_next, *t_prev;
    DNSSVCBParamKey_t                         key;
    void                                     *value;
} DNSSVCBRDataFieldValue_t;

typedef struct {
    struct dns_answer_svcbrdata_field_value  *t_next, *t_prev;
    int                                       t_count;
} DNSSVCBRDataFieldValueHead_t;

typedef struct dns_answer_svcbrdata {
    uint16_t                       priority;
    char                          *dname;
    DNSSVCBRDataFieldValueHead_t   fieldValues;
} DNSSVCBRData_t ;

typedef struct dns_answer_caadata {
    char *tag;
    char *value;
    uint8_t flags;
} DNSCAARData_t;

typedef struct dns_answer {
    struct dns_answer  *t_next, *t_prev;
    ArkimeStringHead_t  flags;
    union {
        char            *cname;
        DNSMXRData_t    *mx;
        char            *nsdname;
        uint32_t         ipA;
        struct in6_addr *ipAAAA;
        char            *txt;
        DNSCAARData_t   *caa;
        DNSSVCBRData_t  *svcb;
    };
    uint16_t             packet_uid;
    char                *class;
    char                *type;
    char                *name;
    uint16_t             type_id; // Only used for choosing correct RDATA in union
    uint32_t             ttl;
} DNSAnswer_t;

typedef struct {
    struct dns_answer *t_next, *t_prev;
    int                t_count;
} DNSAnswerHead_t;

typedef struct {
    uint8_t  opcode_id;
    uint16_t type_id;
    uint16_t class_id;
    char    *opcode;
    char    *hostname;
    uint16_t packet_uid;
    char    *class;
    char    *type;
} DNSQuery_t;

typedef struct dns {
    struct dns            *t_next, *t_prev;
    uint32_t               t_hash;
    short                  t_bucket;
    DNSAnswerHead_t        answers;
    DNSQuery_t             query;
    ArkimeStringHashStd_t  hosts;
    ArkimeStringHashStd_t *nsHosts;
    ArkimeStringHashStd_t *mxHosts;
    ArkimeStringHashStd_t *punyHosts;
    GHashTable            *ips;
    GHashTable            *nsIPs;
    GHashTable            *mxIPs;
    char                  *rcode;
    int8_t                 rcode_id;
} DNS_t;

typedef struct {
    struct dns *t_next, *t_prev;
    int         t_count;
} DNSHead_t;

typedef HASH_VAR(t_, DNSHash_t, DNSHead_t, 1);
typedef HASH_VAR(t_, DNSHashStd_t, DNSHead_t, 10);

typedef struct {
    uint8_t            *data[2];
    uint16_t            size[2];
    uint16_t            pos[2];
    uint16_t            len[2];
} DNSInfo_t;

extern ArkimeConfig_t        config;
LOCAL  int                   dnsField;
LOCAL  int                   dnsHostField;
LOCAL  int                   dnsHostMailserverField;
LOCAL  int                   dnsHostNameserverField;
LOCAL  int                   dnsPunyField;
LOCAL  int                   dnsStatusField;
LOCAL  int                   dnsOpcodeField;
LOCAL  int                   dnsQueryTypeField;
LOCAL  int                   dnsQueryClassField;
LOCAL  int                   dnsQueryHostField;

LOCAL  char                  dnsOutputAnswers;

// forward declarations
void dns_save(BSB *jbsb, ArkimeFieldObject_t *object, struct arkime_session *session);
void dns_free_object(ArkimeFieldObject_t *object);
uint32_t dns_hash(const void *key);
int dns_cmp(const void *keyv, const void *elementv);

/******************************************************************************/
LOCAL void dns_free(ArkimeSession_t *UNUSED(session), void *uw)
{
    DNSInfo_t            *info          = uw;

    if (info->data[0])
        free(info->data[0]);
    if (info->data[1])
        free(info->data[1]);
    ARKIME_TYPE_FREE(DNSInfo_t, info);
}
/******************************************************************************/
LOCAL int dns_name_element(BSB *nbsb, BSB *bsb)
{
    int nlen = 0;
    BSB_IMPORT_u08(*bsb, nlen);

    if (nlen == 0 || nlen > BSB_REMAINING(*bsb)) {
        return 1;
    }

    int j;
    for (j = 0; j < nlen; j++) {
        register u_char c = 0;
        BSB_IMPORT_u08(*bsb, c);

        if (!isascii(c)) {
            BSB_EXPORT_u08(*nbsb, 'M');
            BSB_EXPORT_u08(*nbsb, '-');
            c = toascii(c);
        }
        if (!isprint(c)) {
            BSB_EXPORT_u08(*nbsb, '^');
            c ^= 0x40;
        }

        BSB_EXPORT_u08(*nbsb, c);
    }

    return 0;
}
/******************************************************************************/
LOCAL char *dns_name(const uint8_t *full, int fulllen, BSB *inbsb, char *name, int *namelen)
{
    BSB  nbsb;
    int  didPointer = 0;
    BSB  tmpbsb;
    BSB *curbsb;

    BSB_INIT(nbsb, name, *namelen);

    curbsb = inbsb;

    while (BSB_REMAINING(*curbsb)) {
        uint8_t ch = 0;
        BSB_IMPORT_u08(*curbsb, ch);

        if (ch == 0)
            break;

        BSB_EXPORT_rewind(*curbsb, 1);

        if (ch & 0xc0) {
            if (didPointer > 5)
                return 0;
            didPointer++;
            int tpos = 0;
            BSB_IMPORT_u16(*curbsb, tpos);
            tpos &= 0x3fff;

            BSB_INIT(tmpbsb, full + tpos, fulllen - tpos);
            curbsb = &tmpbsb;
            continue;
        }

        if (BSB_LENGTH(nbsb)) {
            BSB_EXPORT_u08(nbsb, '.');
        }

        if (dns_name_element(&nbsb, curbsb) && BSB_LENGTH(nbsb))
            BSB_EXPORT_rewind(nbsb, 1); // Remove last .
    }
    *namelen = BSB_LENGTH(nbsb);
    BSB_EXPORT_u08(nbsb, 0);
    return name;
}
/******************************************************************************/
LOCAL void dns_parser_rr_svcb(DNSSVCBRData_t *svcbData, const uint8_t *data, int length)
{
    if (length < 10)
        return;

    BSB bsb;
    BSB_INIT(bsb, data, length);

    BSB_IMPORT_u16(bsb, svcbData->priority);

    char namebuf[8000];
    int namelen = sizeof(namebuf);
    char *name = dns_name(data, length, &bsb, namebuf, &namelen);

    if (BSB_IS_ERROR(bsb) || !name) {
        ARKIME_TYPE_FREE(DNSSVCBRData_t, svcbData);
        return;
    }

    if (!namelen) {
        svcbData->dname = (char *)"<root>";
        namelen = 6;
    } else {
        svcbData->dname = g_hostname_to_unicode(name);
    }

    DLL_INIT(t_, &(svcbData->fieldValues));

    while (BSB_REMAINING(bsb) > 4 && !BSB_IS_ERROR(bsb)) {
        uint16_t key = 0;
        BSB_IMPORT_u16(bsb, key);
        uint16_t len = 0;
        BSB_IMPORT_u16(bsb, len);

        if (len > BSB_REMAINING(bsb))
            return;

        DNSSVCBRDataFieldValue_t *fieldValue = ARKIME_TYPE_ALLOC0(DNSSVCBRDataFieldValue_t);

        uint8_t *ptr = BSB_WORK_PTR(bsb);

        switch (key) {
        case SVCB_PARAM_KEY_ALPN: { // alpn
            BSB absb;
            BSB_INIT(absb, ptr, len);
            while (BSB_REMAINING(absb) > 1 && !BSB_IS_ERROR(absb)) {
                uint8_t alen = 0;
                BSB_IMPORT_u08(absb, alen);

                uint8_t *aptr = NULL;
                BSB_IMPORT_ptr(absb, aptr, alen);

                if (aptr) {
                    fieldValue->key = SVCB_PARAM_KEY_ALPN;
                    fieldValue->value = g_strndup((const char *)aptr, alen);
                }
            }
        }
        break;
        case SVCB_PARAM_KEY_PORT: { // port
            if (len != 2)
                break;
            uint16_t port = (ptr[0] << 8) | ptr[1];
            fieldValue->key = SVCB_PARAM_KEY_PORT;
            fieldValue->value = ARKIME_TYPE_ALLOC(uint16_t);
            *(uint16_t *)fieldValue->value = port;
        }
        break;
        case SVCB_PARAM_KEY_IPV4_HINT: { // ipv4hint
            if (len != 4)
                break;
            uint32_t ip = (ptr[3] << 24) | (ptr[2] << 16) |  (ptr[1] << 8) | ptr[0];
            fieldValue->key = SVCB_PARAM_KEY_IPV4_HINT;
            fieldValue->value = ARKIME_TYPE_ALLOC(uint32_t);
            *(uint32_t *)fieldValue->value = ip;
        }
        break;
        case SVCB_PARAM_KEY_IPV6_HINT: {// ipv6hint
            if (len != 16)
                break;
            fieldValue->key = SVCB_PARAM_KEY_IPV6_HINT;
            fieldValue->value = g_memdup((const void *)ptr, sizeof(struct in6_addr));
        }
        break;
        }
        BSB_IMPORT_skip(bsb, len);

        DLL_PUSH_TAIL(t_, &(svcbData->fieldValues), fieldValue);
    }

}
/******************************************************************************/
LOCAL void dns_parser(ArkimeSession_t *session, int kind, const uint8_t *data, int len)
{

    if (len < 17)
        return;

    int preexistingObject = 0;
    int jsonLen = 0;

    int id      = (data[0] << 8 | data[1]);
    int qr      = (data[2] >> 7) & 0x1;
    int opcode  = (data[2] >> 3) & 0xf;
    int aa      = (data[2] >> 2) & 0x1;
    int tc      = (data[2] >> 1) & 0x1;
    int rd      = (data[2] >> 0) & 0x1;
    int ra      = (data[3] >> 7) & 0x1;
    //int z       = (data[3] >> 6) & 0x1;
    int ad      = (data[3] >> 5) & 0x1;
    int cd      = (data[3] >> 4) & 0x1;
    if (opcode > 5)
        return;

    int qd_count = (data[4] << 8) | data[5];          /*number of question records*/
    int an_prereqs_count = (data[6] << 8) | data[7];  /*number of answer or prerequisite records*/
    int ns_update_count = (data[8] << 8) | data[9];   /*number of authoritative or update recrods*/
    int ar_count = (data[10] << 8) | data[11];        /*number of additional records*/

    int resultRecordCount [3] = {0};
    resultRecordCount [0] = an_prereqs_count;
    resultRecordCount [1] = ns_update_count;
    resultRecordCount [2] = ar_count;

#ifdef DNSDEBUG
    LOG("DNSDEBUG: [Query/Zone Count: %d], [Answer or Prerequisite Count: %d], [Authoritative or Update RecordCount: %d], [Additional Record Count: %d]", qd_count, an_prereqs_count, ns_update_count, ar_count);
#endif

    switch (kind) {
    case 0:
        arkime_session_add_protocol(session, "dns");
        break;
    case 1:
        arkime_session_add_protocol(session, "llmnr");
        break;
    case 2:
        arkime_session_add_protocol(session, "mdns");
        break;
    }

    if (qd_count != 1) {
        arkime_session_add_tag(session, "dns:qdcount-not-1");
        return;
    }

    ArkimeFieldObject_t *fobject = ARKIME_TYPE_ALLOC0(ArkimeFieldObject_t);
    DNS_t *dns = ARKIME_TYPE_ALLOC0(DNS_t);

    HASH_INIT(s_, dns->hosts, arkime_string_hash, arkime_string_ncmp);
    dns->nsHosts = ARKIME_TYPE_ALLOC(ArkimeStringHashStd_t);
    HASH_INIT(s_, *(dns->nsHosts), arkime_string_hash, arkime_string_ncmp);
    dns->mxHosts = ARKIME_TYPE_ALLOC(ArkimeStringHashStd_t);
    HASH_INIT(s_, *(dns->mxHosts), arkime_string_hash, arkime_string_ncmp);
    dns->punyHosts = ARKIME_TYPE_ALLOC(ArkimeStringHashStd_t);
    HASH_INIT(s_, *(dns->punyHosts), arkime_string_hash, arkime_string_ncmp);

    dns->query.packet_uid = id;
    fobject->object = dns;

    jsonLen = DEFAULT_JSON_LEN;

    BSB bsb;
    BSB_INIT(bsb, data + 12, len - 12);

    /* QD Section */
    char namebuf[8000];
    int namelen = sizeof(namebuf);
    char *name = dns_name(data, len, &bsb, namebuf, &namelen);

    if (BSB_IS_ERROR(bsb) || !name) {
        dns_free_object(fobject);
        return;
    }

    if (!namelen) {
        dns->query.hostname = (char *)"<root>";
        namelen = 6;
    } else {
        dns->query.hostname = g_hostname_to_unicode(name);
        int hostlen = strlen(dns->query.hostname);
        ARKIME_RULES_RUN_FIELD_SET(session, dnsQueryHostField, dns->query.hostname);
        if (g_utf8_validate(dns->query.hostname, hostlen, NULL)) {
            ArkimeString_t *element;

            char *lower = g_ascii_strdown(dns->query.hostname, hostlen);

            HASH_FIND(s_, dns->hosts, lower, element);
            if (element) {
                g_free(lower);
            } else {
                element = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                element->str = lower;
                element->len = hostlen;
                element->utf8 = 1;
                HASH_ADD(s_, dns->hosts, element->str, element);
                ARKIME_RULES_RUN_FIELD_SET(session, dnsHostField, element->str);
                jsonLen += HOST_IP_JSON_LEN;
            }
        }
        if (arkime_memstr((const char *)name, len, "xn--", 4)) {
            ArkimeString_t *hstring;
            HASH_FIND(s_, *(dns->punyHosts), name, hstring);
            if (!hstring) {
                ArkimeString_t *string = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                string->str = (char *)g_ascii_strdown(name, namelen);
                string->len = namelen;
                HASH_ADD(s_, *(dns->punyHosts), string->str, string);
                ARKIME_RULES_RUN_FIELD_SET(session, dnsPunyField, string->str);
            }
        }
    }


    unsigned short qtype = 0, qclass = 0 ;
    BSB_IMPORT_u16(bsb, qtype);
    BSB_IMPORT_u16(bsb, qclass);

    if (qclass < MAX_QCLASSES && qclasses[qclass]) {
        dns->query.class    = qclasses[qclass];
        dns->query.class_id = qclass;
        ARKIME_RULES_RUN_FIELD_SET(session, dnsQueryClassField, dns->query.class);
    }

    if (qtype < MAX_QTYPES && qtypes[qtype]) {
        dns->query.type    = qtypes[qtype];
        dns->query.type_id = qtype;
        ARKIME_RULES_RUN_FIELD_SET(session, dnsQueryTypeField, dns->query.opcode);
    }

    dns->query.opcode_id = opcode;
    dns->query.opcode = opcodes[opcode];
    ARKIME_RULES_RUN_FIELD_SET(session, dnsOpcodeField, dns->query.opcode);


    if (qr == 0) {
        dns->rcode_id    = -1; // Not a response
        if (!arkime_field_object_add(dnsField, session, fobject, jsonLen)) {
            dns_free_object(fobject);
            dns = 0;
        }
        return;
    }

    ArkimeFieldObject_t *existingFObject;

    if (session->fields[dnsField] && session->fields[dnsField]->ohash) {
        HASH_FIND_HASH(o_, *(session->fields[dnsField]->ohash), HASH_HASH(*(session->fields[dnsField]->ohash), fobject), fobject, existingFObject);
        if (existingFObject) {
            dns_free_object(fobject);
            fobject = existingFObject;
            dns = (DNS_t *)fobject->object;
#ifdef DNSDEBUG
            LOG("DNSDEBUG: Retrieved an existing object");
#endif
            preexistingObject = 1;
            if (dns->answers.t_count == 0) {
                DLL_INIT(t_, &dns->answers);
            }
        } else {
#ifdef DNSDEBUG
            LOG("DNSDEBUG: No existing object");
#endif
            DLL_INIT(t_, &dns->answers);
        }
    } else {
#ifdef DNSDEBUG
        LOG("DNSDEBUG: No object added so far");
#endif
        DLL_INIT(t_, &dns->answers);
    }

    if (!dns->ips) {
        dns->ips = g_hash_table_new_full(arkime_field_ip_hash, arkime_field_ip_equal, g_free, NULL);
    }
    if (!dns->nsIPs) {
        dns->nsIPs = g_hash_table_new_full(arkime_field_ip_hash, arkime_field_ip_equal, g_free, NULL);
    }
    if (!dns->mxIPs) {
        dns->mxIPs = g_hash_table_new_full(arkime_field_ip_hash, arkime_field_ip_equal, g_free, NULL);
    }

    dns->rcode_id    = data[3] & 0xf;
    dns->rcode       = rcodes[dns->rcode_id];
    ARKIME_RULES_RUN_FIELD_SET(session, dnsStatusField, dns->rcode);

    int recordType = 0;
    int i;
    unsigned char txtLen = 0;
    for (recordType = RESULT_RECORD_ANSWER; recordType <= RESULT_RECORD_ADDITIONAL; recordType++) {
        int recordNum = resultRecordCount[recordType - 1];
        for (i = 0; BSB_NOT_ERROR(bsb) && i < recordNum; i++) {
            char namebuf[8000];
            int namelen = sizeof(namebuf);
            char *name = dns_name(data, len, &bsb, namebuf, &namelen);

            if (BSB_IS_ERROR(bsb) || !name)
                break;

            DNSAnswer_t *answer = ARKIME_TYPE_ALLOC0(DNSAnswer_t);

            if (!namelen) {
                answer->name = (char *)"<root>";
                namelen = 6;
            } else {
                answer->name = g_hostname_to_unicode(name);
                if (arkime_memstr((const char *)name, len, "xn--", 4)) {
                    ArkimeString_t *hstring;
                    HASH_FIND(s_, *(dns->punyHosts), name, hstring);
                    if (!hstring) {
                        ArkimeString_t *string = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                        string->str = (char *)g_ascii_strdown(name, namelen);
                        string->len = namelen;
                        HASH_ADD(s_, *(dns->punyHosts), string->str, string);
                        ARKIME_RULES_RUN_FIELD_SET(session, dnsPunyField, string->str);
                    }
                }
            }

#ifdef DNSDEBUG
            LOG("DNSDEBUG: RR Name=%s", name);
#endif

            uint16_t antype = 0;
            BSB_IMPORT_u16 (bsb, antype);
            uint16_t anclass = 0;
            BSB_IMPORT_u16 (bsb, anclass);
            uint32_t anttl = 0;
            BSB_IMPORT_u32 (bsb, anttl);
            uint16_t rdlength = 0;
            BSB_IMPORT_u16 (bsb, rdlength);

            if (BSB_REMAINING(bsb) < rdlength) {
                ARKIME_TYPE_FREE(DNSAnswer_t, answer);
                break;
            }

            if (anclass != CLASS_IN) {
                BSB_IMPORT_skip(bsb, rdlength);
                ARKIME_TYPE_FREE(DNSAnswer_t, answer);
                continue;
            }

            switch (antype) {
            case DNS_RR_A: {
                if (rdlength != 4) {
                    BSB_IMPORT_skip(bsb, rdlength);
                    ARKIME_TYPE_FREE(DNSAnswer_t, answer);
                    continue;
                }

                const uint8_t *ptr = BSB_WORK_PTR(bsb);
                answer->ipA = ((uint32_t)(ptr[3])) << 24 | ((uint32_t)(ptr[2])) << 16 | ((uint32_t)(ptr[1])) << 8 | ptr[0];
#ifdef DNSDEBUG
                LOG("DNSDEBUG: RR_A=%u.%u.%u.%u, name=%s", answer->ipA & 0xff, (answer->ipA >> 8) & 0xff, (answer->ipA >> 16) & 0xff, (answer->ipA >> 24) & 0xff, answer->name);
#endif
                struct in6_addr *v = g_malloc(sizeof(struct in6_addr));

                memset(v->s6_addr, 0, 8);
                ((uint32_t *)v->s6_addr)[2] = htonl(0xffff);
                ((uint32_t *)v->s6_addr)[3] = ((uint32_t)(ptr[3])) << 24 | ((uint32_t)(ptr[2])) << 16 | ((uint32_t)(ptr[1])) << 8 | ptr[0];

                ArkimeString_t *hstring = 0;

                HASH_FIND(s_, dns->hosts, answer->name, hstring);
                if (strcmp(dns->query.hostname, answer->name) == 0 || hstring) {
                    struct in6_addr *hostv = g_memdup(v, sizeof(struct in6_addr));
                    g_hash_table_add(dns->ips, hostv);
                    jsonLen += HOST_IP_JSON_LEN;
                }

                HASH_FIND(s_, *(dns->nsHosts), answer->name, hstring);
                if (hstring) {
                    struct in6_addr *nsv = g_memdup(v, sizeof(struct in6_addr));
                    g_hash_table_add(dns->nsIPs, nsv);
                    jsonLen += HOST_IP_JSON_LEN;
                }

                HASH_FIND(s_, *(dns->mxHosts), answer->name, hstring);
                if (hstring) {
                    struct in6_addr *mxv = g_memdup(v, sizeof(struct in6_addr));
                    g_hash_table_add(dns->mxIPs, mxv);
                    jsonLen += HOST_IP_JSON_LEN;
                }

                g_free(v);
            }
            break;
            case DNS_RR_NS: {
                BSB rdbsb;
                BSB_INIT(rdbsb, BSB_WORK_PTR(bsb), rdlength);

                namelen = sizeof(namebuf);
                name = dns_name(data, len, &rdbsb, namebuf, &namelen);

                if (!namelen || BSB_IS_ERROR(rdbsb) || !name) {
                    BSB_IMPORT_skip(bsb, rdlength);
                    ARKIME_TYPE_FREE(DNSAnswer_t, answer);
                    continue;
                }

#ifdef DNSDEBUG
                LOG("DNSDEBUG: RR_NS Name=%s", name);
#endif

                answer->nsdname = g_hostname_to_unicode(name);
                if (g_utf8_validate(answer->nsdname, namelen, NULL)) {
                    ArkimeString_t *element = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                    element->str = g_ascii_strdown(answer->nsdname, namelen);
                    element->len = namelen;
                    element->utf8 = 1;
                    HASH_ADD(s_, *(dns->nsHosts), element->str, element);
                    ARKIME_RULES_RUN_FIELD_SET(session, dnsHostNameserverField, element->str);
                    jsonLen += HOST_IP_JSON_LEN;
                }

                if (arkime_memstr((const char *)name, len, "xn--", 4)) {
                    ArkimeString_t *hstring;
                    HASH_FIND(s_, *(dns->punyHosts), name, hstring);
                    if (!hstring) {
                        ArkimeString_t *string = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                        string->str = (char *)g_ascii_strdown(name, namelen);
                        string->len = namelen;
                        HASH_ADD(s_, *(dns->punyHosts), string->str, string);
                        ARKIME_RULES_RUN_FIELD_SET(session, dnsPunyField, string->str);
                    }
                }
            }
            break;
            case DNS_RR_CNAME: {
                BSB rdbsb;
                BSB_INIT(rdbsb, BSB_WORK_PTR(bsb), rdlength);

                namelen = sizeof(namebuf);
                name = dns_name(data, len, &rdbsb, namebuf, &namelen);

                if (!namelen || BSB_IS_ERROR(rdbsb) || !name) {
                    BSB_IMPORT_skip(bsb, rdlength);
                    ARKIME_TYPE_FREE(DNSAnswer_t, answer);
                    continue;
                }

#ifdef DNSDEBUG
                LOG("DNSDEBUG: RR_CNAME Name=%s", name);
#endif

                answer->cname = g_hostname_to_unicode(name);
                if (g_utf8_validate(answer->cname, namelen, NULL)) {
                    ArkimeString_t *element = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                    element->str = g_ascii_strdown(answer->cname, namelen);
                    element->len = namelen;
                    element->utf8 = 1;
                    HASH_ADD(s_, dns->hosts, element->str, element);
                    ARKIME_RULES_RUN_FIELD_SET(session, dnsHostField, element->str);
                    jsonLen += HOST_IP_JSON_LEN;
                }

                if (arkime_memstr((const char *)name, len, "xn--", 4)) {
                    ArkimeString_t *hstring;
                    HASH_FIND(s_, *(dns->punyHosts), name, hstring);
                    if (!hstring) {
                        ArkimeString_t *string = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                        string->str = (char *)g_ascii_strdown(name, namelen);
                        string->len = namelen;
                        HASH_ADD(s_, *(dns->punyHosts), string->str, string);
                        ARKIME_RULES_RUN_FIELD_SET(session, dnsPunyField, string->str);
                    }
                }
            }
            break;
            case DNS_RR_MX: {
                BSB rdbsb;
                BSB_INIT(rdbsb, BSB_WORK_PTR(bsb), rdlength);
                uint16_t mx_preference = 0;
                BSB_IMPORT_u16(rdbsb, mx_preference);

                namelen = sizeof(namebuf);
                name = dns_name(data, len, &rdbsb, namebuf, &namelen);

                if (!namelen || BSB_IS_ERROR(rdbsb) || !name) {
                    BSB_IMPORT_skip(bsb, rdlength);
                    ARKIME_TYPE_FREE(DNSAnswer_t, answer);
                    continue;
                }

#ifdef DNSDEBUG
                LOG("DNSDEBUG: RR_MX Exchange=%s, Preference=%d", name, mx_preference);
#endif

                answer->mx = ARKIME_TYPE_ALLOC0(DNSMXRData_t);
                (answer->mx)->preference = mx_preference;
                (answer->mx)->exchange = g_hostname_to_unicode(name);
                if (g_utf8_validate(answer->mx->exchange, namelen, NULL)) {
                    ArkimeString_t *element = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                    element->str = g_ascii_strdown(answer->mx->exchange, namelen);
                    element->len = namelen;
                    element->utf8 = 1;
                    HASH_ADD(s_, *(dns->mxHosts), element->str, element);
                    ARKIME_RULES_RUN_FIELD_SET(session, dnsHostMailserverField, element->str);
                    jsonLen += HOST_IP_JSON_LEN;
                }

                if (arkime_memstr((const char *)name, len, "xn--", 4)) {
                    ArkimeString_t *hstring;
                    HASH_FIND(s_, *(dns->punyHosts), name, hstring);
                    if (!hstring) {
                        ArkimeString_t *string = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                        string->str = (char *)g_ascii_strdown(name, namelen);
                        string->len = namelen;
                        HASH_ADD(s_, *(dns->punyHosts), string->str, string);
                        ARKIME_RULES_RUN_FIELD_SET(session, dnsPunyField, string->str);
                    }
                }
            }
            break;
            case DNS_RR_AAAA: {
                if (rdlength != 16) {
                    BSB_IMPORT_skip(bsb, rdlength);
                    ARKIME_TYPE_FREE(DNSAnswer_t, answer);
                    continue;
                }

                const uint8_t *ptr = BSB_WORK_PTR(bsb);

                answer->ipAAAA = g_memdup((const void *)ptr, sizeof(struct in6_addr));

#ifdef DNSDEBUG
                char ipbuf[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, answer->ipAAAA, ipbuf, sizeof(ipbuf));
                LOG("DNSDEBUG: RR_AAAA=%s, name=%s", ipbuf, answer->name);
#endif
                struct in6_addr *v = g_memdup((const void *)ptr, sizeof(struct in6_addr));

                ArkimeString_t *hstring = 0;

                HASH_FIND(s_, dns->hosts, answer->name, hstring);
                if (strcmp(dns->query.hostname, answer->name) == 0 || hstring) {
                    struct in6_addr *hostv = g_memdup(v, sizeof(struct in6_addr));
                    g_hash_table_add(dns->ips, hostv);
                    jsonLen += HOST_IP_JSON_LEN;
                }

                HASH_FIND(s_, *(dns->nsHosts), answer->name, hstring);
                if (hstring) {
                    struct in6_addr *nsv = g_memdup(v, sizeof(struct in6_addr));
                    g_hash_table_add(dns->nsIPs, nsv);
                    jsonLen += HOST_IP_JSON_LEN;
                }

                HASH_FIND(s_, *(dns->mxHosts), answer->name, hstring);
                if (hstring) {
                    struct in6_addr *mxv = g_memdup(v, sizeof(struct in6_addr));
                    g_hash_table_add(dns->mxIPs, mxv);
                    jsonLen += HOST_IP_JSON_LEN;
                }

                g_free(v);
            }
            break;
            case DNS_RR_HTTPS: {
                DNSSVCBRData_t *svcbData = ARKIME_TYPE_ALLOC0(DNSSVCBRData_t);
                dns_parser_rr_svcb(svcbData, BSB_WORK_PTR(bsb), rdlength);
                if (svcbData) {
                    answer->svcb = svcbData;
                    jsonLen += HOST_IP_JSON_LEN;
                    jsonLen += DLL_COUNT(t_, &(answer->svcb->fieldValues)) * 50;
                }
            }
            break;
            break;
            case DNS_RR_TXT: {
                BSB_IMPORT_u08(bsb, txtLen);
                const uint8_t *ptr = BSB_WORK_PTR(bsb);

                answer->txt = g_strndup((const char *)ptr, txtLen);

#ifdef DNSDEBUG
                LOG("DNSDEBUG: RR_TXT=%s", answer->txt);
#endif

                jsonLen += txtLen;
                txtLen = 0;
            }
            break;
            case DNS_RR_CAA: {
                if (rdlength <= 3) {
                    BSB_IMPORT_skip(bsb, rdlength);
                    ARKIME_TYPE_FREE(DNSAnswer_t, answer);
                    continue;
                }

                BSB rdbsb;
                BSB_INIT(rdbsb, BSB_WORK_PTR(bsb), rdlength);

                answer->caa = ARKIME_TYPE_ALLOC0(DNSCAARData_t);

                BSB_IMPORT_u08(rdbsb, answer->caa->flags);

                uint8_t tagLen = 0;
                BSB_IMPORT_u08(rdbsb, tagLen);

                uint16_t valueLen = rdlength - tagLen - 2;

                uint8_t *ptr = BSB_WORK_PTR(rdbsb);
                answer->caa->tag = g_strndup((const char *)ptr, tagLen);
                BSB_EXPORT_skip(rdbsb, tagLen);

                ptr = BSB_WORK_PTR(rdbsb);
                answer->caa->value = g_strndup((const char *)ptr, valueLen);

#ifdef DNSDEBUG
                LOG("DNSDEBUG: RR_CAA %d %s", answer->caa->flags, answer->caa->value);
#endif

                jsonLen += tagLen + valueLen;
            }
            break;
            } /* switch */
            BSB_IMPORT_skip(bsb, rdlength);

            if (anclass <= 255 && qclasses[anclass]) {
                answer->class = qclasses[anclass];
            }

            if (antype <= 255 && qtypes[antype]) {
                answer->type = qtypes[antype];
                answer->type_id = antype;
            }

            answer->ttl = anttl;
            answer->packet_uid = id;

            DLL_INIT(s_, &answer->flags);
            ArkimeString_t *flag;
            if (aa) {
                flag = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                flag->str = g_strndup(flags[1], strlen(flags[1]));
                DLL_PUSH_TAIL(s_, &answer->flags, flag);
            }
            if (tc) {
                flag = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                flag->str = g_strndup(flags[2], strlen(flags[2]));
                DLL_PUSH_TAIL(s_, &answer->flags, flag);
            }
            if (rd) {
                flag = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                flag->str = g_strndup(flags[3], strlen(flags[3]));
                DLL_PUSH_TAIL(s_, &answer->flags, flag);
            }
            if (ra) {
                flag = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                flag->str = g_strndup(flags[4], strlen(flags[4]));
                DLL_PUSH_TAIL(s_, &answer->flags, flag);
            }
            if (ad) {
                flag = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                flag->str = g_strndup(flags[5], strlen(flags[5]));
                DLL_PUSH_TAIL(s_, &answer->flags, flag);
            }
            if (cd) {
                flag = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                flag->str = g_strndup(flags[6], strlen(flags[6]));
                DLL_PUSH_TAIL(s_, &answer->flags, flag);
            }

            DLL_PUSH_TAIL(t_, &dns->answers, answer);
            jsonLen += ANSWER_JSON_LEN;
        } // record loop
    } // record type loop

    if (!arkime_field_object_add(dnsField, session, fobject, jsonLen) && !preexistingObject) {
        dns_free_object(fobject);
        dns = 0;
    }
}
/******************************************************************************/
LOCAL int dns_tcp_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int which)
{
    DNSInfo_t *info = uw;
    while (len >= 2) {

        // First packet of request
        if (info->len[which] == 0) {
            int dnslength = ((data[0] & 0xff) << 8) | (data[1] & 0xff);

            if (dnslength < 18) {
                arkime_parsers_unregister(session, uw);
                return 0;
            }

            // Have all the data in this first packet, just parse it
            if (dnslength <= len - 2) {
                dns_parser(session, 0, data + 2, dnslength);
                data += 2 + dnslength;
                len -= 2 + dnslength;
                continue;
            }
            // Don't have all the data, will need to save off

            if (info->size[which] == 0) {
                info->size[which] = MAX(1024, dnslength);
                info->data[which] = malloc(info->size[which]);
            } else if (info->size[which] < dnslength) {
                info->data[which] = realloc(info->data[which], dnslength);
                if (!info->data[which]) {
                    arkime_parsers_unregister(session, uw);
                    return 0;
                }
                info->size[which] = dnslength;
            }

            memcpy(info->data[which], data + 2, len - 2);
            info->len[which] = dnslength;
            info->pos[which] = len - 2;
            return 0;
        } else {
            int rem = info->len[which] - info->pos[which];
            if (rem <= len) {
                memcpy(info->data[which] + info->pos[which], data, rem);
                len -= rem;
                data += rem;
                dns_parser(session, 0, info->data[which], info->len[which]);
                info->len[which] = 0;
            } else {
                memcpy(info->data[which] + info->pos[which], data, len);
                info->pos[which] += len;
                return 0;
            }
        }
    }
    return 0;
}
/******************************************************************************/
LOCAL void dns_tcp_classify(ArkimeSession_t *session, const uint8_t *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    if (session->port2 == 53 && !arkime_session_has_protocol(session, "dns")) {
        arkime_session_add_protocol(session, "dns");
        DNSInfo_t  *info = ARKIME_TYPE_ALLOC0(DNSInfo_t);
        arkime_parsers_register(session, dns_tcp_parser, info, dns_free);
    }
}
/******************************************************************************/
LOCAL int dns_udp_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int UNUSED(which))
{
    if (uw == 0 || (session->port1 != 53 && session->port2 != 53)) {
        dns_parser(session, (long)uw, data, len);
    }
    return 0;
}
/******************************************************************************/
LOCAL void dns_udp_classify(ArkimeSession_t *session, const uint8_t *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    arkime_parsers_register(session, dns_udp_parser, uw, 0);
}
/******************************************************************************/
#define SAVE_STRING_HEAD(HEAD, STR) \
if (HEAD.s_count > 0) { \
    BSB_EXPORT_cstr(*jbsb, "\"" STR "\":["); \
    while (HEAD.s_count > 0) { \
	DLL_POP_HEAD(s_, &HEAD, string); \
	arkime_db_js0n_str(&*jbsb, (uint8_t *)string->str, string->utf8); \
	BSB_EXPORT_u08(*jbsb, ','); \
	g_free(string->str); \
	ARKIME_TYPE_FREE(ArkimeString_t, string); \
    } \
    BSB_EXPORT_rewind(*jbsb, 1); \
    BSB_EXPORT_u08(*jbsb, ']'); \
    BSB_EXPORT_u08(*jbsb, ','); \
}
/******************************************************************************/
#define SAVE_STRING_HASH(HASH, KEY) \
do { \
    BSB_EXPORT_sprintf(*jbsb, "\"%sCnt\":%d,", KEY, HASH_COUNT(s_, HASH)); \
    BSB_EXPORT_sprintf(*jbsb, "\"%s\":[", KEY); \
    HASH_FORALL_POP_HEAD2(s_, HASH, string) { \
        arkime_db_js0n_str(&*jbsb, (uint8_t *)string->str, string->utf8); \
        BSB_EXPORT_u08(*jbsb, ','); \
        g_free(string->str); \
        ARKIME_TYPE_FREE(ArkimeString_t, string); \
    } \
    BSB_EXPORT_rewind(*jbsb, 1); /* Remove last comma */ \
    BSB_EXPORT_cstr(*jbsb, "],"); \
} while(0)
/*******************************************************************************************/
void dns_save_ip_ghash(BSB *jbsb, struct arkime_session *session, GHashTable *ghash, char *key, int16_t keyLen)
{

    BSB_EXPORT_sprintf(*jbsb, "\"%sCnt\":%u,", key, g_hash_table_size(ghash));

    uint32_t              i;
    uint32_t              asNum[MAX_IPS];
    char                 *asStr[MAX_IPS];
    int                   asLen[MAX_IPS];
    char                 *g[MAX_IPS];
    char                 *rir[MAX_IPS];
    GHashTableIter        iter;
    gpointer              ikey;
    char                  ip[INET6_ADDRSTRLEN];
    uint32_t              cnt = 0;

    BSB_EXPORT_sprintf(*jbsb, "\"%s\":[", key);
    g_hash_table_iter_init (&iter, ghash);
    while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
        arkime_db_geo_lookup6(session, *(struct in6_addr *)ikey, &g[cnt], &asNum[cnt], &asStr[cnt], &asLen[cnt], &rir[cnt]);
        cnt++;
        if (cnt >= MAX_IPS)
            break;

        if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)ikey)) {
            uint32_t ipv4 = ARKIME_V6_TO_V4(*(struct in6_addr *)ikey);
            snprintf(ip, sizeof(ip), "%u.%u.%u.%u", ipv4 & 0xff, (ipv4 >> 8) & 0xff, (ipv4 >> 16) & 0xff, (ipv4 >> 24) & 0xff);
        } else {
            inet_ntop(AF_INET6, ikey, ip, sizeof(ip));
        }

        BSB_EXPORT_sprintf(*jbsb, "\"%s\",", ip);
    }
    BSB_EXPORT_rewind(*jbsb, 1); // Remove last comma
    BSB_EXPORT_cstr(*jbsb, "],");

    BSB_EXPORT_sprintf(*jbsb, "\"%.*sGEO\":[", keyLen - 2, key);
    for (i = 0; i < cnt; i++) {
        if (g[i]) {
            BSB_EXPORT_sprintf(*jbsb, "\"%2.2s\",", g[i]);
        } else {
            BSB_EXPORT_cstr(*jbsb, "\"---\",");
        }
    }
    BSB_EXPORT_rewind(*jbsb, 1); // Remove last comma
    BSB_EXPORT_cstr(*jbsb, "],");

    BSB_EXPORT_sprintf(*jbsb, "\"%.*sASN\":[", keyLen - 2, key);
    for (i = 0; i < cnt; i++) {
        if (asStr[i]) {
            BSB_EXPORT_sprintf(*jbsb, "\"AS%u ", asNum[i]);
            arkime_db_js0n_str_unquoted(jbsb, (uint8_t *)asStr[i], asLen[i], TRUE);
            BSB_EXPORT_cstr(*jbsb, "\",");

        } else {
            BSB_EXPORT_cstr(*jbsb, "\"---\",");
        }
    }
    BSB_EXPORT_rewind(*jbsb, 1); // Remove last comma
    BSB_EXPORT_cstr(*jbsb, "],");

    BSB_EXPORT_sprintf(*jbsb, "\"%.*sRIR\":[", keyLen - 2, key);
    for (i = 0; i < cnt; i++) {
        if (rir[i]) {
            BSB_EXPORT_sprintf(*jbsb, "\"%s\",", rir[i]);
        } else {
            BSB_EXPORT_cstr(*jbsb, "\"\",");
        }
    }
    BSB_EXPORT_rewind(*jbsb, 1); // Remove last comma
    BSB_EXPORT_cstr(*jbsb, "],");

    g_hash_table_destroy(ghash);
}
/*******************************************************************************************/
void dns_save(BSB *jbsb, ArkimeFieldObject_t *object, struct arkime_session *session)
{
    if (object->object == NULL) {
        return;
    }

    char ipAAAA[INET6_ADDRSTRLEN];
    ArkimeString_t *string;

    DNS_t *dns = (DNS_t *)object->object;

#ifdef DNSDEBUG
    int offset = BSB_LENGTH(*jbsb);
#endif

    BSB_EXPORT_u08(*jbsb, '{');

#ifdef DNSDEBUG
    LOG("DNSDEBUG: Host: %s, Opcode: %s, QC: %s, QT: %s", dns->query.hostname, dns->query.opcode, dns->query.class, dns->query.type);
#endif

    BSB_EXPORT_sprintf(*jbsb, "\"opcode\":\"%s\",", dns->query.opcode);
    BSB_EXPORT_sprintf(*jbsb, "\"queryHost\":\"%s\",", dns->query.hostname);
    BSB_EXPORT_sprintf(*jbsb, "\"qc\":\"%s\",", dns->query.class);
    BSB_EXPORT_sprintf(*jbsb, "\"qt\":\"%s\",", dns->query.type);

    if (HASH_COUNT(s_, dns->hosts) > 0) {
        SAVE_STRING_HASH(dns->hosts, "host");
    }
    if (HASH_COUNT(s_, *(dns->nsHosts)) > 0) {
        SAVE_STRING_HASH(*(dns->nsHosts), "nameserverHost");
    }
    if (HASH_COUNT(s_, *(dns->mxHosts)) > 0) {
        SAVE_STRING_HASH(*(dns->mxHosts), "mailserverHost");
    }
    if (HASH_COUNT(s_, *(dns->punyHosts)) > 0) {
        SAVE_STRING_HASH(*(dns->punyHosts), "puny");
    }

    if (dns->ips && g_hash_table_size(dns->ips) > 0) {
        dns_save_ip_ghash(jbsb, session, dns->ips, "ip", 2);
        dns->ips = 0;
    }
    if (dns->nsIPs && g_hash_table_size(dns->nsIPs) > 0) {
        dns_save_ip_ghash(jbsb, session, dns->nsIPs, "nameserverIp", 12);
        dns->nsIPs = 0;
    }
    if (dns->mxIPs && g_hash_table_size(dns->mxIPs) > 0) {
        dns_save_ip_ghash(jbsb, session, dns->mxIPs, "mailserverIp", 12);
        dns->mxIPs = 0;
    }

    if (dns->rcode_id != -1) {
        BSB_EXPORT_sprintf(*jbsb, "\"status\":\"%s\",", dns->rcode);
        if (dnsOutputAnswers) {
            BSB_EXPORT_sprintf(*jbsb, "\"answersCnt\":%d,", DLL_COUNT(t_, &dns->answers));
            if (DLL_COUNT(t_, &dns->answers) > 0) {
                BSB_EXPORT_cstr(*jbsb, "\"answers\":[");
                DNSAnswer_t *answer;
                while (DLL_POP_HEAD(t_, &dns->answers, answer)) {
                    BSB_EXPORT_u08(*jbsb, '{');
                    switch (answer->type_id) {
                    case DNS_RR_A: {
                        BSB_EXPORT_sprintf(*jbsb, "\"ip\":\"%u.%u.%u.%u\",", answer->ipA & 0xff, (answer->ipA >> 8) & 0xff, (answer->ipA >> 16) & 0xff, (answer->ipA >> 24) & 0xff);
                    }
                    break;
                    case DNS_RR_NS: {
                        BSB_EXPORT_sprintf(*jbsb, "\"nameserver\":\"%s\",", answer->nsdname);
                        g_free(answer->nsdname);
                    }
                    break;
                    case DNS_RR_CNAME: {
                        BSB_EXPORT_sprintf(*jbsb, "\"cname\":\"%s\",", answer->cname);
                        g_free(answer->cname);
                    }
                    break;
                    case DNS_RR_MX: {
                        BSB_EXPORT_sprintf(*jbsb, "\"exchange\":\"(%u)%s\",", answer->mx->preference, answer->mx->exchange);
                        g_free(answer->mx->exchange);
                        ARKIME_TYPE_FREE(DNSMXRData_t, answer->mx);
                    }
                    break;
                    case DNS_RR_AAAA: {
                        if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)answer->ipAAAA)) {
                            uint32_t ip = ARKIME_V6_TO_V4(*(struct in6_addr *)answer->ipAAAA);
                            snprintf(ipAAAA, sizeof(ipAAAA), "%u.%u.%u.%u", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff);
                        } else {
                            inet_ntop(AF_INET6, answer->ipAAAA, ipAAAA, sizeof(ipAAAA));
                        }
                        BSB_EXPORT_sprintf(*jbsb, "\"ip\":\"%s\",", ipAAAA);
                        g_free(answer->ipAAAA);
                    }
                    break;
                    case DNS_RR_TXT: {
                        BSB_EXPORT_cstr(*jbsb, "\"txt\":");
                        arkime_db_js0n_str(jbsb, (uint8_t *)answer->txt, 1);
                        BSB_EXPORT_u08(*jbsb, ',');
                        g_free(answer->txt);
                    }
                    break;
                    case DNS_RR_HTTPS: {
                        BSB_EXPORT_sprintf(*jbsb, "\"https\":\"HTTPS %u %s ", answer->svcb->priority, answer->svcb->dname);
                        DNSSVCBRDataFieldValue_t *fieldValue;
                        while (DLL_COUNT(t_, &(answer->svcb->fieldValues)) > 0) {
                            DLL_POP_HEAD(t_, &(answer->svcb->fieldValues), fieldValue);
                            switch (fieldValue->key) {
                            case SVCB_PARAM_KEY_ALPN: {
                                BSB_EXPORT_sprintf(*jbsb, "alpn=\\\"%s\\\" ", (char *)fieldValue->value);
                                g_free((char *)fieldValue->value);
                            }
                            break;
                            case SVCB_PARAM_KEY_PORT: {
                                BSB_EXPORT_sprintf(*jbsb, "port=%u ", *(uint16_t *)fieldValue->value);
                                ARKIME_TYPE_FREE(uint16_t, (uint16_t *)fieldValue->value);
                            }
                            break;
                            case SVCB_PARAM_KEY_IPV4_HINT: {
                                BSB_EXPORT_sprintf(*jbsb, "ipv4hint:\\\"%u.%u.%u.%u\\\" ", *(uint32_t *)(fieldValue->value) & 0xff, (*(uint32_t *)(fieldValue->value) >> 8) & 0xff, (*(uint32_t *)(fieldValue->value) >> 16) & 0xff, (*(uint32_t *)(fieldValue->value) >> 24) & 0xff);
                                ARKIME_TYPE_FREE(uint32_t, (uint32_t *)fieldValue->value);
                            }
                            break;
                            case SVCB_PARAM_KEY_IPV6_HINT: {
                                if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)fieldValue->value)) {
                                    uint32_t ip = ARKIME_V6_TO_V4(*(struct in6_addr *)fieldValue->value);
                                    snprintf(ipAAAA, sizeof(ipAAAA), "%u.%u.%u.%u", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff);
                                } else {
                                    inet_ntop(AF_INET6, fieldValue->value, ipAAAA, sizeof(ipAAAA));
                                }
                                BSB_EXPORT_sprintf(*jbsb, "ipv6hint:\\\"%s\\\" ", ipAAAA);
                                g_free((struct in6_addr *)fieldValue->value);
                            }
                            break;
                            }
                        }
                        BSB_EXPORT_rewind(*jbsb, 1); // remove the last space
                        BSB_EXPORT_cstr(*jbsb, "\",");
                        ARKIME_TYPE_FREE(DNSSVCBRData_t, answer->svcb);
                    }
                    break;
                    case DNS_RR_CAA: {
                        BSB_EXPORT_sprintf(*jbsb, "\"caa\":\"CAA %d %s ", answer->caa->flags, answer->caa->tag);
                        arkime_db_js0n_str_unquoted(jbsb, (uint8_t *)answer->caa->value, strlen(answer->caa->value), 1);
                        BSB_EXPORT_cstr(*jbsb, "\",");
                        g_free(answer->caa->tag);
                        g_free(answer->caa->value);
                        ARKIME_TYPE_FREE(DNSCAARData_t, answer->caa);
                    }
                    break;
                    }

                    BSB_EXPORT_sprintf(*jbsb, "\"class\":\"%s\",", answer->class);
                    BSB_EXPORT_sprintf(*jbsb, "\"type\":\"%s\",", answer->type);
                    BSB_EXPORT_sprintf(*jbsb, "\"ttl\":%u,", answer->ttl);

                    SAVE_STRING_HEAD(answer->flags, "flags");
                    BSB_EXPORT_sprintf(*jbsb, "\"name\":\"%s\",", answer->name);

                    if (answer->name && !(strcmp(answer->name, "<root>") == 0)) {
                        g_free(answer->name);
                    }

                    ARKIME_TYPE_FREE(DNSAnswer_t, answer);

                    BSB_EXPORT_rewind(*jbsb, 1); // Remove the last comma
                    BSB_EXPORT_u08(*jbsb, '}');
                    BSB_EXPORT_u08(*jbsb, ',');
                }
                BSB_EXPORT_rewind(*jbsb, 1); // Remove the last comma
                BSB_EXPORT_u08(*jbsb, ']');
                BSB_EXPORT_u08(*jbsb, ',');
            }
        }
    }

    BSB_EXPORT_rewind(*jbsb, 1); // Remove the last comma
    BSB_EXPORT_u08(*jbsb, '}');

#ifdef DNSDEBUG
    LOG("DNSDEBUG: JSON=%.*s\n", (int)(BSB_LENGTH(*jbsb) - offset), jbsb->buf + offset);
#endif
}
/*******************************************************************************************/
void dns_free_object(ArkimeFieldObject_t *object)
{
    if (object->object == NULL) {
        ARKIME_TYPE_FREE(ArkimeFieldObject_t, object);
        return;
    }

    DNS_t *dns = (DNS_t *)object->object;

    DNSAnswer_t *answer;

    ArkimeString_t *string;
    while (DLL_POP_HEAD(t_, &dns->answers, answer)) {
        while (DLL_POP_HEAD(s_, &answer->flags, string)) {
            g_free(string->str);
            ARKIME_TYPE_FREE(ArkimeString_t, string);
        }
        switch (answer->type_id) {
        case DNS_RR_A: {
            // Nothing to do
        }
        break;
        case DNS_RR_NS: {
            if (answer->nsdname) {
                g_free(answer->nsdname);
            }
        }
        break;
        case DNS_RR_CNAME: {
            if (answer->cname) {
                g_free(answer->cname);
            }
        }
        break;
        case DNS_RR_MX: {
            if (!answer->mx) {
                break;
            }
            if (answer->mx->exchange) {
                g_free(answer->mx->exchange);
            }
            ARKIME_TYPE_FREE(DNSMXRData_t, answer->mx);
        }
        break;
        case DNS_RR_AAAA: {
            if (answer->ipAAAA) {
                g_free(answer->ipAAAA);
            }
        }
        break;
        case DNS_RR_TXT: {
            if (answer->txt) {
                g_free(answer->txt);
            }
        }
        break;
        case DNS_RR_HTTPS: {
            if (!answer->svcb) {
                break;
            }
            if (answer->svcb->dname) {
                g_free(answer->svcb->dname);
            }
            DNSSVCBRDataFieldValue_t *fieldValue;
            while (DLL_COUNT(t_, &(answer->svcb->fieldValues)) > 0) {
                DLL_POP_HEAD(t_, &(answer->svcb->fieldValues), fieldValue);
                switch (fieldValue->key) {
                case SVCB_PARAM_KEY_ALPN: {
                    g_free((char *)fieldValue->value);
                }
                break;
                case SVCB_PARAM_KEY_PORT: {
                    ARKIME_TYPE_FREE(uint16_t, (uint16_t *)fieldValue->value);
                }
                break;
                case SVCB_PARAM_KEY_IPV4_HINT: {
                    ARKIME_TYPE_FREE(uint32_t, (uint32_t *)fieldValue->value);
                }
                break;
                case SVCB_PARAM_KEY_IPV6_HINT: {
                    g_free((struct in6_addr *)fieldValue->value);
                }
                break;
                }
            }
            ARKIME_TYPE_FREE(DNSSVCBRData_t, answer->svcb);
        }
        break;
        case DNS_RR_CAA: {
            if (!answer->caa) {
                break;
            }
            if (answer->caa->tag) {
                g_free(answer->caa->tag);
            }
            if (answer->caa->value) {
                g_free(answer->caa->value);
            }
            ARKIME_TYPE_FREE(DNSCAARData_t, answer->caa);
        }
        break;
        }

        if (answer->name && !(strcmp(answer->name, "<root>") == 0)) {
            g_free(answer->name);
        }
        ARKIME_TYPE_FREE(DNSAnswer_t, answer);
    }

    if (dns->query.hostname && !(strcmp(dns->query.hostname, "<root>") == 0)) {
        g_free(dns->query.hostname);
    }

    ArkimeString_t *hstring;

    HASH_FORALL_POP_HEAD2(s_, dns->hosts, hstring) {
        g_free(hstring->str);
        ARKIME_TYPE_FREE(ArkimeString_t, hstring);
    }

    if (dns->nsHosts) {
        HASH_FORALL_POP_HEAD2(s_, *(dns->nsHosts), hstring) {
            g_free(hstring->str);
            ARKIME_TYPE_FREE(ArkimeString_t, hstring);
        }
        ARKIME_TYPE_FREE(ArkimeStringHashStd_t, dns->nsHosts);
    }
    if (dns->mxHosts) {
        HASH_FORALL_POP_HEAD2(s_, *(dns->mxHosts), hstring) {
            g_free(hstring->str);
            ARKIME_TYPE_FREE(ArkimeString_t, hstring);
        }
        ARKIME_TYPE_FREE(ArkimeStringHashStd_t, dns->mxHosts);
    }
    if (dns->punyHosts) {
        HASH_FORALL_POP_HEAD2(s_, *(dns->punyHosts), hstring) {
            g_free(hstring->str);
            ARKIME_TYPE_FREE(ArkimeString_t, hstring);
        }
        ARKIME_TYPE_FREE(ArkimeStringHashStd_t, dns->punyHosts);
    }
    if (dns->ips) {
        g_hash_table_destroy(dns->ips);
    }
    if (dns->nsIPs) {
        g_hash_table_destroy(dns->nsIPs);
    }
    if (dns->mxIPs) {
        g_hash_table_destroy(dns->mxIPs);
    }

    ARKIME_TYPE_FREE(DNS_t, dns);
    ARKIME_TYPE_FREE(ArkimeFieldObject_t, object);
}
/*******************************************************************************************/
SUPPRESS_UNSIGNED_INTEGER_OVERFLOW
SUPPRESS_SHIFT
SUPPRESS_INT_CONVERSION
uint32_t dns_hash(const void *key)
{
    ArkimeFieldObject_t *obj = (ArkimeFieldObject_t *)key;

    if (obj->object == NULL) {
        return 0;
    }

    DNS_t *dns = (DNS_t *)obj->object;

    uint32_t hostname_hash = FNV_OFFSET;
    uint8_t *s = (uint8_t *) dns->query.hostname;

    while (*s) {
        hostname_hash ^= (uint32_t) * s++; // NOTE: make this toupper(*s) or tolower(*s) if you want case-insensitive hashes
        hostname_hash *= FNV_PRIME; // 32 bit magic FNV-1a prime
    }

    uint32_t hash = (hostname_hash ^ (dns->query.opcode_id << 24 | dns->query.packet_uid << 8) ^ (dns->query.type_id << 16 | dns->query.class_id));

#ifdef DNSDEBUG
    LOG("DNSDEBUG: Host hash: %u/Object hash: %u", hostname_hash, hash);
#endif

    return hash;
}
/*******************************************************************************************/
int dns_cmp(const void *keyv, const void *elementv)
{
    ArkimeFieldObject_t *key      = (ArkimeFieldObject_t *)keyv;
    ArkimeFieldObject_t *element = (ArkimeFieldObject_t *)elementv;

    DNS_t *keyDNS, *elementDNS;

    if (key->object == NULL || element->object == NULL) {
        return 0;
    }

    keyDNS = (DNS_t *)key->object;
    elementDNS = (DNS_t *)element->object;

    // Check the query fields
    if ( ! (keyDNS->query.packet_uid == elementDNS->query.packet_uid))
        return 0;
    if ( ! (keyDNS->query.opcode_id == elementDNS->query.opcode_id))
        return 0;
    if (strcmp(keyDNS->query.hostname, elementDNS->query.hostname) != 0)
        return 0;
    if (strcmp(keyDNS->query.class, elementDNS->query.class) != 0)
        return 0;
    if (strcmp(keyDNS->query.type, elementDNS->query.type) != 0)
        return 0;

    return 1;
}
/******************************************************************************/
LOCAL void *dns_getcb_host(ArkimeSession_t *session, int UNUSED(pos))
{
    if (!session->fields[dnsField])
        return NULL;

    GHashTable *hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

    ArkimeFieldObjectHashStd_t *ohash = session->fields[dnsField]->ohash;
    ArkimeFieldObject_t *object;

    HASH_FORALL2(o_, *ohash, object) {
        DNS_t *dns = (DNS_t *)object->object;
        g_hash_table_insert(hash, dns->query.hostname, (void *)1LL);
        ArkimeString_t *hstring = 0;
        HASH_FORALL2(s_, dns->hosts, hstring) {
            g_hash_table_insert(hash, hstring->str, (void *)1LL);
        }
    }

    arkime_free_later(hash, (GDestroyNotify) g_hash_table_destroy);
    return hash;
}
/******************************************************************************/
LOCAL void *dns_getcb_host_mailserver(ArkimeSession_t *session, int UNUSED(pos))
{
    if (!session->fields[dnsField])
        return NULL;

    GHashTable *hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

    ArkimeFieldObjectHashStd_t *ohash = session->fields[dnsField]->ohash;
    ArkimeFieldObject_t *object;

    HASH_FORALL2(o_, *ohash, object) {
        DNS_t *dns = (DNS_t *)object->object;
        if (dns->mxHosts) {
            ArkimeString_t *hstring = 0;
            HASH_FORALL2(s_, *(dns->mxHosts), hstring) {
                g_hash_table_insert(hash, hstring->str, (void *)1LL);
            }
        }
    }

    arkime_free_later(hash, (GDestroyNotify) g_hash_table_destroy);
    return hash;
}
/******************************************************************************/
LOCAL void *dns_getcb_host_nameserver(ArkimeSession_t *session, int UNUSED(pos))
{
    if (!session->fields[dnsField])
        return NULL;

    GHashTable *hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

    ArkimeFieldObjectHashStd_t *ohash = session->fields[dnsField]->ohash;
    ArkimeFieldObject_t *object;

    HASH_FORALL2(o_, *ohash, object) {
        DNS_t *dns = (DNS_t *)object->object;
        if (dns->nsHosts) {
            ArkimeString_t *hstring = 0;
            HASH_FORALL2(s_, *(dns->nsHosts), hstring) {
                g_hash_table_insert(hash, hstring->str, (void *)1LL);
            }
        }
    }

    arkime_free_later(hash, (GDestroyNotify) g_hash_table_destroy);
    return hash;
}
/******************************************************************************/
LOCAL void *dns_getcb_puny(ArkimeSession_t *session, int UNUSED(pos))
{
    if (!session->fields[dnsField])
        return NULL;

    GHashTable *hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

    ArkimeFieldObjectHashStd_t *ohash = session->fields[dnsField]->ohash;
    ArkimeFieldObject_t *object;

    HASH_FORALL2(o_, *ohash, object) {
        DNS_t *dns = (DNS_t *)object->object;
        if (dns->punyHosts) {
            ArkimeString_t *hstring = 0;
            HASH_FORALL2(s_, *(dns->punyHosts), hstring) {
                g_hash_table_insert(hash, hstring->str, (void *)1LL);
            }
        }
    }

    arkime_free_later(hash, (GDestroyNotify) g_hash_table_destroy);
    return hash;
}
/******************************************************************************/
LOCAL void *dns_getcb_status(ArkimeSession_t *session, int UNUSED(pos))
{
    if (!session->fields[dnsField])
        return NULL;

    GHashTable *hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

    ArkimeFieldObjectHashStd_t *ohash = session->fields[dnsField]->ohash;
    ArkimeFieldObject_t *object;

    HASH_FORALL2(o_, *ohash, object) {
        DNS_t *dns = (DNS_t *)object->object;
        if (dns->rcode)
            g_hash_table_insert(hash, dns->rcode, (void *)1LL);
    }

    arkime_free_later(hash, (GDestroyNotify) g_hash_table_destroy);
    return hash;
}
/******************************************************************************/
LOCAL void *dns_getcb_opcode(ArkimeSession_t *session, int UNUSED(pos))
{
    if (!session->fields[dnsField])
        return NULL;

    GHashTable *hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

    ArkimeFieldObjectHashStd_t *ohash = session->fields[dnsField]->ohash;
    ArkimeFieldObject_t *object;

    HASH_FORALL2(o_, *ohash, object) {
        DNS_t *dns = (DNS_t *)object->object;
        if (dns->query.opcode)
            g_hash_table_insert(hash, dns->query.opcode, (void *)1LL);
    }

    arkime_free_later(hash, (GDestroyNotify) g_hash_table_destroy);
    return hash;
}
/******************************************************************************/
LOCAL void *dns_getcb_query_type(ArkimeSession_t *session, int UNUSED(pos))
{
    if (!session->fields[dnsField])
        return NULL;

    GHashTable *hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

    ArkimeFieldObjectHashStd_t *ohash = session->fields[dnsField]->ohash;
    ArkimeFieldObject_t *object;

    HASH_FORALL2(o_, *ohash, object) {
        DNS_t *dns = (DNS_t *)object->object;
        if (dns->query.type)
            g_hash_table_insert(hash, dns->query.type, (void *)1LL);
    }

    arkime_free_later(hash, (GDestroyNotify) g_hash_table_destroy);
    return hash;
}
/******************************************************************************/
LOCAL void *dns_getcb_query_class(ArkimeSession_t *session, int UNUSED(pos))
{
    if (!session->fields[dnsField])
        return NULL;

    GHashTable *hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

    ArkimeFieldObjectHashStd_t *ohash = session->fields[dnsField]->ohash;
    ArkimeFieldObject_t *object;

    HASH_FORALL2(o_, *ohash, object) {
        DNS_t *dns = (DNS_t *)object->object;
        if (dns->query.class)
            g_hash_table_insert(hash, dns->query.class, (void *)1LL);
    }

    arkime_free_later(hash, (GDestroyNotify) g_hash_table_destroy);
    return hash;
}
/******************************************************************************/
LOCAL void *dns_getcb_query_host(ArkimeSession_t *session, int UNUSED(pos))
{
    if (!session->fields[dnsField])
        return NULL;

    GHashTable *hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

    ArkimeFieldObjectHashStd_t *ohash = session->fields[dnsField]->ohash;
    ArkimeFieldObject_t *object;

    HASH_FORALL2(o_, *ohash, object) {
        DNS_t *dns = (DNS_t *)object->object;
        g_hash_table_insert(hash, dns->query.hostname, (void *)1LL);
    }

    arkime_free_later(hash, (GDestroyNotify) g_hash_table_destroy);
    return hash;
}
/******************************************************************************/
void arkime_parser_init()
{
    dnsOutputAnswers = arkime_config_boolean(NULL, "dnsOutputAnswers", FALSE);

    dnsField = arkime_field_object_register("dns", "DNS Query/Responses", dns_save, dns_free_object, dns_hash, dns_cmp);

    arkime_field_define("dns", "ip",
                        "ip.dns", "IP",  "dns.ip",
                        "IP from DNS result",
                        ARKIME_FIELD_TYPE_IP_GHASH, ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_IPPRE,
                        "aliases", "[\"dns.ip\"]",
                        "category", "ip",
                        (char *)NULL);

    arkime_field_define("dns", "ip",
                        "ip.dns.nameserver", "IP",  "dns.nameserverIp",
                        "IPs for nameservers",
                        ARKIME_FIELD_TYPE_IP_GHASH, ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_IPPRE,
                        "category", "ip",
                        (char *)NULL);

    arkime_field_define("dns", "ip",
                        "ip.dns.mailserver", "IP",  "dns.mailserverIp",
                        "IPs for mailservers",
                        ARKIME_FIELD_TYPE_IP_GHASH, ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_IPPRE,
                        "category", "ip",
                        (char *)NULL);

    arkime_field_define("dns", "ip",
                        "ip.dns.all", "IP", "dnsipall",
                        "Shorthand for ip.dns or ip.dns.nameserver",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        "regex", "^ip\\\\.dns(?:(?!\\\\.(cnt|all)$).)*$",
                        (char *)NULL);

    arkime_field_define("dns", "lotermfield",
                        "host.dns", "Host", "dns.host",
                        "DNS lookup hostname",
                        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_FORCE_UTF8 | ARKIME_FIELD_FLAG_FAKE,
                        "aliases", "[\"dns.host\"]",
                        "category", "host",
                        (char *)NULL);

    dnsHostField = arkime_field_by_exp_add_internal("dns.host", ARKIME_FIELD_TYPE_STR_GHASH, dns_getcb_host, NULL);

    arkime_field_define("dns", "lotextfield",
                        "host.dns.tokens", "Hostname Tokens", "dns.hostTokens",
                        "DNS lookup hostname tokens",
                        ARKIME_FIELD_TYPE_STR_HASH, ARKIME_FIELD_FLAG_FAKE,
                        "aliases", "[\"dns.host.tokens\"]",
                        (char *)NULL);

    arkime_field_define("dns", "lotermfield",
                        "host.dns.nameserver", "NS Host", "dns.nameserverHost",
                        "Hostnames for Name Server",
                        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_FORCE_UTF8 | ARKIME_FIELD_FLAG_FAKE,
                        "category", "host",
                        (char *)NULL);

    dnsHostNameserverField = arkime_field_by_exp_add_internal("host.dns.nameserver", ARKIME_FIELD_TYPE_STR_GHASH, dns_getcb_host_nameserver, NULL);

    arkime_field_define("dns", "lotermfield",
                        "host.dns.mailserver", "MX Host", "dns.mailserverHost",
                        "Hostnames for Mail Exchange Server",
                        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_FORCE_UTF8 | ARKIME_FIELD_FLAG_FAKE,
                        "category", "host",
                        (char *)NULL);

    dnsHostMailserverField = arkime_field_by_exp_add_internal("host.dns.mailserver", ARKIME_FIELD_TYPE_STR_GHASH, dns_getcb_host_mailserver, NULL);

    arkime_field_define("dns", "lotermfield",
                        "host.dns.all", "All Host", "dnshostall",
                        "Shorthand for host.dns or host.dns.nameserver",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        "regex",  "^host\\\\.dns(?:(?!\\\\.(cnt|all)$).)*$",
                        (char *)NULL);

    arkime_field_define("dns", "lotermfield",
                        "dns.puny", "Puny", "dns.puny",
                        "DNS lookup punycode",
                        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    dnsPunyField = arkime_field_by_exp_add_internal("dns.puny", ARKIME_FIELD_TYPE_STR_GHASH, dns_getcb_puny, NULL);

    arkime_field_define("dns", "uptermfield",
                        "dns.status", "Status Code", "dns.status",
                        "DNS lookup return code",
                        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    dnsStatusField = arkime_field_by_exp_add_internal("dns.status", ARKIME_FIELD_TYPE_STR_GHASH, dns_getcb_status, NULL);

    arkime_field_define("dns", "uptermfield",
                        "dns.opcode", "Op Code", "dns.opcode",
                        "DNS lookup op code",
                        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    dnsOpcodeField = arkime_field_by_exp_add_internal("dns.opcode", ARKIME_FIELD_TYPE_STR_GHASH, dns_getcb_opcode, NULL);

    arkime_field_define("dns", "uptermfield",
                        "dns.query.type", "Query Type", "dns.qt",
                        "DNS lookup query type",
                        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    dnsQueryTypeField = arkime_field_by_exp_add_internal("dns.query.type", ARKIME_FIELD_TYPE_STR_GHASH, dns_getcb_query_type, NULL);

    arkime_field_define("dns", "uptermfield",
                        "dns.query.class", "Query Class", "dns.qc",
                        "DNS lookup query class",
                        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    dnsQueryClassField = arkime_field_by_exp_add_internal("dns.query.class", ARKIME_FIELD_TYPE_STR_GHASH, dns_getcb_query_class, NULL);

    arkime_field_define("dns", "lotermfield",
                        "dns.query.host", "Query Host", "dns.queryHost",
                        "DNS Query Name",
                        0, ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_FAKE,
                        "category", "host",
                        (char *)NULL);

    dnsQueryHostField = arkime_field_by_exp_add_internal("dns.query.host", ARKIME_FIELD_TYPE_STR_GHASH, dns_getcb_query_host, NULL);

    arkime_field_define("dns", "integer",
                        "dns.answersCnt", "DNS Answers Cnt", "dns.answersCnt",
                        "Count of DNS Answers",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("dns", "uptermfield",
                        "dns.answerType", "DNS Answer Type", "dns.answers.type",
                        "DNS Answer Type",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("dns", "uptermfield",
                        "dns.answerClass", "DNS Answer Class", "dns.answers.class",
                        "DNS Answer Class",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("dns", "integer",
                        "dns.answerTTL", "DNS Answer TTL", "dns.answers.ttl",
                        "DNS Answer TTL",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("dns", "ip",
                        "dns.answerIP", "DNS Answer IP", "dns.answers.ip",
                        "DNS Answer IP",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("dns", "termfield",
                        "dns.answerCNAME", "DNS Answer CNAME", "dns.answers.cname",
                        "DNS Answer CNAME",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("dns", "termfield",
                        "dns.answerNS", "DNS Answer NS", "dns.answers.nameserver",
                        "DNS Answer NS",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("dns", "termfield",
                        "dns.answerMX", "DNS Answer MX", "dns.answers.exchange",
                        "DNS Answer MX",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("dns", "termfield",
                        "dns.answerHTTPS", "DNS Answer HTTPS", "dns.answers.https",
                        "DNS Answer HTTPS",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("dns", "termfield",
                        "dns.answerTXT", "DNS Answer TXT", "dns.answers.txt",
                        "DNS Answer TXT",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("dns", "termfield",
                        "dns.answerCAA", "DNS Answer CAA", "dns.answers.caa",
                        "DNS Answer CAA",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("dns", "uptermfield",
                        "dns.answerFlags", "DNS Answer Flags", "dns.answers.flags",
                        "DNS Answer Flags",
                        0, ARKIME_FIELD_FLAG_FAKE | ARKIME_FIELD_FLAG_CNT,
                        (char *)NULL);

    arkime_field_define("dns", "termfield",
                        "dns.answerRRName", "DNS RR Name", "dns.answers.name",
                        "DNS Answer RR Name",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    qclasses[1]   = "IN";
    qclasses[2]   = "CS";
    qclasses[3]   = "CH";
    qclasses[4]   = "HS";
    qclasses[255] = "ANY";

    //http://en.wikipedia.org/wiki/List_of_DNS_record_types
    qtypes[1]   = "A";
    qtypes[2]   = "NS";
    qtypes[3]   = "MD";
    qtypes[4]   = "MF";
    qtypes[5]   = "CNAME";
    qtypes[6]   = "SOA";
    qtypes[7]   = "MB";
    qtypes[8]   = "MG";
    qtypes[9]   = "MR";
    qtypes[10]  = "NULL";
    qtypes[11]  = "WKS";
    qtypes[12]  = "PTR";
    qtypes[13]  = "HINFO";
    qtypes[14]  = "MINFO";
    qtypes[15]  = "MX";
    qtypes[16]  = "TXT";
    qtypes[17]  = "RP";
    qtypes[18]  = "AFSDB";
    qtypes[19]  = "X25";
    qtypes[20]  = "ISDN";
    qtypes[21]  = "RT";
    qtypes[22]  = "NSAP";
    qtypes[23]  = "NSAPPTR";
    qtypes[24]  = "SIG";
    qtypes[25]  = "KEY";
    qtypes[26]  = "PX";
    qtypes[27]  = "GPOS";
    qtypes[28]  = "AAAA";
    qtypes[29]  = "LOC";
    qtypes[30]  = "NXT";
    qtypes[31]  = "EID";
    qtypes[32]  = "NIMLOC";
    qtypes[33]  = "SRV";
    qtypes[34]  = "ATMA";
    qtypes[35]  = "NAPTR";
    qtypes[36]  = "KX";
    qtypes[37]  = "CERT";
    qtypes[38]  = "A6";
    qtypes[39]  = "DNAME";
    qtypes[40]  = "SINK";
    qtypes[41]  = "OPT";
    qtypes[42]  = "APL";
    qtypes[43]  = "DS";
    qtypes[44]  = "SSHFP";
    qtypes[46]  = "RRSIG";
    qtypes[47]  = "NSEC";
    qtypes[48]  = "DNSKEY";
    qtypes[49]  = "DHCID";
    qtypes[50]  = "NSEC3";
    qtypes[51]  = "NSEC3PARAM";
    qtypes[52]  = "TLSA";
    qtypes[55]  = "HIP";
    qtypes[65]  = "HTTPS";
    qtypes[99]  = "SPF";
    qtypes[249] = "TKEY";
    qtypes[250] = "TSIG";
    qtypes[252] = "AXFR";
    qtypes[253] = "MAILB";
    qtypes[254] = "MAILA";
    qtypes[255] = "ANY";
    qtypes[257] = "CAA";

    arkime_parsers_classifier_register_port("dns", NULL, 53, ARKIME_PARSERS_PORT_TCP_DST, dns_tcp_classify);

    arkime_parsers_classifier_register_port("dns",   (void *)(long)0,   53, ARKIME_PARSERS_PORT_UDP, dns_udp_classify);
    arkime_parsers_classifier_register_port("llmnr", (void *)(long)1, 5355, ARKIME_PARSERS_PORT_UDP, dns_udp_classify);
    arkime_parsers_classifier_register_port("mdns",  (void *)(long)2, 5353, ARKIME_PARSERS_PORT_UDP, dns_udp_classify);
}
