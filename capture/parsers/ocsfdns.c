#include "arkime.h"
#include <arpa/inet.h>

//#define OCSFDNSDEBUG 1


#define MAX_QTYPES   512
#define MAX_QCLASSES 256

#define FNV_OFFSET ((uint32_t)0x811c9dc5)
#define FNV_PRIME ((uint32_t)0x01000193)

LOCAL  char                 *qclasses[MAX_QCLASSES];
LOCAL  char                 *qtypes[MAX_QTYPES];
LOCAL  char                 *rcodes[24] = {"NOERROR", "FORMERR", "SERVFAIL", "NXDOMAIN", "NOTIMPL", "REFUSED", "YXDOMAIN", "YXRRSET", "NXRRSET", "NOTAUTH", "NOTZONE", "DSOTYPENI", "12", "13", "14", "15", "BADSIG_VERS", "BADKEY", "BADTIME", "BADMODE", "BADNAME", "BADALG", "BADTRUNC", "BADCOOKIE"};
LOCAL  char                 *opcodes[16] = {"QUERY", "IQUERY", "STATUS", "3", "NOTIFY", "UPDATE", "DSO Message", "7", "8", "9", "10", "11", "12", "13", "14", "15"};
LOCAL  char                 *flags[8] = {"UNKNOWN", "AUTHORITATIVE ANSWER", "TRUNCATED RESPONSE", "RECURSION DESIRED", "RECURSION AVAILABLE", "AUTHENTIC DATA", "CHECKING DISABLED", "OTHER"};
LOCAL  char                 *rr_types[5] = {"0", "Answer", "Authoritative", "Additional", "Unknown"};

typedef enum ocsf_dns_type {
    OCSFDNS_RR_A          =   1,
    OCSFDNS_RR_NS         =   2,
    OCSFDNS_RR_CNAME      =   5,
    OCSFDNS_RR_MX         =  15,
    OCSFDNS_RR_TXT        =  16,
    OCSFDNS_RR_AAAA       =  28,
    OCSFDNS_RR_CAA        = 257
} OCSFDNSType_t;

typedef enum ocsf_dns_class {
    CLASS_IN      =     1,
    CLASS_CS      =     2,
    CLASS_CH      =     3,
    CLASS_HS      =     4,
    CLASS_NONE    =   254,
    CLASS_ANY     =   255,
    CLASS_UNKNOWN = 65280
} OCSFDNSClass_t;

typedef enum ocsf_dns_result_record_type {
    RESULT_RECORD_ANSWER          =     1,    /* Answer or Prerequisites Record */
    RESULT_RECORD_AUTHORITATIVE   =     2,    /* Authoritative or Update Record */
    RESULT_RECORD_ADDITIONAL      =     3,    /* Additional Record */
    RESULT_RECORD_UNKNOWN         =     4,    /* Unknown Record*/
} OCSFDNSResultRecordType_t;

typedef struct ocsf_dns_answer_mxrdata {
    uint16_t preference;
    char    *exchange;
} OCSFDNSMXRDATA_t;

typedef struct ocsf_dns_answer_caadata {
    char *tag;
    char *value;
    uint8_t flags;
} OCSFDNSCAADATA_t;

typedef struct ocsf_dns_answer {
    struct ocsf_dns_answer  *t_next, *t_prev;
    ArkimeStringHead_t       flags;
    union {
        char                *cname;
        OCSFDNSMXRDATA_t    *mx;
        char                *nsdname;
        uint32_t             ipA;
        struct in6_addr     *ipAAAA;
        char                *txt;
        OCSFDNSCAADATA_t    *caa;
    };
    uint16_t                 packet_uid;
    char                    *class;
    char                    *type;
    char                    *rr_name;
    char                    *rr_type;
    uint16_t                 type_id; // Only used for choosing correct RDATA in union
    uint32_t                 ttl;
} OCSFDNSAnswer_t;

typedef struct {
    struct ocsf_dns_answer *t_next, *t_prev;
    int                     t_count;
} OCSFDNSAnswerHead_t;

typedef struct {
    uint8_t  opcode_id;
    uint16_t type_id;
    uint16_t class_id;
    char    *opcode;
    char    *hostname;
    uint16_t packet_uid;
    char    *class;
    char    *type;
} OCSFDNSQuery_t;

typedef struct ocsf_dns {
    struct ocsf_dns    *t_next, *t_prev;
    uint32_t            t_hash;
    short               t_bucket;
    uint8_t             activity_id;
    OCSFDNSAnswerHead_t answers;
    OCSFDNSQuery_t      query;
    struct timeval      query_ts;
    struct timeval      response_ts;
    char               *rcode;
    int8_t              rcode_id;
} OCSFDNS_t;

typedef struct {
    struct ocsf_dns *t_next, *t_prev;
    int              t_count;
} OCSFDNSHead_t;

typedef HASH_VAR(t_, OCSFDNSHash_t, OCSFDNSHead_t, 1);
typedef HASH_VAR(t_, OCSFDNSHashStd_t, OCSFDNSHead_t, 10);

