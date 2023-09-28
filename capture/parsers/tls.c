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
#include "arkime.h"
#include "tls-cipher.h"
#include "openssl/objects.h"

extern ArkimeConfig_t        config;
LOCAL  int                   certsField;
LOCAL  int                   hostField;
LOCAL  int                   verField;
LOCAL  int                   cipherField;
LOCAL  int                   ja3Field;
LOCAL  int                   ja3sField;
LOCAL  int                   srcIdField;
LOCAL  int                   dstIdField;
LOCAL  int                   ja3StrField;
LOCAL  int                   ja3sStrField;
LOCAL  int                   ja4Field;

typedef struct {
    unsigned char       buf[8192];
    uint16_t            len;
    char                which;
} TLSInfo_t;

extern unsigned char    arkime_char_to_hexstr[256][3];

LOCAL GChecksum *checksums[ARKIME_MAX_PACKET_THREADS];
LOCAL GChecksum *checksums256[ARKIME_MAX_PACKET_THREADS];

/******************************************************************************/
LOCAL void tls_certinfo_process(ArkimeCertInfo_t *ci, BSB *bsb)
{
    uint32_t apc, atag, alen;
    char lastOid[1000];
    lastOid[0] = 0;

    while (BSB_REMAINING(*bsb)) {
        unsigned char *value = arkime_parsers_asn_get_tlv(bsb, &apc, &atag, &alen);
        if (!value)
            return;

        if (apc) {
            BSB tbsb;
            BSB_INIT(tbsb, value, alen);
            tls_certinfo_process(ci, &tbsb);
        } else if (atag  == 6) {
            arkime_parsers_asn_decode_oid(lastOid, sizeof(lastOid), value, alen);
        } else if (lastOid[0] && (atag == 20 || atag == 19 || atag == 12)) {
            /* 20 == BER_UNI_TAG_TeletexString
             * 19 == BER_UNI_TAG_PrintableString
             * 12 == BER_UNI_TAG_UTF8String
             */
            if (strcmp(lastOid, "2.5.4.3") == 0) {
                ArkimeString_t *element = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                element->utf8 = atag == 12;
                if (element->utf8)
                    element->str = g_utf8_strdown((char*)value, alen);
                else
                    element->str = g_ascii_strdown((char*)value, alen);
                DLL_PUSH_TAIL(s_, &ci->commonName, element);
            } else if (strcmp(lastOid, "2.5.4.10") == 0) {
                ArkimeString_t *element = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                element->utf8 = atag == 12;
                element->str = g_strndup((char*)value, alen);
                DLL_PUSH_TAIL(s_, &ci->orgName, element);
            } else if (strcmp(lastOid, "2.5.4.11") == 0) {
                ArkimeString_t *element = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                element->utf8 = atag == 12;
                element->str = g_strndup((char*)value, alen);
                DLL_PUSH_TAIL(s_, &ci->orgUnit, element);
            }
        }
    }
}
/******************************************************************************/
LOCAL void tls_certinfo_process_publickey(ArkimeCertsInfo_t *certs, unsigned char *data, uint32_t len)
{
    BSB bsb, tbsb;
    BSB_INIT(bsb, data, len);
    char oid[1000];

    uint32_t apc, atag, alen;
    unsigned char *value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    BSB_INIT(tbsb, value, alen);
    value = arkime_parsers_asn_get_tlv(&tbsb, &apc, &atag, &alen);
    if (BSB_IS_ERROR(bsb) || BSB_IS_ERROR(tbsb) || !value) {
        certs->publicAlgorithm = "corrupt";
        return;
    }
    oid[0] = 0;
    arkime_parsers_asn_decode_oid(oid, sizeof(oid), value, alen);

    int nid = OBJ_txt2nid(oid);
    if (nid == 0)
        certs->publicAlgorithm = "unknown";
    else
        certs->publicAlgorithm = OBJ_nid2sn(nid);

    if (nid == NID_X9_62_id_ecPublicKey) {
        value = arkime_parsers_asn_get_tlv(&tbsb, &apc, &atag, &alen);
        if (BSB_IS_ERROR(tbsb) || !value || alen > 12)
            certs->curve = "corrupt";
        else {
            oid[0] = 0;
            arkime_parsers_asn_decode_oid(oid, sizeof(oid), value, alen);
            nid = OBJ_txt2nid(oid);
            if (nid == 0)
                certs->curve = "unknown";
            else
                certs->curve = OBJ_nid2sn(nid);
        }
    }
}
/******************************************************************************/
LOCAL void tls_key_usage (ArkimeCertsInfo_t *certs, BSB *bsb)
{
    uint32_t apc, atag, alen;

    while (BSB_REMAINING(*bsb) >= 2) {
        unsigned char *value = arkime_parsers_asn_get_tlv(bsb, &apc, &atag, &alen);

        if (value && atag == 4 && alen == 4)
            certs->isCA = (value[3] & 0x02);
    }
}
/******************************************************************************/
LOCAL void tls_alt_names(ArkimeSession_t *session, ArkimeCertsInfo_t *certs, BSB *bsb, char *lastOid)
{
    uint32_t apc, atag, alen;

    while (BSB_REMAINING(*bsb) >= 2) {
        unsigned char *value = arkime_parsers_asn_get_tlv(bsb, &apc, &atag, &alen);

        if (!value)
            return;

        if (apc) {
            BSB tbsb;
            BSB_INIT(tbsb, value, alen);
            tls_alt_names(session, certs, &tbsb, lastOid);
            if (certs->alt.s_count > 0) {
                return;
            }
        } else if (atag == 6) {
            arkime_parsers_asn_decode_oid(lastOid, 100, value, alen);
            if (strcmp(lastOid, "2.5.29.15") == 0) {
                tls_key_usage(certs, bsb);
            }
            if (strcmp(lastOid, "2.5.29.17") != 0)
                lastOid[0] = 0;
        } else if (lastOid[0] && atag == 4) {
            BSB tbsb;
            BSB_INIT(tbsb, value, alen);
            tls_alt_names(session, certs, &tbsb, lastOid);
            return;
        } else if (lastOid[0] && atag == 2) {
            if (g_utf8_validate((char *)value, alen, NULL)) {
                ArkimeString_t *element = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                element->str = g_ascii_strdown((char *)value, alen);
                element->len = alen;
                element->utf8 = 1;
                DLL_PUSH_TAIL(s_, &certs->alt, element);
            } else {
                arkime_session_add_tag(session, "bad-altname");
            }
        }
    }
    lastOid[0] = 0;
    return;
}
/******************************************************************************/
// https://tools.ietf.org/html/draft-davidben-tls-grease-00
LOCAL int tls_is_grease_value(uint32_t val)
{
    if ((val & 0x0f) != 0x0a)
        return 0;

    if ((val & 0xff) != ((val >> 8) & 0xff))
        return 0;

    return 1;
}
/******************************************************************************/
LOCAL void tls_session_version(ArkimeSession_t *session, uint16_t ver)
{
    char str[100];

    switch (ver) {
    case 0x0100:
        arkime_field_string_add(verField, session, "SSLv1", 5, TRUE);
        break;
    case 0x0200:
        arkime_field_string_add(verField, session, "SSLv2", 5, TRUE);
        break;
    case 0x0300:
        arkime_field_string_add(verField, session, "SSLv3", 5, TRUE);
        break;
    case 0x0301:
        arkime_field_string_add(verField, session, "TLSv1", 5, TRUE);
        break;
    case 0x0302:
        arkime_field_string_add(verField, session, "TLSv1.1", 7, TRUE);
        break;
    case 0x0303:
        arkime_field_string_add(verField, session, "TLSv1.2", 7, TRUE);
        break;
    case 0x0304:
        arkime_field_string_add(verField, session, "TLSv1.3", 7, TRUE);
        break;
    case 0x7f00 ... 0x7fff:
        snprintf(str, sizeof(str), "TLSv1.3-draft-%02d", ver & 0xff);
        arkime_field_string_add(verField, session, str, -1, TRUE);
        break;
    default:
        snprintf(str, sizeof(str), "0x%04x", ver);
        arkime_field_string_add(verField, session, str, 6, TRUE);
    }
}
/******************************************************************************/
LOCAL void tls_ja4_version(uint16_t ver, char vstr[3])
{
    switch (ver) {
    case 0x0100:
        memcpy(vstr, "s1", 3);
        break;
    case 0x0200:
        memcpy(vstr, "s2", 3);
        break;
    case 0x0300:
        memcpy(vstr, "s3", 3);
        break;
    case 0x0301:
        memcpy(vstr, "10", 3);
        break;
    case 0x0302:
        memcpy(vstr, "11", 3);
        break;
    case 0x0303:
        memcpy(vstr, "12", 3);
        break;
    case 0x0304:
        memcpy(vstr, "13", 3);
        break;
    case 0x7f00 ... 0x7fff:
        memcpy(vstr, "13", 3);
        break;
    default:
        memcpy(vstr, "00", 3);
        break;
    }
}
/******************************************************************************/
LOCAL void tls_process_server_hello(ArkimeSession_t *session, const unsigned char *data, int len)
{
    BSB bsb;
    BSB_INIT(bsb, data, len);

    uint16_t ver = 0;
    BSB_IMPORT_u16(bsb, ver);
    BSB_IMPORT_skip(bsb, 32);     // Random

    if(BSB_IS_ERROR(bsb))
        return;

    int  add12Later = FALSE;

    // If ver is 0x303 that means there should be an extended header with actual version
    if (ver != 0x0303)
        tls_session_version(session, ver);
    else
        add12Later = TRUE;

    /* Parse sessionid, only for SSLv3 - TLSv1.2 */
    if (ver >= 0x0300 && ver <= 0x0303) {
        int skiplen = 0;
        BSB_IMPORT_u08(bsb, skiplen);   // Session Id Length
        if (skiplen > 0 && BSB_REMAINING(bsb) > skiplen) {
            unsigned char *ptr = BSB_WORK_PTR(bsb);
            char sessionId[513];
            int  i;
            for(i=0; i < skiplen; i++) {
                sessionId[i*2] = arkime_char_to_hexstr[ptr[i]][0];
                sessionId[i*2+1] = arkime_char_to_hexstr[ptr[i]][1];
            }
            sessionId[skiplen*2] = 0;
            arkime_field_string_add(dstIdField, session, sessionId, skiplen*2, TRUE);
        }
        BSB_IMPORT_skip(bsb, skiplen);  // Session Id
    }

    uint16_t cipher = 0;
    BSB_IMPORT_u16(bsb, cipher);

    /* Parse cipher */
    char *cipherStr = ciphers[cipher >> 8][cipher & 0xff];
    if (cipherStr)
        arkime_field_string_add(cipherField, session, cipherStr, -1, TRUE);
    else {
        char str[100];
        snprintf(str, sizeof(str), "0x%04x", cipher);
        arkime_field_string_add(cipherField, session, str, 6, TRUE);
    }

    BSB_IMPORT_skip(bsb, 1);


    char ja3[30000];
    BSB ja3bsb;
    char eja3[10000];
    BSB eja3bsb;

    BSB_INIT(ja3bsb, ja3, sizeof(ja3));
    BSB_INIT(eja3bsb, eja3, sizeof(eja3));

    if (BSB_REMAINING(bsb) > 2) {
        int etotlen = 0;
        BSB_IMPORT_u16(bsb, etotlen);  // Extensions Length

        etotlen = MIN(etotlen, BSB_REMAINING(bsb));

        BSB ebsb;
        BSB_INIT(ebsb, BSB_WORK_PTR(bsb), etotlen);

        while (BSB_REMAINING(ebsb) > 0) {
            int etype = 0, elen = 0;

            BSB_IMPORT_u16 (ebsb, etype);
            BSB_IMPORT_u16 (ebsb, elen);

            BSB_EXPORT_sprintf(eja3bsb, "%d-", etype);

            if (elen > BSB_REMAINING(ebsb))
                break;

            if (etype == 0x2b && elen == 2) { // etype 0x2b is supported version
                uint16_t supported_version = 0;
                BSB_IMPORT_u16(ebsb, supported_version);

                if (supported_version == 0x0304) {
                    tls_session_version(session, supported_version);
                    add12Later = FALSE;
                }
                continue; // Already processed ebsb above
            }

            if (etype == 0x10) { // etype 0x10 is alpn
                if (elen == 5 && BSB_REMAINING(ebsb) >= 5 && memcmp(BSB_WORK_PTR(ebsb), "\x00\x03\x02\x68\x32", 5) == 0) {
                    arkime_session_add_protocol(session, "http2");
                }
            }

            BSB_IMPORT_skip (ebsb, elen);
        }
        BSB_EXPORT_rewind(eja3bsb, 1); // Remove last -
    }

    if (add12Later)
        tls_session_version(session, 0x303);

    BSB_EXPORT_sprintf(ja3bsb, "%d,%d,%.*s", ver, cipher, (int)BSB_LENGTH(eja3bsb), eja3);

    if (config.ja3Strings) {
        arkime_field_string_add(ja3sStrField, session, ja3, strlen(ja3), TRUE);
    }

    gchar *md5 = g_compute_checksum_for_data(G_CHECKSUM_MD5, (guchar *)ja3, BSB_LENGTH(ja3bsb));
    if (config.debug > 1) {
        LOG("JA3s: %s => %s", ja3, md5);
    }
    if (!arkime_field_string_add(ja3sField, session, md5, 32, FALSE)) {
        g_free(md5);
    }
}

