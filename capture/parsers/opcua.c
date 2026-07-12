/* opcua.c -- OPC UA Binary protocol (IEC 62541, OPC UA Connection Protocol)
 *
 * Classifies and parses OPC UA Binary traffic.
 *
 * Every OPC UA Binary message starts with an 8-byte header:
 *   bytes 0..2 : 3-byte ASCII message type
 *                  HEL = Hello, ACK = Acknowledge, ERR = Error,
 *                  RHE = ReverseHello, OPN = OpenSecureChannel,
 *                  CLO = CloseSecureChannel, MSG = Service Message
 *   byte  3    : 1-byte ASCII chunk type
 *                  F = Final, C = Chunk (intermediate), A = Abort
 *   bytes 4..7 : uint32 little-endian total message length (including header)
 *
 * From HEL we extract opcua.endpointUrl.
 * From OPN we extract opcua.securityPolicyUri and feed the SenderCertificate
 * DER bytes (when present) into the shared cert parser.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

LOCAL  int endpointUrlField;
LOCAL  int securityPolicyField;
LOCAL  uint32_t tls_process_single_certificate_func;

#define OPCUA_HEADER_LEN  8
#define OPCUA_MAX_MSG     (16 * 1024 * 1024)
#define OPCUA_HEL_PRELUDE 20    /* 5 x uint32 before EndpointUrl */
#define OPCUA_MAX_URL     4096
#define OPCUA_MAX_CERT    (16 * 1024)
#define OPCUA_MAX_MSGS    4     /* per-direction message cap */
#define OPCUA_BUF_MAX     32768 /* allow large OPN with big certs */

/******************************************************************************/
/* Read the uint32 LE msgLen from an 8-byte common header. */
LOCAL uint32_t opcua_msg_len(const uint8_t *p)
{
    BSB bsb;
    BSB_INIT(bsb, p + 4, 4);
    uint32_t msgLen = 0;
    BSB_LIMPORT_u32(bsb, msgLen);
    return msgLen;
}
/******************************************************************************/
LOCAL int opcua_is_valid_header(const uint8_t *data)
{
    /* chunk type must be F, C, or A */
    if (data[3] != 'F' && data[3] != 'C' && data[3] != 'A')
        return 0;

    /* message type must be one of the known 3-letter codes; switch on the
     * first byte first to limit the number of memcmp calls. */
    switch (data[0]) {
    case 'H':
        return memcmp(data + 1, "EL", 2) == 0;
    case 'A':
        return memcmp(data + 1, "CK", 2) == 0;
    case 'E':
        return memcmp(data + 1, "RR", 2) == 0;
    case 'R':
        return memcmp(data + 1, "HE", 2) == 0;
    case 'O':
        return memcmp(data + 1, "PN", 2) == 0;
    case 'C':
        return memcmp(data + 1, "LO", 2) == 0;
    case 'M':
        return memcmp(data + 1, "SG", 2) == 0;
    default:
        return 0;
    }
}
/******************************************************************************/
/* Parse a complete HEL message starting at p (length msgLen). */
LOCAL void opcua_parse_hel(ArkimeSession_t *session, const uint8_t *p, uint32_t msgLen)
{
    if (msgLen < OPCUA_HEADER_LEN + OPCUA_HEL_PRELUDE + 4)
        return;

    BSB bsb;
    BSB_INIT(bsb, p + OPCUA_HEADER_LEN + OPCUA_HEL_PRELUDE, msgLen - OPCUA_HEADER_LEN - OPCUA_HEL_PRELUDE);

    int32_t urlLen = 0;
    BSB_LIMPORT_u32(bsb, urlLen);
    if (BSB_IS_ERROR(bsb))
        return;
    if (urlLen <= 0 || urlLen > OPCUA_MAX_URL || (uint32_t)urlLen > BSB_REMAINING(bsb))
        return;
    arkime_field_string_add(endpointUrlField, session, (const char *)BSB_WORK_PTR(bsb), urlLen, TRUE);
}
/******************************************************************************/
/* Parse a complete OPN message starting at p (length msgLen). The OPN body
 * (after the 8-byte common header) begins with:
 *   SecureChannelId          uint32
 *   SecurityPolicyUri        ByteString (int32 LE length + UTF-8 bytes)
 *   SenderCertificate        ByteString (int32 LE length + DER bytes)
 *   ReceiverCertThumbprint   ByteString
 * Length -1 (0xFFFFFFFF) means null/absent. */
