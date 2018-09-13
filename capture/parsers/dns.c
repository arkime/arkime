/* Copyright 2012-2017 AOL Inc. All rights reserved.
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
#include "moloch.h"

LOCAL  char                 *qclasses[256];
LOCAL  char                 *qtypes[256];
LOCAL  char                 *statuses[16] = {"NOERROR", "FORMERR", "SERVFAIL", "NXDOMAIN", "NOTIMPL", "REFUSED", "YXDOMAIN", "YXRRSET", "NXRRSET", "NOTAUTH", "NOTZONE", "11", "12", "13", "14", "15"};
LOCAL  char                 *opcodes[16] = {"QUERY", "IQUERY", "STATUS", "3", "NOTIFY", "UPDATE", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"};

LOCAL  int                   ipField;
LOCAL  int                   hostField;
LOCAL  int                   punyField;
LOCAL  int                   queryTypeField;
LOCAL  int                   queryClassField;
LOCAL  int                   statusField;
LOCAL  int                   opCodeField;

/*------------VISA Enhancement Begin---------------*/
LOCAL int                   ipQueryField;
LOCAL int                   ipAnswerField;
LOCAL int                   ipAuthoritativeField;
LOCAL int                   ipAdditionalField;
LOCAL int                   ipUpdateField;

LOCAL int                   hostQueryField;
LOCAL int                   hostAnswerField;
LOCAL int                   hostAuthoritativeField;
LOCAL int                   hostAdditionalField;
LOCAL int                   hostUpdateField;
/*------------VISA Enhancement End---------------*/

/*------------VISA Enhancement Begin---------------*/

