/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

/*
 * IKE (Internet Key Exchange) - RFC 2409 (IKEv1), RFC 7296 (IKEv2)
 *
 * ISAKMP Header (28 bytes):
 *   Bytes 0-7:   Initiator SPI
 *   Bytes 8-15:  Responder SPI
 *   Byte 16:     Next Payload
 *   Byte 17:     Version (major 4 bits | minor 4 bits)
 *   Byte 18:     Exchange Type
 *   Byte 19:     Flags
 *   Bytes 20-23: Message ID
 *   Bytes 24-27: Length
 *
 * Payload Header (4 bytes):
 *   Byte 0:      Next Payload
 *   Byte 1:      Critical bit (IKEv2) / Reserved (IKEv1)
 *   Bytes 2-3:   Payload Length
 */

extern ArkimeConfig_t        config;

LOCAL int initiatorSpiField;
LOCAL int responderSpiField;
LOCAL int versionField;
LOCAL int exchangeTypeField;
LOCAL int vendorIdField;
LOCAL int encryptionField;
LOCAL int hashField;
LOCAL int dhGroupField;
LOCAL int authMethodField;

LOCAL const char *ikev1ExchangeTypes[] = {
    [0] = "none",
    [1] = "base",
    [2] = "identity-protection",
    [3] = "authentication-only",
    [4] = "aggressive",
    [5] = "informational",
    [32] = "quick-mode",
    [33] = "new-group-mode"
};

LOCAL const char *ikev2ExchangeTypes[] = {
    [34] = "ike-sa-init",
    [35] = "ike-auth",
    [36] = "create-child-sa",
    [37] = "informational"
};

LOCAL const char *encryptionAlgorithms[] = {
    [1] = "des-cbc",
    [2] = "idea-cbc",
    [3] = "blowfish-cbc",
    [4] = "rc5-r16-b64-cbc",
    [5] = "3des-cbc",
    [6] = "cast-cbc",
    [7] = "aes-cbc",
    [8] = "camellia-cbc",
    [12] = "aes-ctr",
    [13] = "aes-ccm-8",
    [14] = "aes-ccm-12",
    [15] = "aes-ccm-16",
    [18] = "aes-gcm-8",
    [19] = "aes-gcm-12",
    [20] = "aes-gcm-16",
    [23] = "chacha20-poly1305"
};

LOCAL const char *hashAlgorithms[] = {
    [1] = "md5",
    [2] = "sha1",
    [3] = "tiger",
    [4] = "sha2-256",
    [5] = "sha2-384",
    [6] = "sha2-512"
};

LOCAL const char *prfAlgorithms[] = {
    [1] = "prf-hmac-md5",
    [2] = "prf-hmac-sha1",
    [3] = "prf-hmac-tiger",
    [4] = "prf-aes128-xcbc",
    [5] = "prf-hmac-sha2-256",
    [6] = "prf-hmac-sha2-384",
    [7] = "prf-hmac-sha2-512"
};

LOCAL const char *dhGroups[] = {
    [1] = "modp768",
    [2] = "modp1024",
    [5] = "modp1536",
    [14] = "modp2048",
    [15] = "modp3072",
    [16] = "modp4096",
    [17] = "modp6144",
    [18] = "modp8192",
    [19] = "ecp256",
    [20] = "ecp384",
    [21] = "ecp521",
    [22] = "modp1024s160",
    [23] = "modp2048s224",
    [24] = "modp2048s256",
    [25] = "ecp192",
    [26] = "ecp224",
    [27] = "brainpoolp224",
    [28] = "brainpoolp256",
    [29] = "brainpoolp384",
    [30] = "brainpoolp512",
    [31] = "curve25519",
    [32] = "curve448"
};

#define PAYLOAD_SA          1
#define PAYLOAD_VENDOR_ID   13
#define PAYLOAD_SA_V2       33
#define PAYLOAD_VENDOR_ID_V2 43