LOCAL void opcua_parse_opn(ArkimeSession_t *session, const uint8_t *p, uint32_t msgLen)
{
    if (msgLen < OPCUA_HEADER_LEN + 4 + 4)
        return;

    BSB bsb;
    BSB_INIT(bsb, p + OPCUA_HEADER_LEN, msgLen - OPCUA_HEADER_LEN);
    BSB_IMPORT_skip(bsb, 4);                    /* SecureChannelId */

    int32_t sLen = 0;
    BSB_LIMPORT_u32(bsb, sLen);
    if (BSB_IS_ERROR(bsb))
        return;

    if (sLen > 0 && sLen <= OPCUA_MAX_URL && (uint32_t)sLen <= BSB_REMAINING(bsb)) {
        arkime_field_string_add(securityPolicyField, session,
                                (const char *)BSB_WORK_PTR(bsb), sLen, TRUE);
        BSB_IMPORT_skip(bsb, sLen);
    } else if (sLen > 0) {
        /* length set but exceeds our cap or remaining bytes - stream is suspect */
        return;
    }
    /* sLen == 0 (empty) or sLen == -1 (null) both fall through to SenderCertificate */

    int32_t cLen = 0;
    BSB_LIMPORT_u32(bsb, cLen);
    if (BSB_IS_ERROR(bsb))
        return;

    if (cLen > 0 && cLen <= OPCUA_MAX_CERT && (uint32_t)cLen <= BSB_REMAINING(bsb)) {
        arkime_parsers_call_named_func(tls_process_single_certificate_func,
                                       session, BSB_WORK_PTR(bsb), cLen, NULL);
    }
}
/******************************************************************************/
LOCAL int opcua_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int which)
{
    ArkimeParserBuf_t *info = uw;

    if (arkime_parser_buf_add(info, which, data, len) < 0)
        return ARKIME_PARSER_UNREGISTER;

    while (info->len[which] >= OPCUA_HEADER_LEN) {
        const uint8_t *p = info->buf[which];

        if (!opcua_is_valid_header(p))
            return ARKIME_PARSER_UNREGISTER;

        uint32_t msgLen = opcua_msg_len(p);
        if (msgLen < OPCUA_HEADER_LEN || msgLen > OPCUA_MAX_MSG)
            return ARKIME_PARSER_UNREGISTER;
        if (msgLen > (uint32_t)info->bufMax)
            return ARKIME_PARSER_UNREGISTER;   /* won't ever fit; stop trying */
        if ((uint32_t)info->len[which] < msgLen)
            return 0;                          /* wait for more data */

        if (p[0] == 'H' && p[3] == 'F')
            opcua_parse_hel(session, p, msgLen);
        else if (p[0] == 'O' && p[3] == 'F')
            opcua_parse_opn(session, p, msgLen);

        arkime_parser_buf_del(info, which, msgLen);
        info->state[which]++;

        if (info->state[which] >= OPCUA_MAX_MSGS)
            return ARKIME_PARSER_UNREGISTER;
    }

    return 0;
}
/******************************************************************************/
LOCAL void opcua_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < OPCUA_HEADER_LEN || !opcua_is_valid_header(data))
        return;

    uint32_t msgLen = opcua_msg_len(data);
    if (msgLen < OPCUA_HEADER_LEN || msgLen > OPCUA_MAX_MSG)
        return;

    if (arkime_session_has_protocol(session, "opcua"))
        return;

    arkime_session_add_protocol(session, "opcua");

    ArkimeParserBuf_t *info = arkime_parser_buf_create2(1024, OPCUA_BUF_MAX);
    arkime_parsers_register(session, opcua_parser, info, arkime_parser_buf_session_free);
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_parsers_classifier_register_tcp("opcua", NULL, 0, (const uint8_t *)"HELF", 4, opcua_classify);

    endpointUrlField = arkime_field_define("opcua", "termfield",
                                           "opcua.endpointUrl", "OPC UA Endpoint URL", "opcua.endpointUrl",
                                           "OPC UA endpoint URL from the Hello message",
                                           ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                           (char *)NULL);

    securityPolicyField = arkime_field_define("opcua", "termfield",
                                              "opcua.securityPolicyUri", "OPC UA Security Policy", "opcua.securityPolicyUri",
                                              "OPC UA SecurityPolicyUri from the OpenSecureChannel message",
                                              ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                              (char *)NULL);

    tls_process_single_certificate_func = arkime_parsers_get_named_func("tls_process_single_certificate");
}

