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

//#define KRB5_DEBUG 1

extern MolochConfig_t        config;

LOCAL  int realmField;
LOCAL  int cnameField;
LOCAL  int snameField;

#define KRB5_MAX_SIZE 4096
typedef struct {
    unsigned char  data[2][KRB5_MAX_SIZE];
    int            pos[2];
} KRB5Info_t;

/******************************************************************************/
/* wireshark: k5.asn which based on http://www.h5l.org/dist/src/heimdal-1.2.tar.gz
--PrincipalName ::= SEQUENCE {
--      name-type[0]            NAME-TYPE,
--      name-string[1]          SEQUENCE OF GeneralString
--}
 */
LOCAL void krb5_parse_principal_name(MolochSession_t *session, int field, const unsigned char *data, int len)
{
    MolochASNSeq_t seq[10];

    int num = moloch_parsers_asn_get_sequence(seq, 2, data, len, TRUE);

    if (num < 2 || seq[1].tag != 1)
        return;

    num = moloch_parsers_asn_get_sequence(seq, 2, seq[1].value, seq[1].len, TRUE);

    int len0, len1;
    const char *value0, *value1;
    if (num == 1) {
        value0 = moloch_parsers_asn_sequence_to_string(&seq[0], &len0);
        moloch_field_string_add(field, session, value0, len0, TRUE);
    } else if (num == 2) {
        char str[255];
        value0 = moloch_parsers_asn_sequence_to_string(&seq[0], &len0);
        value1 = moloch_parsers_asn_sequence_to_string(&seq[1], &len1);
        snprintf(str, 255, "%.*s/%.*s", len0, value0, len1, value1);
        moloch_field_string_add(field, session, str, len0 + 1 + len1, TRUE);
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
LOCAL void krb5_parse_req_body(MolochSession_t *session, const unsigned char *data, int len)
{
    MolochASNSeq_t seq[12];

    int num = moloch_parsers_asn_get_sequence(seq, 12, data, len, TRUE);
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
            value = moloch_parsers_asn_sequence_to_string(&seq[i], &vlen);
            moloch_field_string_add(realmField, session, value, vlen, TRUE);
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
LOCAL void krb5_parse_req(MolochSession_t *session, const unsigned char *data, int len)
{
    MolochASNSeq_t seq[5];

    int num = moloch_parsers_asn_get_sequence(seq, 5, data, len, TRUE);
    if (num < 3)
        return;

    if (!seq[0].pc || seq[0].tag != 1 || seq[0].value[seq[0].len - 1] != 5) {
        return;
    }

    int msgType = seq[1].value[seq[1].len - 1];
    if (!seq[1].pc || seq[1].tag != 2 || (msgType != 10 && msgType != 12)) {
        return;
    }

    moloch_session_add_protocol(session, "krb5");

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
LOCAL void krb5_parse_rep(MolochSession_t *UNUSED(session), const unsigned char *UNUSED(data), int UNUSED(len))
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
LOCAL void krb5_parse_error(MolochSession_t *UNUSED(session), const unsigned char *UNUSED(data), int UNUSED(len))
{
}
/******************************************************************************/
LOCAL void krb5_parse(MolochSession_t *session, const unsigned char *data, int len)
{
    BSB obsb;
    uint32_t opc, msgType, olen;
    unsigned char *ovalue;

    BSB_INIT(obsb, data, len);
    ovalue = moloch_parsers_asn_get_tlv(&obsb, &opc, &msgType, &olen);
#ifdef KRB5_DEBUG
    LOG("DEBUG1 - opc:%d msgType:%d olen:%d", opc, msgType, olen);
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
LOCAL int krb5_udp_parser(MolochSession_t *session, void *UNUSED(uw), const unsigned char *data, int len, int UNUSED(which))
{
    krb5_parse(session, data, len);
    return 0;
}
/******************************************************************************/
LOCAL void krb5_udp_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (moloch_session_has_protocol(session, "krb5"))
        return;

    BSB obsb;
    uint32_t opc, otag, olen;

    BSB_INIT(obsb, data, len);
    moloch_parsers_asn_get_tlv(&obsb, &opc, &otag, &olen);
#ifdef KRB5_DEBUG
    LOG("enter %d %d %d", opc, otag, olen);
#endif
    if (opc && (otag == 10 || otag == 12 || otag == 30) && len >= (int)olen) {
        moloch_parsers_register(session, krb5_udp_parser, 0, 0);
    }
}
/******************************************************************************/
LOCAL void krb5_free(MolochSession_t UNUSED(*session), void *uw)
{
    KRB5Info_t            *krb5          = uw;

    MOLOCH_TYPE_FREE(KRB5Info_t, krb5);
}
/******************************************************************************/
LOCAL int krb5_tcp_parser(MolochSession_t *session, void *uw, const unsigned char *data, int remaining, int which)
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
    krb5_parse(session, krb5->data[which]+4, len);
    memmove(krb5->data[which], krb5->data[which]+len+4, krb5->pos[which] - len - 4);
    krb5->pos[which] -= (len + 4);
    return 0;
}
/******************************************************************************/
LOCAL void krb5_tcp_classify(MolochSession_t *session, const unsigned char *data, int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    if (which !=0 || data[0] != 0 || data[1] != 0)
        return;

    KRB5Info_t            *krb5          = MOLOCH_TYPE_ALLOC(KRB5Info_t);
    krb5->pos[0] = krb5->pos[1] = 0;

    moloch_parsers_register(session, krb5_tcp_parser, krb5, krb5_free);
}
/******************************************************************************/
void moloch_parser_init()
{

    realmField = moloch_field_define("krb5", "termfield",
        "krb5.realm", "Realm", "krb5.realm",
        "Kerberos 5 Realm",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL);

    cnameField = moloch_field_define("krb5", "termfield",
        "krb5.cname", "cname", "krb5.cname",
        "Kerberos 5 cname",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL);

    snameField = moloch_field_define("krb5", "termfield",
        "krb5.sname", "sname", "krb5.sname",
        "Kerberos 5 sname",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL);

    moloch_parsers_classifier_register_udp("krb5", 0, 7, (unsigned char*)"\x03\x02\x01\x05", 4, krb5_udp_classify);
    moloch_parsers_classifier_register_udp("krb5", 0, 9, (unsigned char*)"\x03\x02\x01\x05", 4, krb5_udp_classify);
    moloch_parsers_classifier_register_tcp("krb5", 0, 11, (unsigned char*)"\x03\x02\x01\x05", 4, krb5_tcp_classify);
    moloch_parsers_classifier_register_tcp("krb5", 0, 13, (unsigned char*)"\x03\x02\x01\x05", 4, krb5_tcp_classify);
}

