/* Copyright 2023 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <inttypes.h>
#include "openssl/objects.h"

extern ArkimeConfig_t        config;

LOCAL int certsField;
LOCAL int certAltField;

extern uint8_t    arkime_char_to_hexstr[256][3];

LOCAL GChecksum *checksums[ARKIME_MAX_PACKET_THREADS];
LOCAL uint32_t tls_process_certificate_wInfo_func;

/******************************************************************************/
/*
 * Certs Info
 */

typedef struct {
    ArkimeStringHead_t  commonName; // 2.5.4.3
    ArkimeStringHead_t  orgName;    // 2.5.4.10
    ArkimeStringHead_t  orgUnit;    // 2.5.4.11
    char                orgUtf8;
} ArkimeCertInfo_t;

typedef struct arkime_certsinfo {
    uint64_t                 notBefore;
    uint64_t                 notAfter;
    ArkimeCertInfo_t         issuer;
    ArkimeCertInfo_t         subject;
    ArkimeStringHead_t       alt;
    uint8_t                 *serialNumber;
    short                    serialNumberLen;
    uint8_t                  hash[60];
    char                     isCA;
    const char              *publicAlgorithm;
    const char              *curve;
    GHashTable              *extra;
} ArkimeCertsInfo_t;

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
/******************************************************************************/
LOCAL void certinfo_save(BSB *jbsb, ArkimeFieldObject_t *object, ArkimeSession_t *session)
{
    if (object->object == NULL) {
        return;
    }

    ArkimeCertsInfo_t *ci = (ArkimeCertsInfo_t *)object->object;

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

    if (ci->alt.s_count > 0) {
        BSB_EXPORT_sprintf(*jbsb, "\"altCnt\":%d,", ci->alt.s_count);
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

    if (ci->extra) {
        GHashTableIter         iter;
        gpointer               ikey;
        gpointer               ival;
        g_hash_table_iter_init (&iter, ci->extra);
        while (g_hash_table_iter_next (&iter, &ikey, &ival)) {
            BSB_EXPORT_sprintf(*jbsb, "\"%s\":\"%s\",", (char *)ikey, (char *)ival);
        }
    }

    BSB_EXPORT_rewind(*jbsb, 1); // Remove last comma

    BSB_EXPORT_u08(*jbsb, '}');
}
/******************************************************************************/
LOCAL void certinfo_free(ArkimeFieldObject_t *object)
{
    if (object->object == NULL) {
        ARKIME_TYPE_FREE(ArkimeFieldObject_t, object);
        return;
    }

    ArkimeCertsInfo_t *ci = (ArkimeCertsInfo_t *)object->object;

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

    if (ci->extra)
        g_hash_table_destroy(ci->extra);

    ARKIME_TYPE_FREE(ArkimeCertsInfo_t, ci);
    ARKIME_TYPE_FREE(ArkimeFieldObject_t, object);
}

/******************************************************************************/
SUPPRESS_UNSIGNED_INTEGER_OVERFLOW
SUPPRESS_SHIFT
SUPPRESS_INT_CONVERSION
LOCAL uint32_t certinfo_hash(const void *key)
{
    ArkimeCertsInfo_t *ci = (ArkimeCertsInfo_t *)key;

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

/******************************************************************************/
LOCAL int certinfo_cmp(const void *keyv, const void *elementv)
{
    ArkimeFieldObject_t *element = (ArkimeFieldObject_t *)elementv;
    if (element->object == NULL) {
        return 0;
    }

    ArkimeCertsInfo_t  *keyCI     = (ArkimeCertsInfo_t *)keyv;
    ArkimeCertsInfo_t  *elementCI = (ArkimeCertsInfo_t *)element->object;

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
LOCAL void certinfo_key_usage (ArkimeCertsInfo_t *certs, BSB *bsb)
{
    uint32_t apc, atag, alen;

    while (BSB_REMAINING(*bsb) >= 2) {
        const uint8_t *value = arkime_parsers_asn_get_tlv(bsb, &apc, &atag, &alen);

        if (value && atag == 4 && alen == 4)
            certs->isCA = (value[3] & 0x02);
    }
}
/******************************************************************************/
LOCAL void certinfo_alt_names(ArkimeSession_t *session, ArkimeCertsInfo_t *certs, BSB *bsb, char *lastOid)
{
    uint32_t apc, atag, alen;

    while (BSB_REMAINING(*bsb) >= 2) {
        uint8_t *value = arkime_parsers_asn_get_tlv(bsb, &apc, &atag, &alen);

        if (!value)
            return;

        if (apc) {
            BSB tbsb;
            BSB_INIT(tbsb, value, alen);
            certinfo_alt_names(session, certs, &tbsb, lastOid);
            if (certs->alt.s_count > 0) {
                return;
            }
        } else if (atag == 6) {
            arkime_parsers_asn_decode_oid(lastOid, 100, value, alen);
            if (strcmp(lastOid, "2.5.29.15") == 0) {
                certinfo_key_usage(certs, bsb);
            }
            if (strcmp(lastOid, "2.5.29.17") != 0)
                lastOid[0] = 0;
        } else if (lastOid[0] && atag == 4) {
            BSB tbsb;
            BSB_INIT(tbsb, value, alen);
            certinfo_alt_names(session, certs, &tbsb, lastOid);
            return;
        } else if (lastOid[0] && atag == 2) {
            if (g_utf8_validate((char *)value, alen, NULL)) {
                ArkimeString_t *element = ARKIME_TYPE_ALLOC0(ArkimeString_t);
                element->str = g_ascii_strdown((char *)value, alen);
                element->len = alen;
                element->utf8 = 1;
                DLL_PUSH_TAIL(s_, &certs->alt, element);
                ARKIME_RULES_RUN_FIELD_SET(session, certAltField, element->str);
            } else {
                arkime_session_add_tag(session, "bad-altname");
            }
        }
    }
    lastOid[0] = 0;
    return;
}
/******************************************************************************/
LOCAL void certinfo_process(ArkimeCertInfo_t *ci, BSB *bsb)
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
            certinfo_process(ci, &tbsb);
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
LOCAL void certinfo_process_publickey(ArkimeCertsInfo_t *certs, uint8_t *data, uint32_t len)
{
    BSB bsb, tbsb;
    BSB_INIT(bsb, data, len);
    char oid[1000];

    uint32_t apc, atag, alen;
    uint8_t *value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

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
LOCAL uint32_t certinfo_process_server_certificate(ArkimeSession_t *session, const uint8_t *data, int len, void UNUSED(*uw))
{

    BSB cbsb;

    BSB_INIT(cbsb, data, len);

    BSB_IMPORT_skip(cbsb, 3); // Length again

    GChecksum *const checksum = checksums[session->thread];

    while (BSB_REMAINING(cbsb) > 3) {
        int            badreason = 0;
        uint8_t *cdata = BSB_WORK_PTR(cbsb);
        int            clen = MIN(BSB_REMAINING(cbsb) - 3, (cdata[0] << 16 | cdata[1] << 8 | cdata[2]));

        ArkimeFieldObject_t *fobject = ARKIME_TYPE_ALLOC0(ArkimeFieldObject_t);

        ArkimeCertsInfo_t *certs = ARKIME_TYPE_ALLOC0(ArkimeCertsInfo_t);
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

        guchar digest[20];
        gsize  dlen = sizeof(digest);

        g_checksum_update(checksum, cdata + 3, clen);
        g_checksum_get_digest(checksum, digest, &dlen);
        g_checksum_reset(checksum);
        if (dlen > 0) {
            int i;
            for (i = 0; i < 20; i++) {
                certs->hash[i * 3] = arkime_char_to_hexstr[digest[i]][0];
                certs->hash[i * 3 + 1] = arkime_char_to_hexstr[digest[i]][1];
                certs->hash[i * 3 + 2] = ':';
            }
        }
        certs->hash[59] = 0;

        /* Certificate */
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen))) {
            badreason = 1;
            goto bad_cert;
        }
        BSB_INIT(bsb, value, alen);

        /* signedCertificate */
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen))) {
            badreason = 2;
            goto bad_cert;
        }
        BSB_INIT(bsb, value, alen);

        /* serialNumber or version*/
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen))) {
            badreason = 3;
            goto bad_cert;
        }

        if (apc) {
            if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen))) {
                badreason = 4;
                goto bad_cert;
            }
        }
        certs->serialNumberLen = alen;
        certs->serialNumber = malloc(alen);
        memcpy(certs->serialNumber, value, alen);

        /* signature */
        if (!arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen)) {
            badreason = 5;
            goto bad_cert;
        }

        /* issuer */
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen))) {
            badreason = 6;
            goto bad_cert;
        }
        BSB tbsb;
        BSB_INIT(tbsb, value, alen);
        certinfo_process(&certs->issuer, &tbsb);

        /* validity */
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen))) {
            badreason = 7;
            goto bad_cert;
        }

        BSB_INIT(tbsb, value, alen);
        if (!(value = arkime_parsers_asn_get_tlv(&tbsb, &apc, &atag, &alen))) {
            badreason = 7;
            goto bad_cert;
        }
        certs->notBefore = arkime_parsers_asn_parse_time(session, atag, value, alen);

        if (!(value = arkime_parsers_asn_get_tlv(&tbsb, &apc, &atag, &alen))) {
            badreason = 7;
            goto bad_cert;
        }
        certs->notAfter = arkime_parsers_asn_parse_time(session, atag, value, alen);

        /* subject */
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen))) {
            badreason = 8;
            goto bad_cert;
        }
        BSB_INIT(tbsb, value, alen);
        certinfo_process(&certs->subject, &tbsb);

        /* subjectPublicKeyInfo */
        if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen))) {
            badreason = 9;
            goto bad_cert;
        }
        certinfo_process_publickey(certs, value, alen);

        /* extensions */
        if (BSB_REMAINING(bsb)) {
            if (!(value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen))) {
                badreason = 10;
                goto bad_cert;
            }
            BSB_INIT(tbsb, value, alen);
            char lastOid[100];
            lastOid[0] = 0;
            certinfo_alt_names(session, certs, &tbsb, lastOid);
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


        if (!arkime_field_object_add(certsField, session, fobject, clen * 2)) {
            certinfo_free(fobject);
            fobject = 0;
            certs = 0;
        }

        BSB_IMPORT_skip(cbsb, clen + 3);

        if (certs)
            arkime_parsers_call_named_func(tls_process_certificate_wInfo_func, session, cdata + 3, clen, certs);

        continue;

