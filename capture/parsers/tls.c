/* Copyright 2012-2014 AOL Inc. All rights reserved.
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
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "moloch.h"

extern MolochConfig_t        config;
static int                   certsField;
static int                   hostField;

typedef struct {
    unsigned char       buf[8192];
    uint16_t            len;
    char                which;
} TLSInfo_t;

/******************************************************************************/
void
tls_certinfo_process(MolochCertInfo_t *ci, BSB *bsb)
{
    int apc, atag, alen;
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
    int apc, atag, alen;

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
void
tls_process(MolochSession_t *session, const unsigned char *data, int len)
{
    BSB sslbsb;

    BSB_INIT(sslbsb, data, len);

    while (BSB_REMAINING(sslbsb) > 5) {
        unsigned char *ssldata = BSB_WORK_PTR(sslbsb);
        int            ssllen = MIN(BSB_REMAINING(sslbsb) - 5, ssldata[3] << 8 | ssldata[4]);

        BSB pbsb;
        BSB_INIT(pbsb, ssldata+5, ssllen);

        while (BSB_REMAINING(pbsb) > 7) {
            unsigned char *pdata = BSB_WORK_PTR(pbsb);
            int            plen = MIN(BSB_REMAINING(pbsb) - 4, pdata[2] << 8 | pdata[3]);

            if (pdata[0] != 0x0b) {
                BSB_IMPORT_skip(pbsb, plen + 4);
                continue;
            }

            BSB cbsb;
            BSB_INIT(cbsb, pdata+7, plen-3); // The - 4 for plen is done above, confusing

            while(BSB_REMAINING(cbsb) > 3) {
                int            badreason = 0;
                unsigned char *cdata = BSB_WORK_PTR(cbsb);
                int            clen = MIN(BSB_REMAINING(cbsb) - 3, (cdata[0] << 16 | cdata[1] << 8 | cdata[2]));

                MolochCertsInfo_t *certs = MOLOCH_TYPE_ALLOC0(MolochCertsInfo_t);
                DLL_INIT(s_, &certs->alt);
                DLL_INIT(s_, &certs->subject.commonName);
                DLL_INIT(s_, &certs->issuer.commonName);

                int            atag, alen, apc;
                unsigned char *value;

                BSB            bsb;
                BSB_INIT(bsb, cdata + 3, clen);

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

            BSB_IMPORT_skip(pbsb, plen + 4);
        }

        BSB_IMPORT_skip(sslbsb, ssllen + 5);
    }
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
    if (which != tls->which)
        return 0;

    memcpy(tls->buf + tls->len, data, MIN(remaining, (int)sizeof(tls->buf)-tls->len));
    tls->len += MIN(remaining, (int)sizeof(tls->buf)-tls->len);

    if (tls->len > 4096) {
        tls_process(session, tls->buf, tls->len);
        tls->len = 0;
        moloch_parsers_unregister(session, uw);
    }
    return 0;
}
/******************************************************************************/
void tls_save(MolochSession_t *session, void *uw, int UNUSED(final))
{
    TLSInfo_t            *tls          = uw;

    if (tls->len > 0) {
        tls_process(session, tls->buf, tls->len);
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
void tls_classify(MolochSession_t *session, const unsigned char *data, int len, int which)
{
    if (len < 6 || data[2] > 0x03)
        return;

    if (moloch_nids_has_protocol(session, "tls"))
        return;


    /* 1 Content Type - 0x16
     * 2 Version 0x0301 - 0x03-03
     * 2 Length
     * 1 Message Type 1 - Client Hello, 2 Server Hello
     */
    if (data[2] <= 0x03 && (data[5] == 1 || data[5] == 2)) {
        moloch_nids_add_tag(session, "protocol:tls");
        moloch_nids_add_protocol(session, "tls");

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
        "TLS Info",
        MOLOCH_FIELD_TYPE_CERTSINFO,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_NODB,
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

    hostField = moloch_field_define("http", "lotermfield",
        "host.http", "Hostname", "ho", 
        "HTTP host header field", 
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT, 
        "aliases", "[\"http.host\"]", NULL);

    moloch_parsers_classifier_register_tcp("tls", 0, (unsigned char*)"\x16\x03", 2, tls_classify);
}

