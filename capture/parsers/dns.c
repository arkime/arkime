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

static char                 *qclasses[256];
static char                 *qtypes[256];
static char                 *statuses[16] = {"NOERROR", "FORMERR", "SERVFAIL", "NXDOMAIN", "NOTIMPL", "REFUSED", "YXDOMAIN", "YXRRSET", "NXRRSET", "NOTAUTH", "NOTZONE", "11", "12", "13", "14", "15"};
static char                 *opcodes[16] = {"QUERY", "IQUERY", "STATUS", "3", "NOTIFY", "UPDATE", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"};

static int                   ipField;
static int                   hostField;
static int                   queryTypeField;
static int                   queryClassField;
static int                   statusField;
static int                   opCodeField;

typedef struct {
    unsigned char      *data[2];
    uint16_t            size[2];
    uint16_t            pos[2];
    uint16_t            len[2];
} DNSInfo_t;

extern MolochConfig_t        config;

/******************************************************************************/
void dns_free(MolochSession_t *UNUSED(session), void *uw)
{
    DNSInfo_t            *info          = uw;

    if (info->data[0])
        free(info->data[0]);
    if (info->data[1])
        free(info->data[1]);
    MOLOCH_TYPE_FREE(DNSInfo_t, info);
}
/******************************************************************************/
int dns_name_element(BSB *nbsb, BSB *bsb)
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
unsigned char *dns_name(const unsigned char *full, int fulllen, BSB *inbsb, unsigned char *name, int *namelen)
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
void dns_parser(MolochSession_t *session, int kind, const unsigned char *data, int len)
{

    if (len < 17)
        return;

    int qr      = (data[2] >> 7) & 0x1;
    int opcode  = (data[2] >> 3) & 0xf;

    if (opcode > 5)
        return;

    int qdcount = (data[4] << 8) | data[5];
    int ancount = (data[6] << 8) | data[7];
    int prereqs = opcode == 5?(data[8] << 8) | data[9]:0;
    int updates = opcode == 5?(data[10] << 8) | data[11]:0;

    if (qdcount > 10 || qdcount <= 0)
        return;

    BSB bsb;
    BSB_INIT(bsb, data + 12, len - 12);

    /* QD Section */
    int i;
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

        if (opcode == 5)
            continue;

        char *lower = g_ascii_strdown((char*)name, namelen);

        if (qclass <= 255 && qclasses[qclass]) {
            moloch_field_string_add(queryClassField, session, qclasses[qclass], -1, TRUE);
        }

        if (qtype <= 255 && qtypes[qtype]) {
            moloch_field_string_add(queryTypeField, session, qtypes[qtype], -1, TRUE);
        }

        if (lower && !moloch_field_string_add(hostField, session, lower, namelen, FALSE)) {
            g_free(lower);
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

    for (i = 0; BSB_NOT_ERROR(bsb) && i < ancount + prereqs + updates; i++) {
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

        if (anclass != 1) {
            BSB_IMPORT_skip(bsb, rdlength);
            continue;
        }

        switch (antype) {
        case 1: {
            if (rdlength != 4)
                break;
            struct in_addr in;
            unsigned char *ptr = BSB_WORK_PTR(bsb);
            in.s_addr = ptr[3] << 24 | ptr[2] << 16 | ptr[1] << 8 | ptr[0];

            moloch_field_int_add(ipField, session, in.s_addr);

            if (opcode == 5) {
                char *lower = g_ascii_strdown((char*)name, namelen);
                if (lower && !moloch_field_string_add(hostField, session, lower, namelen, FALSE)) {
                    g_free(lower);
                }
            }
            break;
        }
        case 5: {
            BSB rdbsb;
            BSB_INIT(rdbsb, BSB_WORK_PTR(bsb), rdlength);

            namelen = sizeof(namebuf);
            unsigned char *name = dns_name(data, len, &rdbsb, namebuf, &namelen);

            if (!namelen || BSB_IS_ERROR(rdbsb) || !name)
                continue;

            char *lower = g_ascii_strdown((char*)name, namelen);

            if (lower && !moloch_field_string_add(hostField, session, lower, namelen, FALSE)) {
                g_free(lower);
            }
            break;
        }
        case 15: {
            BSB rdbsb;
            BSB_INIT(rdbsb, BSB_WORK_PTR(bsb), rdlength);
            BSB_IMPORT_skip(rdbsb, 2); // preference

            namelen = sizeof(namebuf);
            unsigned char *name = dns_name(data, len, &rdbsb, namebuf, &namelen);

            if (!namelen || BSB_IS_ERROR(rdbsb) || !name)
                continue;

            char *lower = g_ascii_strdown((char*)name, namelen);

            if (lower && !moloch_field_string_add(hostField, session, lower, namelen, FALSE)) {
                g_free(lower);
            }
        }
        } /* switch */
        BSB_IMPORT_skip(bsb, rdlength);
    }
}
/******************************************************************************/
int dns_tcp_parser(MolochSession_t *session, void *uw, const unsigned char *data, int len, int which)
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
void dns_tcp_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    if (/*which == 0 &&*/ session->port2 == 53 && !moloch_session_has_protocol(session, "dns")) {
        moloch_session_add_protocol(session, "dns");
        DNSInfo_t  *info= MOLOCH_TYPE_ALLOC0(DNSInfo_t);
        moloch_parsers_register(session, dns_tcp_parser, info, dns_free);
    }
}
/******************************************************************************/
int dns_udp_parser(MolochSession_t *session, void *uw, const unsigned char *data, int len, int UNUSED(which))
{
    if (uw == 0 || (session->port1 != 53 && session->port2 != 53)) {
        dns_parser(session, (long)uw, data, len);
    }
    return 0;
}
/******************************************************************************/
void dns_udp_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    moloch_parsers_register(session, dns_udp_parser, uw, 0);
}
/******************************************************************************/
void moloch_parser_init()
{
    ipField = moloch_field_define("dns", "ip",
        "ip.dns", "IP",  "dnsip",
        "IP from DNS result",
        MOLOCH_FIELD_TYPE_IP_GHASH, MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_IPPRE,
        "aliases", "[\"dns.ip\"]",
        "category", "ip",
        NULL);

    hostField = moloch_field_define("dns", "lotermfield",
        "host.dns", "Host", "dnsho",
        "DNS host looked up",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        "aliases", "[\"dns.host\"]",
        "category", "host",
        NULL);

    statusField = moloch_field_define("dns", "uptermfield",
        "dns.status", "Status Code", "dns.status-term",
        "DNS lookup return code",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_COUNT,
        NULL);

    opCodeField = moloch_field_define("dns", "uptermfield",
        "dns.opcode", "Op Code", "dns.opcode-term",
        "DNS lookup op code",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_COUNT,
        NULL);

    queryTypeField = moloch_field_define("dns", "uptermfield",
        "dns.query.type", "Query Type", "dns.qt-term",
        "DNS lookup query type",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_COUNT,
        NULL);

    queryClassField = moloch_field_define("dns", "uptermfield",
        "dns.query.class", "Query Class", "dns.qc-term",
        "DNS lookup query class",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_COUNT,
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