typedef struct {
    const uint8_t *pattern;
    int            len;
    const char    *name;
} VendorID;

LOCAL VendorID knownVendors[] = {
    {(const uint8_t *)"\x4a\x13\x1c\x81\x07\x03\x58\x45", 8, "rfc3947-nat-t"},
    {(const uint8_t *)"\x90\xcb\x80\x91\x3e\xbb\x69\x6e", 8, "draft-ietf-nat-t-02"},
    {(const uint8_t *)"\xcd\x60\x46\x43\x35\xdf\x21\xf8", 8, "draft-ietf-nat-t-03"},
    {(const uint8_t *)"\x7d\x94\x19\xa6\x53\x10\xca\x6f", 8, "draft-ietf-nat-t-rfc"},
    {(const uint8_t *)"\xaf\xca\xd7\x13\x68\xa1\xf1\xc9", 8, "dpd"},
    {(const uint8_t *)"\x12\xf5\xf2\x8c\x45\x71\x68\xa9", 8, "cisco-unity"},
    {(const uint8_t *)"\x09\x00\x26\x89\xdf\xd6\xb7\x12", 8, "xauth"},
    {(const uint8_t *)"\x1f\x07\xf7\x0e\xaa\x65\x14\xd3", 8, "cisco-concentrator"},
    {(const uint8_t *)"\x40\x48\xb7\xd5\x6e\xbc\xe8\x85", 8, "ikev2"},
    {(const uint8_t *)"\x4d\x53\x2d\x4d\x61\x6d\x69\x65", 8, "ms-ikev2"},
    {(const uint8_t *)"\x1e\x2b\x51\x69\x05\x99\x1c\x7d", 8, "windows"},
    {(const uint8_t *)"\x4f\x45\x2e\x48\x4a\x52\x41\x4e", 8, "fortigate"},
    {(const uint8_t *)"\x16\x6f\x93\x2d\x55\xeb\x64\xd8", 8, "strongswan"},
    {(const uint8_t *)"\x69\x93\x69\x22\x87\x41\xc6\xd4", 8, "openswan"},
    {(const uint8_t *)"\x4f\x50\x45\x4e\x53\x77\x61\x6e", 8, "openswan2"},
    {(const uint8_t *)"\xfb\xf4\x76\x14\x98\x40\x31\xfa", 8, "checkpoint"},
    {(const uint8_t *)"\xf4\xed\x19\xe0\xc1\x14\xeb\x51", 8, "checkpoint-ng"},
    {NULL, 0, NULL}
};

/******************************************************************************/
LOCAL const char *ike_lookup_vendor(const uint8_t *data, int len)
{
    for (int i = 0; knownVendors[i].pattern != NULL; i++) {
        if (len >= knownVendors[i].len &&
            memcmp(data, knownVendors[i].pattern, knownVendors[i].len) == 0) {
            return knownVendors[i].name;
        }
    }
    return NULL;
}

