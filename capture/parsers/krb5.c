/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

//#define KRB5_DEBUG 1

extern ArkimeConfig_t        config;

LOCAL  int realmField;
LOCAL  int cnameField;
LOCAL  int snameField;
LOCAL  int etypeField;
LOCAL  int errorCodeField;

// Encryption type names - RFC 3961/3962/8009
LOCAL const char *krb5Etypes[] = {
    [1]  = "des-cbc-crc",
    [2]  = "des-cbc-md4",
    [3]  = "des-cbc-md5",
    [5]  = "des3-cbc-md5",
    [7]  = "des3-cbc-sha1",
    [16] = "des3-cbc-sha1-kd",
    [17] = "aes128-cts-hmac-sha1-96",
    [18] = "aes256-cts-hmac-sha1-96",
    [19] = "aes128-cts-hmac-sha256-128",
    [20] = "aes256-cts-hmac-sha384-192",
    [23] = "rc4-hmac",
    [24] = "rc4-hmac-exp",
    [25] = "camellia128-cts-cmac",
    [26] = "camellia256-cts-cmac",
};

// Error code names - RFC 4120
LOCAL const char *krb5Errors[] = {
    [0]  = "KDC_ERR_NONE",
    [1]  = "KDC_ERR_NAME_EXP",
    [2]  = "KDC_ERR_SERVICE_EXP",
    [3]  = "KDC_ERR_BAD_PVNO",
    [4]  = "KDC_ERR_C_OLD_MAST_KVNO",
    [5]  = "KDC_ERR_S_OLD_MAST_KVNO",
    [6]  = "KDC_ERR_C_PRINCIPAL_UNKNOWN",
    [7]  = "KDC_ERR_S_PRINCIPAL_UNKNOWN",
    [8]  = "KDC_ERR_PRINCIPAL_NOT_UNIQUE",
    [9]  = "KDC_ERR_NULL_KEY",
    [10] = "KDC_ERR_CANNOT_POSTDATE",
    [11] = "KDC_ERR_NEVER_VALID",
    [12] = "KDC_ERR_POLICY",
    [13] = "KDC_ERR_BADOPTION",
    [14] = "KDC_ERR_ETYPE_NOSUPP",
    [15] = "KDC_ERR_SUMTYPE_NOSUPP",
    [16] = "KDC_ERR_PADATA_TYPE_NOSUPP",
    [17] = "KDC_ERR_TRTYPE_NOSUPP",
    [18] = "KDC_ERR_CLIENT_REVOKED",
    [19] = "KDC_ERR_SERVICE_REVOKED",
    [20] = "KDC_ERR_TGT_REVOKED",
    [21] = "KDC_ERR_CLIENT_NOTYET",
    [22] = "KDC_ERR_SERVICE_NOTYET",
    [23] = "KDC_ERR_KEY_EXPIRED",
    [24] = "KDC_ERR_PREAUTH_FAILED",
    [25] = "KDC_ERR_PREAUTH_REQUIRED",
    [26] = "KDC_ERR_SERVER_NOMATCH",
    [27] = "KDC_ERR_MUST_USE_USER2USER",
    [28] = "KDC_ERR_PATH_NOT_ACCEPTED",
    [29] = "KDC_ERR_SVC_UNAVAILABLE",
    [31] = "KRB_AP_ERR_BAD_INTEGRITY",
    [32] = "KRB_AP_ERR_TKT_EXPIRED",
    [33] = "KRB_AP_ERR_TKT_NYV",
    [34] = "KRB_AP_ERR_REPEAT",
    [35] = "KRB_AP_ERR_NOT_US",
    [36] = "KRB_AP_ERR_BADMATCH",
    [37] = "KRB_AP_ERR_SKEW",
    [38] = "KRB_AP_ERR_BADADDR",
    [39] = "KRB_AP_ERR_BADVERSION",
    [40] = "KRB_AP_ERR_MSG_TYPE",
    [41] = "KRB_AP_ERR_MODIFIED",
    [42] = "KRB_AP_ERR_BADORDER",
    [44] = "KRB_AP_ERR_BADKEYVER",
    [45] = "KRB_AP_ERR_NOKEY",
    [46] = "KRB_AP_ERR_MUT_FAIL",
    [47] = "KRB_AP_ERR_BADDIRECTION",
    [48] = "KRB_AP_ERR_METHOD",
    [49] = "KRB_AP_ERR_BADSEQ",
    [50] = "KRB_AP_ERR_INAPP_CKSUM",
    [51] = "KRB_AP_PATH_NOT_ACCEPTED",
    [52] = "KRB_ERR_RESPONSE_TOO_BIG",
    [60] = "KRB_ERR_GENERIC",
    [61] = "KRB_ERR_FIELD_TOOLONG",
    [62] = "KDC_ERROR_CLIENT_NOT_TRUSTED",
    [63] = "KDC_ERROR_KDC_NOT_TRUSTED",
    [64] = "KDC_ERROR_INVALID_SIG",
    [65] = "KDC_ERR_KEY_TOO_WEAK",
    [66] = "KDC_ERR_CERTIFICATE_MISMATCH",
    [67] = "KRB_AP_ERR_NO_TGT",
    [68] = "KDC_ERR_WRONG_REALM",
    [69] = "KRB_AP_ERR_USER_TO_USER_REQUIRED",
    [70] = "KDC_ERR_CANT_VERIFY_CERTIFICATE",
    [71] = "KDC_ERR_INVALID_CERTIFICATE",
    [72] = "KDC_ERR_REVOKED_CERTIFICATE",
    [73] = "KDC_ERR_REVOCATION_STATUS_UNKNOWN",
    [74] = "KDC_ERR_REVOCATION_STATUS_UNAVAILABLE",
    [75] = "KDC_ERR_CLIENT_NAME_MISMATCH",
    [76] = "KDC_ERR_KDC_NAME_MISMATCH",
};