/******************************************************************************/
LOCAL void tls_process_server_certificate(ArkimeSession_t *session, const unsigned char *data, int len)
{

    BSB cbsb;

    BSB_INIT(cbsb, data, len);

    BSB_IMPORT_skip(cbsb, 3); // Length again

    GChecksum * const checksum = checksums[session->thread];

    while(BSB_REMAINING(cbsb) > 3) {
        int            badreason = 0;
        unsigned char *cdata = BSB_WORK_PTR(cbsb);
        int            clen = MIN(BSB_REMAINING(cbsb) - 3, (cdata[0] << 16 | cdata[1] << 8 | cdata[2]));


        ArkimeCertsInfo_t *certs = ARKIME_TYPE_ALLOC0(ArkimeCertsInfo_t);
        DLL_INIT(s_, &certs->alt);
        DLL_INIT(s_, &certs->subject.commonName);
        DLL_INIT(s_, &certs->subject.orgName);
        DLL_INIT(s_, &certs->subject.orgUnit);
        DLL_INIT(s_, &certs->issuer.commonName);
        DLL_INIT(s_, &certs->issuer.orgName);
        DLL_INIT(s_, &certs->issuer.orgUnit);

        uint32_t       atag, alen, apc;
        unsigned char *value;

        BSB            bsb;
        BSB_INIT(bsb, cdata + 3, clen);

        guchar digest[20];
        gsize  dlen = sizeof(digest);

        g_checksum_update(checksum, cdata+3, clen);
        g_checksum_get_digest(checksum, digest, &dlen);
        if (dlen > 0) {
            int i;
            for(i = 0; i < 20; i++) {
                certs->hash[i*3] = arkime_char_to_hexstr[digest[i]][0];
                certs->hash[i*3+1] = arkime_char_to_hexstr[digest[i]][1];
                certs->hash[i*3+2] = ':';
            }
        }
        certs->hash[59] = 0;
        g_checksum_reset(checksum);

        /* Certificate */
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {badreason = 1; goto bad_cert;}
        BSB_INIT(bsb, value, alen);

        /* signedCertificate */
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {badreason = 2; goto bad_cert;}
        BSB_INIT(bsb, value, alen);

        /* serialNumber or version*/
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {badreason = 3; goto bad_cert;}

        if (apc) {
            if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
                {badreason = 4; goto bad_cert;}
        }
        certs->serialNumberLen = alen;
        certs->serialNumber = malloc(alen);
        memcpy(certs->serialNumber, value, alen);

        /* signature */
        if (!arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen))
            {badreason = 5; goto bad_cert;}

        /* issuer */
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {badreason = 6; goto bad_cert;}
        BSB tbsb;
        BSB_INIT(tbsb, value, alen);
        tls_certinfo_process(&certs->issuer, &tbsb);

        /* validity */
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {badreason = 7; goto bad_cert;}

        BSB_INIT(tbsb, value, alen);
        if (!(value = arkime_parsers_asn_get_tlv(&tbsb, &apc, &atag, &alen)))
            {badreason = 7; goto bad_cert;}
        certs->notBefore = arkime_parsers_asn_parse_time(session, atag, value, alen);

        if (!(value = arkime_parsers_asn_get_tlv(&tbsb, &apc, &atag, &alen)))
            {badreason = 7; goto bad_cert;}
        certs->notAfter = arkime_parsers_asn_parse_time(session, atag, value, alen);

        /* subject */
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {badreason = 8; goto bad_cert;}
        BSB_INIT(tbsb, value, alen);
        tls_certinfo_process(&certs->subject, &tbsb);

        /* subjectPublicKeyInfo */
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {badreason = 9; goto bad_cert;}
        tls_certinfo_process_publickey(certs, value, alen);

        /* extensions */
        if (BSB_REMAINING(bsb)) {
            if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
                {badreason = 10; goto bad_cert;}
            BSB_INIT(tbsb, value, alen);
            char lastOid[100];
            lastOid[0] = 0;
            tls_alt_names(session, certs, &tbsb, lastOid);
        }

        // no previous certs AND not a CA AND either no orgName or the same orgName AND the same 1 commonName
        if (!session->fields[certsField] &&
            !certs->isCA &&
            ((certs->subject.orgName.s_count == 1 && certs->issuer.orgName.s_count == 1 && strcmp(certs->subject.orgName.s_next->str, certs->issuer.orgName.s_next->str) == 0) ||
             (certs->subject.orgName.s_count == 0 && certs->issuer.orgName.s_count == 0)) &&
            certs->subject.commonName.s_count == 1 &&
            certs->issuer.commonName.s_count == 1 &&
            strcmp(certs->subject.commonName.s_next->str, certs->issuer.commonName.s_next->str) == 0) {

            arkime_session_add_tag(session, "cert:self-signed");
        }

        if (certs->isCA) {
            arkime_session_add_tag(session, "cert:certificate-authority");
        }


        if (!arkime_field_certsinfo_add(certsField, session, certs, clen*2)) {
            arkime_field_certsinfo_free(certs);
        }

        BSB_IMPORT_skip(cbsb, clen + 3);

        continue;

    bad_cert:
        if (config.debug)
            LOG("bad cert %d - %d", badreason, clen);
        arkime_field_certsinfo_free(certs);
        break;
    }
}
/******************************************************************************/
/* @data the data inside the record layer
 * @len  the length of data inside record layer
 */
