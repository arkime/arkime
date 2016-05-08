/* Copyright 2012-2016 AOL Inc. All rights reserved.
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
#include "tls-cipher.h"

extern MolochConfig_t        config;
LOCAL  int                   certsField;
LOCAL  int                   hostField;
LOCAL  int                   verField;
LOCAL  int                   cipherField;
LOCAL  int                   srcIdField;
LOCAL  int                   dstIdField;

typedef struct {
    unsigned char       buf[8192];
    uint16_t            len;
    char                which;
} TLSInfo_t;

extern unsigned char    moloch_char_to_hexstr[256][3];

LOCAL GChecksum *checksums[MOLOCH_MAX_PACKET_THREADS];

/******************************************************************************/
void
tls_certinfo_process(MolochCertInfo_t *ci, BSB *bsb)
{
    uint32_t apc, atag, alen;
    char lastOid[1000];
    lastOid[0] = 0;

    while (BSB_REMAINING(*bsb)) {
        unsigned char *value = moloch_parsers_asn_get_tlv(bsb, &apc, &atag, &alen);
        if (!value)
            return;

        if (apc) {
            BSB tbsb;
            BSB_INIT(tbsb, value, alen);
            tls_certinfo_process(ci, &tbsb);
        } else if (atag  == 6)  {
            moloch_parsers_asn_decode_oid(lastOid, sizeof(lastOid), value, alen);
        } else if (lastOid[0] && (atag == 20 || atag == 19 || atag == 12))  {
            /* 20 == BER_UNI_TAG_TeletexString
             * 19 == BER_UNI_TAG_PrintableString
             * 12 == BER_UNI_TAG_UTF8String
             */
            if (strcmp(lastOid, "2.5.4.3") == 0) {
                MolochString_t *element = MOLOCH_TYPE_ALLOC(MolochString_t);
                element->utf8 = atag == 12;
                if (element->utf8)
                    element->str = g_utf8_strdown((char*)value, alen);
                else
                    element->str = g_ascii_strdown((char*)value, alen);
                DLL_PUSH_TAIL(s_, &ci->commonName, element);
            } else if (strcmp(lastOid, "2.5.4.10") == 0) {
                if (ci->orgName) {
                    LOG("Multiple orgName %s => %.*s", ci->orgName, alen, value);
                    free(ci->orgName);
                }
                ci->orgUtf8 = atag == 12;
                ci->orgName = g_strndup((char*)value, alen);
            }
        }
    }
}
/******************************************************************************/
void
tls_alt_names(MolochCertsInfo_t *certs, BSB *bsb, char *lastOid)
{
    uint32_t apc, atag, alen;

    while (BSB_REMAINING(*bsb) >= 2) {
        unsigned char *value = moloch_parsers_asn_get_tlv(bsb, &apc, &atag, &alen);

        if (!value)
            return;

        if (apc) {
            BSB tbsb;
            BSB_INIT(tbsb, value, alen);
            tls_alt_names(certs, &tbsb, lastOid);
            if (certs->alt.s_count > 0) {
                return;
            }
        } else if (atag == 6)  {
            moloch_parsers_asn_decode_oid(lastOid, 100, value, alen);
            if (strcmp(lastOid, "2.5.29.17") != 0)
                lastOid[0] = 0;
        } else if (lastOid[0] && atag == 4) {
            BSB tbsb;
            BSB_INIT(tbsb, value, alen);
            tls_alt_names(certs, &tbsb, lastOid);
            return;
        } else if (lastOid[0] && atag == 2) {
            MolochString_t *element = MOLOCH_TYPE_ALLOC0(MolochString_t);
            element->str = g_ascii_strdown((char*)value, alen);
            element->len = alen;
            DLL_PUSH_TAIL(s_, &certs->alt, element);
        }
    }
    lastOid[0] = 0;
    return;
}
/******************************************************************************/
void tls_process_server_hello(MolochSession_t *session, const unsigned char *data, int len)
{
    BSB bsb;
    BSB_INIT(bsb, data, len);

    unsigned char *ver;
    BSB_IMPORT_ptr(bsb, ver, 2);

    BSB_IMPORT_skip(bsb, 32);     // Random

    int skiplen = 0;
    BSB_IMPORT_u08(bsb, skiplen);   // Session Id Length
    if (skiplen > 0 && BSB_REMAINING(bsb) > skiplen) {
        unsigned char *ptr = BSB_WORK_PTR(bsb);
        char sessionId[513];
        int  i;

        for(i=0; i < skiplen; i++) {
            sessionId[i*2] = moloch_char_to_hexstr[ptr[i]][0];
            sessionId[i*2+1] = moloch_char_to_hexstr[ptr[i]][1];
        }
        sessionId[skiplen*2] = 0;
        moloch_field_string_add(dstIdField, session, sessionId, skiplen*2, TRUE);
    }
    BSB_IMPORT_skip(bsb, skiplen);  // Session Id


    unsigned char *cipher;
    BSB_IMPORT_ptr(bsb, cipher, 2);

    if (!BSB_IS_ERROR(bsb)) {
        char str[100];
        if (ver[0] == 3) {
            switch (ver[1]) {
            case 0:
                moloch_field_string_add(verField, session, "SSLv3", 5, TRUE);
                break;
            case 1:
                moloch_field_string_add(verField, session, "TLSv1", 5, TRUE);
                break;
            case 2:
                moloch_field_string_add(verField, session, "TLSv1.1", 7, TRUE);
                break;
            case 3:
                moloch_field_string_add(verField, session, "TLSv1.2", 7, TRUE);
                break;
            default:
                snprintf(str, sizeof(str), "0x%02x.%02x", ver[0], ver[1]);
                moloch_field_string_add(verField, session, str, 6, TRUE);
            }
        } else {
            snprintf(str, sizeof(str), "0x%02x.%02x", ver[0], ver[1]);
            moloch_field_string_add(verField, session, str, 7, TRUE);
        }


        char *cipherStr = ciphers[cipher[0]][cipher[1]];
        if (cipherStr)
            moloch_field_string_add(cipherField, session, cipherStr, -1, TRUE);
        else {
            snprintf(str, sizeof(str), "%02X%02X", cipher[0], cipher[1]);
            moloch_field_string_add(cipherField, session, str, 4, TRUE);
        }
    }
}

