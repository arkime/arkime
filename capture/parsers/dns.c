#include <string.h>
#include <ctype.h>
#include "moloch.h"

static char                 *qclasses[256];
static char                 *qtypes[256];

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
unsigned char *dns_name(const unsigned char *full, int fulllen, BSB *inbsb, int *namelen)
{
    static unsigned char  name[8000];
    BSB  nbsb;
    int  didPointer = 0;
    BSB  tmpbsb;
    BSB *curbsb;

    BSB_INIT(nbsb, name, sizeof(name));

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
void dns_parser(MolochSession_t *session, const unsigned char *data, int len) 
{

    if (len < 18)
        return;

    int qr      = (data[2] >> 7) & 0x1;
    int opcode  = (data[2] >> 3) & 0xf;

    if (opcode != 0)
        return;

    int qdcount = (data[4] << 8) | data[5];
    int ancount = (data[6] << 8) | data[7];

    if (qdcount > 10 || qdcount <= 0)
        return;

    BSB bsb;
    BSB_INIT(bsb, data + 12, len - 12);

    /* QD Section */
    int i;
    for (i = 0; BSB_NOT_ERROR(bsb) && i < qdcount; i++) {
        int namelen;
        unsigned char *name = dns_name(data, len, &bsb, &namelen);

        if (!namelen || BSB_IS_ERROR(bsb))
            break;

        unsigned short qtype = 0 , qclass = 0 ;
        BSB_IMPORT_u16(bsb, qtype);
        BSB_IMPORT_u16(bsb, qclass);

        char *lower = g_ascii_strdown((char*)name, namelen);

        if (qclass <= 255 && qclasses[qclass]) {
            moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, qclasses[qclass]);
        }

        if (qtype <= 255 && qtypes[qtype]) {
            moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, qtypes[qtype]);
        }

        if (lower && !moloch_field_string_add(MOLOCH_FIELD_DNS_HOST, session, lower, namelen, FALSE)) {
            g_free(lower);
        }
    }
    moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:dns");

    if (qr == 0)
        return;

    for (i = 0; BSB_NOT_ERROR(bsb) && i < ancount; i++) {
        int namelen;
        dns_name(data, len, &bsb, &namelen);

        if (BSB_IS_ERROR(bsb))
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

            moloch_field_int_add(MOLOCH_FIELD_DNS_IP, session, in.s_addr);
            break;
        }
        case 5: {
            BSB rdbsb;
            BSB_INIT(rdbsb, BSB_WORK_PTR(bsb), rdlength);

            int namelen;
            unsigned char *name = dns_name(data, len, &rdbsb, &namelen);

            if (!namelen || BSB_IS_ERROR(rdbsb))
                continue;

            char *lower = g_ascii_strdown((char*)name, namelen);

            if (lower && !moloch_field_string_add(MOLOCH_FIELD_DNS_HOST, session, lower, namelen, FALSE)) {
                g_free(lower);
            }
            break;
        }
        case 15: {
            BSB rdbsb;
            BSB_INIT(rdbsb, BSB_WORK_PTR(bsb), rdlength);
            BSB_IMPORT_skip(rdbsb, 2); // preference

            int namelen;
            unsigned char *name = dns_name(data, len, &rdbsb, &namelen);

            if (!namelen || BSB_IS_ERROR(rdbsb))
                continue;

            char *lower = g_ascii_strdown((char*)name, namelen);

            if (lower && !moloch_field_string_add(MOLOCH_FIELD_DNS_HOST, session, lower, namelen, FALSE)) {
                g_free(lower);
            }
        }
        } /* switch */
        BSB_IMPORT_skip(bsb, rdlength);
    }
}

