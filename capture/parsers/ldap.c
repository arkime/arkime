/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL  int bindNameField;
LOCAL  int opField;
LOCAL  int resultField;

LOCAL const char *ldapOps[26] = {
    [0] = "bind", [1] = "bind_response", [2] = "unbind", [3] = "search", [4] = "search_entry", [5] = "search_result",
    [6] = "modify", [7] = "modify_response", [8] = "add", [9] = "add_response", [10] = "delete", [11] = "delete_response",
    [12] = "modifydn", [13] = "modifydn_response", [14] = "compare", [15] = "compare_response", [16] = "abandon",
    [19] = "search_reference", [23] = "extended", [24] = "extended_response", [25] = "intermediate_response"
};

LOCAL const char *ldapResults[] = {
    [0] = "success", [1] = "operationsError", [2] = "protocolError", [3] = "timeLimitExceeded", [4] = "sizeLimitExceeded",
    [5] = "compareFalse", [6] = "compareTrue", [7] = "authMethodNotSupported", [8] = "strongerAuthRequired",
    [9] = "partialResults", [10] = "referral", [11] = "adminLimitExceeded", [12] = "unavailableCriticalExtension",
    [13] = "confidentialityRequired", [14] = "saslBindInProgress", [16] = "noSuchAttribute", [17] = "undefinedAttributeType",
    [18] = "inappropriateMatching", [19] = "constraintViolation", [20] = "attributeOrValueExists", [21] = "invalidAttributeSyntax",
    [32] = "noSuchObject", [33] = "aliasProblem", [34] = "invalidDNSyntax", [36] = "aliasDereferencingProblem",
    [48] = "inappropriateAuthentication", [49] = "invalidCredentials", [50] = "insufficientAccessRights", [51] = "busy",
    [52] = "unavailable", [53] = "unwillingToPerform", [54] = "loopDetect", [60] = "sortControlMissing", [61] = "offsetRangeError",
    [64] = "namingViolation", [65] = "objectClassViolation", [66] = "notAllowedOnNonLeaf", [67] = "notAllowedOnRDN",
    [68] = "entryAlreadyExists", [69] = "objectClassModsProhibited", [71] = "affectsMultipleDSAs", [76] = "virtualListViewError",
    [80] = "other"
};
LOCAL  int authTypeField;

