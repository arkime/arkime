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
#include "moloch.h"

extern MolochConfig_t        config;

LOCAL  int                   versionField;
LOCAL  int                   communityField;
LOCAL  int                   errorField;
LOCAL  int                   variableField;
LOCAL  int                   typeField;

LOCAL  char                 *types[8] = {"GetRequest", "GetNextRequest", "GetResponse", "SetRequest", "Trap", "GetBulkRequest", "InformRequest", "SNMPv2-Trap"};
LOCAL  int                   lens[8];

/******************************************************************************/
LOCAL int snmp_parser(MolochSession_t *session, void *UNUSED(uw), const unsigned char *data, int len, int UNUSED(which))
{
    int version;
    uint32_t dataType;
    uint32_t apc, atag, alen;
    BSB bsb;

    BSB_INIT(bsb, data, len);
    unsigned char *value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value || atag != 16 || alen < 16)
        return MOLOCH_PARSER_UNREGISTER;

    BSB_INIT(bsb, value, alen);


    // Version
    value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value || atag != 2 || alen != 1 || value[0] > 3)
        return MOLOCH_PARSER_UNREGISTER;

    version = value[0] + 1;
    moloch_field_int_add(versionField, session, version);

    if (version > 2)
        return 0;

    // Community
    value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value || apc != 0 || alen == 0)
        return MOLOCH_PARSER_UNREGISTER;

    moloch_field_string_add(communityField, session, (char *)value, alen, TRUE);

    value = moloch_parsers_asn_get_tlv(&bsb, &apc, &dataType, &alen);

    if (value && dataType < 8) {
        moloch_field_string_add(typeField, session, types[dataType], lens[dataType], TRUE);
    }

    if (dataType > 2 || !apc || !value || !alen)
        return 0;

    BSB_INIT(bsb, value, alen);


    // Request Id
    value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value)
        return 0;

    //  Error Status
    value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value)
        return 0;

    if (alen == 1 && value[0]) {
        moloch_field_int_add(errorField, session, value[0]);
    }

    //  Error Index
    value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value)
        return 0;

    // Variable-Bindings
    value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value || apc != 1)
        return 0;

    BSB_INIT(bsb, value, alen);
    while (BSB_REMAINING(bsb) && !BSB_IS_ERROR(bsb)) {
        value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

        if (!value || apc != 1)
            return 0;

        BSB obsb;
        char oid[100];
        BSB_INIT(obsb, value, alen);
        value = moloch_parsers_asn_get_tlv(&obsb, &apc, &atag, &alen);

        if (!value || apc != 0)
            return 0;

        moloch_parsers_asn_decode_oid(oid, sizeof(oid), value, alen);
        moloch_field_string_add(variableField, session, (char *)oid, -1, TRUE);
    }

    return 0;
}
/******************************************************************************/
LOCAL void snmp_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    uint32_t apc, atag, alen;
    BSB bsb;

    BSB_INIT(bsb, data, len);
    unsigned char *value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value || atag != 16 || alen < 16)
        return;

    BSB_INIT(bsb, value, alen);

    value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value || atag != 2 || alen != 1 || value[0] > 3)
        return;

    moloch_session_add_protocol(session, "snmp");
    moloch_parsers_register(session, snmp_parser, uw, 0);
}
/******************************************************************************/
void moloch_parser_init()
{
    for (int i = 0; i < 8; i++) {
        lens[i] = strlen(types[i]);
    }
    CLASSIFY_UDP("snmp", 0, "\x30", snmp_classify);

    versionField = moloch_field_define("snmp", "integer",
        "snmp.version", "Version", "snmp.version",
        "SNMP Version",
        MOLOCH_FIELD_TYPE_INT_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    communityField = moloch_field_define("snmp", "termfield",
        "snmp.community", "Community", "snmp.community",
        "SNMP Community",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    errorField = moloch_field_define("snmp", "integer",
        "snmp.error", "Error Code", "snmp.error",
        "SNMP Error Code",
        MOLOCH_FIELD_TYPE_INT_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    variableField = moloch_field_define("snmp", "termfield",
        "snmp.variable", "Variable", "snmp.variable",
        "SNMP Variable",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    typeField = moloch_field_define("snmp", "termfield",
        "snmp.type", "Type", "snmp.type",
        "SNMP Type",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);
}