/******************************************************************************/
/* wireshark: k5.asn which based on http://www.h5l.org/dist/src/heimdal-1.2.tar.gz
--PrincipalName ::= SEQUENCE {
--      name-type[0]            NAME-TYPE,
--      name-string[1]          SEQUENCE OF GeneralString
--}
 */
LOCAL void krb5_parse_principal_name(ArkimeSession_t *session, int field, const uint8_t *data, int len)
{
    ArkimeASNSeq_t seq[10];

    int num = arkime_parsers_asn_get_sequence(seq, 2, data, len, TRUE);

    if (num < 2 || seq[1].tag != 1)
        return;

    num = arkime_parsers_asn_get_sequence(seq, 2, seq[1].value, seq[1].len, TRUE);

    int len0, len1;
    const char *value0;
    if (num == 1) {
        value0 = arkime_parsers_asn_sequence_to_string(&seq[0], &len0);
        if (value0 && len0 > 0)
            arkime_field_string_add(field, session, value0, len0, TRUE);
    } else if (num == 2) {
        char str[255];
        value0 = arkime_parsers_asn_sequence_to_string(&seq[0], &len0);
        const char *value1 = arkime_parsers_asn_sequence_to_string(&seq[1], &len1);
        int slen = snprintf(str, sizeof(str), "%.*s/%.*s", len0, value0, len1, value1);
        if (slen >= (int)sizeof(str))
            slen = sizeof(str) - 1;
        arkime_field_string_add(field, session, str, slen, TRUE);
    }
}
/******************************************************************************/
/* wireshark: k5.asn which based on http://www.h5l.org/dist/src/heimdal-1.2.tar.gz
--KDC-REQ-BODY ::= SEQUENCE {
--      kdc-options[0]          KDCOptions,
--      cname[1]                PrincipalName OPTIONAL, - - Used only in AS-REQ
--      realm[2]                Realm,  - - Server's realm
                                        -- Also client's in AS-REQ
--      sname[3]                PrincipalName OPTIONAL,
--      from[4]                 KerberosTime OPTIONAL,
--      till[5]                 KerberosTime OPTIONAL,
--      rtime[6]                KerberosTime OPTIONAL,
--      nonce[7]                Krb5int32,
--      etype[8]                SEQUENCE OF ENCTYPE, - - EncryptionType,
                                        -- in preference order
--      addresses[9]            HostAddresses OPTIONAL,
--      enc-authorization-data[10] EncryptedData OPTIONAL,
                                        -- Encrypted AuthorizationData encoding
--      additional-tickets[11]  SEQUENCE OF Ticket OPTIONAL
--}
*/
LOCAL void krb5_parse_req_body(ArkimeSession_t *session, const uint8_t *data, int len)
{
    ArkimeASNSeq_t seq[12];

    int num = arkime_parsers_asn_get_sequence(seq, 12, data, len, TRUE);
    if (num < 2)
        return;
    int vlen;
    const char *value;
    for (int i = 0; i < num; i++) {
        switch (seq[i].tag) {
        case 1:
            krb5_parse_principal_name(session, cnameField, seq[i].value, seq[i].len);
            break;
        case 2:
            value = arkime_parsers_asn_sequence_to_string(&seq[i], &vlen);
            if (value && vlen > 0)
                arkime_field_string_add(realmField, session, value, vlen, TRUE);
            break;
        case 3:
            krb5_parse_principal_name(session, snameField, seq[i].value, seq[i].len);
            break;
        case 8: {
            // etype - SEQUENCE OF ENCTYPE
            ArkimeASNSeq_t etypes[20];
            int numEtypes = arkime_parsers_asn_get_sequence(etypes, 20, seq[i].value, seq[i].len, TRUE);
            for (int j = 0; j < numEtypes; j++) {
                int etype = arkime_parsers_asn_sequence_to_int(&etypes[j]);
                if (etype >= 0 && etype < (int)ARRAY_LEN(krb5Etypes) && krb5Etypes[etype]) {
                    arkime_field_string_add(etypeField, session, krb5Etypes[etype], -1, TRUE);
                }
            }
            break;
        }
        }
    }
}

