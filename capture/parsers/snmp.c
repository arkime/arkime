/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL  int                   versionField;
LOCAL  int                   communityField;
LOCAL  int                   userField;
LOCAL  int                   errorField;
LOCAL  int                   variableField;
LOCAL  int                   typeField;
LOCAL  int                   trapOidField;

LOCAL  char                 *types[8] = {"GetRequest", "GetNextRequest", "GetResponse", "SetRequest", "Trap", "GetBulkRequest", "InformRequest", "SNMPv2-Trap"};
LOCAL  int                   lens[8];

// SNMP error codes (RFC 3416)
LOCAL const char *snmp_error_names[] = {
    [0] = "noError",
    [1] = "tooBig",
    [2] = "noSuchName",
    [3] = "badValue",
    [4] = "readOnly",
    [5] = "genErr",
    [6] = "noAccess",
    [7] = "wrongType",
    [8] = "wrongLength",
    [9] = "wrongEncoding",
    [10] = "wrongValue",
    [11] = "noCreation",
    [12] = "inconsistentValue",
    [13] = "resourceUnavailable",
    [14] = "commitFailed",
    [15] = "undoFailed",
    [16] = "authorizationError",
    [17] = "notWritable",
    [18] = "inconsistentName"
};

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

    // Version: wire value 0=v1, 1=v2c, 3=v3
    if (value[0] == 3) {
        version = 3;
    } else {
        version = value[0] + 1;  // v1=1, v2c=2
    }
    arkime_field_int_add(versionField, session, version);

    // SNMPv3 has different structure
    if (version == 3) {
        // msgGlobalData (SEQUENCE)
        value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);
        if (!value || atag != 16)
            return 0;

        // Skip to msgSecurityParameters (OCTET STRING containing USM)
        value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);
        if (!value || atag != 4 || alen < 2)
            return 0;

        // Parse USM security parameters
        BSB usm;
        BSB_INIT(usm, value, alen);
        value = arkime_parsers_asn_get_tlv(&usm, &apc, &atag, &alen);
        if (!value || atag != 16)
            return 0;

        BSB_INIT(usm, value, alen);

        // msgAuthoritativeEngineID (OCTET STRING)
        value = arkime_parsers_asn_get_tlv(&usm, &apc, &atag, &alen);
        if (!value)
            return 0;

        // msgAuthoritativeEngineBoots (INTEGER)
        value = arkime_parsers_asn_get_tlv(&usm, &apc, &atag, &alen);
        if (!value)
            return 0;

        // msgAuthoritativeEngineTime (INTEGER)
        value = arkime_parsers_asn_get_tlv(&usm, &apc, &atag, &alen);
        if (!value)
            return 0;

        // msgUserName (OCTET STRING) - the security-relevant field!
        value = arkime_parsers_asn_get_tlv(&usm, &apc, &atag, &alen);
        if (value && atag == 4 && alen > 0) {
            arkime_field_string_add(userField, session, (char *)value, alen, TRUE);
        }

        return 0;  // Don't parse encrypted PDU
    }

    // SNMPv1/v2c: Community string
    value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value || apc != 0 || atag != 4 || alen == 0)
        return ARKIME_PARSER_UNREGISTER;

    arkime_field_string_add(communityField, session, (char *)value, alen, TRUE);

    value = arkime_parsers_asn_get_tlv(&bsb, &apc, &dataType, &alen);

    if (value && dataType < ARRAY_LEN(types)) {
        arkime_field_string_add(typeField, session, types[dataType], lens[dataType], TRUE);
    } else {
        // This is probably not a SNMP stream after all
        return ARKIME_PARSER_UNREGISTER;
    }

    if (!apc || !value || !alen)
        return 0;

    BSB_INIT(bsb, value, alen);

    // SNMPv1 Trap (dataType == 4) has different format
    if (dataType == 4) {
        // Enterprise OID
        value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);
        if (value && atag == 6 && alen > 0) {
            char oid[100];
            arkime_parsers_asn_decode_oid(oid, sizeof(oid), value, alen);
            arkime_field_string_add(trapOidField, session, oid, -1, TRUE);
        }
        return 0;
    }

    // GetBulkRequest (dataType == 5) has non-repeaters/max-repetitions instead of error
    if (dataType == 5) {
        // Skip non-repeaters and max-repetitions
        value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);
        if (!value)
            return 0;
        value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);
        if (!value)
            return 0;
    } else {
        // Request Id
        value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);
        if (!value)
            return 0;

        // Error Status
        value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);
        if (!value)
            return 0;

        if (alen >= 1 && value[0] > 0 && value[0] < ARRAY_LEN(snmp_error_names) && snmp_error_names[value[0]]) {
            arkime_field_string_add(errorField, session, snmp_error_names[value[0]], -1, TRUE);
        }

        // Error Index
        value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);
        if (!value)
            return 0;
    }

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

        // Add both the raw OID and a friendly name if available
        arkime_field_string_add(variableField, session, oid, -1, TRUE);

        // Check for SNMPv2 Trap OID (1.3.6.1.6.3.1.1.4.1.0)
        if (dataType == 7 && strcmp(oid, "1.3.6.1.6.3.1.1.4.1.0") == 0) {
            // Next value is the trap OID
            value = arkime_parsers_asn_get_tlv(&obsb, &apc, &atag, &alen);
            if (value && atag == 6 && alen > 0) {
                char trapOid[100];
                arkime_parsers_asn_decode_oid(trapOid, sizeof(trapOid), value, alen);
                arkime_field_string_add(trapOidField, session, trapOid, -1, TRUE);
            }
        }
    }

    return 0;
}
/******************************************************************************/
LOCAL void snmp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "snmp"))
        return;

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
    arkime_parsers_register(session, snmp_parser, NULL, 0);
}
/******************************************************************************/
void arkime_parser_init()
{
    for (int i = 0; i < ARRAY_LEN(types); i++) {
        lens[i] = strlen(types[i]);
    }
    CLASSIFY_UDP("snmp", 0, "\x30", snmp_classify);

    userField = arkime_field_by_db("user");

    versionField = arkime_field_define("snmp", "integer",
                                       "snmp.version", "Version", "snmp.version",
                                       "SNMP Version",
                                       ARKIME_FIELD_TYPE_INT_HASH,  ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    communityField = arkime_field_define("snmp", "termfield",
                                         "snmp.community", "Community", "snmp.community",
                                         "SNMP Community String",
                                         ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                         (char *)NULL);

    errorField = arkime_field_define("snmp", "termfield",
                                     "snmp.error", "Error", "snmp.error",
                                     "SNMP Error",
                                     ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                     (char *)NULL);

    variableField = arkime_field_define("snmp", "termfield",
                                        "snmp.variable", "Variable", "snmp.variable",
                                        "SNMP Variable OID",
                                        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);

    typeField = arkime_field_define("snmp", "termfield",
                                    "snmp.type", "Type", "snmp.type",
                                    "SNMP PDU Type",
                                    ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                    (char *)NULL);

    trapOidField = arkime_field_define("snmp", "termfield",
                                       "snmp.trap-oid", "Trap OID", "snmp.trapOid",
                                       "SNMP Trap OID",
                                       ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);
}
