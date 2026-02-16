/* c1222.c
 *
 * Copyright 2026 All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * ANSI C12.22 Protocol Parser
 * Parses C12.22 (IEEE 1703) smart meter communication protocol
 * which uses ACSE (Association Control Service Element) over TCP port 1153
 */

#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL int calledApTitleField;
LOCAL int callingApTitleField;
LOCAL int calledApInvocationIdField;
LOCAL int callingApInvocationIdField;
LOCAL int securityModeField;
LOCAL int responseControlField;

LOCAL const char *c1222_security_modes[] = {
    "cleartext",
    "cleartext-with-auth",
    "ciphertext-with-auth",
    "unknown"
};

LOCAL const char *c1222_response_controls[] = {
    "always",
    "on-exception",
    "never",
    "unknown"
};

/******************************************************************************/
// Parse the user-information element which contains EPSEM flags
LOCAL void c1222_parse_user_info(ArkimeSession_t *session, const uint8_t *data, int len)
{
    // user-information wraps: EXTERNAL { [0] IMPLICIT OCTET STRING }
    // Structure: 0x28 len 0x81 len <epsem-flags> <epsem-data>
    BSB bsb;
    BSB_INIT(bsb, data, len);

    uint32_t pc, tag, tlen;
    uint8_t *value = arkime_parsers_asn_get_tlv(&bsb, &pc, &tag, &tlen);
    if (!value)
        return;

    // Expect constructed tag 8 (0x28 = context[8] constructed)
    BSB innerBsb;
    BSB_INIT(innerBsb, value, tlen);

    value = arkime_parsers_asn_get_tlv(&innerBsb, &pc, &tag, &tlen);
    if (!value || tlen < 1)
        return;

    // EPSEM flags byte
    uint8_t flags = value[0];
    int securityMode = (flags >> 2) & 0x03;
    int responseControl = flags & 0x03;

    if (securityMode <= 2)
        arkime_field_string_add(securityModeField, session, c1222_security_modes[securityMode], -1, TRUE);
    else
        arkime_field_string_add(securityModeField, session, c1222_security_modes[3], -1, TRUE);

    if (responseControl <= 2)
        arkime_field_string_add(responseControlField, session, c1222_response_controls[responseControl], -1, TRUE);
    else
        arkime_field_string_add(responseControlField, session, c1222_response_controls[3], -1, TRUE);
}

/******************************************************************************/
// Parse a C12.22 ACSE message (inner content after 0x60 wrapper)
LOCAL void c1222_parse_message(ArkimeSession_t *session, const uint8_t *data, int len)
{
    ArkimeASNSeq_t seq[10];
    int num = arkime_parsers_asn_get_sequence(seq, 10, data, len, FALSE);

    for (int i = 0; i < num; i++) {
        switch (seq[i].tag) {
        case 4: { // called-AP-invocation-id [4]
            int val = arkime_parsers_asn_sequence_to_int(&seq[i]);
            if (val >= 0)
                arkime_field_int_add(calledApInvocationIdField, session, val);
            break;
        }
        case 2: // called-AP-title [2]
        case 6: { // calling-AP-title [6]
            // Unwrap to find the OID inside
            BSB oidBsb;
            BSB_INIT(oidBsb, seq[i].value, seq[i].len);
            uint32_t opc, otag, olen;
            const uint8_t *ovalue = arkime_parsers_asn_get_tlv(&oidBsb, &opc, &otag, &olen);
            if (ovalue && otag == 6) { // OID tag
                char oidStr[256];
                arkime_parsers_asn_decode_oid(oidStr, sizeof(oidStr), ovalue, olen);
                int field = (seq[i].tag == 2) ? calledApTitleField : callingApTitleField;
                arkime_field_string_add(field, session, oidStr, -1, TRUE);
            }
            break;
        }
        case 8: { // calling-AP-invocation-id [8]
            int val = arkime_parsers_asn_sequence_to_int(&seq[i]);
            if (val >= 0)
                arkime_field_int_add(callingApInvocationIdField, session, val);
            break;
        }
        case 30: // user-information [30] (0xbe)
            c1222_parse_user_info(session, seq[i].value, seq[i].len);
            break;
        }
    }
}