typedef struct {
    uint8_t            *data[2];
    uint16_t            size[2];
    uint16_t            pos[2];
    uint16_t            len[2];
} OCSFDNSInfo_t;

extern ArkimeConfig_t        config;
LOCAL  int                   ocsfDNSField;
LOCAL char                   ocsfDNSStrictMode;

// forward declarations
void ocsf_dns_save(BSB *jbsb, ArkimeFieldObject_t *object, struct arkime_session *session);
void ocsf_dns_free_object(ArkimeFieldObject_t *object);
uint32_t ocsf_dns_hash(const void *key);
int ocsf_dns_cmp(const void *keyv, const void *elementv);

/******************************************************************************/
LOCAL void ocsf_dns_free(ArkimeSession_t *UNUSED(session), void *uw)
{
    OCSFDNSInfo_t            *info          = uw;

    if (info->data[0])
        free(info->data[0]);
    if (info->data[1])
        free(info->data[1]);
    ARKIME_TYPE_FREE(OCSFDNSInfo_t, info);
}
/******************************************************************************/
LOCAL int ocsf_dns_name_element(BSB *nbsb, BSB *bsb)
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
LOCAL char *ocsf_dns_name(const uint8_t *full, int fulllen, BSB *inbsb, char *name, int *namelen)
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

        if (ocsf_dns_name_element(&nbsb, curbsb) && BSB_LENGTH(nbsb))
            BSB_EXPORT_rewind(nbsb, 1); // Remove last .
    }
    *namelen = BSB_LENGTH(nbsb);
    BSB_EXPORT_u08(nbsb, 0);
    return name;
}
/******************************************************************************/
LOCAL void ocsf_dns_parser(ArkimeSession_t *session, int kind, const uint8_t *data, int len)
{

    if (len < 17)
        return;

    int preexistingObject = 0;

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

#ifdef OCSFDNSDEBUG
    LOG("OCSFDNSDEBUG: [Query/Zone Count: %d], [Answer or Prerequisite Count: %d], [Authoritative or Update RecordCount: %d], [Additional Record Count: %d]", qd_count, an_prereqs_count, ns_update_count, ar_count);
#endif

    if (qd_count != 1) {
        arkime_session_add_tag(session, "dns-qdcount-not-1");
        return;
    }

    ArkimeFieldObject_t *fobject = ARKIME_TYPE_ALLOC0(ArkimeFieldObject_t);
    OCSFDNS_t *dns = ARKIME_TYPE_ALLOC0(OCSFDNS_t);

    dns->query.packet_uid = id;
    fobject->object = dns;

    BSB bsb;
    BSB_INIT(bsb, data + 12, len - 12);

    /* QD Section */
    char namebuf[8000];
    int namelen = sizeof(namebuf);
    dns->query.hostname = g_hostname_to_unicode(ocsf_dns_name(data, len, &bsb, namebuf, &namelen));

    if (BSB_IS_ERROR(bsb) || !dns->query.hostname) {
        ocsf_dns_free_object(fobject);
        return;
    }

    if (!namelen) {
        g_free(dns->query.hostname);
        dns->query.hostname = (char *)"<root>";
        namelen = 6;
    }

    unsigned short qtype = 0, qclass = 0 ;
    BSB_IMPORT_u16(bsb, qtype);
    BSB_IMPORT_u16(bsb, qclass);

    if (qclass < MAX_QCLASSES && qclasses[qclass]) {
        dns->query.class    = g_strndup(qclasses[qclass], strlen(qclasses[qclass]));
        dns->query.class_id = qclass;
    }

    if (qtype < MAX_QTYPES && qtypes[qtype]) {
        dns->query.type    = g_strndup(qtypes[qtype], strlen(qtypes[qtype]));
        dns->query.type_id = qtype;
    }

    dns->query.opcode_id = opcode;
    dns->query.opcode = g_strndup(opcodes[opcode], strlen(opcodes[opcode]));

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


    if (qr == 0) {
        dns->rcode_id    = -1; // Not a response
        dns->query_ts = session->lastPacket;
        dns->activity_id = 1;
#ifdef OCSFDNSDEBUG
        LOG("OCSFDNSDEBUG: Parsed a query with TS secs: %lu, usecs: %lu", dns->query_ts.tv_sec, dns->query_ts.tv_usec);
#endif
        if (!arkime_field_object_add(ocsfDNSField, session, fobject, 720/*static + query*/)) {
            ocsf_dns_free_object(fobject);
            dns = 0;
        }
        return;
    }

    ArkimeFieldObject_t *existingFObject;

    if (session->fields[ocsfDNSField] && session->fields[ocsfDNSField]->ohash) {
        HASH_FIND_HASH(o_, *(session->fields[ocsfDNSField]->ohash), HASH_HASH(*(session->fields[ocsfDNSField]->ohash), fobject), fobject, existingFObject);
        if (existingFObject) {
            ocsf_dns_free_object(fobject);
            fobject = existingFObject;
            dns = (OCSFDNS_t *)fobject->object;
#ifdef OCSFDNSDEBUG
            LOG("OCSFDNSDEBUG: Retrieved an existing object");
#endif
            preexistingObject = 1;
            if (dns->answers.t_count == 0) {
                DLL_INIT(t_, &dns->answers);
            }
        } else {
#ifdef OCSFDNSDEBUG
            LOG("OCSFDNSDEBUG: No existing object");
#endif
            DLL_INIT(t_, &dns->answers);
        }
    } else {
#ifdef OCSFDNSDEBUG
        LOG("OCSFDNSDEBUG: No object added so far");
#endif
        DLL_INIT(t_, &dns->answers);
    }

    dns->rcode_id    = data[3] & 0xf;
    dns->rcode       = g_strndup(rcodes[dns->rcode_id], strlen(rcodes[dns->rcode_id]));
    dns->response_ts = session->lastPacket;
    if (dns->activity_id == 1) {
        dns->activity_id = 6;
    } else {
        dns->activity_id = 2;
    }

    int recordType = 0;
    int i;
    unsigned char txtLen = 0;
    unsigned short extraLen = 0;
    for (recordType = RESULT_RECORD_ANSWER; recordType <= RESULT_RECORD_ADDITIONAL; recordType++) {
        int recordNum = resultRecordCount[recordType - 1];
        for (i = 0; BSB_NOT_ERROR(bsb) && i < recordNum; i++) {
            char namebuf[8000];
            int namelen = sizeof(namebuf);
            char *name = g_hostname_to_unicode(ocsf_dns_name(data, len, &bsb, namebuf, &namelen));

            if (BSB_IS_ERROR(bsb) || !name)
                break;

#ifdef OCSFDNSDEBUG
            LOG("OCSFDNSDEBUG: RR Name=%s", name);
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
                break;
            }

            if (anclass != CLASS_IN) {
                BSB_IMPORT_skip(bsb, rdlength);
                continue;
            }

            OCSFDNSAnswer_t *answer = ARKIME_TYPE_ALLOC0(OCSFDNSAnswer_t);

            if (!namelen) {
                answer->rr_name = (char *)"<root>";
                namelen = 6;
                g_free(name);
            } else {
                answer->rr_name = name;
            }

            answer->rr_type = g_strndup(rr_types[recordType], strlen(rr_types[recordType]));

            switch (antype) {
            case OCSFDNS_RR_A: {
                if (rdlength != 4) {
                    BSB_IMPORT_skip(bsb, rdlength);
                    ARKIME_TYPE_FREE(OCSFDNSAnswer_t, answer);
                    continue;
                }

                const uint8_t *ptr = BSB_WORK_PTR(bsb);
                answer->ipA = ((uint32_t)(ptr[3])) << 24 | ((uint32_t)(ptr[2])) << 16 | ((uint32_t)(ptr[1])) << 8 | ptr[0];
#ifdef OCSFDNSDEBUG
                LOG("OCSFDNSDEBUG: RR_A=%u.%u.%u.%u, name=%s", answer->ipA & 0xff, (answer->ipA >> 8) & 0xff, (answer->ipA >> 16) & 0xff, (answer->ipA >> 24) & 0xff, answer->rr_name);
#endif
            }
            break;
            case OCSFDNS_RR_NS: {
                BSB rdbsb;
                BSB_INIT(rdbsb, BSB_WORK_PTR(bsb), rdlength);

                namelen = sizeof(namebuf);
                name = ocsf_dns_name(data, len, &rdbsb, namebuf, &namelen);

                if (!namelen || BSB_IS_ERROR(rdbsb) || !name) {
                    BSB_IMPORT_skip(bsb, rdlength);
                    ARKIME_TYPE_FREE(OCSFDNSAnswer_t, answer);
                    continue;
                }

#ifdef OCSFDNSDEBUG
                LOG("OCSFDNSDEBUG: RR_NS Name=%s", name);
#endif

                answer->nsdname = g_hostname_to_unicode(name);
            }
            break;
            case OCSFDNS_RR_CNAME: {
                BSB rdbsb;
                BSB_INIT(rdbsb, BSB_WORK_PTR(bsb), rdlength);

                namelen = sizeof(namebuf);
                name = ocsf_dns_name(data, len, &rdbsb, namebuf, &namelen);

                if (!namelen || BSB_IS_ERROR(rdbsb) || !name) {
                    BSB_IMPORT_skip(bsb, rdlength);
                    ARKIME_TYPE_FREE(OCSFDNSAnswer_t, answer);
                    continue;
                }

#ifdef OCSFDNSDEBUG
                LOG("OCSFDNSDEBUG: RR_CNAME Name=%s", name);
#endif

                answer->cname = g_hostname_to_unicode(name);
            }
            break;
            case OCSFDNS_RR_MX: {
                BSB rdbsb;
                BSB_INIT(rdbsb, BSB_WORK_PTR(bsb), rdlength);
                uint16_t mx_preference = 0;
                BSB_IMPORT_u16(rdbsb, mx_preference);

                namelen = sizeof(namebuf);
                name = ocsf_dns_name(data, len, &rdbsb, namebuf, &namelen);

                if (!namelen || BSB_IS_ERROR(rdbsb) || !name) {
                    BSB_IMPORT_skip(bsb, rdlength);
                    ARKIME_TYPE_FREE(OCSFDNSAnswer_t, answer);
                    continue;
                }

#ifdef OCSFDNSDEBUG
                LOG("OCSFDNSDEBUG: RR_MX Exchange=%s, Preference=%d", name, mx_preference);
#endif

                answer->mx = ARKIME_TYPE_ALLOC0(OCSFDNSMXRDATA_t);
                (answer->mx)->preference = mx_preference;
                (answer->mx)->exchange = g_hostname_to_unicode(name);
            }
            break;
            case OCSFDNS_RR_AAAA: {
                if (rdlength != 16) {
                    BSB_IMPORT_skip(bsb, rdlength);
                    ARKIME_TYPE_FREE(OCSFDNSAnswer_t, answer);
                    continue;
                }

                const uint8_t *ptr = BSB_WORK_PTR(bsb);

                answer->ipAAAA = g_memdup((const void *)ptr, sizeof(struct in6_addr));

#ifdef OCSFDNSDEBUG
                char ipbuf[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, answer->ipAAAA, ipbuf, sizeof(ipbuf));
                LOG("OCSFDNSDEBUG: RR_AAAA=%s, name=%s", ipbuf, answer->rr_name);
#endif
            }
            break;
            case OCSFDNS_RR_TXT: {
                BSB_IMPORT_u08(bsb, txtLen);
                const uint8_t *ptr = BSB_WORK_PTR(bsb);

                answer->txt = g_strndup((const char *)ptr, txtLen);

#ifdef OCSFDNSDEBUG
                LOG("OCSFDNSDEBUG: RR_TXT=%s", answer->txt);
#endif

                extraLen += txtLen;
                txtLen = 0;
            }
            break;
            case OCSFDNS_RR_CAA: {
                if (rdlength <= 3) {
                    BSB_IMPORT_skip(bsb, rdlength);
                    ARKIME_TYPE_FREE(OCSFDNSAnswer_t, answer);
                    continue;
                }

                BSB rdbsb;
                BSB_INIT(rdbsb, BSB_WORK_PTR(bsb), rdlength);

                answer->caa = ARKIME_TYPE_ALLOC0(OCSFDNSCAADATA_t);

                BSB_IMPORT_u08(rdbsb, answer->caa->flags);

                uint8_t tagLen = 0;
                BSB_IMPORT_u08(rdbsb, tagLen);

                uint16_t valueLen = rdlength - tagLen - 2;

                uint8_t *ptr = BSB_WORK_PTR(rdbsb);
                answer->caa->tag = g_strndup((const char *)ptr, tagLen);
                BSB_EXPORT_skip(rdbsb, tagLen);

                ptr = BSB_WORK_PTR(rdbsb);
                answer->caa->value = g_strndup((const char *)ptr, valueLen);

#ifdef OCSFDNSDEBUG
                LOG("OCSFDNSDEBUG: RR_CAA %d %s", answer->caa->flags, answer->caa->value);
#endif

                extraLen += tagLen + valueLen;
            }
            break;
            } /* switch */
            BSB_IMPORT_skip(bsb, rdlength);

            if (anclass <= 255 && qclasses[anclass]) {
                answer->class = g_strndup(qclasses[anclass], strlen(qclasses[anclass]));
            }

            if (antype <= 255 && qtypes[antype]) {
                answer->type = g_strndup(qtypes[antype], strlen(qtypes[antype]));
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
        } // record loop
    } // record type loop

    if (!arkime_field_object_add(ocsfDNSField, session, fobject, 720/*static + query*/ + (resultRecordCount[0] + resultRecordCount[1] + resultRecordCount[2]) * 180/*180 per RR*/ + extraLen) && !preexistingObject) {
        ocsf_dns_free_object(fobject);
        dns = 0;
    }
}
/******************************************************************************/
LOCAL int ocsf_dns_tcp_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int which)
{
    OCSFDNSInfo_t *info = uw;
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
                ocsf_dns_parser(session, 0, data + 2, dnslength);
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
                ocsf_dns_parser(session, 0, info->data[which], info->len[which]);
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
LOCAL void ocsf_dns_tcp_classify(ArkimeSession_t *session, const uint8_t *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    if (session->port2 == 53 && !arkime_session_has_protocol(session, "dns")) {
        arkime_session_add_protocol(session, "dns");
        OCSFDNSInfo_t  *info = ARKIME_TYPE_ALLOC0(OCSFDNSInfo_t);
        arkime_parsers_register(session, ocsf_dns_tcp_parser, info, ocsf_dns_free);
    }
}
/******************************************************************************/
LOCAL int ocsf_dns_udp_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int UNUSED(which))
{
    if (uw == 0 || (session->port1 != 53 && session->port2 != 53)) {
        ocsf_dns_parser(session, (long)uw, data, len);
    }
    return 0;
}
/******************************************************************************/
LOCAL void oscf_dns_udp_classify(ArkimeSession_t *session, const uint8_t *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    arkime_parsers_register(session, ocsf_dns_udp_parser, uw, 0);
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
/*******************************************************************************************/
void ocsf_dns_save(BSB *jbsb, ArkimeFieldObject_t *object, struct arkime_session *session)
{
    if (object->object == NULL) {
        return;
    }

    char ipsrc[INET6_ADDRSTRLEN];
    char ipdst[INET6_ADDRSTRLEN];
    char ipAAAA[INET6_ADDRSTRLEN];

    OCSFDNS_t *dns = (OCSFDNS_t *)object->object;

    BSB_EXPORT_u08(*jbsb, '{');

    if (ocsfDNSStrictMode) {
        BSB_EXPORT_cstr(*jbsb, "\"category_uid\":4,");
        BSB_EXPORT_cstr(*jbsb, "\"class_uid\":4003,");
        BSB_EXPORT_cstr(*jbsb, "\"type_uid\":400306,");
        BSB_EXPORT_cstr(*jbsb, "\"severity_id\":1,");
        BSB_EXPORT_cstr(*jbsb, "\"metadata\":{\"product\":{\"vendor_name\":\"arkime\"},\"version\":\"1.1.0\"},");
        BSB_EXPORT_sprintf(*jbsb, "\"activity_uid\":%u,", dns->activity_id);

        struct timeval currentTime;
        gettimeofday(&currentTime, NULL);

        BSB_EXPORT_sprintf(*jbsb,
                           "\"time\":%" PRIu64 ",",
                           ((uint64_t)currentTime.tv_sec) * 1000 + ((uint64_t)currentTime.tv_usec) / 1000);

        BSB_EXPORT_sprintf(*jbsb,
                           "\"query_time\":%" PRIu64 ",",
                           ((uint64_t)dns->query_ts.tv_sec) * 1000 + ((uint64_t)dns->query_ts.tv_usec) / 1000);

        BSB_EXPORT_sprintf(*jbsb,
                           "\"response_time\":%" PRIu64 ",",
                           ((uint64_t)dns->response_ts.tv_sec) * 1000 + ((uint64_t)dns->response_ts.tv_usec) / 1000);
    }

    BSB_EXPORT_cstr(*jbsb, "\"query\":{");
    BSB_EXPORT_sprintf(*jbsb, "\"opcode_id\":%u,", dns->query.opcode_id);
    BSB_EXPORT_sprintf(*jbsb, "\"opcode\":\"%s\",", dns->query.opcode);
    BSB_EXPORT_sprintf(*jbsb, "\"packet_uid\":%u,", dns->query.packet_uid);
    BSB_EXPORT_sprintf(*jbsb, "\"hostname\":\"%s\",", dns->query.hostname);
    BSB_EXPORT_sprintf(*jbsb, "\"class\":\"%s\",", dns->query.class);
    BSB_EXPORT_sprintf(*jbsb, "\"type\":\"%s\",", dns->query.type);
    BSB_EXPORT_rewind(*jbsb, 1); // Remove last comma
    BSB_EXPORT_u08(*jbsb, '}');
    BSB_EXPORT_u08(*jbsb, ',');

    if (ocsfDNSStrictMode) {
        if (IN6_IS_ADDR_V4MAPPED(&session->addr1)) {
            uint32_t ip = ARKIME_V6_TO_V4(session->addr1);
            snprintf(ipsrc, sizeof(ipsrc), "%u.%u.%u.%u", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff);
            ip = ARKIME_V6_TO_V4(session->addr2);
            snprintf(ipdst, sizeof(ipdst), "%u.%u.%u.%u", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff);
        } else {
            inet_ntop(AF_INET6, &session->addr1, ipsrc, sizeof(ipsrc));
            inet_ntop(AF_INET6, &session->addr2, ipdst, sizeof(ipdst));
        }

        BSB_EXPORT_cstr(*jbsb, "\"dst_endpoint\":{");
        BSB_EXPORT_sprintf(*jbsb, "\"ip\":\"%s\",", ipdst);
        BSB_EXPORT_sprintf(*jbsb, "\"port\":%u,", session->port2);
        BSB_EXPORT_rewind(*jbsb, 1); // Remove last comma
        BSB_EXPORT_u08(*jbsb, '}');
        BSB_EXPORT_u08(*jbsb, ',');

        BSB_EXPORT_cstr(*jbsb, "\"src_endpoint\":{");
        BSB_EXPORT_sprintf(*jbsb, "\"ip\":\"%s\",", ipsrc);
        BSB_EXPORT_sprintf(*jbsb, "\"port\":%u,", session->port1);
        BSB_EXPORT_rewind(*jbsb, 1); // Remove last comma
        BSB_EXPORT_u08(*jbsb, '}');
        BSB_EXPORT_u08(*jbsb, ',');
    }

    if (dns->rcode_id != -1) {
        BSB_EXPORT_sprintf(*jbsb, "\"rcode_id\":%u,", dns->rcode_id);
        BSB_EXPORT_sprintf(*jbsb, "\"rcode\":\"%s\",", dns->rcode);
        BSB_EXPORT_sprintf(*jbsb, "\"answersCnt\":%d,", DLL_COUNT(t_, &dns->answers));
        if (DLL_COUNT(t_, &dns->answers) > 0) {
            BSB_EXPORT_cstr(*jbsb, "\"answers\":[");
            OCSFDNSAnswer_t *answer;
            while (DLL_POP_HEAD(t_, &dns->answers, answer)) {
                BSB_EXPORT_u08(*jbsb, '{');
                switch (answer->type_id) {
                case OCSFDNS_RR_A: {
                    BSB_EXPORT_sprintf(*jbsb, "\"rdata\":\"%u.%u.%u.%u\",", answer->ipA & 0xff, (answer->ipA >> 8) & 0xff, (answer->ipA >> 16) & 0xff, (answer->ipA >> 24) & 0xff);
                }
                break;
                case OCSFDNS_RR_NS: {
                    BSB_EXPORT_sprintf(*jbsb, "\"rdata\":\"%s\",", answer->nsdname);
                    g_free(answer->nsdname);
                }
                break;
                case OCSFDNS_RR_CNAME: {
                    BSB_EXPORT_sprintf(*jbsb, "\"rdata\":\"%s\",", answer->cname);
                    g_free(answer->cname);
                }
                break;
                case OCSFDNS_RR_MX: {
                    BSB_EXPORT_sprintf(*jbsb, "\"rdata\":\"(%u)%s\",", answer->mx->preference, answer->mx->exchange);
                    g_free(answer->mx->exchange);
                    ARKIME_TYPE_FREE(OCSFDNSMXRDATA_t, answer->mx);
                }
                break;
                case OCSFDNS_RR_AAAA: {
                    if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)answer->ipAAAA)) {
                        uint32_t ip = ARKIME_V6_TO_V4(*(struct in6_addr *)answer->ipAAAA);
                        snprintf(ipAAAA, sizeof(ipAAAA), "%u.%u.%u.%u", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff);
                    } else {
                        inet_ntop(AF_INET6, answer->ipAAAA, ipAAAA, sizeof(ipAAAA));
                    }
                    BSB_EXPORT_sprintf(*jbsb, "\"rdata\":\"%s\",", ipAAAA);
                    g_free(answer->ipAAAA);
                }
                break;
                case OCSFDNS_RR_TXT: {
                    BSB_EXPORT_cstr(*jbsb, "\"rdata\":");
                    arkime_db_js0n_str(jbsb, (uint8_t *)answer->txt, 1);
                    BSB_EXPORT_u08(*jbsb, ',');
                    g_free(answer->txt);
                }
                break;
                case OCSFDNS_RR_CAA: {
                    BSB_EXPORT_sprintf(*jbsb, "\"rdata\":\"CAA %d %s ", answer->caa->flags, answer->caa->tag);
                    arkime_db_js0n_str_unquoted(jbsb, (uint8_t *)answer->caa->value, strlen(answer->caa->value), 1);
                    BSB_EXPORT_cstr(*jbsb, "\",");
                    g_free(answer->caa->tag);
                    g_free(answer->caa->value);
                    ARKIME_TYPE_FREE(OCSFDNSCAADATA_t, answer->caa);
                }
                break;
                }

                BSB_EXPORT_sprintf(*jbsb, "\"class\":\"%s\",", answer->class);
                if (answer->class) {
                    g_free(answer->class);
                }
                BSB_EXPORT_sprintf(*jbsb, "\"type\":\"%s\",", answer->type);
                if (answer->type) {
                    g_free(answer->type);
                }
                BSB_EXPORT_sprintf(*jbsb, "\"packet_uid\":%u,", answer->packet_uid);
                BSB_EXPORT_sprintf(*jbsb, "\"ttl\":%u,", answer->ttl);

                ArkimeString_t *string;
                SAVE_STRING_HEAD(answer->flags, "flags");
                if (!ocsfDNSStrictMode) {
                    BSB_EXPORT_sprintf(*jbsb, "\"rr_name\":\"%s\",", answer->rr_name);
                    BSB_EXPORT_sprintf(*jbsb, "\"rr_type\":\"%s\",", answer->rr_type);
                }
                if (answer->rr_type) {
                    g_free(answer->rr_type);
                }
                if (answer->rr_name && !(strcmp(answer->rr_name, "<root>") == 0)) {
                    g_free(answer->rr_name);
                }

                ARKIME_TYPE_FREE(OCSFDNSAnswer_t, answer);

                BSB_EXPORT_rewind(*jbsb, 1); // Remove the last comma
                BSB_EXPORT_u08(*jbsb, '}');
                BSB_EXPORT_u08(*jbsb, ',');
            }
            BSB_EXPORT_rewind(*jbsb, 1); // Remove the last comma
            BSB_EXPORT_u08(*jbsb, ']');
            BSB_EXPORT_u08(*jbsb, ',');
        }
        BSB_EXPORT_rewind(*jbsb, 1); // Remove the last comma
    }

    BSB_EXPORT_u08(*jbsb, '}');
}
/*******************************************************************************************/
void ocsf_dns_free_object(ArkimeFieldObject_t *object)
{
    if (object->object == NULL) {
        ARKIME_TYPE_FREE(ArkimeFieldObject_t, object);
        return;
    }

    OCSFDNS_t *dns = (OCSFDNS_t *)object->object;

    OCSFDNSAnswer_t *answer;

    ArkimeString_t *string;
    while (DLL_POP_HEAD(t_, &dns->answers, answer)) {
        while (DLL_POP_HEAD(s_, &answer->flags, string)) {
            g_free(string->str);
            ARKIME_TYPE_FREE(ArkimeString_t, string);
        }
        switch (answer->type_id) {
        case OCSFDNS_RR_A: {
            // Nothing to do
        }
        break;
        case OCSFDNS_RR_NS: {
            if (answer->nsdname) {
                g_free(answer->nsdname);
            }
        }
        break;
        case OCSFDNS_RR_CNAME: {
            if (answer->cname) {
                g_free(answer->cname);
            }
        }
        break;
        case OCSFDNS_RR_MX: {
            if (answer->mx->exchange) {
                g_free(answer->mx->exchange);
            }
            ARKIME_TYPE_FREE(OCSFDNSMXRDATA_t, answer->mx);
        }
        break;
        case OCSFDNS_RR_AAAA: {
            if (answer->ipAAAA) {
                g_free(answer->ipAAAA);
            }
        }
        break;
        case OCSFDNS_RR_TXT: {
            if (answer->txt) {
                g_free(answer->txt);
            }
        }
        break;
        case OCSFDNS_RR_CAA: {
            if (answer->caa->tag) {
                g_free(answer->caa->tag);
            }
            if (answer->caa->value) {
                g_free(answer->caa->value);
            }
            ARKIME_TYPE_FREE(OCSFDNSCAADATA_t, answer->caa);
        }
        break;
        }

        if (answer->class) {
            g_free(answer->class);
        }
        if (answer->type) {
            g_free(answer->type);
        }
        if (answer->rr_type) {
            g_free(answer->rr_type);
        }
        if (answer->rr_name && !(strcmp(answer->rr_name, "<root>") == 0)) {
            g_free(answer->rr_name);
        }
        ARKIME_TYPE_FREE(OCSFDNSAnswer_t, answer);
    }

    if (dns->query.hostname && !(strcmp(dns->query.hostname, "<root>") == 0)) {
        g_free(dns->query.hostname);
    }
    if (dns->query.class) {
        g_free(dns->query.class);
    }
    if (dns->query.type) {
        g_free(dns->query.type);
    }
    if (dns->query.opcode) {
        g_free(dns->query.opcode);
    }
    if (dns->rcode) {
        g_free(dns->rcode);
    }

    ARKIME_TYPE_FREE(OCSFDNS_t, dns);
    ARKIME_TYPE_FREE(ArkimeFieldObject_t, object);
}
/*******************************************************************************************/
SUPPRESS_UNSIGNED_INTEGER_OVERFLOW
SUPPRESS_SHIFT
SUPPRESS_INT_CONVERSION
uint32_t ocsf_dns_hash(const void *key)
{
    ArkimeFieldObject_t *obj = (ArkimeFieldObject_t *)key;

    if (obj->object == NULL) {
        return 0;
    }

    OCSFDNS_t *dns = (OCSFDNS_t *)obj->object;

    uint32_t hostname_hash = FNV_OFFSET;
    uint8_t *s = (uint8_t *) dns->query.hostname;

    while (*s) {
        hostname_hash ^= (uint32_t) * s++; // NOTE: make this toupper(*s) or tolower(*s) if you want case-insensitive hashes
        hostname_hash *= FNV_PRIME; // 32 bit magic FNV-1a prime
    }

    uint32_t hash = (hostname_hash ^ (dns->query.opcode_id << 24 | dns->query.packet_uid << 8) ^ (dns->query.type_id << 16 | dns->query.class_id));

#ifdef OCSFDNSDEBUG
    LOG("OCSFDNSDEBUG: Host hash: %u/Object hash: %u", hostname_hash, hash);
#endif

    return hash;
}
/*******************************************************************************************/
int ocsf_dns_cmp(const void *keyv, const void *elementv)
{
    ArkimeFieldObject_t *key      = (ArkimeFieldObject_t *)keyv;
    ArkimeFieldObject_t *element = (ArkimeFieldObject_t *)elementv;

    OCSFDNS_t *keyODNS, *elementODNS;

    if (key->object == NULL || element->object == NULL) {
        return 0;
    }

    keyODNS = (OCSFDNS_t *)key->object;
    elementODNS = (OCSFDNS_t *)element->object;

    // Check the query fields
    if ( ! (keyODNS->query.packet_uid == elementODNS->query.packet_uid))
        return 0;
    if ( ! (keyODNS->query.opcode_id == elementODNS->query.opcode_id))
        return 0;
    if (strcmp(keyODNS->query.hostname, elementODNS->query.hostname) != 0)
        return 0;
    if (strcmp(keyODNS->query.class, elementODNS->query.class) != 0)
        return 0;
    if (strcmp(keyODNS->query.type, elementODNS->query.type) != 0)
        return 0;

    return 1;
}
/******************************************************************************/
void arkime_parser_init()
{
    ocsfDNSStrictMode = arkime_config_boolean(NULL, "ocsfStrictMode", FALSE);

    ocsfDNSField = arkime_field_object_register("ocsfdns", "DNS Query/Responses in OCSF format", ocsf_dns_save, ocsf_dns_free_object, ocsf_dns_hash, ocsf_dns_cmp);

    arkime_field_define("ocsfdns", "integer",
                        "ocsfdns.cnt", "OCSF DNS events Cnt", "ocsfdnsCnt",
                        "Count of OCSF DNS events",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("ocsfdns", "uptermfield",
                        "ocsfdns.responseCode", "OCSF DNS Response Code", "ocsfdns.rcode",
                        "OCSF DNS Response code",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("ocsfdns", "uptermfield",
                        "ocsfdns.opcode", "OCSF DNS Opcode", "ocsfdns.query.opcode",
                        "OCSF DNS Opcode",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("ocsfdns", "uptermfield",
                        "ocsfdns.queryType", "OCSF DNS Query Type", "ocsfdns.query.type",
                        "OCSF DNS Query Type",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("ocsfdns", "uptermfield",
                        "ocsfdns.queryClass", "OCSF DNS Query Class", "ocsfdns.query.class",
                        "OCSF DNS Query Class",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("ocsfdns", "termfield",
                        "ocsfdns.hostname", "OCSF DNS Query Hostname", "ocsfdns.query.hostname",
                        "OCSF DNS Query Hostname",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("ocsfdns", "integer",
                        "ocsfdns.packetUID", "OCSF DNS Query Packet UID", "ocsfdns.query.packet_uid",
                        "OCSF DNS Query Packet UID",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("ocsfdns", "integer",
                        "ocsfdns.answersCnt", "OCSF DNS Answers Cnt", "ocsfdns.answersCnt",
                        "Count of OCSF DNS Answers",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("ocsfdns", "uptermfield",
                        "ocsfdns.answerType", "OCSF DNS Answer Type", "ocsfdns.answers.type",
                        "OCSF DNS Answer Type",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("ocsfdns", "uptermfield",
                        "ocsfdns.answerClass", "OCSF DNS Answer Class", "ocsfdns.answers.class",
                        "OCSF DNS Answer Class",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("ocsfdns", "integer",
                        "ocsfdns.answerPacketUID", "OCSF DNS Answer Packet UID", "ocsfdns.answers.packet_uid",
                        "OCSF DNS Answer Packet UID",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("ocsfdns", "integer",
                        "ocsfdns.answerTTL", "OCSF DNS Answer TTL", "ocsfdns.answers.ttl",
                        "OCSF DNS Answer TTL",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("ocsfdns", "termfield",
                        "ocsfdns.answerRDATA", "OCSF DNS Answer RDATA", "ocsfdns.answers.rdata",
                        "OCSF DNS Answer RDATA",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("ocsfdns", "uptermfield",
                        "ocsfdns.answerFlags", "OCSF DNS Answer Flags", "ocsfdns.answers.flags",
                        "OCSF DNS Answer Flags",
                        0, ARKIME_FIELD_FLAG_FAKE | ARKIME_FIELD_FLAG_CNT,
                        (char *)NULL);

    arkime_field_define("ocsfdns", "termfield",
                        "ocsfdns.answerRRName", "OCSF DNS RR Name", "ocsfdns.answers.rr_name",
                        "OCSF DNS Answer RR Name",
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
    qtypes[99]  = "SPF";
    qtypes[249] = "TKEY";
    qtypes[250] = "TSIG";
    qtypes[252] = "AXFR";
    qtypes[253] = "MAILB";
    qtypes[254] = "MAILA";
    qtypes[255] = "ANY";
    qtypes[257] = "CAA";

    arkime_parsers_classifier_register_port("dns", NULL, 53, ARKIME_PARSERS_PORT_TCP_DST, ocsf_dns_tcp_classify);

    arkime_parsers_classifier_register_port("dns",   (void *)(long)0,   53, ARKIME_PARSERS_PORT_UDP, oscf_dns_udp_classify);
    arkime_parsers_classifier_register_port("llmnr", (void *)(long)1, 5355, ARKIME_PARSERS_PORT_UDP, oscf_dns_udp_classify);
    arkime_parsers_classifier_register_port("mdns",  (void *)(long)2, 5353, ARKIME_PARSERS_PORT_UDP, oscf_dns_udp_classify);
}
