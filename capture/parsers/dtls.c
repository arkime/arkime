/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

//#define DTLSDEBUG 1

typedef struct {
    ArkimeStringHead_t  commonName; // 2.5.4.3
    ArkimeStringHead_t  orgName;    // 2.5.4.10
    ArkimeStringHead_t  orgUnit;    // 2.5.4.11
    char                orgUtf8;
} CertNames_t;

typedef struct {
    uint64_t           notBefore;
    uint64_t           notAfter;
    CertNames_t        issuer;
    CertNames_t        subject;
    ArkimeStringHead_t alt;
    uint8_t           *serialNumber;
    short              serialNumberLen;
    uint8_t            hash[60];
    char               isCA;
    const char        *publicAlgorithm;
    const char        *curve;
} CertInfo_t;

extern ArkimeConfig_t        config;
LOCAL  int                   certsField;

void certinfo_save(BSB *jbsb, ArkimeFieldObject_t *object, ArkimeSession_t *session);
void certinfo_free(ArkimeFieldObject_t *object);
uint32_t certinfo_hash(const void *key);
int certinfo_cmp(const void *keyv, const void *elementv);

/******************************************************************************/
LOCAL void dtls_certinfo_process(CertNames_t *ci, BSB *bsb)
{
    uint32_t apc, atag, alen;
    char lastOid[1000];
    lastOid[0] = 0;

    while (BSB_REMAINING(*bsb)) {
        uint8_t *value = arkime_parsers_asn_get_tlv(bsb, &apc, &atag, &alen);
        if (!value)
            return;

        if (apc) {
            BSB tbsb;
            BSB_INIT(tbsb, value, alen);
            dtls_certinfo_process(ci, &tbsb);
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
                    element->str = g_utf8_strdown((char * )value, alen);
                else
                    element->str = g_ascii_strdown((char *)value, alen);
                DLL_PUSH_TAIL(s_, &ci->commonName, element);
            } else if (strcmp(lastOid, "2.5.4.10") == 0) {
                ArkimeString_t *element = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                element->utf8 = atag == 12;
                element->str = g_strndup((char *)value, alen);
                DLL_PUSH_TAIL(s_, &ci->orgName, element);
            } else if (strcmp(lastOid, "2.5.4.11") == 0) {
                ArkimeString_t *element = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                element->utf8 = atag == 12;
                element->str = g_strndup((char *)value, alen);
                DLL_PUSH_TAIL(s_, &ci->orgUnit, element);
            }
        }
    }
}
/******************************************************************************/
LOCAL void dtls_key_usage (CertInfo_t *certs, BSB *bsb)
{
    uint32_t apc, atag, alen;

    while (BSB_REMAINING(*bsb) >= 2) {
        const uint8_t *value = arkime_parsers_asn_get_tlv(bsb, &apc, &atag, &alen);

        if (value && atag == 4 && alen == 4)
            certs->isCA = (value[3] & 0x02);
    }
}
/******************************************************************************/
LOCAL void dtls_alt_names(CertInfo_t *certs, BSB *bsb, char *lastOid)
{
    uint32_t apc, atag, alen;

    while (BSB_REMAINING(*bsb) >= 2) {
        uint8_t *value = arkime_parsers_asn_get_tlv(bsb, &apc, &atag, &alen);

        if (!value)
            return;

        if (apc) {
            BSB tbsb;
            BSB_INIT(tbsb, value, alen);
            dtls_alt_names(certs, &tbsb, lastOid);
            if (certs->alt.s_count > 0) {
                return;
            }
        } else if (atag == 6) {
            arkime_parsers_asn_decode_oid(lastOid, 100, value, alen);
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
            ArkimeString_t *element = ARKIME_TYPE_ALLOC0(ArkimeString_t);
            element->str = g_ascii_strdown((char *)value, alen);
            element->len = alen;
            element->utf8 = 1;
            DLL_PUSH_TAIL(s_, &certs->alt, element);
        }
    }
    lastOid[0] = 0;
    return;
}
/******************************************************************************/
LOCAL void dtls_process_server_certificate(ArkimeSession_t *session, const uint8_t *data, int len)
{

    BSB cbsb;

    BSB_INIT(cbsb, data, len);

    BSB_IMPORT_skip(cbsb, 3); // Length again

    //GChecksum * const checksum = checksums[session->thread];

    while(BSB_REMAINING(cbsb) > 3) {
        int            badreason = 0;
        uint8_t *cdata = BSB_WORK_PTR(cbsb);
        int            clen = MIN(BSB_REMAINING(cbsb) - 3, (cdata[0] << 16 | cdata[1] << 8 | cdata[2]));

        ArkimeFieldObject_t *fobject = ARKIME_TYPE_ALLOC0(ArkimeFieldObject_t);

        CertInfo_t *certs = ARKIME_TYPE_ALLOC0(CertInfo_t);
        DLL_INIT(s_, &certs->alt);
        DLL_INIT(s_, &certs->subject.commonName);
        DLL_INIT(s_, &certs->subject.orgName);
        DLL_INIT(s_, &certs->subject.orgUnit);
        DLL_INIT(s_, &certs->issuer.commonName);
        DLL_INIT(s_, &certs->issuer.orgName);
        DLL_INIT(s_, &certs->issuer.orgUnit);

        fobject->object = certs;

        uint32_t       atag, alen, apc;
        uint8_t *value;

        BSB            bsb;
        BSB_INIT(bsb, cdata + 3, clen);

        /*guchar digest[20];
        gsize  len = sizeof(digest);

        g_checksum_update(checksum, cdata + 3, clen);
        g_checksum_get_digest(checksum, digest, &len);
        if (len > 0) {
            int i;
            for(i = 0; i < 20; i++) {
                certs->hash[i * 3] = arkime_char_to_hexstr[digest[i]][0];
                certs->hash[i * 3 + 1] = arkime_char_to_hexstr[digest[i]][1];
                certs->hash[i * 3 + 2] = ':';
            }
        }
        certs->hash[59] = 0;
        g_checksum_reset(checksum);*/

        /* Certificate */
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
        {
            badreason = 1;
            goto bad_cert;
        }
        BSB_INIT(bsb, value, alen);

        /* signedCertificate */
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
        {
            badreason = 2;
            goto bad_cert;
        }
        BSB_INIT(bsb, value, alen);

        /* serialNumber or version*/
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
        {
            badreason = 3;
            goto bad_cert;
        }

        if (apc) {
            if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {
                badreason = 4;
                goto bad_cert;
            }
        }
        certs->serialNumberLen = alen;
        certs->serialNumber = malloc(alen);
        memcpy(certs->serialNumber, value, alen);

        /* signature */
        if (!arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen))
        {
            badreason = 5;
            goto bad_cert;
        }

        /* issuer */
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
        {
            badreason = 6;
            goto bad_cert;
        }
        BSB tbsb;
        BSB_INIT(tbsb, value, alen);
        dtls_certinfo_process(&certs->issuer, &tbsb);

        /* validity */
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
        {
            badreason = 7;
            goto bad_cert;
        }

        BSB_INIT(tbsb, value, alen);
        if (!(value = arkime_parsers_asn_get_tlv(&tbsb, &apc, &atag, &alen)))
        {
            badreason = 7;
            goto bad_cert;
        }
        certs->notBefore = arkime_parsers_asn_parse_time(session, atag, value, alen);

        if (!(value = arkime_parsers_asn_get_tlv(&tbsb, &apc, &atag, &alen)))
        {
            badreason = 7;
            goto bad_cert;
        }
        certs->notAfter = arkime_parsers_asn_parse_time(session, atag, value, alen);

        /* subject */
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
        {
            badreason = 8;
            goto bad_cert;
        }
        BSB_INIT(tbsb, value, alen);
        dtls_certinfo_process(&certs->subject, &tbsb);

        /* subjectPublicKeyInfo */
        if (!arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen))
        {
            badreason = 9;
            goto bad_cert;
        }

        /* extensions */
        if (BSB_REMAINING(bsb)) {
            if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)))
            {
                badreason = 10;
                goto bad_cert;
            }
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

            arkime_session_add_tag(session, "cert:self-signed");
        }


        if (!arkime_field_object_add(certsField, session, fobject, clen * 2)) {
            certinfo_free(fobject);
            fobject = 0;
        }

        BSB_IMPORT_skip(cbsb, clen + 3);

        continue;