/******************************************************************************/
/* wireshark: k5.asn which based on http://www.h5l.org/dist/src/heimdal-1.2.tar.gz
--KDC-REQ ::= SEQUENCE {
--      pvno[1]                 Krb5int32,
--      msg-type[2]             MESSAGE-TYPE,
--      padata[3]               METHOD-DATA OPTIONAL,
--      req-body[4]             KDC-REQ-BODY
--}
*/
LOCAL void krb5_parse_req(ArkimeSession_t *session, const uint8_t *data, int len)
{
    ArkimeASNSeq_t seq[5];

    int num = arkime_parsers_asn_get_sequence(seq, 5, data, len, TRUE);
    if (num < 3 || seq[0].len == 0 || seq[1].len == 0)
        return;

    if (!seq[0].pc || seq[0].tag != 1 || seq[0].value[seq[0].len - 1] != 5) {
        return;
    }

    int msgType = seq[1].value[seq[1].len - 1];
    if (!seq[1].pc || seq[1].tag != 2 || (msgType != 10 && msgType != 12)) {
        return;
    }

    arkime_session_add_protocol(session, "krb5");

    if (seq[2].pc && seq[2].tag == 4) {
        krb5_parse_req_body(session, seq[2].value, seq[2].len);
    } else if (num == 4 && seq[3].pc && seq[3].tag == 4) {
        krb5_parse_req_body(session, seq[3].value, seq[3].len);
    }
}
/******************************************************************************/
/* wireshark: k5.asn which based on http://www.h5l.org/dist/src/heimdal-1.2.tar.gz
--KDC-REP ::= SEQUENCE {
--      pvno[0]                 Krb5int32,
--      msg-type[1]             MESSAGE-TYPE,
--      padata[2]               METHOD-DATA OPTIONAL,
--      crealm[3]               Realm,
--      cname[4]                PrincipalName,
--      ticket[5]               Ticket,
--      enc-part[6]             EncryptedData
--}
*/
LOCAL void krb5_parse_rep(ArkimeSession_t *session, const uint8_t *UNUSED(data), int UNUSED(len))
{
    arkime_session_add_protocol(session, "krb5");
}
/******************************************************************************/
/* wireshark: k5.asn which based on http://www.h5l.org/dist/src/heimdal-1.2.tar.gz
--KRB-ERROR ::= [APPLICATION 30] SEQUENCE {
--      pvno[0]                 Krb5int32,
--      msg-type[1]             MESSAGE-TYPE,
--      ctime[2]                KerberosTime OPTIONAL,
--      cusec[3]                Krb5int32 OPTIONAL,
--      stime[4]                KerberosTime,
--      susec[5]                Krb5int32,
--      error-code[6]           Krb5int32,
--      crealm[7]               Realm OPTIONAL,
--      cname[8]                PrincipalName OPTIONAL,
--      realm[9]                Realm, - - Correct realm
--      sname[10]               PrincipalName, - - Correct name
--      e-text[11]              GeneralString OPTIONAL,
--      e-data[12]              OCTET STRING OPTIONAL
--}
*/
LOCAL void krb5_parse_error(ArkimeSession_t *session, const uint8_t *data, int len)
{
    ArkimeASNSeq_t seq[13];

    int num = arkime_parsers_asn_get_sequence(seq, 13, data, len, TRUE);
#ifdef KRB5_DEBUG
    LOG("DEBUG_ERROR - num:%d", num);
#endif
    if (num < 2)
        return;

    arkime_session_add_protocol(session, "krb5");

    int vlen;
    const char *value;
    for (int i = 0; i < num; i++) {
#ifdef KRB5_DEBUG
        LOG("DEBUG_ERROR - i:%d tag:%d len:%d", i, seq[i].tag, seq[i].len);
#endif
        switch (seq[i].tag) {
        case 6: {
            // error-code
            int errorCode = arkime_parsers_asn_sequence_to_int(&seq[i]);
#ifdef KRB5_DEBUG
            LOG("DEBUG_ERROR - errorCode:%d", errorCode);
#endif
            if (errorCode >= 0 && errorCode < (int)ARRAY_LEN(krb5Errors) && krb5Errors[errorCode]) {
                arkime_field_string_add(errorCodeField, session, krb5Errors[errorCode], -1, TRUE);
            }
            break;
        }
        case 7:
            // crealm
            value = arkime_parsers_asn_sequence_to_string(&seq[i], &vlen);
            if (value && vlen > 0)
                arkime_field_string_add(realmField, session, value, vlen, TRUE);
            break;
        case 8:
            // cname
            krb5_parse_principal_name(session, cnameField, seq[i].value, seq[i].len);
            break;
        case 9:
            // realm (server's realm)
            value = arkime_parsers_asn_sequence_to_string(&seq[i], &vlen);
            if (value && vlen > 0)
                arkime_field_string_add(realmField, session, value, vlen, TRUE);
            break;
        case 10:
            // sname
            krb5_parse_principal_name(session, snameField, seq[i].value, seq[i].len);
            break;
        }
    }
}
/******************************************************************************/
LOCAL void krb5_parse(ArkimeSession_t *session, const uint8_t *data, int len)
{
    BSB obsb;
    uint32_t opc, msgType, olen;
    const uint8_t *ovalue;

    BSB_INIT(obsb, data, len);
    ovalue = arkime_parsers_asn_get_tlv(&obsb, &opc, &msgType, &olen);
#ifdef KRB5_DEBUG
    LOG("DEBUG1 - opc:%u msgType:%u olen:%u", opc, msgType, olen);
#endif
    if (!opc)
        return;

    switch (msgType) {
    case 10:
    case 12:
        krb5_parse_req(session, ovalue, olen);
        break;
    case 11:
    case 13:
        krb5_parse_rep(session, ovalue, olen);
        break;
    case 30:
        krb5_parse_error(session, ovalue, olen);
        break;
    }
}
/******************************************************************************/
LOCAL int krb5_udp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    krb5_parse(session, data, len);
    return 0;
}
/******************************************************************************/
LOCAL void krb5_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "krb5"))
        return;

    BSB obsb;
    uint32_t opc, otag, olen;

    BSB_INIT(obsb, data, len);
    arkime_parsers_asn_get_tlv(&obsb, &opc, &otag, &olen);