/******************************************************************************/
LOCAL void ike_parse_transform_v1(ArkimeSession_t *session, BSB *bsb)
{
    // IKEv1 Transform: Transform# (1) | TransformID (1) | Reserved (2) | Attributes...
    if (BSB_REMAINING(*bsb) < 4)
        return;

    BSB_IMPORT_skip(*bsb, 4);

    while (BSB_REMAINING(*bsb) >= 4 && !BSB_IS_ERROR(*bsb)) {
        uint16_t attrType = 0;
        BSB_IMPORT_u16(*bsb, attrType);

        int af = (attrType >> 15) & 0x01;
        int type = attrType & 0x7fff;

        if (af == 1) {
            // TV format - 2 byte value
            uint16_t value = 0;
            BSB_IMPORT_u16(*bsb, value);

            switch (type) {
            case 1: // Encryption
                if (value < ARRAY_LEN(encryptionAlgorithms) && encryptionAlgorithms[value])
                    arkime_field_string_add(encryptionField, session, encryptionAlgorithms[value], -1, TRUE);
                break;
            case 2: // Hash
                if (value < ARRAY_LEN(hashAlgorithms) && hashAlgorithms[value])
                    arkime_field_string_add(hashField, session, hashAlgorithms[value], -1, TRUE);
                break;
            case 3: // Auth Method
                switch (value) {
                case 1: arkime_field_string_add(authMethodField, session, "psk", -1, TRUE); break;
                case 2: arkime_field_string_add(authMethodField, session, "dss-sig", -1, TRUE); break;
                case 3: arkime_field_string_add(authMethodField, session, "rsa-sig", -1, TRUE); break;
                case 4:
                case 5: arkime_field_string_add(authMethodField, session, "rsa-enc", -1, TRUE); break;
                case 64221: arkime_field_string_add(authMethodField, session, "hybrid-rsa", -1, TRUE); break;
                case 65001: arkime_field_string_add(authMethodField, session, "xauth-psk", -1, TRUE); break;
                case 65005: arkime_field_string_add(authMethodField, session, "xauth-rsa", -1, TRUE); break;
                }
                break;
            case 4: // DH Group
                if (value < ARRAY_LEN(dhGroups) && dhGroups[value])
                    arkime_field_string_add(dhGroupField, session, dhGroups[value], -1, TRUE);
                break;
            }
        } else {
            // TLV format
            uint16_t attrLen = 0;
            BSB_IMPORT_u16(*bsb, attrLen);
            BSB_IMPORT_skip(*bsb, attrLen);
        }
    }
}

/******************************************************************************/
LOCAL void ike_parse_transform_v2(ArkimeSession_t *session, BSB *bsb)
{
    // IKEv2 Transform: Last (1) | Reserved (1) | Length (2) | Type (1) | Reserved (1) | ID (2)
    if (BSB_REMAINING(*bsb) < 8)
        return;

    uint8_t transformType = 0;
    uint16_t transformId = 0;

    BSB_IMPORT_skip(*bsb, 4);  // last, reserved, length
    BSB_IMPORT_u08(*bsb, transformType);
    BSB_IMPORT_skip(*bsb, 1);
    BSB_IMPORT_u16(*bsb, transformId);

    switch (transformType) {
    case 1: // ENCR
        if (transformId < ARRAY_LEN(encryptionAlgorithms) && encryptionAlgorithms[transformId])
            arkime_field_string_add(encryptionField, session, encryptionAlgorithms[transformId], -1, TRUE);
        break;
    case 2: // PRF
        if (transformId < ARRAY_LEN(prfAlgorithms) && prfAlgorithms[transformId])
            arkime_field_string_add(hashField, session, prfAlgorithms[transformId], -1, TRUE);
        break;
    case 4: // DH
        if (transformId < ARRAY_LEN(dhGroups) && dhGroups[transformId])
            arkime_field_string_add(dhGroupField, session, dhGroups[transformId], -1, TRUE);
        break;
    }
}

/******************************************************************************/
LOCAL void ike_parse_proposal_v1(ArkimeSession_t *session, BSB *bsb)
{
    // Proposal: Proposal# (1) | ProtocolID (1) | SPISize (1) | NumTransforms (1)
    if (BSB_REMAINING(*bsb) < 4)
        return;

    uint8_t spiSize = 0;
    uint8_t numTransforms = 0;

    BSB_IMPORT_skip(*bsb, 2);
    BSB_IMPORT_u08(*bsb, spiSize);
    BSB_IMPORT_u08(*bsb, numTransforms);

    if (spiSize > 0)
        BSB_IMPORT_skip(*bsb, spiSize);

    for (int i = 0; i < numTransforms && BSB_REMAINING(*bsb) >= 4 && !BSB_IS_ERROR(*bsb); i++) {
        uint16_t transformLen = 0;

        BSB_IMPORT_skip(*bsb, 2);  // next, reserved
        BSB_IMPORT_u16(*bsb, transformLen);

        if (transformLen < 4 || transformLen > BSB_REMAINING(*bsb) + 4)
            break;

        BSB tbsb;
        BSB_IMPORT_bsb(*bsb, tbsb, transformLen - 4);
        ike_parse_transform_v1(session, &tbsb);
    }
}