bad_cert:
        if (config.debug)
            LOG("bad cert %d - %d", badreason, clen);
        certinfo_free(fobject);
        break;
    }
}
/******************************************************************************/
LOCAL int dtls_udp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    BSB bbuf;

    // 22 is handshake
    if (data[0] != 22) {
        arkime_parsers_unregister(session, uw);
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
            BSB_IMPORT_skip(msgBuf, 2); // msgSeq
            uint32_t frameOffset = 0;
            BSB_IMPORT_u24(msgBuf, frameOffset);
            BSB_IMPORT_skip(msgBuf, 3); // frameLength
            // ALW fix - don't handle fragmented packets yet
            if (frameOffset != 0) {
                BSB_IMPORT_skip(msgBuf, handshakeLen);
                continue;
            }

            // Not enough data left
            if (BSB_IS_ERROR(msgBuf) || handshakeLen > BSB_REMAINING(msgBuf))
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
LOCAL void dtls_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 100 || data[13] != 1)
        return;
    arkime_session_add_protocol(session, "dtls");
    arkime_parsers_register(session, dtls_udp_parser, uw, 0);
}
/******************************************************************************/
LOCAL void arkime_db_js0n_str(BSB *bsb, uint8_t *in, gboolean utf8)
{
    BSB_EXPORT_u08(*bsb, '"');
    while (*in) {
        switch(*in) {
        case '\b':
            BSB_EXPORT_cstr(*bsb, "\\b");
            break;
        case '\n':
            BSB_EXPORT_cstr(*bsb, "\\n");
            break;
        case '\r':
            BSB_EXPORT_cstr(*bsb, "\\r");
            break;
        case '\f':
            BSB_EXPORT_cstr(*bsb, "\\f");
            break;
        case '\t':
            BSB_EXPORT_cstr(*bsb, "\\t");
            break;
        case '"':
            BSB_EXPORT_cstr(*bsb, "\\\"");
            break;
        case '\\':
            BSB_EXPORT_cstr(*bsb, "\\\\");
            break;
        case '/':
            BSB_EXPORT_cstr(*bsb, "\\/");
            break;
        default:
            if(*in < 32) {
                BSB_EXPORT_sprintf(*bsb, "\\u%04x", *in);
            } else if (utf8) {
                if ((*in & 0xf0) == 0xf0) {
                    if (!in[1] || !in[2] || !in[3]) goto end;
                    BSB_EXPORT_ptr(*bsb, in, 4);
                    in += 3;
                } else if ((*in & 0xf0) == 0xe0) {
                    if (!in[1] || !in[2]) goto end;
                    BSB_EXPORT_ptr(*bsb, in, 3);
                    in += 2;
                } else if ((*in & 0xf0) == 0xd0) {
                    if (!in[1]) goto end;
                    BSB_EXPORT_ptr(*bsb, in, 2);
                    in += 1;
                } else {
                    BSB_EXPORT_u08(*bsb, *in);
                }
            } else {
                if(*in & 0x80) {
                    BSB_EXPORT_u08(*bsb, (0xc0 | (*in >> 6)));
                    BSB_EXPORT_u08(*bsb, (0x80 | (*in & 0x3f)));
                } else {
                    BSB_EXPORT_u08(*bsb, *in);
                }
            }
            break;
        }
        in++;
    }

end:
    BSB_EXPORT_u08(*bsb, '"');
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

void certinfo_save(BSB *jbsb, ArkimeFieldObject_t *object, ArkimeSession_t *session) {
    if (object->object == NULL) {
        return;
    }

    CertInfo_t *ci = (CertInfo_t *)object->object;

    ArkimeString_t *string;

    BSB_EXPORT_u08(*jbsb, '{');

    BSB_EXPORT_sprintf(*jbsb, "\"hash\":\"%s\",", ci->hash);

    if (ci->publicAlgorithm)
        BSB_EXPORT_sprintf(*jbsb, "\"publicAlgorithm\":\"%s\",", ci->publicAlgorithm);
    if (ci->curve)
        BSB_EXPORT_sprintf(*jbsb, "\"curve\":\"%s\",", ci->curve);

    SAVE_STRING_HEAD(ci->issuer.commonName, "issuerCN");
    SAVE_STRING_HEAD(ci->issuer.orgName, "issuerON");
    SAVE_STRING_HEAD(ci->issuer.orgUnit, "issuerOU");
    SAVE_STRING_HEAD(ci->subject.commonName, "subjectCN");
    SAVE_STRING_HEAD(ci->subject.orgName, "subjectON");
    SAVE_STRING_HEAD(ci->subject.orgUnit, "subjectOU");

    if (ci->serialNumber) {
        int k;
        BSB_EXPORT_cstr(*jbsb, "\"serial\":\"");
        for (k = 0; k < ci->serialNumberLen; k++) {
            BSB_EXPORT_sprintf(*jbsb, "%02x", ci->serialNumber[k]);
        }
        BSB_EXPORT_u08(*jbsb, '"');
        BSB_EXPORT_u08(*jbsb, ',');
    }

    if (ci->alt.s_count > 0) { \
        BSB_EXPORT_sprintf(*jbsb, "\"altCnt\":%d,", ci->alt.s_count); \
    }
    SAVE_STRING_HEAD(ci->alt, "alt");

    BSB_EXPORT_sprintf(*jbsb, "\"notBefore\":%" PRId64 ",", ci->notBefore * 1000);
    BSB_EXPORT_sprintf(*jbsb, "\"notAfter\":%" PRId64 ",", ci->notAfter * 1000);
    if (ci->notAfter < ((uint64_t)session->firstPacket.tv_sec)) {
        BSB_EXPORT_sprintf(*jbsb, "\"remainingDays\":%" PRId64 ",", ((int64_t)0));
        BSB_EXPORT_sprintf(*jbsb, "\"remainingSeconds\":%" PRId64 ",", ((int64_t)0));
    } else {
        BSB_EXPORT_sprintf(*jbsb, "\"remainingDays\":%" PRId64 ",", ((int64_t)ci->notAfter - ((uint64_t)session->firstPacket.tv_sec)) / (60 * 60 * 24));
        BSB_EXPORT_sprintf(*jbsb, "\"remainingSeconds\":%" PRId64 ",", ((int64_t)ci->notAfter - ((uint64_t)session->firstPacket.tv_sec)));
    }
    BSB_EXPORT_sprintf(*jbsb, "\"validDays\":%" PRId64 ",", ((int64_t)ci->notAfter - (int64_t)ci->notBefore) / (60 * 60 * 24));
    BSB_EXPORT_sprintf(*jbsb, "\"validSeconds\":%" PRId64 ",", ((int64_t)ci->notAfter - (int64_t)ci->notBefore));

    BSB_EXPORT_rewind(*jbsb, 1); // Remove last comma

    BSB_EXPORT_u08(*jbsb, '}');
}

void certinfo_free(ArkimeFieldObject_t *object) {
    if (object->object == NULL) {
        ARKIME_TYPE_FREE(ArkimeFieldObject_t, object);
        return;
    }

    CertInfo_t *ci = (CertInfo_t *)object->object;

    ArkimeString_t *string;

    while (DLL_POP_HEAD(s_, &ci->alt, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    while (DLL_POP_HEAD(s_, &ci->issuer.commonName, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    while (DLL_POP_HEAD(s_, &ci->issuer.orgName, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    while (DLL_POP_HEAD(s_, &ci->issuer.orgUnit, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    while (DLL_POP_HEAD(s_, &ci->subject.commonName, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    while (DLL_POP_HEAD(s_, &ci->subject.orgName, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    while (DLL_POP_HEAD(s_, &ci->subject.orgUnit, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    if (ci->serialNumber)
        free(ci->serialNumber);

    ARKIME_TYPE_FREE(CertInfo_t, ci);
    ARKIME_TYPE_FREE(ArkimeFieldObject_t, object);
}

SUPPRESS_UNSIGNED_INTEGER_OVERFLOW
SUPPRESS_SHIFT
SUPPRESS_INT_CONVERSION
uint32_t certinfo_hash(const void *key) {
    ArkimeFieldObject_t *obj = (ArkimeFieldObject_t *)key;
    
    CertInfo_t *ci;

    if (obj->object == NULL) {
        return 0;
    }
    
    ci = (CertInfo_t *)obj->object;

    if (ci->serialNumberLen == 0) {
        return ((ci->issuer.commonName.s_count << 18) |
                (ci->issuer.orgName.s_count << 12) |
                (ci->subject.commonName.s_count << 6) |
                (ci->subject.orgName.s_count));
    }
    return ((ci->serialNumber[0] << 28) |
            (ci->serialNumber[ci->serialNumberLen - 1] << 24) |
            (ci->issuer.commonName.s_count << 18) |
            (ci->issuer.orgName.s_count << 12) |
            (ci->subject.commonName.s_count << 6) |
            (ci->subject.orgName.s_count));
}

int certinfo_cmp(const void *keyv, const void *elementv) {
    ArkimeFieldObject_t *key      = (ArkimeFieldObject_t *)keyv;
    ArkimeFieldObject_t *element = (ArkimeFieldObject_t *)elementv;

    CertInfo_t *keyCI, *elementCI;

    if (key->object == NULL || element->object == NULL) {
        return 0;
    }

    keyCI = (CertInfo_t *)key->object;
    elementCI = (CertInfo_t *)element->object;

    // Make sure all the easy things to check are the same
    if ( !((keyCI->serialNumberLen == elementCI->serialNumberLen) &&
           (memcmp(keyCI->serialNumber, elementCI->serialNumber, elementCI->serialNumberLen) == 0) &&
           (keyCI->issuer.commonName.s_count == elementCI->issuer.commonName.s_count) &&
           (keyCI->issuer.orgName.s_count == elementCI->issuer.orgName.s_count) &&
           (keyCI->issuer.orgUnit.s_count == elementCI->issuer.orgUnit.s_count) &&
           (keyCI->subject.commonName.s_count == elementCI->subject.commonName.s_count) &&
           (keyCI->subject.orgName.s_count == elementCI->subject.orgName.s_count) &&
           (keyCI->subject.orgUnit.s_count == elementCI->subject.orgUnit.s_count)
          )
       ) {

        return 0;
    }

    // Now see if all the other items are the same

    ArkimeString_t *kstr, *estr;
    for (kstr = keyCI->issuer.commonName.s_next, estr = elementCI->issuer.commonName.s_next;
         kstr != (void *) & (keyCI->issuer.commonName);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    for (kstr = keyCI->issuer.orgName.s_next, estr = elementCI->issuer.orgName.s_next;
         kstr != (void *) & (keyCI->issuer.orgName);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    for (kstr = keyCI->issuer.orgUnit.s_next, estr = elementCI->issuer.orgUnit.s_next;
         kstr != (void *) & (keyCI->issuer.orgUnit);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    for (kstr = keyCI->subject.commonName.s_next, estr = elementCI->subject.commonName.s_next;
         kstr != (void *) & (keyCI->subject.commonName);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    for (kstr = keyCI->subject.orgName.s_next, estr = elementCI->subject.orgName.s_next;
         kstr != (void *) & (keyCI->subject.orgName);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    for (kstr = keyCI->subject.orgUnit.s_next, estr = elementCI->subject.orgUnit.s_next;
         kstr != (void *) & (keyCI->subject.orgUnit);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    return 1;
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_parsers_classifier_register_udp("dtls", NULL, 0, (const uint8_t *)"\x16\x01\x00", 3, dtls_udp_classify);
    arkime_parsers_classifier_register_udp("dtls", NULL, 0, (const uint8_t *)"\x16\xfe\xff", 3, dtls_udp_classify);
    arkime_parsers_classifier_register_udp("dtls", NULL, 0, (const uint8_t *)"\x16\xfe\xfe", 3, dtls_udp_classify);
    arkime_parsers_classifier_register_udp("dtls", NULL, 0, (const uint8_t *)"\x16\xfe\xfd", 3, dtls_udp_classify);

    certsField = arkime_field_object_register("cert", "Certificates info", certinfo_save, certinfo_free, certinfo_hash, certinfo_cmp);
}