/* Reference https://github.com/spc476/SPCDNS/blob/master/src/dns.h */
typedef enum dns_type
{
  RR_A          =   1,  /* IPv4 Address                     + RFC-1035 */
  RR_NS         =   2,  /* Name server                      + RFC-1035 */
  RR_MD         =   3,  /* Mail Destination                 O+ RFC-1035 */
  RR_MF         =   4,  /* Mail Forwarder                   O+ RFC-1035 */
  RR_CNAME      =   5,  /* Canonical name                   + RFC-1035 */
  RR_SOA        =   6,  /* Start of Authority               + RFC-1035 */
  RR_MB         =   7,  /* Mailbox                          E+ RFC-1035 */
  RR_MG         =   8,  /* Mailgroup                        E+ RFC-1035 */
  RR_MR         =   9,  /* Mailrename                       E+ RFC-1035 */
  RR_NULL       =  10,  /* NULL resource                    E+ RFC-1035 */
  RR_WKS        =  11,  /* Well Known Service               + RFC-1035 */
  RR_PTR        =  12,  /* Pointer                          + RFC-1035 */
  RR_HINFO      =  13,  /* Host Info                        + RFC-1035 */
  RR_MINFO      =  14,  /* Mailbox/mail list info           + RFC-1035 */
  RR_MX         =  15,  /* Mail Exchange                    + RFC-1035 */
  RR_TXT        =  16,  /* Text                             + RFC-1035 */
  RR_RP         =  17,  /* Responsible Person               + RFC-1183 */
  RR_AFSDB      =  18,  /* Andrew File System DB            + RFC-1183 RFC-5864 */
  RR_X25        =  19,  /* X.25 address, route binding      + RFC-1183 */
  RR_ISDN       =  20,  /* ISDN address, route binding      + RFC-1183 */
  RR_RT         =  21,  /* Route Through                    + RFC-1183 */
  RR_NSAP       =  22,  /* Network Service Access Proto     + RFC-1348 RFC-1706 */
  RR_NSAP_PTR   =  23,  /* NSAP Pointer                     + RFC-1348 */
  RR_SIG        =  24,  /* Signature                        RFC-2065 RFC-2535 RFC-3755 RFC-4034 */
  RR_KEY        =  25,  /* Key                              RFC-2065 RFC-2535 RFC-3755 RFC-4034 */
  RR_PX         =  26,  /* X.400 mail mapping               + RFC-2163 */
  RR_GPOS       =  27,  /* Geographical position            O+ RFC-1712 */
  RR_AAAA       =  28,  /* IPv6 Address                     + RFC-1886 RFC-3596 */
  RR_LOC        =  29,  /* Location                         + RFC-1876 */
  RR_NXT        =  30,  /* Next RR                          RFC-2065 RFC-2535 RFC-3755 */
  RR_EID        =  31,  /* Endpoint Identifier              ???      */
  RR_NIMLOC     =  32,  /* Nimrod Locator                   ???      */
  RR_SRV        =  33,  /* Service                          + RFC-2782 */
  RR_ATM        =  34,  /* ATM Address                      ???      */
  RR_NAPTR      =  35,  /* Naming Authority Pointer         + RFC-2168 RFC-2915 RFC-3403 */
  RR_KX         =  36,  /* Key Exchange                     RFC-2230 */
  RR_CERT       =  37,  /* Certification                    RFC-4398 */
  RR_A6         =  38,  /* IPv6 Address                     RFC-2874 RFC-3658 */
  RR_DNAME      =  39,  /* Non-terminal DNAME (IPv6)        RFC-2672 */
  RR_SINK       =  40,  /* Kitchen sink                     E  ???      */
  RR_OPT        =  41,  /* EDNS0 option (meta-RR)           + RFC-2671 */
  RR_APL        =  42,  /* Address Prefix List              RFC-3123 */
  RR_DS         =  43,  /* Delegation Signer                RFC-3658 RFC-4034 */
  RR_SSHFP      =  44,  /* SSH Key Fingerprint              RFC-4255 */
  RR_ISECKEY    =  45,  /* IP Security Key                  RFC-4025 */
  RR_RRSIG      =  46,  /* Resource Record Signature        RFC-3755 RFC-4034 */
  RR_NSEC       =  47,  /* Next Security Record             RFC-3755 RFC-4034 */
  RR_DNSKEY     =  48,  /* DNS Security Key                 RFC-3755 RFC-4034 */
  RR_DHCID      =  49,  /* DHCID                            RFC-4701 */
  RR_NSEC3      =  50,  /* NSEC3                            RFC-5155 */
  RR_NSEC3PARAM =  51,  /* NSEC3PARAM                       RFC-5155 */
  RR_HIP        =  55,  /* Host Identity Protocol           RFC-5205 */
  RR_NINFO      =  56,  /* NINFO                            ???      */
  RR_RKEY       =  57,  /* RKEY                             ???      */
  RR_TALINK     =  58,  /* Trust Anchor Link                ???      */
  RR_SPF        =  99,  /* Sender Policy Framework          O+ RFC-4408 */
  RR_UINFO      = 100,  /* IANA Reserved                    ???      */
  RR_UID        = 101,  /* IANA Reserved                    ???      */
  RR_GID        = 102,  /* IANA Reserved                    ???      */
  RR_UNSPEC     = 103,  /* IANA Reserved                    ???      */

    /* Query types, >= 128 */
  
  RR_TKEY       = 249,  /* Transaction Key                  RFC-2930 */
  RR_TSIG       = 250,  /* Transaction Signature            RFC-2845 */
  RR_IXFR       = 251,  /* Incremental zone transfer        RFC-1995 */
  RR_AXFR       = 252,  /* Transfer of zone                 RFC-1035 RFC-5936 */
  RR_MAILB      = 253,  /* Mailbox related records          RFC-1035 */
  RR_MAILA      = 254,  /* Mail agent RRs (obsolete)        O  RFC-1035 */
  RR_ANY        = 255,  /* All records                      RFC-1035 */

  RR_UNKNOWN    = 65280 /* Unknown record type              RFC-2929 */
} dns_type_t;

typedef enum dns_class
{
  CLASS_IN      =     1,    /* Internet                     RFC-1035 */
  CLASS_CS      =     2,    /* CSNET (obsolete)             RFC-1035 */
  CLASS_CH      =     3,    /* CHAOS                        RFC-1035 */
  CLASS_HS      =     4,    /* Hesiod                       RFC-1035 */
  CLASS_NONE    =   254,    /*                              RFC-2136 */
  CLASS_ANY     =   255,    /* All classes                  RFC-1035 */
  CLASS_UNKNOWN = 65280     /* Unknown class                RFC-2929 */
} dns_class_t;

typedef enum dns_result_record_type
{
  RESULT_RECORD_ANSWER                  =     1,    /* Answer Record                    */
  RESULT_RECORD_AUTHORITATIVE           =     2,    /* Authoritative Record (obsolete)  */
  RESULT_RECORD_ADDITIONAL              =     3,    /* Additional Record                */
  RESULT_RECORD_UNKNOWN                 =     4,    /* Unknown Record                   */
} dns_result_record_type_t;
/*------------VISA Enhancement End---------------*/

