/* Copyright 2012-2016 AOL Inc. All rights reserved.
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
/******************************************************************************/
void ldap_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    BSB bsb;
    BSB_INIT(bsb, data, len);

    uint32_t apc, atag, alen;
    unsigned char *value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);
    if (value && apc && atag == 16) {
        BSB_INIT(bsb, value, alen);

        // messageID
        value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);
        if (apc != 0 || atag != 2)
            return;

        // protocolOp
        value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);
        if (apc != 1 || atag > 25)
            return;

        moloch_session_add_protocol(session, "ldap");
    }
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_parsers_classifier_register_tcp("ldap", NULL, 0, (unsigned char*)"\x30", 1, ldap_classify);
    moloch_parsers_classifier_register_udp("ldap", NULL, 0, (unsigned char*)"\x30", 1, ldap_classify);
}