bad_cert:
        if (config.debug)
            LOG("bad cert %d - %d", badreason, clen);
        certinfo_free(fobject);
        break;
    }
    return 0;
}
/******************************************************************************/
void arkime_field_certsinfo_update_extra (void *cert, char *key, char *value)
{
    ArkimeCertsInfo_t *certs = cert;

    if (!certs->extra) {
        certs->extra = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    }

    g_hash_table_replace(certs->extra, key, value);
}
/******************************************************************************/
LOCAL void certs_get_free(void *ptr)
{
    g_ptr_array_free((GPtrArray *)ptr, TRUE);
}
/******************************************************************************/
LOCAL void *certs_getcb_alt(ArkimeSession_t *session, int UNUSED(pos))
{
    if (!session->fields[certsField])
        return NULL;

    ArkimeString_t *string;
    const ArkimeFieldObjectHashStd_t *ohash = session->fields[certsField]->ohash;
    ArkimeFieldObject_t *object;

    GPtrArray *array = g_ptr_array_new();
    HASH_FORALL2(o_, *ohash, object) {
        DLL_FOREACH(s_, &((ArkimeCertsInfo_t *)object->object)->alt, string) {
            g_ptr_array_add(array, string->str);
        }
    }

    arkime_free_later(array, (GDestroyNotify) certs_get_free);
    return array;
}
/******************************************************************************/
void arkime_parser_init()
{
    certsField = arkime_field_object_register("cert", "Certificates info", certinfo_save, certinfo_free, certinfo_hash, certinfo_cmp);

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

    certAltField = arkime_field_by_exp_add_internal("cert.alt", ARKIME_FIELD_TYPE_STR_ARRAY, certs_getcb_alt, NULL);

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

    int t;
    for (t = 0; t < config.packetThreads; t++) {
        checksums[t] = g_checksum_new(G_CHECKSUM_SHA1);
    }

    arkime_parsers_add_named_func("tls_process_server_certificate", certinfo_process_server_certificate);
    tls_process_certificate_wInfo_func = arkime_parsers_get_named_func("tls_process_certificate_wInfo");
}