/******************************************************************************/
int dns_tcp_parser(MolochSession_t *session, void *UNUSED(uw), const unsigned char *data, int len) 
{
    if (session->which == 1) {
        int l = ((data[0]&0xff) << 8) | (data[1] & 0xff);
        dns_parser(session, data+2, MIN(l, len)-2);
    }
    return 0;
}
/******************************************************************************/
void dns_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len))
{
    if (session->which == 0 && session->port2 == 53 && !moloch_nids_has_tag(session, MOLOCH_FIELD_TAGS, "protocol:dns")) {
        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:dns");
        moloch_parsers_register(session, dns_tcp_parser, 0, 0);
    }
}
/******************************************************************************/
void dns_udp_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len))
{
    if (session->port1 == 53 || session->port2 == 53)
        dns_parser(session, data, len);
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_field_define_internal(MOLOCH_FIELD_DNS_IP,        "dnsip",  MOLOCH_FIELD_TYPE_IP_HASH,   MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_DNS_HOST,      "dnsho",  MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);


    qclasses[1]   = "dns:qclass:IN";
    qclasses[2]   = "dns:qclass:CS";
    qclasses[3]   = "dns:qclass:CH";
    qclasses[4]   = "dns:qclass:HS";
    qclasses[255] = "dns:qclass:ANY";

    //http://en.wikipedia.org/wiki/List_of_DNS_record_types
    qtypes[1]   = "dns:qtype:A";
    qtypes[2]   = "dns:qtype:NS";
    qtypes[3]   = "dns:qtype:MD";
    qtypes[4]   = "dns:qtype:MF";
    qtypes[5]   = "dns:qtype:CNAME";
    qtypes[6]   = "dns:qtype:SOA";
    qtypes[7]   = "dns:qtype:MB";
    qtypes[8]   = "dns:qtype:MG";
    qtypes[9]   = "dns:qtype:MR";
    qtypes[10]  = "dns:qtype:NULL";
    qtypes[11]  = "dns:qtype:WKS";
    qtypes[12]  = "dns:qtype:PTR";
    qtypes[13]  = "dns:qtype:HINFO";
    qtypes[14]  = "dns:qtype:MINFO";
    qtypes[15]  = "dns:qtype:MX";
    qtypes[16]  = "dns:qtype:TXT";
    qtypes[17]  = "dns:qtype:RP";
    qtypes[18]  = "dns:qtype:AFSDB";
    qtypes[19]  = "dns:qtype:X25";
    qtypes[20]  = "dns:qtype:ISDN";
    qtypes[21]  = "dns:qtype:RT";
    qtypes[22]  = "dns:qtype:NSAP";
    qtypes[23]  = "dns:qtype:NSAPPTR";
    qtypes[24]  = "dns:qtype:SIG";
    qtypes[25]  = "dns:qtype:KEY";
    qtypes[26]  = "dns:qtype:PX";
    qtypes[27]  = "dns:qtype:GPOS";
    qtypes[28]  = "dns:qtype:AAAA";
    qtypes[29]  = "dns:qtype:LOC";
    qtypes[30]  = "dns:qtype:NXT";
    qtypes[31]  = "dns:qtype:EID";
    qtypes[32]  = "dns:qtype:NIMLOC";
    qtypes[33]  = "dns:qtype:SRV";
    qtypes[34]  = "dns:qtype:ATMA";
    qtypes[35]  = "dns:qtype:NAPTR";
    qtypes[36]  = "dns:qtype:KX";
    qtypes[37]  = "dns:qtype:CERT";
    qtypes[38]  = "dns:qtype:A6";
    qtypes[39]  = "dns:qtype:DNAME";
    qtypes[40]  = "dns:qtype:SINK";
    qtypes[41]  = "dns:qtype:OPT";
    qtypes[42]  = "dns:qtype:APL";
    qtypes[43]  = "dns:qtype:DS";
    qtypes[44]  = "dns:qtype:SSHFP";
    qtypes[46]  = "dns:qtype:RRSIG";
    qtypes[47]  = "dns:qtype:NSEC";
    qtypes[48]  = "dns:qtype:DNSKEY";
    qtypes[49]  = "dns:qtype:DHCID";
    qtypes[50]  = "dns:qtype:NSEC3";
    qtypes[51]  = "dns:qtype:NSEC3PARAM";
    qtypes[52]  = "dns:qtype:TLSA";
    qtypes[55]  = "dns:qtype:HIP";
    qtypes[99]  = "dns:qtype:SPF";
    qtypes[249] = "dns:qtype:TKEY";
    qtypes[250] = "dns:qtype:TSIG";
    qtypes[252] = "dns:qtype:AXFR";
    qtypes[253] = "dns:qtype:MAILB";
    qtypes[254] = "dns:qtype:MAILA";
    qtypes[255] = "dns:qtype:ANY";

    moloch_parsers_classifier_register_tcp("dns", 4, (unsigned char*)"\x01\x00", 2, dns_classify);
    moloch_parsers_classifier_register_udp("dns", 2, (unsigned char*)"\x01\x00", 2, dns_udp_classify);
    moloch_parsers_classifier_register_udp("dns", 2, (unsigned char*)"\x81\x80", 2, dns_udp_classify);
}