#ifdef KRB5_DEBUG
    LOG("enter %u %u %u", opc, otag, olen);
#endif
    if (opc && (otag == 10 || otag == 12 || otag == 30) && len >= (int)olen) {
        arkime_parsers_register(session, krb5_udp_parser, 0, 0);
    }
}
/******************************************************************************/
LOCAL int krb5_tcp_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int remaining, int which)
{
    ArkimeParserBuf_t *krb5 = uw;

    arkime_parser_buf_add(krb5, which, data, remaining);

    if (krb5->len[which] < 4)
        return 0;

    int len = (krb5->buf[which][2] << 8) | krb5->buf[which][3];
    if (krb5->len[which] < len + 4)
        return 0;
    krb5_parse(session, krb5->buf[which] + 4, len);
    arkime_parser_buf_del(krb5, which, len + 4);
    return 0;
}
/******************************************************************************/
LOCAL void krb5_tcp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    // TCP Kerberos: 4-byte length prefix, then ASN.1
    // First 2 bytes of length should be 0 for reasonable message sizes
    if (len < 5 || data[0] != 0 || data[1] != 0)
        return;

    // Check ASN.1 application tag at offset 4
    // Valid tags: 0x6a (AS-REQ), 0x6b (AS-REP), 0x6c (TGS-REQ), 0x6d (TGS-REP), 0x7e (KRB-ERROR)
    uint8_t tag = data[4];
    if (tag != 0x6a && tag != 0x6b && tag != 0x6c && tag != 0x6d && tag != 0x7e)
        return;

    ArkimeParserBuf_t     *krb5          = arkime_parser_buf_create();

    arkime_parsers_register(session, krb5_tcp_parser, krb5, arkime_parser_buf_session_free);
}
/******************************************************************************/
void arkime_parser_init()
{

    realmField = arkime_field_define("krb5", "termfield",
                                     "krb5.realm", "Realm", "krb5.realm",
                                     "Kerberos 5 Realm",
                                     ARKIME_FIELD_TYPE_STR_GHASH,  ARKIME_FIELD_FLAG_CNT,
                                     (char *)NULL);

    cnameField = arkime_field_define("krb5", "termfield",
                                     "krb5.cname", "cname", "krb5.cname",
                                     "Kerberos 5 cname",
                                     ARKIME_FIELD_TYPE_STR_GHASH,  ARKIME_FIELD_FLAG_CNT,
                                     (char *)NULL);

    snameField = arkime_field_define("krb5", "termfield",
                                     "krb5.sname", "sname", "krb5.sname",
                                     "Kerberos 5 sname",
                                     ARKIME_FIELD_TYPE_STR_GHASH,  ARKIME_FIELD_FLAG_CNT,
                                     (char *)NULL);

    etypeField = arkime_field_define("krb5", "termfield",
                                     "krb5.etype", "Encryption Type", "krb5.etype",
                                     "Kerberos 5 encryption type",
                                     ARKIME_FIELD_TYPE_STR_GHASH,  ARKIME_FIELD_FLAG_CNT,
                                     (char *)NULL);

    errorCodeField = arkime_field_define("krb5", "termfield",
                                         "krb5.error", "Error Code", "krb5.error",
                                         "Kerberos 5 error code",
                                         ARKIME_FIELD_TYPE_STR_GHASH,  ARKIME_FIELD_FLAG_CNT,
                                         (char *)NULL);

    arkime_parsers_classifier_register_udp("krb5", 0, 7, (uint8_t *)"\x03\x02\x01\x05", 4, krb5_udp_classify);
    arkime_parsers_classifier_register_udp("krb5", 0, 9, (uint8_t *)"\x03\x02\x01\x05", 4, krb5_udp_classify);
    arkime_parsers_classifier_register_tcp("krb5", 0, 11, (uint8_t *)"\x03\x02\x01\x05", 4, krb5_tcp_classify);
    arkime_parsers_classifier_register_tcp("krb5", 0, 13, (uint8_t *)"\x03\x02\x01\x05", 4, krb5_tcp_classify);
}