/******************************************************************************/
LOCAL void ike_parse_proposal_v2(ArkimeSession_t *session, BSB *bsb)
{
    // IKEv2 Proposal: Last (1) | Reserved (1) | Length (2) | Proposal# (1) | ProtocolID (1) | SPISize (1) | NumTransforms (1)
    if (BSB_REMAINING(*bsb) < 8)
        return;

    uint8_t spiSize = 0;
    uint8_t numTransforms = 0;

    BSB_IMPORT_skip(*bsb, 6);
    BSB_IMPORT_u08(*bsb, spiSize);
    BSB_IMPORT_u08(*bsb, numTransforms);

    if (spiSize > 0)
        BSB_IMPORT_skip(*bsb, spiSize);

    for (int i = 0; i < numTransforms && BSB_REMAINING(*bsb) >= 8 && !BSB_IS_ERROR(*bsb); i++) {
        uint16_t transformLen = 0;

        BSB_IMPORT_skip(*bsb, 2);  // last, reserved
        BSB_IMPORT_u16(*bsb, transformLen);

        if (transformLen < 8 || transformLen > BSB_REMAINING(*bsb) + 4)
            break;

        BSB tbsb;
        BSB_IMPORT_bsb(*bsb, tbsb, transformLen - 4);
        ike_parse_transform_v2(session, &tbsb);
    }
}

/******************************************************************************/
LOCAL void ike_parse_sa_v1(ArkimeSession_t *session, BSB *bsb)
{
    // SA Payload: DOI (4) | Situation (4) | Proposals...
    if (BSB_REMAINING(*bsb) < 8)
        return;

    uint32_t doi = 0;
    BSB_IMPORT_u32(*bsb, doi);
    BSB_IMPORT_skip(*bsb, 4);  // situation

    if (doi != 1)  // IPSEC DOI
        return;

    while (BSB_REMAINING(*bsb) >= 4 && !BSB_IS_ERROR(*bsb)) {
        uint8_t nextPayload = 0;
        uint16_t proposalLen = 0;

        BSB_IMPORT_u08(*bsb, nextPayload);
        BSB_IMPORT_skip(*bsb, 1);
        BSB_IMPORT_u16(*bsb, proposalLen);

        if (proposalLen < 4 || proposalLen > BSB_REMAINING(*bsb) + 4)
            break;

        BSB pbsb;
        BSB_IMPORT_bsb(*bsb, pbsb, proposalLen - 4);
        ike_parse_proposal_v1(session, &pbsb);

        if (nextPayload == 0)
            break;
    }
}

/******************************************************************************/
LOCAL void ike_parse_sa_v2(ArkimeSession_t *session, BSB *bsb)
{
    while (BSB_REMAINING(*bsb) >= 8 && !BSB_IS_ERROR(*bsb)) {
        uint8_t lastSubstruc = 0;
        uint16_t proposalLen = 0;

        BSB_IMPORT_u08(*bsb, lastSubstruc);
        BSB_IMPORT_skip(*bsb, 1);
        BSB_IMPORT_u16(*bsb, proposalLen);

        if (proposalLen < 8 || proposalLen > BSB_REMAINING(*bsb) + 4)
            break;

        BSB pbsb;
        BSB_IMPORT_bsb(*bsb, pbsb, proposalLen - 4);
        ike_parse_proposal_v2(session, &pbsb);

        if (lastSubstruc == 0)
            break;
    }
}

