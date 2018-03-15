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

extern MolochConfig_t        config;

LOCAL  int bindNameField;
LOCAL  int authTypeField;

typedef struct {
    unsigned char       buf[2][8192];
    int                 len[2];
} LDAPInfo_t;
/******************************************************************************/
LOCAL void ldap_process(MolochSession_t *session, LDAPInfo_t *ldap, int which)
{
    BSB obsb, ibsb;
    uint32_t opc, otag, olen;
    uint32_t ipc, itag, ilen;
    uint32_t protocolOp;
    unsigned char *ovalue, *ivalue;

    BSB_INIT(obsb, ldap->buf[which], ldap->len[which]);
    ldap->len[which] = -1; // stop any calls for this direction

    while (BSB_REMAINING(obsb) > 5) {
        ovalue = moloch_parsers_asn_get_tlv(&obsb, &opc, &otag, &olen);

        BSB_INIT(ibsb, ovalue, olen);

        // messageID
        moloch_parsers_asn_get_tlv(&ibsb, &ipc, &itag, &ilen);
        if (ipc != 0 || itag != 2)
            return;

        // protocolOp
        ivalue = moloch_parsers_asn_get_tlv(&ibsb, &ipc, &protocolOp, &ilen);
        if (ipc != 1 || protocolOp > 25)
            return;

        if (protocolOp == 0) {
            BSB_INIT(ibsb, ivalue, ilen);
            ivalue = moloch_parsers_asn_get_tlv(&ibsb, &ipc, &itag, &ilen); // version
            if (!ivalue)
                continue;

            ivalue = moloch_parsers_asn_get_tlv(&ibsb, &ipc, &itag, &ilen); // name
            if (!ivalue)
                continue;
            if (ilen == 0) {
                moloch_field_string_add(bindNameField, session, "<ROOT>", 6, TRUE);
            } else {
                moloch_field_string_add(bindNameField, session, (const char*)ivalue, ilen, TRUE);
            }
            ivalue = moloch_parsers_asn_get_tlv(&ibsb, &ipc, &itag, &ilen); // auth
            if (!ivalue)
                continue;

            char str[100];
            switch (itag) {
            case 0:
                if (ilen == 0)
                    moloch_field_string_add(authTypeField, session, "none", 4, TRUE);
                else
                    moloch_field_string_add(authTypeField, session, "simple", 6, TRUE);
                break;
            case 3:
                moloch_field_string_add(authTypeField, session, "sasl", 4, TRUE);
                break;
            case 10:
                moloch_field_string_add(authTypeField, session, "ntlmsspNegotiate", 16, TRUE); // from wireshark
                break;
            case 11:
                moloch_field_string_add(authTypeField, session, "ntlmsspAuth", 11, TRUE); // from wireshark
                break;
            default:
                snprintf(str, sizeof(str), "%d", itag);
                moloch_field_string_add(authTypeField, session, str, -1, TRUE);

            }
        } else if (protocolOp == 23) {
            int len = BSB_SIZE(obsb) - olen - 2;
            moloch_parsers_classify_tcp(session, ldap->buf[which] + olen + 2, len, which);
            moloch_packet_process_data(session, ldap->buf[which] + olen + 2, len, which);
            return;
        } else if (protocolOp == 24) {
            int len = BSB_SIZE(obsb) - olen - 2;
            moloch_packet_process_data(session, ldap->buf[which] + olen + 2, len, which);
        }
    }
}
/******************************************************************************/
LOCAL int ldap_parser(MolochSession_t *session, void *uw, const unsigned char *data, int remaining, int which)
{
    LDAPInfo_t            *ldap          = uw;

    if (ldap->len[which] == -1) { // Stop recursion
        return 0;
    }

    // Copy the data we have
    memcpy(ldap->buf[which] + ldap->len[which], data, MIN(remaining, (int)sizeof(ldap->buf[which])-ldap->len[which]));
    ldap->len[which] += MIN(remaining, (int)sizeof(ldap->buf[which])-ldap->len[which]);

    if (ldap->len[which] > 6000) {
        ldap_process(session, ldap, which);
        if (ldap->len[(which + 1) %2] == -1) // If other direction is finished then unregister
            moloch_parsers_unregister(session, ldap);
    }

    return 0;
}
/******************************************************************************/
LOCAL void ldap_save(MolochSession_t *session, void *uw, int UNUSED(final))
{
    LDAPInfo_t            *ldap          = uw;

    if (ldap->len[0] > 5) {
        ldap_process(session, ldap, 0);
    }

    if (ldap->len[1] > 5) {
        ldap_process(session, ldap, 1);
    }
}
/******************************************************************************/
LOCAL void ldap_free(MolochSession_t *UNUSED(session), void *uw)
{
    LDAPInfo_t            *ldap          = uw;

    MOLOCH_TYPE_FREE(LDAPInfo_t, ldap);
}
/******************************************************************************/
LOCAL void ldap_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (moloch_session_has_protocol(session, "ldap"))
        return;

    BSB bsb;
    BSB_INIT(bsb, data, len);

    uint32_t apc, atag, alen;
    unsigned char *value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);
    if (value && apc && atag == 16) {
        BSB_INIT(bsb, value, alen);

        // messageID
        value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);
        if (!value || apc != 0 || atag != 2)
            return;

        // protocolOp
        value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);
        if (!value || apc != 1 || atag > 25)
            return;

        moloch_session_add_protocol(session, "ldap");
        LDAPInfo_t  *ldap = MOLOCH_TYPE_ALLOC(LDAPInfo_t);
        ldap->len[0]         = 0;
        ldap->len[1]         = 0;

        moloch_parsers_register2(session, ldap_parser, ldap, ldap_free, ldap_save);
    }
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_parsers_classifier_register_tcp("ldap", NULL, 0, (unsigned char*)"\x30", 1, ldap_classify);
    moloch_parsers_classifier_register_udp("ldap", NULL, 0, (unsigned char*)"\x30", 1, ldap_classify);

    authTypeField = moloch_field_define("ldap", "termfield",
        "ldap.authtype", "Auth Type", "ldap.authtype",
        "The auth type of ldap bind",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL);

    bindNameField = moloch_field_define("ldap", "termfield",
        "ldap.bindname", "Bind Name", "ldap.bindname",
        "The bind name of ldap bind",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL);
}
