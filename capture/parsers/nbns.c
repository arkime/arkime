/* Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * NetBIOS Name Service (NBNS) parser - RFC 1001/1002
 */
#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL int                    nameField;
LOCAL int                    queryTypeField;
LOCAL int                    queryHostField;
LOCAL int                    hostField;
LOCAL int                    ipField;

/******************************************************************************/
// Decode first-level encoded NetBIOS name
// Each character is encoded as two bytes: 'A' + high nibble, 'A' + low nibble
// Returns decoded length, or -1 on error
LOCAL int nbns_decode_name(BSB *bsb, char *out, int outLen)
{
    uint8_t nameLen = 0;
    BSB_IMPORT_u08(*bsb, nameLen);

    if (BSB_IS_ERROR(*bsb) || nameLen != 0x20 || outLen < 16)
        return -1;

    const uint8_t *data = BSB_WORK_PTR(*bsb);
    if (BSB_REMAINING(*bsb) < 32)
        return -1;

    int outPos = 0;
    for (int i = 0; i < 30; i += 2) {  // 15 name chars; byte 16 is the suffix
        if (data[i] < 'A' || data[i] > 'P' || data[i + 1] < 'A' || data[i + 1] > 'P')
            return -1;

        char c = ((data[i] - 'A') << 4) | (data[i + 1] - 'A');
        if (c >= 0x20 && c <= 0x7e)  // Printable ASCII only
            out[outPos++] = c;
    }
    // NetBIOS names are space-padded to 15 chars; trim trailing spaces (embedded
    // spaces are legal and must be preserved).
    while (outPos > 0 && out[outPos - 1] == ' ')
        outPos--;
    out[outPos] = '\0';
    BSB_IMPORT_skip(*bsb, 32);

    // Skip scope ID (labels until null)
    while (!BSB_IS_ERROR(*bsb) && BSB_REMAINING(*bsb) > 0) {
        uint8_t scopeLen = 0;
        BSB_IMPORT_u08(*bsb, scopeLen);
        if (scopeLen == 0)
            break;
        BSB_IMPORT_skip(*bsb, scopeLen);
    }

    return outPos;
}
/******************************************************************************/
// Decode a raw 32-byte first-level-encoded NetBIOS label (no length byte) into
// out (must hold >= 16 bytes), trimming trailing space padding. Returns length.
LOCAL int nbns_decode_label(const uint8_t *data, char *out)
{
    int outPos = 0;
    for (int i = 0; i < 30; i += 2) {  // 15 name chars; byte 16 is the suffix
        if (data[i] < 'A' || data[i] > 'P' || data[i + 1] < 'A' || data[i + 1] > 'P')
            break;
        char c = ((data[i] - 'A') << 4) | (data[i + 1] - 'A');
        if (c >= 0x20 && c <= 0x7e)
            out[outPos++] = c;
    }
    while (outPos > 0 && out[outPos - 1] == ' ')
        outPos--;
    out[outPos] = '\0';
    return outPos;
}
/******************************************************************************/
LOCAL const char *nbns_query_type_str(uint16_t type)
{
    switch (type) {
    case 0x0020:
        return "NB";        // NetBIOS general name
    case 0x0021:
        return "NBSTAT";    // NetBIOS node status
    default:
        return NULL;
    }
}
/******************************************************************************/
LOCAL void nbns_parser(ArkimeSession_t *session, const uint8_t *data, int len)
{
    BSB bsb;
    BSB_INIT(bsb, data, len);

    if (len < 50)  // Minimum: 12 header + 1 len + 32 name + 1 null + 4 type/class
        return;

    arkime_session_add_protocol(session, "nbns");

    // Parse header
    uint16_t flags = 0;
    uint16_t qdCount = 0, anCount = 0, nsCount = 0, arCount = 0;

    BSB_IMPORT_skip(bsb, 2);  // transId
    BSB_IMPORT_u16(bsb, flags);
    BSB_IMPORT_u16(bsb, qdCount);
    BSB_IMPORT_u16(bsb, anCount);
    BSB_IMPORT_u16(bsb, nsCount);
    BSB_IMPORT_u16(bsb, arCount);

    if (BSB_IS_ERROR(bsb))
        return;

    int qr = (flags >> 15) & 0x1;  // 0=query, 1=response

    // Parse question section
    for (int q = 0; q < qdCount && !BSB_IS_ERROR(bsb); q++) {
        char name[16];
        int nameLen = nbns_decode_name(&bsb, name, sizeof(name));
        if (nameLen < 0)
            return;

        uint16_t qtype = 0;
        BSB_IMPORT_u16(bsb, qtype);
        BSB_IMPORT_skip(bsb, 2);  // qclass

        if (BSB_IS_ERROR(bsb))
            return;

        const char *qtypeStr = nbns_query_type_str(qtype);
        if (qtypeStr) {
            arkime_field_string_add(queryTypeField, session, qtypeStr, -1, TRUE);
        }

        if (nameLen > 0) {
            arkime_field_string_add(nameField, session, name, nameLen, TRUE);
            if (qr == 0) {
                arkime_field_string_add(queryHostField, session, name, nameLen, TRUE);
            }
        }
    }

    // Parse all resource records (answer + authority + additional all share the
    // same RR wire format and appear in that order).
    int totalAnswers = anCount + nsCount + arCount;
    for (int a = 0; a < totalAnswers && !BSB_IS_ERROR(bsb); a++) {
        char name[16];
        int nameLen = 0;

        // Check for compression pointer or full name
        uint8_t firstByte = 0;
        BSB_IMPORT_u08(bsb, firstByte);
        if (BSB_IS_ERROR(bsb))
            return;

        if ((firstByte & 0xc0) == 0xc0) {
            // Compression pointer: 14-bit offset from the start of the message.
            // Resolve it so compressed answer names populate nbns.host/name.
            uint8_t secondByte = 0;
            BSB_IMPORT_u08(bsb, secondByte);
            if (BSB_IS_ERROR(bsb))
                return;
            int offset = ((firstByte & 0x3f) << 8) | secondByte;
            // Pointed-to name is a length byte (0x20) followed by 32 encoded bytes
            if (offset + 33 <= len && data[offset] == 0x20)
                nameLen = nbns_decode_label(data + offset + 1, name);
        } else if (firstByte == 0x20) {
            // Full encoded name - decode it
            if (BSB_REMAINING(bsb) < 32)
                return;

            nameLen = nbns_decode_label(BSB_WORK_PTR(bsb), name);
            BSB_IMPORT_skip(bsb, 32);

            // Skip scope
            while (!BSB_IS_ERROR(bsb) && BSB_REMAINING(bsb) > 0) {
                uint8_t scopeLen = 0;
                BSB_IMPORT_u08(bsb, scopeLen);
                if (scopeLen == 0)
                    break;
                BSB_IMPORT_skip(bsb, scopeLen);
            }
        } else {
            return;
        }

        // Type, class, TTL, rdlength
        uint16_t rtype = 0, rdlength = 0;
        BSB_IMPORT_u16(bsb, rtype);
        BSB_IMPORT_skip(bsb, 6);  // rclass + ttl
        BSB_IMPORT_u16(bsb, rdlength);

        if (BSB_IS_ERROR(bsb) || rdlength > BSB_REMAINING(bsb))
            return;

        if (nameLen > 0) {
            arkime_field_string_add(nameField, session, name, nameLen, TRUE);
            arkime_field_string_add(hostField, session, name, nameLen, TRUE);
        }

        // Parse rdata for NB records (type 0x0020) to extract IP addresses
        if (rtype == 0x0020 && rdlength >= 6) {
            int remaining = rdlength;
            while (remaining >= 6 && !BSB_IS_ERROR(bsb)) {
                BSB_IMPORT_skip(bsb, 2);  // nbFlags

                // Read IP in network byte order
                uint32_t ipAddr = 0;
                BSB_IMPORT_byte(bsb, &ipAddr, 4);
                if (BSB_IS_ERROR(bsb))
                    break;
                remaining -= 6;

                arkime_field_ip4_add(ipField, session, ipAddr);
            }
            // Skip any leftover bytes if rdlength is not a multiple of 6
            if (remaining > 0)
                BSB_IMPORT_skip(bsb, remaining);
        } else {
            // Skip rdata for other record types
            BSB_IMPORT_skip(bsb, rdlength);
        }
    }
}
/******************************************************************************/
LOCAL int nbns_udp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    nbns_parser(session, data, len);
    return 0;
}
/******************************************************************************/
LOCAL void nbns_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "nbns"))
        return;

    // NBNS header: 12 bytes, then 0x20 (encoded name length) followed by uppercase A-P
    if (len < 50)
        return;

    // Check for encoded NetBIOS name length byte
    if (data[12] != 0x20)
        return;

    // Verify first few bytes of encoded name are valid (A-P range)
    for (int i = 13; i < 17; i++) {
        if (data[i] < 'A' || data[i] > 'P')
            return;
    }

    arkime_parsers_register(session, nbns_udp_parser, 0, 0);
}
/******************************************************************************/
void arkime_parser_init()
{
    nameField = arkime_field_define("nbns", "termfield",
                                    "nbns.name", "Name", "nbns.name",
                                    "NetBIOS names",
                                    ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                    (char *)NULL);

    queryTypeField = arkime_field_define("nbns", "termfield",
                                         "nbns.queryType", "Query Type", "nbns.queryType",
                                         "NetBIOS query types",
                                         ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                         (char *)NULL);

    queryHostField = arkime_field_define("nbns", "termfield",
                                         "nbns.queryHost", "Query Host", "nbns.queryHost",
                                         "NetBIOS hosts being queried",
                                         ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                         (char *)NULL);

    hostField = arkime_field_define("nbns", "termfield",
                                    "nbns.host", "Host", "nbns.host",
                                    "NetBIOS hosts from responses",
                                    ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                    (char *)NULL);

    ipField = arkime_field_define("nbns", "ip",
                                  "nbns.ip", "IP", "nbns.ip",
                                  "IP addresses from NetBIOS responses",
                                  ARKIME_FIELD_TYPE_IP_GHASH, ARKIME_FIELD_FLAG_CNT,
                                  (char *)NULL);

    // Register port-based classifier for UDP port 137
    arkime_parsers_classifier_register_port("nbns", NULL, 137, ARKIME_PARSERS_PORT_UDP, nbns_udp_classify);
}
