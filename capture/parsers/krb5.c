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

#define KRB5_MAX_SIZE 4096
typedef struct {
    uint8_t  data[2][KRB5_MAX_SIZE];
    int      pos[2];
} KRB5Info_t;

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
    const char *value0, *value1;
    if (num == 1) {
        value0 = arkime_parsers_asn_sequence_to_string(&seq[0], &len0);
        if (value0 && len0 > 0)
            arkime_field_string_add(field, session, value0, len0, TRUE);
    } else if (num == 2) {
        char str[255];
        value0 = arkime_parsers_asn_sequence_to_string(&seq[0], &len0);
        value1 = arkime_parsers_asn_sequence_to_string(&seq[1], &len1);
        snprintf(str, 255, "%.*s/%.*s", len0, value0, len1, value1);
        arkime_field_string_add(field, session, str, len0 + 1 + len1, TRUE);
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
    int i;
    int vlen;
    const char *value;
    for (i = 0; i < num; i++) {
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
LOCAL void krb5_parse_rep(ArkimeSession_t *UNUSED(session), const uint8_t *UNUSED(data), int UNUSED(len))
{
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
LOCAL void krb5_parse_error(ArkimeSession_t *UNUSED(session), const uint8_t *UNUSED(data), int UNUSED(len))
{
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
LOCAL void krb5_free(ArkimeSession_t UNUSED(*session), void *uw)
{
    KRB5Info_t            *krb5          = uw;

    ARKIME_TYPE_FREE(KRB5Info_t, krb5);
}
/******************************************************************************/
LOCAL int krb5_tcp_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int remaining, int which)
{
    KRB5Info_t *krb5 = uw;

    remaining = MIN(remaining, KRB5_MAX_SIZE - krb5->pos[which]);
    memcpy(krb5->data[which] + krb5->pos[which], data, remaining);
    krb5->pos[which] += remaining;

    if (krb5->pos[which] < 4)
        return 0;

    int len = (krb5->data[which][2] << 8) | krb5->data[which][3];
    if (krb5->pos[which] < len + 4)
        return 0;
    krb5_parse(session, krb5->data[which] + 4, len);
    memmove(krb5->data[which], krb5->data[which] + len + 4, krb5->pos[which] - len - 4);
    krb5->pos[which] -= (len + 4);
    return 0;
}
/******************************************************************************/
LOCAL void krb5_tcp_classify(ArkimeSession_t *session, const uint8_t *data, int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    if (len < 2 || which != 0 || data[0] != 0 || data[1] != 0)
        return;

    KRB5Info_t            *krb5          = ARKIME_TYPE_ALLOC(KRB5Info_t);
    krb5->pos[0] = krb5->pos[1] = 0;

    arkime_parsers_register(session, krb5_tcp_parser, krb5, krb5_free);
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

    arkime_parsers_classifier_register_udp("krb5", 0, 7, (uint8_t *)"\x03\x02\x01\x05", 4, krb5_udp_classify);
    arkime_parsers_classifier_register_udp("krb5", 0, 9, (uint8_t *)"\x03\x02\x01\x05", 4, krb5_udp_classify);
    arkime_parsers_classifier_register_tcp("krb5", 0, 11, (uint8_t *)"\x03\x02\x01\x05", 4, krb5_tcp_classify);
    arkime_parsers_classifier_register_tcp("krb5", 0, 13, (uint8_t *)"\x03\x02\x01\x05", 4, krb5_tcp_classify);
}