LOCAL int tls_process_server_handshake_record(ArkimeSession_t *session, const unsigned char *data, int len)
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
// Comparison function for qsort
int compare_uint16_t(const void* a, const void* b) {
    return (int)(*(const uint16_t *)a - *(const uint16_t *)b);
}
/******************************************************************************/
void tls_process_client_hello_data(ArkimeSession_t *session, const unsigned char *data, int len)
{
    if (len < 7)
        return;

    char ja3[30000];
    BSB ja3bsb;
    char ecfja3[1000];
    BSB ecfja3bsb;
    char eja3[10000];
    BSB eja3bsb;
    char ecja3[10000];
    BSB ecja3bsb;

    char     ja4HasSNI = 'i';
    uint16_t ja4Ciphers[256];
    uint8_t  ja4NumCiphers = 0;
    uint8_t  ja4NumExtensions = 0;
    uint16_t ja4Extensions[256];
    uint8_t  ja4NumExtensionsSome = 0;
    uint8_t  ja4NumAlgos = 0;
    uint16_t ja4Algos[256];
    uint8_t  ja4ALPN[2] = {'0', '0'};

    BSB_INIT(ja3bsb, ja3, sizeof(ja3));
    BSB_INIT(ecja3bsb, ecja3, sizeof(ecja3));
    BSB_INIT(ecfja3bsb, ecfja3, sizeof(ecfja3));
    BSB_INIT(eja3bsb, eja3, sizeof(eja3));

    BSB pbsb;
    BSB_INIT(pbsb, data, len);

    unsigned char *pdata = BSB_WORK_PTR(pbsb);
    int            plen = MIN(BSB_REMAINING(pbsb) - 4, pdata[2] << 8 | pdata[3]);

    uint16_t ver = 0;
    BSB_IMPORT_skip(pbsb, 4); // type + len
    BSB_IMPORT_u16(pbsb, ver);

    BSB_EXPORT_sprintf(ja3bsb, "%d,", ver);


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
                sessionId[i*2] = arkime_char_to_hexstr[ptr[i]][0];
                sessionId[i*2+1] = arkime_char_to_hexstr[ptr[i]][1];
            }
            sessionId[skiplen*2] = 0;
            arkime_field_string_add(srcIdField, session, sessionId, skiplen*2, TRUE);
        }
        BSB_IMPORT_skip(cbsb, skiplen);  // Session Id

        BSB_IMPORT_u16(cbsb, skiplen);   // Ciper Suites Length
        while (BSB_NOT_ERROR(cbsb) && skiplen > 0) {
            uint16_t c = 0;
            BSB_IMPORT_u16(cbsb, c);
            if (!tls_is_grease_value(c)) {
                BSB_EXPORT_sprintf(ja3bsb, "%d-", c);
            }
            ja4Ciphers[ja4NumCiphers] = c;
            ja4NumCiphers++;
            skiplen -= 2;
        }
        BSB_EXPORT_rewind(ja3bsb, 1); // Remove last -
        BSB_EXPORT_u08(ja3bsb, ',');

        BSB_IMPORT_u08(cbsb, skiplen);   // Compression Length
        BSB_IMPORT_skip(cbsb, skiplen);  // Compressions

        if (BSB_REMAINING(cbsb) >= 6) {
            int etotlen = 0;
            BSB_IMPORT_u16(cbsb, etotlen);  // Extensions Length

            etotlen = MIN(etotlen, BSB_REMAINING(cbsb));

            BSB ebsb;
            BSB_INIT(ebsb, BSB_WORK_PTR(cbsb), etotlen);

            while (BSB_REMAINING(ebsb) >= 4) {
                uint16_t etype = 0, elen = 0;

                BSB_IMPORT_u16 (ebsb, etype);
                BSB_IMPORT_u16 (ebsb, elen);

                if (tls_is_grease_value(etype)) {
                    BSB_IMPORT_skip (ebsb, elen);
                }

                ja4NumExtensions++;
                ja4Extensions[ja4NumExtensionsSome] = etype;
                ja4NumExtensionsSome++;

                BSB_EXPORT_sprintf(eja3bsb, "%d-", etype);

                if (elen > BSB_REMAINING(ebsb))
                    break;


                if (etype == 0) { // SNI
                    ja4NumExtensionsSome--;
                    BSB bsb;
                    BSB_IMPORT_bsb (ebsb, bsb, elen);

                    int sni = 0;
                    BSB_IMPORT_u16(bsb, sni); // list len
                    if (sni != BSB_REMAINING(bsb))
                        continue;

                    BSB_IMPORT_u08(bsb, sni); // type
                    if (sni != 0)
                        continue;

                    BSB_IMPORT_u16(bsb, sni); // len
                    if (sni != BSB_REMAINING(bsb))
                        continue;

                    arkime_field_string_add(hostField, session, (char *)BSB_WORK_PTR(bsb), sni, TRUE);
                    ja4HasSNI = 'd';
                } else if (etype == 0x000a) { // Elliptic Curves
                    BSB bsb;
                    BSB_IMPORT_bsb (ebsb, bsb, elen);

                    uint16_t llen = 0;
                    BSB_IMPORT_u16(bsb, llen); // list len
                    while (llen > 0 && !BSB_IS_ERROR(bsb)) {
                        uint16_t c = 0;
                        BSB_IMPORT_u16(bsb, c);
                        if (!tls_is_grease_value(c)) {
                            BSB_EXPORT_sprintf(ecja3bsb, "%d-", c);
                        }
                        llen -= 2;
                    }
                    BSB_EXPORT_rewind(ecja3bsb, 1); // Remove last -
                } else if (etype == 0x000b) { // Elliptic Curves point formats
                    BSB bsb;
                    BSB_IMPORT_bsb (ebsb, bsb, elen);

                    uint16_t llen = 0;
                    BSB_IMPORT_u08(bsb, llen); // list len
                    while (llen > 0 && !BSB_IS_ERROR(bsb)) {
                        uint8_t c = 0;
                        BSB_IMPORT_u08(bsb, c);
                        BSB_EXPORT_sprintf(ecfja3bsb, "%d-", c);
                        llen -= 1;
                    }
                    BSB_EXPORT_rewind(ecfja3bsb, 1); // Remove last -
                } else if (etype == 0x000d) { // Signature Algorithms
                    BSB bsb;
                    BSB_IMPORT_bsb (ebsb, bsb, elen);

                    uint16_t llen = 0;
                    BSB_IMPORT_u16(bsb, llen); // list len
                    while (llen > 0 && !BSB_IS_ERROR(bsb)) {
                        uint16_t a = 0;
                        BSB_IMPORT_u16(bsb, a);
                        ja4Algos[ja4NumAlgos++] = a;
                        llen -= 2;
                    }
                } else if (etype == 0x10) { // ALPN
                    ja4NumExtensionsSome--;
                    BSB bsb;
                    BSB_IMPORT_bsb (ebsb, bsb, elen);

                    BSB_IMPORT_skip (bsb, 2); // len
                    uint8_t plen = 0;
                    BSB_IMPORT_u08 (bsb, plen); // len
                    unsigned char *pstr = NULL;
                    BSB_IMPORT_ptr (bsb, pstr, plen);
                    if (plen > 0 && pstr && !BSB_IS_ERROR(bsb)) {
                        ja4ALPN[0] = pstr[0];
                        ja4ALPN[1] = pstr[plen-1];
                    }
                } else if (etype == 0x2b) { // etype 0x2b is supported version
                    BSB bsb;
                    BSB_IMPORT_bsb (ebsb, bsb, elen);

                    uint16_t llen = 0;
                    BSB_IMPORT_u08(bsb, llen); // list len
                    while (llen > 0 && !BSB_IS_ERROR(bsb)) {
                        uint16_t supported_version = 0;
                        BSB_IMPORT_u16(bsb, supported_version);
                        ver = MAX(supported_version, ver);
                        llen--;
                    }
                } else {
                    BSB_IMPORT_skip (ebsb, elen);
                }
            }
            BSB_EXPORT_rewind(eja3bsb, 1); // Remove last -
        }
    }

    if (BSB_LENGTH(ja3bsb) > 0 && BSB_NOT_ERROR(ja3bsb) && BSB_NOT_ERROR(ecja3bsb) && BSB_NOT_ERROR(eja3bsb) && BSB_NOT_ERROR(ecfja3bsb)) {
        BSB_EXPORT_sprintf(ja3bsb, "%.*s,%.*s,%.*s", (int)BSB_LENGTH(eja3bsb), eja3, (int)BSB_LENGTH(ecja3bsb), ecja3, (int)BSB_LENGTH(ecfja3bsb), ecfja3);

        if (config.ja3Strings) {
            arkime_field_string_add(ja3StrField, session, ja3, strlen(ja3), TRUE);
        }

        gchar *md5 = g_compute_checksum_for_data(G_CHECKSUM_MD5, (guchar *)ja3, BSB_LENGTH(ja3bsb));

        if (config.debug > 1) {
            LOG("JA3: %s => %s", ja3, md5);
        }
        if (!arkime_field_string_add(ja3Field, session, md5, 32, FALSE)) {
            g_free(md5);
        }
    }

    char vstr[3];
    tls_ja4_version(ver, vstr);

    char ja4[37];
    ja4[36] = 0;
    ja4[0] = (session->ipProtocol == IPPROTO_TCP) ? 't' : 'q';
    ja4[1] = vstr[0];
    ja4[2] = vstr[1];
    ja4[3] = ja4HasSNI;
    ja4[4] = (ja4NumCiphers / 10) + '0';
    ja4[5] = (ja4NumCiphers % 10) + '0';
    ja4[6] = (ja4NumExtensions / 10) + '0';
    ja4[7] = (ja4NumExtensions % 10) + '0';
    ja4[8] = ja4ALPN[0];
    ja4[9] = ja4ALPN[1];
    ja4[10] = '_';

    char tmpBuf[10*256];
    BSB tmpBSB;

    // Sort ciphers, convert to hex, first 12 bytes of sha256
    qsort(ja4Ciphers, ja4NumCiphers, 2, compare_uint16_t);
    BSB_INIT(tmpBSB, tmpBuf, sizeof(tmpBuf));
    for (int i = 0; i < ja4NumCiphers; i++) {
        BSB_EXPORT_sprintf(tmpBSB, "%04x,", ja4Ciphers[i]);
    }
    BSB_EXPORT_rewind(tmpBSB, 1); // Remove last ,

    GChecksum * const checksum = checksums256[session->thread];

    if (BSB_LENGTH(tmpBSB) > 0) {
        g_checksum_update(checksum, (guchar *)tmpBuf, BSB_LENGTH(tmpBSB));
        memcpy(ja4 + 11, g_checksum_get_string(checksum), 12);
        g_checksum_reset(checksum);
    } else {
        memcpy(ja4 + 11, "000000000000", 12);
    }

    ja4[23] = '_';

    // Sort the extensions, convert to hex, add unsorted Algos, first 12 bytes of sha256
    qsort(ja4Extensions, ja4NumExtensionsSome, 2, compare_uint16_t);
    BSB_INIT(tmpBSB, tmpBuf, sizeof(tmpBuf));
    for (int i = 0; i < ja4NumExtensionsSome; i++) {
        BSB_EXPORT_sprintf(tmpBSB, "%04x,", ja4Extensions[i]);
    }
    BSB_EXPORT_rewind(tmpBSB, 1); // Remove last ,
    if (ja4NumAlgos > 0) {
        BSB_EXPORT_u08(tmpBSB, '_');
        for (int i = 0; i < ja4NumAlgos; i++) {
            BSB_EXPORT_sprintf(tmpBSB, "%04x,", ja4Algos[i]);
        }
        BSB_EXPORT_rewind(tmpBSB, 1); // Remove last ,
    }

    if (BSB_LENGTH(tmpBSB) > 0) {
        g_checksum_update(checksum, (guchar *)tmpBuf, BSB_LENGTH(tmpBSB));
        memcpy(ja4 + 24, g_checksum_get_string(checksum), 12);
        g_checksum_reset(checksum);
    } else {
        memcpy(ja4 + 24, "000000000000", 12);
    }

    // Add the field
    arkime_field_string_add(ja4Field, session, ja4, 36, TRUE);
}
/******************************************************************************/
LOCAL void tls_process_client(ArkimeSession_t *session, const unsigned char *data, int len)
{
    BSB sslbsb;

    BSB_INIT(sslbsb, data, len);

    if (BSB_REMAINING(sslbsb) > 5) {
        unsigned char *ssldata = BSB_WORK_PTR(sslbsb);
        int            ssllen = MIN(BSB_REMAINING(sslbsb) - 5, ssldata[3] << 8 | ssldata[4]);


        tls_process_client_hello_data(session, ssldata + 5, ssllen);
    }
}