/******************************************************************************/
LOCAL int ike_udp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    BSB bsb;
    int offset = 0;

    // NAT-T on port 4500 - skip 4-byte non-ESP marker
    if ((session->port1 == 4500 || session->port2 == 4500) && len >= 4) {
        if (data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 0)
            offset = 4;
    }

    BSB_INIT(bsb, data + offset, len - offset);

    if (BSB_REMAINING(bsb) < 28)
        return 0;

    // ISAKMP header
    const uint8_t *initiatorSpi = BSB_WORK_PTR(bsb);
    BSB_IMPORT_skip(bsb, 8);

    const uint8_t *responderSpi = BSB_WORK_PTR(bsb);
    BSB_IMPORT_skip(bsb, 8);

    uint8_t nextPayload = 0;
    uint8_t version = 0;
    uint8_t exchangeType = 0;
    uint8_t flags = 0;

    BSB_IMPORT_u08(bsb, nextPayload);
    BSB_IMPORT_u08(bsb, version);
    BSB_IMPORT_u08(bsb, exchangeType);
    BSB_IMPORT_u08(bsb, flags);
    BSB_IMPORT_skip(bsb, 8);  // message ID + length

    if (BSB_IS_ERROR(bsb))
        return 0;

    int majorVersion = (version >> 4) & 0x0f;
    int isV2 = (majorVersion == 2);

    // Initiator SPI
    char spiStr[20];
    arkime_sprint_hex_string(spiStr, initiatorSpi, 8);
    arkime_field_string_add(initiatorSpiField, session, spiStr, 16, TRUE);

    // Responder SPI (if not zero)
    int hasResponder = 0;
    for (int i = 0; i < 8; i++) {
        if (responderSpi[i] != 0) { hasResponder = 1; break; }
    }
    if (hasResponder) {
        arkime_sprint_hex_string(spiStr, responderSpi, 8);
        arkime_field_string_add(responderSpiField, session, spiStr, 16, TRUE);
    }

    // Version (major.minor)
    int minorVersion = version & 0x0f;
    char versionStr[8];
    snprintf(versionStr, sizeof(versionStr), "%d.%d", majorVersion, minorVersion);
    arkime_field_string_add(versionField, session, versionStr, -1, TRUE);

    // Exchange type
    const char *exchangeStr = NULL;
    if (isV2) {
        if (exchangeType >= 34 && exchangeType <= 37)
            exchangeStr = ikev2ExchangeTypes[exchangeType];
    } else {
        if (exchangeType <= 5)
            exchangeStr = ikev1ExchangeTypes[exchangeType];
        else if (exchangeType == 32)
            exchangeStr = "quick-mode";
        else if (exchangeType == 33)
            exchangeStr = "new-group-mode";
    }
    if (exchangeStr)
        arkime_field_string_add(exchangeTypeField, session, exchangeStr, -1, TRUE);

    // Check if encrypted
    int encrypted = isV2 ? (flags & 0x08) : (flags & 0x01);
    if (encrypted)
        return 0;

    // Parse payloads
    while (nextPayload != 0 && BSB_REMAINING(bsb) >= 4 && !BSB_IS_ERROR(bsb)) {
        uint8_t currentPayload = nextPayload;
        uint16_t payloadLen = 0;

        BSB_IMPORT_u08(bsb, nextPayload);
        BSB_IMPORT_skip(bsb, 1);
        BSB_IMPORT_u16(bsb, payloadLen);

        if (payloadLen < 4 || payloadLen > BSB_REMAINING(bsb) + 4)
            break;

        BSB pbsb;
        BSB_IMPORT_bsb(bsb, pbsb, payloadLen - 4);

        switch (currentPayload) {
        case PAYLOAD_SA:
            if (!isV2)
                ike_parse_sa_v1(session, &pbsb);
            break;

        case PAYLOAD_SA_V2:
            if (isV2)
                ike_parse_sa_v2(session, &pbsb);
            break;

        case PAYLOAD_VENDOR_ID:
        case PAYLOAD_VENDOR_ID_V2:
            if (BSB_REMAINING(pbsb) > 0) {
                const uint8_t *vendorData = BSB_WORK_PTR(pbsb);
                int vendorLen = BSB_REMAINING(pbsb);
                const char *vendorName = ike_lookup_vendor(vendorData, vendorLen);
                if (vendorName) {
                    arkime_field_string_add(vendorIdField, session, vendorName, -1, TRUE);
                } else {
                    int hexLen = (vendorLen > 16) ? 16 : vendorLen;
                    char hexStr[33];
                    arkime_sprint_hex_string(hexStr, vendorData, hexLen);
                    arkime_field_string_add(vendorIdField, session, hexStr, hexLen * 2, TRUE);
                }
            }
            break;
        }
    }

    return 0;
}