/******************************************************************************/
LOCAL void c1222_parse(ArkimeSession_t *session, const uint8_t *data, int len)
{
    BSB bsb;
    BSB_INIT(bsb, data, len);

    uint32_t opc, otag, olen;
    const uint8_t *ovalue = arkime_parsers_asn_get_tlv(&bsb, &opc, &otag, &olen);
    if (!opc || !ovalue)
        return;

    // Tag 0 = APPLICATION[0] CONSTRUCTED (0x60)
    if (otag != 0)
        return;

    c1222_parse_message(session, ovalue, olen);
}

/******************************************************************************/
LOCAL int c1222_tcp_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int which)
{
    ArkimeParserBuf_t *c1222 = uw;

    arkime_parser_buf_add(c1222, which, data, len);

    while (c1222->len[which] >= 2) {
        // Peek at ASN.1 length to determine message boundary
        BSB bsb;
        BSB_INIT(bsb, c1222->buf[which], c1222->len[which]);

        uint32_t opc, otag, olen;
        uint8_t *ovalue = arkime_parsers_asn_get_tlv(&bsb, &opc, &otag, &olen);
        if (!ovalue)
            break;

        int total = (ovalue + olen) - c1222->buf[which];
        if (total > (int)c1222->len[which])
            break; // Need more data

        c1222_parse(session, c1222->buf[which], total);
        arkime_parser_buf_del(c1222, which, total);
    }

    return 0;
}

/******************************************************************************/
LOCAL void c1222_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "c1222"))
        return;

    if (len < 6)
        return;

    // C12.22 starts with 0x60 (ACSE APPLICATION[0] CONSTRUCTED)
    if (data[0] != 0x60)
        return;

    // Decode the outer TLV
    BSB bsb;
    BSB_INIT(bsb, data, len);
    uint32_t opc, otag, olen;
    uint8_t *ovalue = arkime_parsers_asn_get_tlv(&bsb, &opc, &otag, &olen);
    if (!opc || !ovalue || otag != 0)
        return;

    // First inner element should be a context tag for AP-title or invocation-id
    BSB innerBsb;
    BSB_INIT(innerBsb, ovalue, olen);
    uint32_t ipc, itag, ilen;
    arkime_parsers_asn_get_tlv(&innerBsb, &ipc, &itag, &ilen);

    // Valid first elements: called-AP-title[2], called-AP-invocation-id[4], calling-AP-title[6]
    if (itag != 2 && itag != 4 && itag != 6)
        return;

    arkime_session_add_protocol(session, "c1222");

    ArkimeParserBuf_t *c1222 = arkime_parser_buf_create();
    arkime_parsers_register(session, c1222_tcp_parser, c1222, arkime_parser_buf_session_free);
}

/******************************************************************************/
void arkime_parser_init()
{
    calledApTitleField = arkime_field_define("c1222", "termfield",
                                             "c1222.calledApTitle", "Called AP Title", "c1222.calledApTitle",
                                             "C12.22 called AP title OID",
                                             ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                             (char *)NULL);

    callingApTitleField = arkime_field_define("c1222", "termfield",
                                              "c1222.callingApTitle", "Calling AP Title", "c1222.callingApTitle",
                                              "C12.22 calling AP title OID",
                                              ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                              (char *)NULL);

    calledApInvocationIdField = arkime_field_define("c1222", "integer",
                                                    "c1222.calledApInvocationId", "Called AP Invocation ID", "c1222.calledApInvocationId",
                                                    "C12.22 called AP invocation identifier",
                                                    ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                                    (char *)NULL);

    callingApInvocationIdField = arkime_field_define("c1222", "integer",
                                                     "c1222.callingApInvocationId", "Calling AP Invocation ID", "c1222.callingApInvocationId",
                                                     "C12.22 calling AP invocation identifier",
                                                     ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                                     (char *)NULL);

    securityModeField = arkime_field_define("c1222", "termfield",
                                            "c1222.securityMode", "Security Mode", "c1222.securityMode",
                                            "C12.22 EPSEM security mode",
                                            ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                            (char *)NULL);

    responseControlField = arkime_field_define("c1222", "termfield",
                                               "c1222.responseControl", "Response Control", "c1222.responseControl",
                                               "C12.22 EPSEM response control",
                                               ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                               (char *)NULL);

    arkime_parsers_classifier_register_tcp("c1222", NULL, 0, (uint8_t *)"\x60", 1, c1222_classify);
}