/******************************************************************************/
LOCAL int tls_parser(ArkimeSession_t *session, void *uw, const unsigned char *data, int remaining, int which)
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
        arkime_parsers_unregister(session, uw);
        return 0;
    }

    // Need the whole record
    int need = ((tls->buf[3] << 8) | tls->buf[4]) + 5;
    if (need > tls->len)
        return 0;

    if (tls_process_server_handshake_record(session, tls->buf + 5, need - 5)) {
        tls->len = 0;
        arkime_parsers_unregister(session, uw);
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
LOCAL void tls_save(ArkimeSession_t *session, void *uw, int UNUSED(final))
{
    TLSInfo_t            *tls          = uw;

    if (tls->len > 5 && tls->buf[0] == 0x16) {
        tls_process_server_handshake_record(session, tls->buf+5, tls->len-5);
        tls->len = 0;
    }
}
/******************************************************************************/
LOCAL void tls_free(ArkimeSession_t *UNUSED(session), void *uw)
{
    TLSInfo_t            *tls          = uw;

    ARKIME_TYPE_FREE(TLSInfo_t, tls);
}
/******************************************************************************/
LOCAL void tls_classify(ArkimeSession_t *session, const unsigned char *data, int len, int which, void *UNUSED(uw))
{
    if (len < 6 || data[2] > 0x03)
        return;

    if (arkime_session_has_protocol(session, "tls"))
        return;


    /* 1 Content Type - 0x16
     * 2 Version 0x0301 - 0x03-03
     * 2 Length
     * 1 Message Type 1 - Client Hello, 2 Server Hello
     */
    if (data[2] <= 0x03 && (data[5] == 1 || data[5] == 2)) {
        arkime_session_add_protocol(session, "tls");

        TLSInfo_t  *tls = ARKIME_TYPE_ALLOC(TLSInfo_t);
        tls->len        = 0;

        arkime_parsers_register2(session, tls_parser, tls, tls_free, tls_save);

        if (data[5] == 1) {
            tls_process_client(session, data, (int)len);
            tls->which      = (which + 1) % 2;
        } else {
            tls->which      = which;
        }
    }
}
/******************************************************************************/
void arkime_parser_init()
{
    certsField = arkime_field_define("cert", "notreal",
        "cert", "cert", "cert",
        "CERT Info",
        ARKIME_FIELD_TYPE_CERTSINFO,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_NODB,
        (char *)NULL);

    arkime_field_define("cert", "integer",
        "cert.cnt", "Cert Cnt", "certCnt",
        "Count of certificates",
        0, ARKIME_FIELD_FLAG_FAKE,
        (char *)NULL);

    arkime_field_define("cert", "lotermfield",
        "cert.alt", "Alt Name", "cert.alt",
        "Certificate alternative names",
        0,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_FAKE,
        (char *)NULL);

    arkime_field_define("cert", "lotermfield",
        "cert.serial", "Serial Number", "cert.serial",
        "Serial Number",
        0, ARKIME_FIELD_FLAG_FAKE,
        (char *)NULL);

    arkime_field_define("cert", "lotermfield",
        "cert.issuer.cn", "Issuer CN", "cert.issuerCN",
        "Issuer's common name",
        0, ARKIME_FIELD_FLAG_FAKE,
        (char *)NULL);

    arkime_field_define("cert", "lotermfield",
        "cert.subject.cn", "Subject CN", "cert.subjectCN",
        "Subject's common name",
        0, ARKIME_FIELD_FLAG_FAKE,
        (char *)NULL);

    arkime_field_define("cert", "termfield",
        "cert.issuer.on", "Issuer ON", "cert.issuerON",
        "Issuer's organization name",
        0, ARKIME_FIELD_FLAG_FAKE,
        (char *)NULL);

    arkime_field_define("cert", "termfield",
        "cert.subject.on", "Subject ON", "cert.subjectON",
        "Subject's organization name",
        0, ARKIME_FIELD_FLAG_FAKE,
        (char *)NULL);

    arkime_field_define("cert", "termfield",
        "cert.issuer.ou", "Issuer Org Unit", "cert.issuerOU",
        "Issuer's organizational unit",
        0, ARKIME_FIELD_FLAG_FAKE,
        (char *)NULL);

    arkime_field_define("cert", "termfield",
        "cert.subject.ou", "Subject Org Unit", "cert.subjectOU",
        "Subject's organizational unit",
        0, ARKIME_FIELD_FLAG_FAKE,
        (char *)NULL);

    arkime_field_define("cert", "lotermfield",
        "cert.hash", "Hash", "cert.hash",
        "SHA1 hash of entire certificate",
        0, ARKIME_FIELD_FLAG_FAKE,
        (char *)NULL);

    arkime_field_define("cert", "date",
        "cert.notbefore", "Not Before", "cert.notBefore",
        "Certificate is not valid before this date",
        0, ARKIME_FIELD_FLAG_FAKE,
        (char *)NULL);

    arkime_field_define("cert", "date",
        "cert.notafter", "Not After", "cert.notAfter",
        "Certificate is not valid after this date",
        0, ARKIME_FIELD_FLAG_FAKE,
        (char *)NULL);

    arkime_field_define("cert", "integer",
        "cert.validfor", "Days Valid For", "cert.validDays",
        "Certificate is valid for this many days total",
        0, ARKIME_FIELD_FLAG_FAKE,
        (char *)NULL);

    arkime_field_define("cert", "integer",
        "cert.remainingDays", "Days remaining", "cert.remainingDays",
        "Certificate is still valid for this many days",
        0, ARKIME_FIELD_FLAG_FAKE,
        (char *)NULL);

    arkime_field_define("cert", "integer",
        "cert.validforSeconds", "Seconds Valid For", "cert.validSeconds",
        "Certificate is valid for this many seconds total",
        0, ARKIME_FIELD_FLAG_FAKE,
        (char *)NULL);

    arkime_field_define("cert", "integer",
        "cert.remainingSeconds", "Seconds remaining", "cert.remainingSeconds",
        "Certificate is still valid for this many seconds",
        0, ARKIME_FIELD_FLAG_FAKE,
        (char *)NULL);

    arkime_field_define("cert", "termfield",
        "cert.curve", "Curve", "cert.curve",
        "Curve Algorithm",
        0, ARKIME_FIELD_FLAG_FAKE,
        (char *)NULL);

    arkime_field_define("cert", "termfield",
        "cert.publicAlgorithm", "Public Algorithm", "cert.publicAlgorithm",
        "Public Key Algorithm",
        0, ARKIME_FIELD_FLAG_FAKE,
        (char *)NULL);

    hostField = arkime_field_by_exp("host.http");

    verField = arkime_field_define("tls", "termfield",
        "tls.version", "Version", "tls.version",
        "SSL/TLS version field",
        ARKIME_FIELD_TYPE_STR_GHASH,  ARKIME_FIELD_FLAG_CNT,
        (char *)NULL);

    cipherField = arkime_field_define("tls", "uptermfield",
        "tls.cipher", "Cipher", "tls.cipher",
        "SSL/TLS cipher field",
        ARKIME_FIELD_TYPE_STR_GHASH,  ARKIME_FIELD_FLAG_CNT,
        (char *)NULL);

    ja3Field = arkime_field_define("tls", "lotermfield",
        "tls.ja3", "JA3", "tls.ja3",
        "SSL/TLS JA3 field",
        ARKIME_FIELD_TYPE_STR_GHASH,  ARKIME_FIELD_FLAG_CNT,
        (char *)NULL);

    ja4Field = arkime_field_define("tls", "lotermfield",
        "tls.ja4", "JA4", "tls.ja4",
        "SSL/TLS JA4 field",
        ARKIME_FIELD_TYPE_STR_GHASH,  ARKIME_FIELD_FLAG_CNT,
        (char *)NULL);

    ja3sField = arkime_field_define("tls", "lotermfield",
        "tls.ja3s", "JA3S", "tls.ja3s",
        "SSL/TLS JA3S field",
        ARKIME_FIELD_TYPE_STR_GHASH,  ARKIME_FIELD_FLAG_CNT,
        (char *)NULL);

    dstIdField = arkime_field_define("tls", "lotermfield",
        "tls.sessionid.dst", "Dst Session Id", "tls.dstSessionId",
        "SSL/TLS Dst Session Id",
        ARKIME_FIELD_TYPE_STR_GHASH,  0,
        (char *)NULL);

    srcIdField = arkime_field_define("tls", "lotermfield",
        "tls.sessionid.src", "Src Session Id", "tls.srcSessionId",
        "SSL/TLS Src Session Id",
        ARKIME_FIELD_TYPE_STR_GHASH,  0,
        (char *)NULL);

    arkime_field_define("general", "lotermfield",
        "tls.sessionid", "Src or Dst Session Id", "tlsidall",
        "Shorthand for tls.sessionid.src or tls.sessionid.dst",
        0,  ARKIME_FIELD_FLAG_FAKE,
        "regex", "^tls\\\\.sessionid\\\\.(?:(?!\\\\.cnt$).)*$",
        (char *)NULL);

    if (config.ja3Strings) {
        ja3sStrField = arkime_field_define("tls","lotermfield",
            "tls.ja3sstring", "JA3SSTR", "tls.ja3sstring",
            "SSL/TLS JA3S String field",
            ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
            (char *)NULL);

        ja3StrField = arkime_field_define("tls","lotermfield",
            "tls.ja3string", "JA3STR", "tls.ja3string",
            "SSL/TLS JA3 String field",
            ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
            (char *)NULL);
    }

    arkime_parsers_classifier_register_tcp("tls", NULL, 0, (unsigned char*)"\x16\x03", 2, tls_classify);

    int t;
    for (t = 0; t < config.packetThreads; t++) {
        checksums[t] = g_checksum_new(G_CHECKSUM_SHA1);
        checksums256[t] = g_checksum_new(G_CHECKSUM_SHA256);
    }
}

