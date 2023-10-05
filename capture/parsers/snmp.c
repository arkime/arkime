/******************************************************************************/
/* Copyright Yahoo Inc.
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
#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL  int                   versionField;
LOCAL  int                   communityField;
LOCAL  int                   errorField;
LOCAL  int                   variableField;
LOCAL  int                   typeField;

LOCAL  char                 *types[8] = {"GetRequest", "GetNextRequest", "GetResponse", "SetRequest", "Trap", "GetBulkRequest", "InformRequest", "SNMPv2-Trap"};
LOCAL  int                   lens[8];

/******************************************************************************/
LOCAL int snmp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    int version;
    uint32_t dataType;
    uint32_t apc, atag, alen;
    BSB bsb;

    BSB_INIT(bsb, data, len);
    uint8_t *value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value || atag != 16 || alen < 16)
        return ARKIME_PARSER_UNREGISTER;

    BSB_INIT(bsb, value, alen);

    // Version
    value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value || atag != 2 || alen != 1 || value[0] > 3)
        return ARKIME_PARSER_UNREGISTER;

    version = value[0] + 1;
    arkime_field_int_add(versionField, session, version);

    // Only try and decode version 1 & 2
    if (version > 2)
        return ARKIME_PARSER_UNREGISTER;

    // Community
    value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value || apc != 0 || alen == 0)
        return ARKIME_PARSER_UNREGISTER;

    arkime_field_string_add(communityField, session, (char *)value, alen, TRUE);

    value = arkime_parsers_asn_get_tlv(&bsb, &apc, &dataType, &alen);

    if (value && dataType < 8) {
        arkime_field_string_add(typeField, session, types[dataType], lens[dataType], TRUE);
    } else {
        // This is probably not a SNMP stream after all
        return ARKIME_PARSER_UNREGISTER;
    }

    if (!apc || !value || !alen)
        return 0;

    // Trap & GetBulkRequest have different formats
    if (dataType == 4 || dataType == 5)
        return 0;

    BSB_INIT(bsb, value, alen);

    // Request Id
    value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value)
        return 0;

    //  Error Status
    value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value)
        return 0;

    if (alen == 1 && value[0]) {
        arkime_field_int_add(errorField, session, value[0]);
    }

    //  Error Index
    value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value)
        return 0;

    // Variable-Bindings
    value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value || apc != 1)
        return 0;

    BSB_INIT(bsb, value, alen);
    while (BSB_REMAINING(bsb) && !BSB_IS_ERROR(bsb)) {
        value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

        if (!value || apc != 1)
            return 0;

        BSB obsb;
        char oid[100];
        BSB_INIT(obsb, value, alen);
        value = arkime_parsers_asn_get_tlv(&obsb, &apc, &atag, &alen);

        if (!value || apc != 0)
            return 0;

        arkime_parsers_asn_decode_oid(oid, sizeof(oid), value, alen);
        arkime_field_string_add(variableField, session, (char *)oid, -1, TRUE);
    }

    return 0;
}
/******************************************************************************/
LOCAL void snmp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    uint32_t apc, atag, alen;
    BSB bsb;

    if (len < 12)
        return;

    if (session->port1 != 161 && session->port1 != 162 && session->port1 != 8161 &&
        session->port2 != 161 && session->port2 != 162 && session->port2 != 8161) {
        return;
    }

    BSB_INIT(bsb, data, len);
    uint8_t *value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value || atag != 16 || alen < 16)
        return;

    BSB_INIT(bsb, value, alen);

    value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value || atag != 2 || alen != 1 || value[0] > 3)
        return;

    arkime_session_add_protocol(session, "snmp");
    arkime_parsers_register(session, snmp_parser, uw, 0);
}
/******************************************************************************/
void arkime_parser_init()
{
    for (int i = 0; i < 8; i++) {
        lens[i] = strlen(types[i]);
    }
    CLASSIFY_UDP("snmp", 0, "\x30", snmp_classify);

    versionField = arkime_field_define("snmp", "integer",
        "snmp.version", "Version", "snmp.version",
        "SNMP Version",
        ARKIME_FIELD_TYPE_INT_HASH,  ARKIME_FIELD_FLAG_CNT,
        (char *)NULL);

    communityField = arkime_field_define("snmp", "termfield",
        "snmp.community", "Community", "snmp.community",
        "SNMP Community",
        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
        (char *)NULL);

    errorField = arkime_field_define("snmp", "integer",
        "snmp.error", "Error Code", "snmp.error",
        "SNMP Error Code",
        ARKIME_FIELD_TYPE_INT_HASH,  ARKIME_FIELD_FLAG_CNT,
        (char *)NULL);

    variableField = arkime_field_define("snmp", "termfield",
        "snmp.variable", "Variable", "snmp.variable",
        "SNMP Variable",
        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
        (char *)NULL);

    typeField = arkime_field_define("snmp", "termfield",
        "snmp.type", "Type", "snmp.type",
        "SNMP Type",
        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
        (char *)NULL);
}
