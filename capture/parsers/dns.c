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

//#define DNSDEBUG 1

LOCAL  char                 *qclasses[256];
LOCAL  char                 *qtypes[256];
LOCAL  char                 *statuses[16] = {"NOERROR", "FORMERR", "SERVFAIL", "NXDOMAIN", "NOTIMPL", "REFUSED", "YXDOMAIN", "YXRRSET", "NXRRSET", "NOTAUTH", "NOTZONE", "11", "12", "13", "14", "15"};
LOCAL  char                 *opcodes[16] = {"QUERY", "IQUERY", "STATUS", "3", "NOTIFY", "UPDATE", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"};

LOCAL  int                   ipField;
LOCAL  int                   ipNameServerField;
LOCAL  int                   ipMailServerField;
LOCAL  int                   hostField;
LOCAL  int                   hostNameServerField;
LOCAL  int                   hostMailServerField;
LOCAL  int                   punyField;
LOCAL  int                   queryTypeField;
LOCAL  int                   queryClassField;
LOCAL  int                   statusField;
LOCAL  int                   opCodeField;

typedef enum dns_type
{
  RR_A          =   1,
  RR_NS         =   2,
  RR_CNAME      =   5,
  RR_MX         =  15,
  RR_AAAA       =  28
} DNSType_t;

typedef enum dns_class
{
  CLASS_IN      =     1,
  CLASS_CS      =     2,
  CLASS_CH      =     3,
  CLASS_HS      =     4,
  CLASS_NONE    =   254,
  CLASS_ANY     =   255,
  CLASS_UNKNOWN = 65280
} DNSClass_t;

typedef enum dns_result_record_type
{
  RESULT_RECORD_ANSWER          =     1,    /* Answer or Prerequisites Record */
  RESULT_RECORD_AUTHORITATIVE   =     2,    /* Authoritative or Update Record */
  RESULT_RECORD_ADDITIONAL      =     3,    /* Additional Record */
  RESULT_RECORD_UNKNOWN         =     4,    /* Unknown Record*/
} DNSResultRecordType_t;

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
LOCAL void dns_add_host(int field, MolochSession_t *session, char *string, int len)
{
    moloch_field_string_add_host(field, session, string, len);
    if (moloch_memstr((const char *)string, len, "xn--", 4)) {
        moloch_field_string_add_lower(punyField, session, string, len);
    }
}
/******************************************************************************/
LOCAL int dns_find_host(int pos, MolochSession_t *session, char *string, int len) {

    char *host = 0;
    MolochField_t *field = 0;
    MolochString_t *hstring = 0;

    if (config.fields[pos]->flags & MOLOCH_FIELD_FLAG_DISABLED || pos >= session->maxFields)
        return FALSE;

    if (!session->fields[pos]) // Hash list has not been created
        return FALSE;

    if (len == -1 ) {
        len = strlen(string);
    }

    if (string[len] == 0)
        host = g_hostname_to_unicode(string);
    else {
        char ch = string[len];
        string[len] = 0;
        host = g_hostname_to_unicode(string);
        string[len] = ch;
    }

    // If g_hostname_to_unicode fails, just use the input
    if (!host) {
        return FALSE;
    }

    field = session->fields[pos];
    HASH_FIND(s_, *(field->shash), host, hstring);

    g_free(host);

    if (hstring) // hostname found
        return TRUE;

    return FALSE;
}
/******************************************************************************/
LOCAL void dns_parser(MolochSession_t *session, int kind, const unsigned char *data, int len)
{

    if (len < 17)
        return;

    int qr      = (data[2] >> 7) & 0x1;
    int opcode  = (data[2] >> 3) & 0xf;
 /*
    int aa      = (data[2] >> 2) & 0x1;
    int tc      = (data[2] >> 1) & 0x1;
    int rd      = (data[2] >> 0) & 0x1;
    int ra      = (data[3] >> 7) & 0x1;
    int z       = (data[3] >> 6) & 0x1;
    int ad      = (data[3] >> 5) & 0x1;
    int cd      = (data[3] >> 4) & 0x1;
*/
    if (opcode > 5)
        return;

    int qd_count = (data[4] << 8) | data[5];                                                          /*number of question records*/
    int an_prereqs_count = (data[6] << 8) | data[7];                                                  /*number of answer or prerequisite records*/
    int ns_update_count = (opcode == 5 || config.parseDNSRecordAll)? (data[8] << 8) | data[9]:0;      /*number of authoritative or update recrods*/
    int ar_count = (opcode == 5 || config.parseDNSRecordAll)? (data[10] << 8) | data[11]:0;           /*number of additional records*/

    int resultRecordCount [3] = {0};
    resultRecordCount [0] = an_prereqs_count;
    resultRecordCount [1] = ns_update_count;
    resultRecordCount [2] = ar_count;

#ifdef DNSDEBUG
        LOG("DNSDEBUG: [Query/Zone Count: %d], [Answer or Prerequisite Count: %d], [Authoritative or Update RecordCount: %d], [Additional Record Count: %d]", qd_count, an_prereqs_count, ns_update_count, ar_count);
#endif

    if (qd_count > 10 || qd_count <= 0)
        return;

    BSB bsb;
    BSB_INIT(bsb, data + 12, len - 12);

    /* QD Section */
    int i;
    for (i = 0; BSB_NOT_ERROR(bsb) && i < qd_count; i++) {
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
            dns_add_host(hostField, session, (char *)name, namelen);
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
    for (recordType= RESULT_RECORD_ANSWER; recordType <= RESULT_RECORD_ADDITIONAL; recordType++) {
        int recordNum = resultRecordCount[recordType-1];
        for (i = 0; BSB_NOT_ERROR(bsb) && i < recordNum; i++) {

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

                if (opcode == 5) { // update
                    moloch_field_ip4_add(ipField, session, in.s_addr);
                    dns_add_host(hostField, session, (char *)name, namelen);
                } else {
                    if (dns_find_host(hostField, session, (char *)name, namelen)) { // IP for looked-up hostname
                        moloch_field_ip4_add(ipField, session, in.s_addr);
                    }

                    if (config.parseDNSRecordAll) {
                        if (dns_find_host(hostNameServerField, session, (char *)name, namelen)){ // IP for name-server
                            moloch_field_ip4_add(ipNameServerField, session, in.s_addr);
                        }

                        if (dns_find_host(hostMailServerField, session, (char *)name, namelen)){ // IP for mail-exchange
                            moloch_field_ip4_add(ipMailServerField, session, in.s_addr);
                        }
                    }
                }
                break;
            }
            case RR_NS: {

                if (!config.parseDNSRecordAll)
                    break;

                BSB rdbsb;
                BSB_INIT(rdbsb, BSB_WORK_PTR(bsb), rdlength);

                namelen = sizeof(namebuf);
                name = dns_name(data, len, &rdbsb, namebuf, &namelen);

                if (!namelen || BSB_IS_ERROR(rdbsb) || !name)
                    continue;

                dns_add_host(hostNameServerField, session, (char *)name, namelen);

                break;
            }
            case RR_CNAME: {
                BSB rdbsb;
                BSB_INIT(rdbsb, BSB_WORK_PTR(bsb), rdlength);

                namelen = sizeof(namebuf);
                name = dns_name(data, len, &rdbsb, namebuf, &namelen);

                if (!namelen || BSB_IS_ERROR(rdbsb) || !name)
                    continue;

                dns_add_host(hostField, session, (char *)name, namelen);

                break;
            }
            case RR_MX: {
                BSB rdbsb;
                BSB_INIT(rdbsb, BSB_WORK_PTR(bsb), rdlength);
                BSB_IMPORT_skip(rdbsb, 2); // preference

                namelen = sizeof(namebuf);
                name = dns_name(data, len, &rdbsb, namebuf, &namelen);

                if (!namelen || BSB_IS_ERROR(rdbsb) || !name)
                    continue;

                if (config.parseDNSRecordAll)
                    dns_add_host(hostMailServerField, session, (char *)name, namelen);
                else
                    dns_add_host(hostField, session, (char*)name, namelen);

                break;
            }
            case RR_AAAA: {
                if (rdlength != 16)
                    break;
                unsigned char *ptr = BSB_WORK_PTR(bsb);

                if (opcode == 5) { // update
                    moloch_field_ip6_add(ipField, session, ptr);
                    dns_add_host(hostField, session, (char *)name, namelen);
                } else {
                    if (dns_find_host(hostField, session, (char *)name, namelen)) { // IP for looked-up hostname
                        moloch_field_ip6_add(ipField, session, ptr);
                    }

                    if (config.parseDNSRecordAll) {
                        if (dns_find_host(hostNameServerField, session, (char *)name, namelen)){ // IP for name-server
                            moloch_field_ip6_add(ipNameServerField, session, ptr);
                        }

                        if (dns_find_host(hostMailServerField, session, (char *)name, namelen)){ // IP for mail-server
                            moloch_field_ip6_add(ipMailServerField, session, ptr);
                        }
                    }
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
        (char *)NULL);

    ipNameServerField = moloch_field_define("dns", "ip",
        "ip.dns.nameserver", "IP",  "dns.nameserverIp",
        "IPs for nameservers",
        MOLOCH_FIELD_TYPE_IP_GHASH, MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_IPPRE,
        "category", "ip",
        (char *)NULL);

    ipMailServerField = moloch_field_define("dns", "ip",
        "ip.dns.mailserver", "IP",  "dns.mailserverIp",
        "IPs for mailservers",
        MOLOCH_FIELD_TYPE_IP_GHASH, MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_IPPRE,
        "category", "ip",
        (char *)NULL);

    moloch_field_define("dns", "ip",
        "ip.dns.all", "IP", "dnsipall",
        "Shorthand for ip.dns or ip.dns.nameserver",
        0, MOLOCH_FIELD_FLAG_FAKE,
        "regex", "^ip\\\\.dns(?:(?!\\\\.(cnt|all)$).)*$",
        (char *)NULL);

    hostField = moloch_field_define("dns", "lotermfield",
        "host.dns", "Host", "dns.host",
        "DNS lookup hostname",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_FORCE_UTF8,
        "aliases", "[\"dns.host\"]",
        "category", "host",
        (char *)NULL);

    moloch_field_define("dns", "lotextfield",
        "host.dns.tokens", "Hostname Tokens", "dns.hostTokens",
        "DNS lookup hostname tokens",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_FAKE,
        "aliases", "[\"dns.host.tokens\"]",
        (char *)NULL);

    hostNameServerField = moloch_field_define("dns", "lotermfield",
        "host.dns.nameserver", "NS Host", "dns.nameserverHost",
        "Hostnames for Name Server",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_FORCE_UTF8,
        "category", "host",
        (char *)NULL);

    hostMailServerField = moloch_field_define("dns", "lotermfield",
        "host.dns.mailserver", "MX Host", "dns.mailserverHost",
        "Hostnames for Mail Exchange Server",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_FORCE_UTF8,
        "category", "host",
        (char *)NULL);

    moloch_field_define("dns", "lotermfield",
        "host.dns.all", "All Host", "dnshostall",
        "Shorthand for host.dns or host.dns.nameserver",
        0, MOLOCH_FIELD_FLAG_FAKE,
        "regex",  "^host\\\\.dns(?:(?!\\\\.(cnt|all)$).)*$",
        (char *)NULL);

    punyField = moloch_field_define("dns", "lotermfield",
        "dns.puny", "Puny", "dns.puny",
        "DNS lookup punycode",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    statusField = moloch_field_define("dns", "uptermfield",
        "dns.status", "Status Code", "dns.status",
        "DNS lookup return code",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    opCodeField = moloch_field_define("dns", "uptermfield",
        "dns.opcode", "Op Code", "dns.opcode",
        "DNS lookup op code",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    queryTypeField = moloch_field_define("dns", "uptermfield",
        "dns.query.type", "Query Type", "dns.qt",
        "DNS lookup query type",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    queryClassField = moloch_field_define("dns", "uptermfield",
        "dns.query.class", "Query Class", "dns.qc",
        "DNS lookup query class",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
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

    moloch_parsers_classifier_register_port("dns", NULL, 53, MOLOCH_PARSERS_PORT_TCP_DST, dns_tcp_classify);

    moloch_parsers_classifier_register_port("dns",   (void*)(long)0,   53, MOLOCH_PARSERS_PORT_UDP, dns_udp_classify);
    moloch_parsers_classifier_register_port("llmnr", (void*)(long)1, 5355, MOLOCH_PARSERS_PORT_UDP, dns_udp_classify);
    moloch_parsers_classifier_register_port("mdns",  (void*)(long)2, 5353, MOLOCH_PARSERS_PORT_UDP, dns_udp_classify);

}