/******************************************************************************/
LOCAL void ldap_process(ArkimeSession_t *session, ArkimeParserBuf_t *ldap, int which)
{
    BSB obsb, ibsb;
    uint32_t opc, otag, olen;
    uint32_t ipc, itag, ilen;
    uint32_t protocolOp;

    BSB_INIT(obsb, ldap->buf[which], ldap->len[which]);
    ldap->len[which] = -1; // stop any calls for this direction

    while (BSB_REMAINING(obsb) > 5) {
        uint8_t *ovalue = arkime_parsers_asn_get_tlv(&obsb, &opc, &otag, &olen);

        BSB_INIT(ibsb, ovalue, olen);

        // messageID
        arkime_parsers_asn_get_tlv(&ibsb, &ipc, &itag, &ilen);
        if (ipc != 0 || itag != 2)
            return;

        // protocolOp
        uint8_t *ivalue = arkime_parsers_asn_get_tlv(&ibsb, &ipc, &protocolOp, &ilen);
        if (ipc != 1 || protocolOp > 25)
            return;
        if (ldapOps[protocolOp])
            arkime_field_string_add(opField, session, ldapOps[protocolOp], -1, TRUE);

        if (protocolOp == 0) {
            BSB_INIT(ibsb, ivalue, ilen);
            ivalue = arkime_parsers_asn_get_tlv(&ibsb, &ipc, &itag, &ilen); // version
            if (!ivalue)
                continue;

            ivalue = arkime_parsers_asn_get_tlv(&ibsb, &ipc, &itag, &ilen); // name
            if (!ivalue)
                continue;
            if (ilen == 0) {
                arkime_field_string_add(bindNameField, session, "<ROOT>", 6, TRUE);
            } else {
                arkime_field_string_add(bindNameField, session, (const char *)ivalue, ilen, TRUE);
            }
            ivalue = arkime_parsers_asn_get_tlv(&ibsb, &ipc, &itag, &ilen); // auth
            if (!ivalue)
                continue;

            char str[100];
            switch (itag) {
            case 0:
                if (ilen == 0)
                    arkime_field_string_add(authTypeField, session, "none", 4, TRUE);
                else
                    arkime_field_string_add(authTypeField, session, "simple", 6, TRUE);
                break;
            case 3: {
                arkime_field_string_add(authTypeField, session, "sasl", 4, TRUE);
                /* SaslCredentials ::= SEQUENCE { mechanism LDAPString,
                 *                                credentials OCTET STRING OPTIONAL }
                 * Tag 3 is context-specific IMPLICIT, so the value is the inner
                 * sequence contents directly. */
                BSB sbsb;
                BSB_INIT(sbsb, ivalue, ilen);
                uint32_t spc, stag, slen;
                const uint8_t *svalue = arkime_parsers_asn_get_tlv(&sbsb, &spc, &stag, &slen);
                if (svalue && stag == 4) {
                    svalue = arkime_parsers_asn_get_tlv(&sbsb, &spc, &stag, &slen);
                    /* For now only handle NTLMSSP blobs delivered directly
                     * (mechanism "NTLM"); SPNEGO/GSSAPI wrapping not yet
                     * unwrapped. */
                    if (svalue && stag == 4 && slen >= 8 &&
                        memcmp(svalue, "NTLMSSP\0", 8) == 0) {
                        arkime_parsers_ntlm_decode(session, svalue, slen);
                    }
                }
                break;
            }
            case 10:
                arkime_field_string_add(authTypeField, session, "ntlmsspNegotiate", 16, TRUE); // from wireshark
                arkime_parsers_ntlm_decode(session, ivalue, ilen);
                break;
            case 11:
                arkime_field_string_add(authTypeField, session, "ntlmsspAuth", 11, TRUE); // from wireshark
                arkime_parsers_ntlm_decode(session, ivalue, ilen);
                break;
            default:
                snprintf(str, sizeof(str), "%d", (int)itag);
                arkime_field_string_add(authTypeField, session, str, -1, TRUE);

            }
        } else if (protocolOp == 1) {
            /* BindResponse: look for NTLMSSP Type 2 challenge in either the
             * matchedDN field (AD hack) or the [7] serverSaslCreds field. */
            BSB rbsb;
            BSB_INIT(rbsb, ivalue, ilen);
            uint32_t rpc, rtag, rlen;
            // resultCode (ENUMERATED)
            const uint8_t *rcvalue = arkime_parsers_asn_get_tlv(&rbsb, &rpc, &rtag, &rlen);
            if (rcvalue && rtag == 10 && rlen == 1 && rcvalue[0] < ARRAY_LEN(ldapResults) && ldapResults[rcvalue[0]])
                arkime_field_string_add(resultField, session, ldapResults[rcvalue[0]], -1, TRUE);
            // matchedDN (OCTET STRING)
            const uint8_t *rvalue = arkime_parsers_asn_get_tlv(&rbsb, &rpc, &rtag, &rlen);
            if (rvalue && rlen >= 8 && memcmp(rvalue, "NTLMSSP\0", 8) == 0) {
                arkime_parsers_ntlm_decode(session, rvalue, rlen);
            }
            // diagnosticMessage (OCTET STRING)
            arkime_parsers_asn_get_tlv(&rbsb, &rpc, &rtag, &rlen);
            // optional referral [3] or serverSaslCreds [7]
            while (BSB_REMAINING(rbsb) > 2) {
                rvalue = arkime_parsers_asn_get_tlv(&rbsb, &rpc, &rtag, &rlen);
                if (!rvalue)
                    break;
                if (rtag == 7 && rlen >= 8 &&
                    memcmp(rvalue, "NTLMSSP\0", 8) == 0) {
                    arkime_parsers_ntlm_decode(session, rvalue, rlen);
                    break;
                }
            }
        } else if (protocolOp == 23) {
            int len = BSB_REMAINING(obsb);
            if (len > 0) {
                arkime_parsers_classify_tcp(session, BSB_WORK_PTR(obsb), len, which);
                arkime_packet_process_data(session, BSB_WORK_PTR(obsb), len, which);
            }
            return;
        } else if (protocolOp == 24) {
            int len = BSB_REMAINING(obsb);
            if (len > 0) {
                arkime_packet_process_data(session, BSB_WORK_PTR(obsb), len, which);
            }
        }
    }
}
/******************************************************************************/
LOCAL int ldap_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int remaining, int which)
{
    ArkimeParserBuf_t     *ldap          = uw;

    if (ldap->len[which] == -1) { // Stop recursion
        return 0;
    }

    // Copy the data we have
    if (arkime_parser_buf_add(ldap, which, data, remaining) == -1) {
        ldap->len[which] = -1;
        if (ldap->len[(which + 1) % 2] == -1)
            return ARKIME_PARSER_UNREGISTER;
        return 0;
    }

    if (ldap->len[which] > 6000) {
        ldap_process(session, ldap, which);
        if (ldap->len[(which + 1) % 2] == -1) // If other direction is finished then unregister
            arkime_parsers_unregister(session, ldap);
    }

    return 0;
}
/******************************************************************************/
LOCAL void ldap_save(ArkimeSession_t *session, void *uw, int UNUSED(final))
{
    ArkimeParserBuf_t     *ldap          = uw;

    if (ldap->len[0] > 5) {
        ldap_process(session, ldap, 0);
    }

    if (ldap->len[1] > 5) {
        ldap_process(session, ldap, 1);
    }
}
/******************************************************************************/
LOCAL void ldap_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "ldap"))
        return;

    BSB bsb;
    BSB_INIT(bsb, data, len);

    uint32_t apc, atag, alen;
    uint8_t *value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);
    if (value && apc && atag == 16) {
        BSB_INIT(bsb, value, alen);

        // messageID
        value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);
        if (!value || apc != 0 || atag != 2)
            return;

        // protocolOp must be APPLICATION class constructed (raw byte 0x60-0x7F).
        // This rejects SNMPv3 / other ASN.1 sequences whose second child is a
        // UNIVERSAL SEQUENCE (0x30) that would otherwise pass the apc/atag check.
        if (BSB_REMAINING(bsb) < 2 || (BSB_WORK_PTR(bsb)[0] & 0xC0) != 0x40)
            return;

        // protocolOp
        value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);
        if (!value || apc != 1 || atag > 25)
            return;

        arkime_session_add_protocol(session, "ldap");
        ArkimeParserBuf_t  *ldap = arkime_parser_buf_create();

        arkime_parsers_register2(session, ldap_parser, ldap, arkime_parser_buf_session_free, ldap_save);
    }
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_parsers_classifier_register_tcp("ldap", NULL, 0, (uint8_t *)"\x30", 1, ldap_classify);
    arkime_parsers_classifier_register_udp("ldap", NULL, 0, (uint8_t *)"\x30", 1, ldap_classify);

    authTypeField = arkime_field_define("ldap", "termfield",
                                        "ldap.authtype", "Auth Type", "ldap.authtype",
                                        "The auth type of ldap bind",
                                        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);

    opField = arkime_field_define("ldap", "termfield",
                                  "ldap.op", "Operation", "ldap.op",
                                  "LDAP protocol operations seen",
                                  ARKIME_FIELD_TYPE_STR_HASH, ARKIME_FIELD_FLAG_CNT,
                                  (char *)NULL);

    resultField = arkime_field_define("ldap", "termfield",
                                      "ldap.result", "Bind Result", "ldap.result",
                                      "LDAP bind response result code",
                                      ARKIME_FIELD_TYPE_STR_HASH, ARKIME_FIELD_FLAG_CNT,
                                      (char *)NULL);

    bindNameField = arkime_field_define("ldap", "termfield",
                                        "ldap.bindname", "Bind Name", "ldap.bindname",
                                        "The bind name of ldap bind",
                                        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);
}