typedef struct {
    unsigned char      *data[2];
    uint16_t            size[2];
    uint16_t            pos[2];
    uint16_t            len[2];
} DNSInfo_t;

extern MolochConfig_t        config;

/******************************************************************************/
LOCAL void dns_free(MolochSession_t *UNUSED(session), void *uw)
{
    DNSInfo_t            *info          = uw;

    if (info->data[0])
        free(info->data[0]);
    if (info->data[1])
        free(info->data[1]);
    MOLOCH_TYPE_FREE(DNSInfo_t, info);
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
LOCAL unsigned char *dns_name(const unsigned char *full, int fulllen, BSB *inbsb, unsigned char *name, int *namelen)
{
    BSB  nbsb;
    int  didPointer = 0;
    BSB  tmpbsb;
    BSB *curbsb;

    BSB_INIT(nbsb, name, *namelen);

    curbsb = inbsb;

    while (BSB_REMAINING(*curbsb)) {
        unsigned char ch = 0;
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

            BSB_INIT(tmpbsb, full+tpos, fulllen - tpos);
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
LOCAL void dns_add_host(MolochSession_t *session, char *string, int len)
{
    moloch_field_string_add_host(hostField, session, string, len);
    if (moloch_memstr((const char *)string, len, "xn--", 4)) {
        moloch_field_string_add_lower(punyField, session, string, len);
    }
}
/******************************************************************************/
LOCAL void dns_parser(MolochSession_t *session, int kind, const unsigned char *data, int len)
{

    if (len < 17)
        return;

    int qr      = (data[2] >> 7) & 0x1;
    int opcode  = (data[2] >> 3) & 0xf;
    /*------------VISA Enhancement Begin---------------*/
    int aa      = (data[2] >> 2) & 0x1;
    int tc      = (data[2] >> 1) & 0x1;
    int rd      = (data[2] >> 0) & 0x1;
    int ra      = (data[3] >> 7) & 0x1;
    int z       = (data[3] >> 6) & 0x1;
    int ad      = (data[3] >> 5) & 0x1;
    int cd      = (data[3] >> 4) & 0x1;
/*------------VISA Enhancement End---------------*/

    if (opcode > 5)
        return;

    int qdcount = (data[4] << 8) | data[5];                     /*number of question records*/ 
    int ancount = (data[6] << 8) | data[7];                     /*number of answer recrods*/
    int prereqs = opcode == 5?(data[8] << 8) | data[9]:0;       /*number of prerequisite recrods in UPDATE*/
    int updates = opcode == 5?(data[10] << 8) | data[11]:0;     /*number of update records in UPDATE*/

/*------------VISA Enhancement Begin---------------*/
    int nscount = (data[8] << 8) | data[9];   /*number of authoritative records*/
    int arcount = (data[10] << 8) | data[11]; /*number of additional records*/

    int resultRecordCount [3];
    resultRecordCount [0] = ancount;
    resultRecordCount [1] = nscount;
    resultRecordCount [2] = arcount;

    int _ipField = 0;
    int _hostField = 0;

#ifdef DNSDEBUG
        LOG("DNSDEBUG: [Query/Zone Count (qd/zo count): %d], [Answer/Prerequisite Count (an/pre count): %d], [Authority Record/Update Count (ns/up count): %d], [Additional Information Count (ar/ad count): %d]", qdcount, ancount, nscount, arcount);
#endif
/*------------VISA Enhancement End-----------------*/

    if (qdcount > 10 || qdcount <= 0)
        return;

    BSB bsb;
    BSB_INIT(bsb, data + 12, len - 12);

    /* QD Section */
    int i;
    _ipField = ipQueryField;
    _hostField = hostQueryField;
    for (i = 0; BSB_NOT_ERROR(bsb) && i < qdcount; i++) {
        unsigned char  namebuf[8000];
        int namelen = sizeof(namebuf);
        unsigned char *name = dns_name(data, len, &bsb, namebuf, &namelen);

        if (BSB_IS_ERROR(bsb) || !name)
            break;

        if (!namelen) {
            name = (unsigned char*)"<root>";
            namelen = 6;
        }

        unsigned short qtype = 0 , qclass = 0 ;
        BSB_IMPORT_u16(bsb, qtype);
        BSB_IMPORT_u16(bsb, qclass);

        if (opcode == 5)/* Skip Zone records in UPDATE query*/ 
          continue;

        if (qclass <= 255 && qclasses[qclass]) {
            moloch_field_string_add(queryClassField, session, qclasses[qclass], -1, TRUE);
        }

        if (qtype <= 255 && qtypes[qtype]) {
            moloch_field_string_add(queryTypeField, session, qtypes[qtype], -1, TRUE);
        }

        if (namelen > 0) {
            dns_add_host(session, (char *)name, namelen);
            moloch_field_string_add_lower(_hostField, session, (char *)name, namelen);
        }
    }
    moloch_field_string_add(opCodeField, session, opcodes[opcode], -1, TRUE);
    switch(kind) {
    case 0:
        moloch_session_add_protocol(session, "dns");
        break;
    case 1:
        moloch_session_add_protocol(session, "llmnr");
        break;
    case 2:
        moloch_session_add_protocol(session, "mdns");
        break;
    }

    if (qr == 0 && opcode != 5)
        return;

    if (qr != 0) {
        int rcode      = data[3] & 0xf;
        moloch_field_string_add(statusField, session, statuses[rcode], -1, TRUE);
    }
    int recordType = 0;
    for (recordType= RESULT_RECORD_ANSWER ; recordType <= RESULT_RECORD_ADDITIONAL; recordType++) {

      int recordNum = resultRecordCount[recordType-1];
      
      for (i = 0; BSB_NOT_ERROR(bsb) && i < recordNum; i++) {

          if (recordType == RESULT_RECORD_ANSWER) {
              _ipField = ipAnswerField;
              _hostField = hostAnswerField;
          } else if (recordType == RESULT_RECORD_AUTHORITATIVE) {
              _ipField = ipAuthoritativeField;
              _hostField = hostAuthoritativeField;
          } else if (recordType == RESULT_RECORD_ADDITIONAL) {
              _ipField = ipAdditionalField;
              _hostField = hostAdditionalField;
          } else {
              return;
          }
    //for (i = 0; BSB_NOT_ERROR(bsb) && i < ancount + prereqs + updates; i++) {
        unsigned char  namebuf[8000];
        int namelen = sizeof(namebuf);
        unsigned char *name = dns_name(data, len, &bsb, namebuf, &namelen);

        if (BSB_IS_ERROR(bsb) || !name)
            break;

        uint16_t antype = 0;
        BSB_IMPORT_u16 (bsb, antype);
        uint16_t anclass = 0;
        BSB_IMPORT_u16 (bsb, anclass);
        BSB_IMPORT_skip(bsb, 4); // ttl
        uint16_t rdlength = 0;
        BSB_IMPORT_u16 (bsb, rdlength);

        if (BSB_REMAINING(bsb) < rdlength) {
            break;
        }

        if (anclass != CLASS_IN) {
            BSB_IMPORT_skip(bsb, rdlength);
            continue;
        }

        switch (antype) {
        case RR_A: {
            if (rdlength != 4)
                break;
            struct in_addr in;
            unsigned char *ptr = BSB_WORK_PTR(bsb);
            in.s_addr = ((uint32_t)(ptr[3])) << 24 | ((uint32_t)(ptr[2])) << 16 | ((uint32_t)(ptr[1])) << 8 | ptr[0];

            moloch_field_ip4_add(ipField, session, in.s_addr);
            dns_add_host(session, (char *)name, namelen);

            if (opcode == 5) { /*IPs/ Hostname in update/prerequisites records of UPDATE query*/ 
              moloch_field_ip4_add(ipUpdateField, session, in.s_addr);
              moloch_field_string_add_lower(hostUpdateField, session, (char *)name, namelen);
            } else { /*IPs/Hostname in answer/authoritative/additional records of UPDATE query*/ 
              moloch_field_ip4_add(_ipField, session, in.s_addr);
              moloch_field_string_add_lower(_hostField, session, (char *)name, namelen);
            }
            break;
        }
        case RR_NS:
        case RR_CNAME: {
            BSB rdbsb;
            BSB_INIT(rdbsb, BSB_WORK_PTR(bsb), rdlength);

            namelen = sizeof(namebuf);
            unsigned char *name = dns_name(data, len, &rdbsb, namebuf, &namelen);

            if (!namelen || BSB_IS_ERROR(rdbsb) || !name)
                continue;

            dns_add_host(session, (char *)name, namelen);

            if (opcode == 5) { /* Hostname in update/prerequisites records of UPDATE query*/ 
               moloch_field_string_add_lower(hostUpdateField, session, (char *)name, namelen);
            } else { /*Hostname in answer/authoritative/additional records of UPDATE query*/ 
              moloch_field_string_add_lower(_hostField, session, (char *)name, namelen);
            }
            break;
        }
        case RR_MX: {
            BSB rdbsb;
            BSB_INIT(rdbsb, BSB_WORK_PTR(bsb), rdlength);
            BSB_IMPORT_skip(rdbsb, 2); // preference

            namelen = sizeof(namebuf);
            unsigned char *name = dns_name(data, len, &rdbsb, namebuf, &namelen);

            if (!namelen || BSB_IS_ERROR(rdbsb) || !name)
                continue;

            dns_add_host(session, (char *)name, namelen);

            if (opcode == 5) { /* Hostname in update/prerequisites records of UPDATE query*/ 
              moloch_field_string_add_lower(hostUpdateField, session, (char *)name, namelen);
            } else { /*Hostname in answer/authoritative/additional records of UPDATE query*/ 
              moloch_field_string_add_lower(_hostField, session, (char *)name, namelen);
            }
            break;
        }
        case RR_AAAA: {
            if (rdlength != 16)
                break;
            unsigned char *ptr = BSB_WORK_PTR(bsb);

            moloch_field_ip6_add(ipField, session, ptr);
            dns_add_host(session, (char *)name, namelen);

            if (opcode == 5) { /*IPs/Hostname in update/prerequisites records of UPDATE query*/ 
              moloch_field_ip6_add(ipUpdateField, session, ptr);
              moloch_field_string_add_lower(hostUpdateField, session, (char *)name, namelen);
            } else { /*IPs/Hostname in answer/authoritative/additional records of UPDATE query*/ 
              moloch_field_ip6_add(_ipField, session, ptr);
              moloch_field_string_add_lower(_hostField, session, (char *)name, namelen);
            }
            break;
        }
        } /* switch */
        BSB_IMPORT_skip(bsb, rdlength);
      }
    }
}
/******************************************************************************/
LOCAL int dns_tcp_parser(MolochSession_t *session, void *uw, const unsigned char *data, int len, int which)
{
    DNSInfo_t *info = uw;
    while (len >= 2) {

        // First packet of request
        if (info->len[which] == 0) {
            int dnslength = ((data[0]&0xff) << 8) | (data[1] & 0xff);

            if (dnslength < 18) {
                moloch_parsers_unregister(session, uw);
                return 0;
            }

            if (info->size[which] == 0) {
                info->size[which] = MAX(1024,dnslength);
                info->data[which] = malloc(info->size[which]);
            } else if (info->size[which] < dnslength) {
                free(info->data[which]);
                info->data[which] = malloc(dnslength);
                info->size[which] = dnslength;
            }

            // Have all the data in this first packet, just parse it
            if (dnslength <= len-2) {
                dns_parser(session, 0, data+2, dnslength);
                data += 2 + dnslength;
                len -= 2 + dnslength;
            } else {
                memcpy(info->data[which], data+2, len-2);
                info->len[which] = dnslength;
                info->pos[which] = len-2;
                return 0;
            }
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
LOCAL void dns_tcp_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    if (/*which == 0 &&*/ session->port2 == 53 && !moloch_session_has_protocol(session, "dns")) {
        moloch_session_add_protocol(session, "dns");
        DNSInfo_t  *info= MOLOCH_TYPE_ALLOC0(DNSInfo_t);
        moloch_parsers_register(session, dns_tcp_parser, info, dns_free);
    }
}
/******************************************************************************/
LOCAL int dns_udp_parser(MolochSession_t *session, void *uw, const unsigned char *data, int len, int UNUSED(which))
{
    if (uw == 0 || (session->port1 != 53 && session->port2 != 53)) {
        dns_parser(session, (long)uw, data, len);
    }
    return 0;
}
/******************************************************************************/
LOCAL void dns_udp_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    moloch_parsers_register(session, dns_udp_parser, uw, 0);
}
/******************************************************************************/
void moloch_parser_init()
{
    ipField = moloch_field_define("dns", "ip",
        "ip.dns", "IP",  "dns.ip",
        "IP from DNS result",
        MOLOCH_FIELD_TYPE_IP_GHASH, MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_IPPRE,
        "aliases", "[\"dns.ip\"]",
        "category", "ip",
        NULL);

    hostField = moloch_field_define("dns", "lotermfield",
        "host.dns", "Host", "dns.host",
        "DNS lookup hostname",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_FORCE_UTF8,
        "aliases", "[\"dns.host\"]",
        "category", "host",
        NULL);

    punyField = moloch_field_define("dns", "lotermfield",
        "dns.puny", "Puny", "dns.puny",
        "DNS lookup punycode",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL);

/*------------VISA Enhancement Begin---------------*/
    ipQueryField = moloch_field_define("dns", "ip",
        "ip.dns.query", "IP",  "dns.query.ip",
        "IP in DNS query",
        MOLOCH_FIELD_TYPE_IP_GHASH, MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_IPPRE,
        "category", "ip",
        NULL);

    ipAnswerField = moloch_field_define("dns", "ip",
        "ip.dns.answer", "IP",  "dns.answer.ip",
        "IP in answer section of DNS result",
        MOLOCH_FIELD_TYPE_IP_GHASH, MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_IPPRE,
        "category", "ip",
        NULL);

    ipAuthoritativeField = moloch_field_define("dns", "ip",
        "ip.dns.authritative", "IP",  "dns.authoritative.ip",
        "IP in authritative section of DNS result",
        MOLOCH_FIELD_TYPE_IP_GHASH, MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_IPPRE,
        "category", "ip",
        NULL);

    ipAdditionalField = moloch_field_define("dns", "ip",
        "ip.dns.additional", "IP",  "dns.additional.ip",
        "IP in additional section of DNS result",
        MOLOCH_FIELD_TYPE_IP_GHASH, MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_IPPRE,
        "category", "ip",
        NULL);

    ipUpdateField = moloch_field_define("dns", "ip",
        "ip.dns.update", "IP",  "dns.update.ip",
        "IP in update section of DNS result",
        MOLOCH_FIELD_TYPE_IP_GHASH, MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_IPPRE,
        "category", "ip",
        NULL);

    hostQueryField = moloch_field_define("dns", "lotermfield",
        "host.dns.query", "Host", "dns.query.host",
        "DNS hosts in query",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        "category", "host",
        NULL);

    hostAnswerField = moloch_field_define("dns", "lotermfield",
        "host.dns.answer", "Host", "dns.answer.host",
        "DNS hosts in answer section of DNS result",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        "category", "host",
        NULL);

    hostAuthoritativeField = moloch_field_define("dns", "lotermfield",
        "host.dns.authritative", "Host", "dns.authoritative.host",
        "DNS hosts in authritative section of DNS result",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        "category", "host",
        NULL);

    hostAdditionalField = moloch_field_define("dns", "lotermfield",
        "host.dns.additional", "Host", "dns.additional.host",
        "DNS hosts in additional section of DNS result",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        "category", "host",
        NULL);

    hostUpdateField = moloch_field_define("dns", "lotermfield",
        "host.dns.update", "Host", "dns.update.host",
        "DNS hosts in update section of DNS result",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        "category", "host",
        NULL);
/*------------VISA Enhancement End-------------------*/

    statusField = moloch_field_define("dns", "uptermfield",
        "dns.status", "Status Code", "dns.status",
        "DNS lookup return code",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL);

    opCodeField = moloch_field_define("dns", "uptermfield",
        "dns.opcode", "Op Code", "dns.opcode",
        "DNS lookup op code",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL);

    queryTypeField = moloch_field_define("dns", "uptermfield",
        "dns.query.type", "Query Type", "dns.qt",
        "DNS lookup query type",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL);

    queryClassField = moloch_field_define("dns", "uptermfield",
        "dns.query.class", "Query Class", "dns.qc",
        "DNS lookup query class",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL);


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

    moloch_parsers_classifier_register_port("dns", NULL, 53, MOLOCH_PARSERS_PORT_TCP_DST, dns_tcp_classify);

    moloch_parsers_classifier_register_port("dns",   (void*)(long)0,   53, MOLOCH_PARSERS_PORT_UDP, dns_udp_classify);
    moloch_parsers_classifier_register_port("llmnr", (void*)(long)1, 5355, MOLOCH_PARSERS_PORT_UDP, dns_udp_classify);
    moloch_parsers_classifier_register_port("mdns",  (void*)(long)2, 5353, MOLOCH_PARSERS_PORT_UDP, dns_udp_classify);

}