/******************************************************************************/
LOCAL void ike_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    int offset = 0;

    // NAT-T on port 4500
    if ((session->port1 == 4500 || session->port2 == 4500) && len >= 4) {
        if (data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 0)
            offset = 4;
        else
            return;
    }

    if (len - offset < 28)
        return;

    data += offset;
    len -= offset;

    uint8_t version = data[17];
    int majorVersion = (version >> 4) & 0x0f;
    if (majorVersion != 1 && majorVersion != 2)
        return;

    uint8_t exchangeType = data[18];
    if (majorVersion == 1) {
        if (exchangeType > 5 && exchangeType < 32)
            return;
        if (exchangeType > 33 && exchangeType < 240)
            return;
    } else {
        if (exchangeType < 34 || exchangeType > 37)
            return;
    }

    uint8_t flags = data[19];
    if (majorVersion == 1) {
        if (flags & 0xf8)
            return;
    } else {
        if (flags & 0xc7)
            return;
    }

    arkime_session_add_protocol(session, "isakmp");
    arkime_parsers_register(session, ike_udp_parser, 0, 0);
}

/******************************************************************************/
void arkime_parser_init()
{
    initiatorSpiField = arkime_field_define("isakmp", "termfield",
        "isakmp.initiator-spi", "Initiator SPI", "isakmp.initiatorSpi",
        "ISAKMP initiator security parameter index",
        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT, (char *)NULL);

    responderSpiField = arkime_field_define("isakmp", "termfield",
        "isakmp.responder-spi", "Responder SPI", "isakmp.responderSpi",
        "ISAKMP responder security parameter index",
        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT, (char *)NULL);

    versionField = arkime_field_define("isakmp", "termfield",
        "isakmp.version", "Version", "isakmp.version",
        "ISAKMP version",
        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT, (char *)NULL);

    exchangeTypeField = arkime_field_define("isakmp", "termfield",
        "isakmp.exchange-type", "Exchange Type", "isakmp.exchangeType",
        "ISAKMP exchange type",
        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT, (char *)NULL);

    vendorIdField = arkime_field_define("isakmp", "termfield",
        "isakmp.vendor-id", "Vendor ID", "isakmp.vendorId",
        "ISAKMP vendor identifier",
        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT, (char *)NULL);

    encryptionField = arkime_field_define("isakmp", "termfield",
        "isakmp.encryption", "Encryption", "isakmp.encryption",
        "ISAKMP encryption algorithm",
        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT, (char *)NULL);

    hashField = arkime_field_define("isakmp", "termfield",
        "isakmp.hash", "Hash/PRF", "isakmp.hash",
        "ISAKMP hash or PRF algorithm",
        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT, (char *)NULL);

    dhGroupField = arkime_field_define("isakmp", "termfield",
        "isakmp.dh-group", "DH Group", "isakmp.dhGroup",
        "ISAKMP Diffie-Hellman group",
        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT, (char *)NULL);

    authMethodField = arkime_field_define("isakmp", "termfield",
        "isakmp.auth-method", "Auth Method", "isakmp.authMethod",
        "ISAKMP authentication method",
        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT, (char *)NULL);

    arkime_parsers_classifier_register_port("isakmp", NULL, 500, ARKIME_PARSERS_PORT_UDP, ike_udp_classify);
    arkime_parsers_classifier_register_port("isakmp", NULL, 4500, ARKIME_PARSERS_PORT_UDP, ike_udp_classify);
}