#define char2num(ch) (isdigit(ch)?((ch) - '0'):0)
#define str2num(str) (char2num((str)[0]) * 10 + char2num((str)[1]))
#define str4num(str) (char2num((str)[0]) * 1000 + char2num((str)[1]) * 100 + char2num((str)[2]) * 10 + char2num((str)[3]))

/******************************************************************************/
uint64_t tls_parse_time(int tag, unsigned char* value, int len)
{
    int        offset = 0;
    int        pos = 0;
    struct tm  tm;

    //UTCTime
    if (tag == 23 && len > 12) {
        if (len > 17 && value[12] != 'Z')
            offset = str2num(value+13) * 60 + str2num(value+15);

        if (value[12] == '-')
            offset = -offset;

        tm.tm_year = str2num(value+0);
        tm.tm_mon  = str2num(value+2) - 1;
        tm.tm_mday = str2num(value+4);
        tm.tm_hour = str2num(value+6);
        tm.tm_min  = str2num(value+8);
        tm.tm_sec  = str2num(value+10);

        if (tm.tm_year < 50)
            tm.tm_year += 100;

        return timegm(&tm) + offset;
    }
    //GeneralizedTime
    else if (tag == 24 && len >= 10) {
        memset(&tm, 0, sizeof(tm));
        tm.tm_year = str4num(value+0) - 1900;
        tm.tm_mon  = str2num(value+4) - 1;
        tm.tm_mday = str2num(value+6);
        tm.tm_hour = str2num(value+8);
        if (len < 10 || value[10] == 'Z' || value[10] == '+' || value[10] == '-') {
            pos = 10;
            goto gtdone;
        }
        tm.tm_min  = str2num(value+10);
        if (len < 12 || value[12] == 'Z' || value[12] == '+' || value[12] == '-') {
            pos = 12;
            goto gtdone;
        }
        tm.tm_sec  = str2num(value+12);
        if (len < 14 || value[14] == 'Z' || value[14] == '+' || value[14] == '-') {
            pos = 14;
            goto gtdone;
        }
        if (value[14] == '.') {
            pos = 18;
        } else {
            pos = 14;
        }
    gtdone:
        if (pos == len) {
            return mktime(&tm);
        }

        if (pos + 5 < len && (value[pos] == '+' || value[pos] == '-')) {
            offset = str2num(value+pos+1) * 60 +  str2num(value+pos+3);

            if (value[pos] == '-')
                offset = -offset;
        }

        return timegm(&tm) + offset;
    }
    return 0;
}
/******************************************************************************/
void tls_process_server_certificate(MolochSession_t *session, const unsigned char *data, int len)
{

    BSB cbsb;

    BSB_INIT(cbsb, data, len);

    BSB_IMPORT_skip(cbsb, 3); // Length again

    GChecksum * const checksum = checksums[session->thread];

    while(BSB_REMAINING(cbsb) > 3) {
        int            badreason = 0;
        unsigned char *cdata = BSB_WORK_PTR(cbsb);
        int            clen = MIN(BSB_REMAINING(cbsb) - 3, (cdata[0] << 16 | cdata[1] << 8 | cdata[2]));


        MolochCertsInfo_t *certs = MOLOCH_TYPE_ALLOC0(MolochCertsInfo_t);
        DLL_INIT(s_, &certs->alt);
        DLL_INIT(s_, &certs->subject.commonName);
        DLL_INIT(s_, &certs->issuer.commonName);

        uint32_t       atag, alen, apc;
        unsigned char *value;

        BSB            bsb;
        BSB_INIT(bsb, cdata + 3, clen);

        guchar digest[20];
        gsize  len = sizeof(digest);

        g_checksum_update(checksum, cdata+3, clen);
        g_checksum_get_digest(checksum, digest, &len);
        if (len > 0) {
            int i;
            for(i = 0; i < 20; i++) {
                certs->hash[i*3] = moloch_char_to_hexstr[digest[i]][0];
                certs->hash[i*3+1] = moloch_char_to_hexstr[digest[i]][1];
                certs->hash[i*3+2] = ':';
            }
        }
        certs->hash[59] = 0;
        g_checksum_reset(checksum);

        /* Certificate */
        if (!(value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {badreason = 1; goto bad_cert;}
        BSB_INIT(bsb, value, alen);

        /* signedCertificate */
        if (!(value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {badreason = 2; goto bad_cert;}
        BSB_INIT(bsb, value, alen);

        /* serialNumber or version*/
        if (!(value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {badreason = 3; goto bad_cert;}

        if (apc) {
            if (!(value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
                {badreason = 4; goto bad_cert;}
        }
        certs->serialNumberLen = alen;
        certs->serialNumber = malloc(alen);
        memcpy(certs->serialNumber, value, alen);

        /* signature */
        if (!(value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {badreason = 5; goto bad_cert;}

        /* issuer */
        if (!(value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {badreason = 6; goto bad_cert;}
        BSB tbsb;
        BSB_INIT(tbsb, value, alen);
        tls_certinfo_process(&certs->issuer, &tbsb);

        /* validity */
        if (!(value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {badreason = 7; goto bad_cert;}

        BSB_INIT(tbsb, value, alen);
        if (!(value = moloch_parsers_asn_get_tlv(&tbsb, &apc, &atag, &alen)))
            {badreason = 7; goto bad_cert;}
        certs->notBefore = tls_parse_time(atag, value, alen);

        if (!(value = moloch_parsers_asn_get_tlv(&tbsb, &apc, &atag, &alen)))
            {badreason = 7; goto bad_cert;}
        certs->notAfter = tls_parse_time(atag, value, alen);

        /* subject */
        if (!(value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {badreason = 8; goto bad_cert;}
        BSB_INIT(tbsb, value, alen);
        tls_certinfo_process(&certs->subject, &tbsb);

        /* subjectPublicKeyInfo */
        if (!(value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {badreason = 9; goto bad_cert;}

        /* extensions */
        if (BSB_REMAINING(bsb)) {
            if (!(value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
                {badreason = 10; goto bad_cert;}
            BSB tbsb;
            BSB_INIT(tbsb, value, alen);
            char lastOid[100];
            lastOid[0] = 0;
            tls_alt_names(certs, &tbsb, lastOid);
        }

        if (!moloch_field_certsinfo_add(certsField, session, certs, clen*2)) {
            moloch_field_certsinfo_free(certs);
        }

        BSB_IMPORT_skip(cbsb, clen + 3);

        continue;

    bad_cert:
        if (config.debug)
            LOG("bad cert %d - %d", badreason, clen);
        moloch_field_certsinfo_free(certs);
        break;
    }
}
/******************************************************************************/
/* @data the data inside the record layer
 * @len  the length of data inside record layer
 */
int
tls_process_server_handshake_record(MolochSession_t *session, const unsigned char *data, int len)
{
    BSB rbsb;

    BSB_INIT(rbsb, data, len);

    while (BSB_REMAINING(rbsb) >= 4) {
        unsigned char *hdata = BSB_WORK_PTR(rbsb);
        int hlen = MIN(BSB_REMAINING(rbsb), (hdata[1] << 16 | hdata[2] << 8 | hdata[3]) + 4);

        switch(hdata[0]) {
        case 2:
            tls_process_server_hello(session, hdata+4, hlen-4);
            break;
        case 11:
            tls_process_server_certificate(session, hdata + 4, hlen - 4);
            break;
        case 14:
            return 1;
        }

        BSB_IMPORT_skip(rbsb, hlen);
    }
    return 0;
}
/******************************************************************************/
void
tls_process_client(MolochSession_t *session, const unsigned char *data, int len)
{
    BSB sslbsb;

    BSB_INIT(sslbsb, data, len);

    if (BSB_REMAINING(sslbsb) > 5) {
        unsigned char *ssldata = BSB_WORK_PTR(sslbsb);
        int            ssllen = MIN(BSB_REMAINING(sslbsb) - 5, ssldata[3] << 8 | ssldata[4]);

        BSB pbsb;
        BSB_INIT(pbsb, ssldata+5, ssllen);

        if (BSB_REMAINING(pbsb) > 7) {
            unsigned char *pdata = BSB_WORK_PTR(pbsb);
            int            plen = MIN(BSB_REMAINING(pbsb) - 4, pdata[2] << 8 | pdata[3]);

            BSB cbsb;
            BSB_INIT(cbsb, pdata+6, plen-2); // The - 4 for plen is done above, confusing

            if(BSB_REMAINING(cbsb) > 32) {
                BSB_IMPORT_skip(cbsb, 32);     // Random

                int skiplen = 0;
                BSB_IMPORT_u08(cbsb, skiplen);   // Session Id Length
                if (skiplen > 0 && BSB_REMAINING(cbsb) > skiplen) {
                    unsigned char *ptr = BSB_WORK_PTR(cbsb);
                    char sessionId[513];
                    int  i;

                    for(i=0; i < skiplen; i++) {
                        sessionId[i*2] = moloch_char_to_hexstr[ptr[i]][0];
                        sessionId[i*2+1] = moloch_char_to_hexstr[ptr[i]][1];
                    }
                    sessionId[skiplen*2] = 0;
                    moloch_field_string_add(srcIdField, session, sessionId, skiplen*2, TRUE);
                }
                BSB_IMPORT_skip(cbsb, skiplen);  // Session Id

                BSB_IMPORT_u16(cbsb, skiplen);   // Ciper Suites Length
                BSB_IMPORT_skip(cbsb, skiplen);  // Ciper Suites

                BSB_IMPORT_u08(cbsb, skiplen);   // Compression Length
                BSB_IMPORT_skip(cbsb, skiplen);  // Compressions

                if (BSB_REMAINING(cbsb) > 2) {
                    int etotlen = 0;
                    BSB_IMPORT_u16(cbsb, etotlen);  // Extensions Length

                    etotlen = MIN(etotlen, BSB_REMAINING(cbsb));

                    BSB ebsb;
                    BSB_INIT(ebsb, BSB_WORK_PTR(cbsb), etotlen);

                    while (BSB_REMAINING(ebsb) > 0) {
                        int etype = 0, elen = 0;

                        BSB_IMPORT_u16 (ebsb, etype);
                        BSB_IMPORT_u16 (ebsb, elen);
                        if (etype != 0) {
                            BSB_IMPORT_skip (ebsb, elen);
                            continue;
                        }

                        if (elen > BSB_REMAINING(ebsb))
                            break;

                        BSB snibsb;
                        BSB_INIT(snibsb, BSB_WORK_PTR(ebsb), elen);
                        BSB_IMPORT_skip (ebsb, elen);

                        int sni = 0;
                        BSB_IMPORT_u16(snibsb, sni); // list len
                        if (sni != BSB_REMAINING(snibsb))
                            continue;

                        BSB_IMPORT_u08(snibsb, sni); // type
                        if (sni != 0)
                            continue;

                        BSB_IMPORT_u16(snibsb, sni); // len
                        if (sni != BSB_REMAINING(snibsb))
                            continue;

                        moloch_field_string_add(hostField, session, (char *)BSB_WORK_PTR(snibsb), sni, TRUE);
                    }
                }
            }
        }

        BSB_IMPORT_skip(sslbsb, ssllen + 5);
    }
}

/******************************************************************************/
int tls_parser(MolochSession_t *session, void *uw, const unsigned char *data, int remaining, int which)
{
    TLSInfo_t            *tls          = uw;

    // If not the server half ignore
    if (which != tls->which)
        return 0;

    // Copy the data we have
    memcpy(tls->buf + tls->len, data, MIN(remaining, (int)sizeof(tls->buf)-tls->len));
    tls->len += MIN(remaining, (int)sizeof(tls->buf)-tls->len);

    // Make sure we have header
    if (tls->len < 5)
        return 0;

    // Not handshake protocol, stop looking
    if (tls->buf[0] != 0x16) {
        tls->len = 0;
        moloch_parsers_unregister(session, uw);
        return 0;
    }

    // Need the whole record
    int need = ((tls->buf[3] << 8) | tls->buf[4]) + 5;
    if (need > tls->len)
        return 0;

    if (tls_process_server_handshake_record(session, tls->buf + 5, need - 5)) {
        tls->len = 0;
        moloch_parsers_unregister(session, uw);
        return 0;
    }
    tls->len -= need;

    // Still more data to process
    if (tls->len) {
        memmove(tls->buf, tls->buf+need, tls->len);
        return 0;
    }

    return 0;
}
/******************************************************************************/
void tls_save(MolochSession_t *session, void *uw, int UNUSED(final))
{
    TLSInfo_t            *tls          = uw;

    if (tls->len > 5 && tls->buf[0] == 0x16) {
        tls_process_server_handshake_record(session, tls->buf+5, tls->len-5);
        tls->len = 0;
    }
}
/******************************************************************************/
void tls_free(MolochSession_t *UNUSED(session), void *uw)
{
    TLSInfo_t            *tls          = uw;

    MOLOCH_TYPE_FREE(TLSInfo_t, tls);
}
/******************************************************************************/
void tls_classify(MolochSession_t *session, const unsigned char *data, int len, int which, void *UNUSED(uw))
{
    if (len < 6 || data[2] > 0x03)
        return;

    if (moloch_session_has_protocol(session, "tls"))
        return;


    /* 1 Content Type - 0x16
     * 2 Version 0x0301 - 0x03-03
     * 2 Length
     * 1 Message Type 1 - Client Hello, 2 Server Hello
     */
    if (data[2] <= 0x03 && (data[5] == 1 || data[5] == 2)) {
        moloch_session_add_protocol(session, "tls");

        TLSInfo_t  *tls = MOLOCH_TYPE_ALLOC(TLSInfo_t);
        tls->len        = 0;

        moloch_parsers_register2(session, tls_parser, tls, tls_free, tls_save);

        if (data[5] == 1) {
            tls_process_client(session, data, (int)len);
            tls->which      = (which + 1) % 2;
        } else {
            tls->which      = which;
        }
    }
}
/******************************************************************************/
void moloch_parser_init()
{
    certsField = moloch_field_define("cert", "notreal",
        "cert", "tls", "tls",
        "CERT Info",
        MOLOCH_FIELD_TYPE_CERTSINFO,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_NODB,
        NULL);

    moloch_field_define("cert", "integer",
        "cert.cnt", "Cert Cnt", "tlscnt",
        "Count of certificates",
        0, MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    moloch_field_define("cert", "lotermfield",
        "cert.alt", "Alt Name", "tls.alt",
        "Certificate alternative names",
        0,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    moloch_field_define("cert", "lotermfield",
        "cert.serial", "Serial Number", "tls.sn",
        "Serial Number",
        0, MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    moloch_field_define("cert", "lotermfield",
        "cert.issuer.cn", "Issuer CN", "tls.iCn",
        "Issuer's common name",
        0, MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    moloch_field_define("cert", "lotermfield",
        "cert.subject.cn", "Subject CN", "tls.sCn",
        "Subject's common name",
        0, MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    moloch_field_define("cert", "lotextfield",
        "cert.issuer.on", "Issuer ON", "tls.iOn",
        "Issuer's organization name",
        0, MOLOCH_FIELD_FLAG_FAKE,
        "rawField", "rawiOn",
        NULL);

    moloch_field_define("cert", "lotextfield",
        "cert.subject.on", "Subject ON", "tls.sOn",
        "Subject's organization name",
        0, MOLOCH_FIELD_FLAG_FAKE,
        "rawField", "rawsOn",
        NULL);

    moloch_field_define("cert", "lotextfield",
        "cert.hash", "Hash", "tls.hash",
        "SHA1 hash of entire certificate",
        0, MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    moloch_field_define("cert", "seconds",
        "cert.notbefore", "Not Before", "tls.notBefore",
        "Certificate is not valid before this date",
        0, MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    moloch_field_define("cert", "seconds",
        "cert.notafter", "Not After", "tls.notAfter",
        "Certificate is not valid after this date",
        0, MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    moloch_field_define("cert", "integer",
        "cert.validfor", "Days Valid For", "tls.diffDays",
        "Certificate is valid for this may days",
        0, MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    hostField = moloch_field_define("http", "lotermfield",
        "host.http", "Hostname", "ho", 
        "HTTP host header field", 
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT, 
        "aliases", "[\"http.host\"]", NULL);

    verField = moloch_field_define("tls", "termfield",
        "tls.version", "Version", "tlsver-term", 
        "SSL/TLS version field", 
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT, 
        NULL);

    cipherField = moloch_field_define("tls", "uptermfield",
        "tls.cipher", "Cipher", "tlscipher-term", 
        "SSL/TLS cipher field", 
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT, 
        NULL);

    dstIdField = moloch_field_define("tls", "lotermfield",
        "tls.sessionid.dst", "Dst Session Id", "tlsdstid-term", 
        "SSL/TLS Dst Session Id", 
        MOLOCH_FIELD_TYPE_STR_HASH,  0, 
        NULL);

    srcIdField = moloch_field_define("tls", "lotermfield",
        "tls.sessionid.src", "Src Session Id", "tlssrcid-term", 
        "SSL/TLS Src Session Id", 
        MOLOCH_FIELD_TYPE_STR_HASH,  0, 
        NULL);

    moloch_field_define("general", "lotermfield",
        "tls.sessionid", "Src or Dst Session Id", "tlsidall",
        "Shorthand for tls.sessionid.src or tls.sessionid.dst",
        0,  MOLOCH_FIELD_FLAG_FAKE,
        "regex", "^tls\\\\.sessionid\\\\.(?:(?!\\\\.cnt$).)*$",
        NULL);

    moloch_parsers_classifier_register_tcp("tls", NULL, 0, (unsigned char*)"\x16\x03", 2, tls_classify);

    int t;
    for (t = 0; t < config.packetThreads; t++) {
        checksums[t] = g_checksum_new(G_CHECKSUM_SHA1);
    }
}

