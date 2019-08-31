/* Copyright 2019 AOL Inc. All rights reserved.
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

//#define DTLSDEBUG 1

extern MolochConfig_t        config;
LOCAL  int                   certsField;

/******************************************************************************/
LOCAL void dtls_certinfo_process(MolochCertInfo_t *ci, BSB *bsb)
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
            dtls_certinfo_process(ci, &tbsb);
        } else if (atag  == 6)  {
            moloch_parsers_asn_decode_oid(lastOid, sizeof(lastOid), value, alen);
        } else if (lastOid[0] && (atag == 20 || atag == 19 || atag == 12))  {
            /* 20 == BER_UNI_TAG_TeletexString
             * 19 == BER_UNI_TAG_PrintableString
             * 12 == BER_UNI_TAG_UTF8String
             */
            if (strcmp(lastOid, "2.5.4.3") == 0) {
                MolochString_t *element = MOLOCH_TYPE_ALLOC0(MolochString_t);
                element->utf8 = atag == 12;
                if (element->utf8)
                    element->str = g_utf8_strdown((char*)value, alen);
                else
                    element->str = g_ascii_strdown((char*)value, alen);
                DLL_PUSH_TAIL(s_, &ci->commonName, element);
            } else if (strcmp(lastOid, "2.5.4.10") == 0) {
                MolochString_t *element = MOLOCH_TYPE_ALLOC0(MolochString_t);
                element->utf8 = atag == 12;
                element->str = g_strndup((char*)value, alen);
                DLL_PUSH_TAIL(s_, &ci->orgName, element);
            }
        }
    }
}
/******************************************************************************/
LOCAL void dtls_key_usage (MolochCertsInfo_t *certs, BSB *bsb)
{
    uint32_t apc, atag, alen;

    while (BSB_REMAINING(*bsb) >= 2) {
        unsigned char *value = moloch_parsers_asn_get_tlv(bsb, &apc, &atag, &alen);

        if (value && atag == 4 && alen == 4)
            certs->isCA = (value[3] & 0x02);
    }
}
/******************************************************************************/
LOCAL void dtls_alt_names(MolochCertsInfo_t *certs, BSB *bsb, char *lastOid)
{
    uint32_t apc, atag, alen;

    while (BSB_REMAINING(*bsb) >= 2) {
        unsigned char *value = moloch_parsers_asn_get_tlv(bsb, &apc, &atag, &alen);

        if (!value)
            return;

        if (apc) {
            BSB tbsb;
            BSB_INIT(tbsb, value, alen);
            dtls_alt_names(certs, &tbsb, lastOid);
            if (certs->alt.s_count > 0) {
                return;
            }
        } else if (atag == 6)  {
            moloch_parsers_asn_decode_oid(lastOid, 100, value, alen);
            if (strcmp(lastOid, "2.5.29.15") == 0) {
                dtls_key_usage(certs, bsb);
            }
            if (strcmp(lastOid, "2.5.29.17") != 0)
                lastOid[0] = 0;
        } else if (lastOid[0] && atag == 4) {
            BSB tbsb;
            BSB_INIT(tbsb, value, alen);
            dtls_alt_names(certs, &tbsb, lastOid);
            return;
        } else if (lastOid[0] && atag == 2) {
            MolochString_t *element = MOLOCH_TYPE_ALLOC0(MolochString_t);
            element->str = g_ascii_strdown((char*)value, alen);
            element->len = alen;
            element->utf8 = 1;
            DLL_PUSH_TAIL(s_, &certs->alt, element);
        }
    }
    lastOid[0] = 0;
    return;
}
/******************************************************************************/
LOCAL void dtls_process_server_certificate(MolochSession_t *session, const unsigned char *data, int len)
{

    BSB cbsb;

    BSB_INIT(cbsb, data, len);

    BSB_IMPORT_skip(cbsb, 3); // Length again

    //GChecksum * const checksum = checksums[session->thread];

    while(BSB_REMAINING(cbsb) > 3) {
        int            badreason = 0;
        unsigned char *cdata = BSB_WORK_PTR(cbsb);
        int            clen = MIN(BSB_REMAINING(cbsb) - 3, (cdata[0] << 16 | cdata[1] << 8 | cdata[2]));


        MolochCertsInfo_t *certs = MOLOCH_TYPE_ALLOC0(MolochCertsInfo_t);
        DLL_INIT(s_, &certs->alt);
        DLL_INIT(s_, &certs->subject.commonName);
        DLL_INIT(s_, &certs->subject.orgName);
        DLL_INIT(s_, &certs->issuer.commonName);
        DLL_INIT(s_, &certs->issuer.orgName);

        uint32_t       atag, alen, apc;
        unsigned char *value;

        BSB            bsb;
        BSB_INIT(bsb, cdata + 3, clen);

        /*guchar digest[20];
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
        g_checksum_reset(checksum);*/

        /* Certificate */
        if (!(value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {badreason = 1; goto bad_cert;}
        BSB_INIT(bsb, value, alen);
        LOG("ALW: %d %d %d", apc, atag, alen);

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
        dtls_certinfo_process(&certs->issuer, &tbsb);

        /* validity */
        if (!(value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {badreason = 7; goto bad_cert;}

        BSB_INIT(tbsb, value, alen);
        if (!(value = moloch_parsers_asn_get_tlv(&tbsb, &apc, &atag, &alen)))
            {badreason = 7; goto bad_cert;}
        certs->notBefore = moloch_parsers_asn_parse_time(session, atag, value, alen);

        if (!(value = moloch_parsers_asn_get_tlv(&tbsb, &apc, &atag, &alen)))
            {badreason = 7; goto bad_cert;}
        certs->notAfter = moloch_parsers_asn_parse_time(session, atag, value, alen);

        /* subject */
        if (!(value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {badreason = 8; goto bad_cert;}
        BSB_INIT(tbsb, value, alen);
        dtls_certinfo_process(&certs->subject, &tbsb);

        /* subjectPublicKeyInfo */
        if (!(value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {badreason = 9; goto bad_cert;}

        /* extensions */
        if (BSB_REMAINING(bsb)) {
            if (!(value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
                {badreason = 10; goto bad_cert;}
            BSB_INIT(tbsb, value, alen);
            char lastOid[100];
            lastOid[0] = 0;
            dtls_alt_names(certs, &tbsb, lastOid);
        }

        // no previous certs AND not a CA AND either no orgName or the same orgName AND the same 1 commonName
        if (!session->fields[certsField] &&
            !certs->isCA &&
            ((certs->subject.orgName.s_count == 1 && certs->issuer.orgName.s_count == 1 && strcmp(certs->subject.orgName.s_next->str, certs->issuer.orgName.s_next->str) == 0) ||
             (certs->subject.orgName.s_count == 0 && certs->issuer.orgName.s_count == 0)) &&
            certs->subject.commonName.s_count == 1 &&
            certs->issuer.commonName.s_count == 1 &&
            strcmp(certs->subject.commonName.s_next->str, certs->issuer.commonName.s_next->str) == 0) {

            moloch_session_add_tag(session, "cert:self-signed");
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
LOCAL int dtls_udp_parser(MolochSession_t *session, void *UNUSED(uw), const unsigned char *data, int len, int UNUSED(which))
{
    BSB bbuf;

    // 22 is handshake
    if (data[0] != 22) {
        moloch_parsers_unregister(session, uw);
        return 0;
    }

    BSB_INIT(bbuf, data, len);

    while (BSB_NOT_ERROR(bbuf) && BSB_REMAINING(bbuf) > 11) {
        BSB_IMPORT_skip(bbuf, 11);
        uint16_t tlen = 0;
        BSB_IMPORT_u16(bbuf, tlen);

        if (tlen > BSB_REMAINING(bbuf))
            return 0;

        BSB msgBuf;
        BSB_INIT(msgBuf, BSB_WORK_PTR(bbuf), tlen);
        BSB_IMPORT_skip(bbuf, tlen);

        while (BSB_NOT_ERROR(bbuf) && BSB_NOT_ERROR(msgBuf) && BSB_REMAINING(msgBuf) > 12) {
            uint8_t handshakeType = 0;
            BSB_IMPORT_u08(msgBuf, handshakeType);
            uint32_t handshakeLen = 0;
            BSB_IMPORT_u24(msgBuf, handshakeLen);
            uint16_t msgSeq = 0;
            BSB_IMPORT_u16(msgBuf, msgSeq);
            uint32_t frameOffset = 0;
            BSB_IMPORT_u24(msgBuf, frameOffset);
            uint32_t frameLength = 0;
            BSB_IMPORT_u24(msgBuf, frameLength);
            // ALW fix - don't handle fragmented packets yet
            if (frameOffset != 0) {
                BSB_IMPORT_skip(msgBuf, handshakeLen);
                continue;
            }

            // Not enough data left
            if (handshakeLen > BSB_REMAINING(msgBuf))
                break;

            switch (handshakeType) {
            case 11: // Certificate
                dtls_process_server_certificate(session, BSB_WORK_PTR(msgBuf), handshakeLen);
                BSB_IMPORT_skip(msgBuf, handshakeLen);
                break;
            default:
                BSB_IMPORT_skip(msgBuf, handshakeLen);
            }
        }
    }

    return 0;
}
/******************************************************************************/
LOCAL void dtls_udp_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 100 || data[13] != 1)
        return;
    moloch_session_add_protocol(session, "dtls");
    moloch_parsers_register(session, dtls_udp_parser, uw, 0);
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_parsers_classifier_register_udp("dtls", NULL, 0, (const unsigned char *)"\x16\x01\x00", 3, dtls_udp_classify);
    moloch_parsers_classifier_register_udp("dtls", NULL, 0, (const unsigned char *)"\x16\xfe\xff", 3, dtls_udp_classify);
    moloch_parsers_classifier_register_udp("dtls", NULL, 0, (const unsigned char *)"\x16\xfe\xfe", 3, dtls_udp_classify);
    moloch_parsers_classifier_register_udp("dtls", NULL, 0, (const unsigned char *)"\x16\xfe\xfd", 3, dtls_udp_classify);

    certsField = moloch_field_define("cert", "notreal",
        "cert", "cert", "cert",
        "CERT Info",
        MOLOCH_FIELD_TYPE_CERTSINFO,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_NODB,
        (char *)NULL);
}
