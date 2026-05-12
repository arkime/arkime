/* Copyright 2026 Andy Wick All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * openvpn.c -- OpenVPN classifier and parser
 *
 * Detection is based on the OpenVPN HARD_RESET_CLIENT_V2 control packet, which
 * has a very distinctive byte layout. The byte patterns, the set of valid
 * `tls-auth` HMAC sizes (0 / 16 / 20 / 32 / 64) and the overall approach of
 * stripping the OpenVPN framing and forwarding the inner SSL/TLS stream to the
 * TLS analyzer are derived from corelight's Zeek/Spicy OpenVPN analyzer:
 *
 *   https://github.com/corelight/zeek-spicy-openvpn
 *
 * Layout of a HARD_RESET_CLIENT_V2 record:
 *
 *   byte 0       = 0x38   (opcode P_CONTROL_HARD_RESET_CLIENT_V2 << 3, key_id 0)
 *   bytes 1..8   = session_id (8 bytes, arbitrary)
 *   [if hmac]    = hmac(N) + packet_id(4) + net_time(4)   where N in {16,20,32,64}
 *   1 byte       = packet_id_array_len = 0
 *   4 bytes      = packet_id           = 0
 *
 * So a valid HARD_RESET_CLIENT_V2 has total length 14 / 38 / 42 / 54 / 86
 * (no auth / HMAC-MD5 / HMAC-SHA1 / HMAC-SHA256 / HMAC-SHA512) with the last
 * five bytes always zero.
 *
 * For UDP each datagram carries one OpenVPN record. For TCP each record is
 * prefixed with a 2-byte big-endian length.
 *
 * Once classified, this parser walks subsequent OpenVPN records, extracts the
 * SSL/TLS stream carried inside P_CONTROL_V1 records and forwards individual
 * TLS handshake messages (ClientHello / ServerHello / Certificate) to the TLS
 * parser via its named functions, giving us JA3/JA4, SNI and certificates for
 * OpenVPN connections regardless of port.
 */

#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL uint32_t tls_process_client_hello_func;
LOCAL uint32_t tls_process_server_hello_func;
LOCAL uint32_t tls_process_server_certificate_func;

#define OPENVPN_OPCODE_P_CONTROL_HARD_RESET_CLIENT_V2 7
#define OPENVPN_OPCODE_P_CONTROL_HARD_RESET_SERVER_V2 8
#define OPENVPN_OPCODE_P_CONTROL_V1                   4
#define OPENVPN_OPCODE_P_DATA_V1                      6
#define OPENVPN_OPCODE_P_DATA_V2                      9

#define OPENVPN_BUF_MAX     65535
#define OPENVPN_MAX_FRAMES 256

typedef struct {
    ArkimeParserBuf_t *tls;        /* accumulated inner TLS stream per direction */
    ArkimeParserBuf_t *tcp;        /* TCP framing reassembly (only used for TCP) */
    uint16_t           frames;     /* OpenVPN records processed */
    uint8_t            hmacSize;   /* 0 / 16 / 20 / 32 / 64 */
    uint8_t            isTcp;
} OpenVPN_t;

/******************************************************************************/
LOCAL int openvpn_hmac_from_client_hardreset_len(int len)
{
    switch (len) {
    case 14:
        return 0;
    case 38:
        return 16;
    case 42:
        return 20;
    case 54:
        return 32;
    case 86:
        return 64;
    default:
        return -1;
    }
}
/******************************************************************************/
/* Validate a HARD_RESET_CLIENT_V2 record body (starting at the opcode byte).
 * Returns hmac size on match, -1 otherwise. */
LOCAL int openvpn_validate_client_hardreset(const uint8_t *data, int len)
{
    if (len < 14 || data[0] != 0x38)
        return -1;
    int hmac = openvpn_hmac_from_client_hardreset_len(len);
    if (hmac < 0)
        return -1;
    /* Last 5 bytes are packet_id_array_len(1)=0 then packet_id(4)=0. */
    for (int i = 1; i <= 5; i++) {
        if (data[len - i])
            return -1;
    }
    return hmac;
}
/******************************************************************************/
/* Walk the accumulated TLS stream for one direction and dispatch handshake
 * messages to the TLS parser. Slides any fully-consumed prefix off the buffer. */
LOCAL int openvpn_consume_tls(OpenVPN_t *ov, ArkimeSession_t *session, int which)
{
    while (ov->tls->len[which] >= 5) {
        BSB bsb;
        BSB_INIT(bsb, ov->tls->buf[which], ov->tls->len[which]);

        uint8_t  ctype = 0;
        uint16_t reclen = 0;
        BSB_IMPORT_u08(bsb, ctype);
        BSB_IMPORT_skip(bsb, 2);    /* version */
        BSB_IMPORT_u16(bsb, reclen);

        /* Only handshake records (0x16) are interesting. Anything else --
         * ChangeCipherSpec / Alert / ApplicationData -- means the TLS
         * handshake is done; nothing more for us to extract. */
        if (ctype != 0x16) {
            return ARKIME_PARSER_UNREGISTER;
        }
        if (BSB_REMAINING(bsb) < reclen)
            return 0;

        BSB hsb;
        BSB_IMPORT_bsb(bsb, hsb, reclen);

        while (BSB_REMAINING(hsb) >= 4) {
            const uint8_t *hstart = BSB_WORK_PTR(hsb);
            uint8_t        htype = 0;
            uint32_t       hlen = 0;
            BSB_IMPORT_u08(hsb, htype);
            BSB_IMPORT_u24(hsb, hlen);
            const uint8_t *body;
            BSB_IMPORT_ptr(hsb, body, hlen);
            if (BSB_IS_ERROR(hsb)) {
                return ARKIME_PARSER_UNREGISTER;
            }
            switch (htype) {
            case 1: /* ClientHello -- expects the 4-byte handshake header included */
                arkime_parsers_call_named_func(tls_process_client_hello_func,
                                               session, hstart, hlen + 4, NULL);
                break;
            case 2: /* ServerHello -- body only */
                arkime_parsers_call_named_func(tls_process_server_hello_func,
                                               session, body, hlen, NULL);
                break;
            case 11: /* Certificate -- body only */
                arkime_parsers_call_named_func(tls_process_server_certificate_func,
                                               session, body, hlen, NULL);
                break;
            }
        }

        arkime_parser_buf_del(ov->tls, which, 5 + reclen);
    }
    return 0;
}
/******************************************************************************/
/* Process one OpenVPN record body (starting at the opcode byte). */
LOCAL int openvpn_process_record(OpenVPN_t *ov, ArkimeSession_t *session,
                                  const uint8_t *p, int len, int which)
{
    if (++ov->frames > OPENVPN_MAX_FRAMES) {
        return ARKIME_PARSER_UNREGISTER;
    }

    BSB bsb;
    BSB_INIT(bsb, p, len);

    uint8_t op = 0;
    BSB_IMPORT_u08(bsb, op);
    if (BSB_IS_ERROR(bsb))
        return 0;
    uint8_t opcode = op >> 3;
    /* Data frames mean the tunnel is up; no more TLS handshake to extract. */
    if (opcode == OPENVPN_OPCODE_P_DATA_V1 || opcode == OPENVPN_OPCODE_P_DATA_V2)
        return ARKIME_PARSER_UNREGISTER;
    if (opcode != OPENVPN_OPCODE_P_CONTROL_V1)
        return 0;

    /* Layout (after opcode):
     *   session_id(8)
     *   [hmac(hmac_size) + packet_id(4) + net_time(4)] if hmac_size > 0
     *   packet_id_array_len(1) + packet_id_array(4*n)
     *   remote_session_id(8) if n > 0
     *   packet_id(4)
     *   ssl_data */
    BSB_IMPORT_skip(bsb, 8);
    if (ov->hmacSize)
        BSB_IMPORT_skip(bsb, ov->hmacSize + 4 + 4);

    uint8_t arrlen = 0;
    BSB_IMPORT_u08(bsb, arrlen);
    BSB_IMPORT_skip(bsb, (int)arrlen * 4);
    if (arrlen > 0)
        BSB_IMPORT_skip(bsb, 8);
    BSB_IMPORT_skip(bsb, 4);

    if (BSB_IS_ERROR(bsb)) {
        return ARKIME_PARSER_UNREGISTER;
    }

    int            ssLen  = BSB_REMAINING(bsb);
    const uint8_t *ssData = BSB_WORK_PTR(bsb);
    if (ssLen <= 0)
        return 0;

    /* Append to the inner TLS stream and try to consume any complete records.
     * Truncation here means a single inner TLS record exceeded our buffer
     * cap; framing is unrecoverable, stop. */
    if (arkime_parser_buf_add(ov->tls, which, ssData, ssLen) < 0) {
        return ARKIME_PARSER_UNREGISTER;
    }
    return openvpn_consume_tls(ov, session, which);
}
/******************************************************************************/
LOCAL int openvpn_parser(ArkimeSession_t *session, void *uw,
                         const uint8_t *data, int remaining, int which)
{
    OpenVPN_t *ov = uw;

    if (!ov->isTcp) {
        return openvpn_process_record(ov, session, data, remaining, which);
    }

    /* TCP: append to framing buffer, then drain length-prefixed records.
     * Truncation means a record larger than our cap; framing is lost. */
    if (arkime_parser_buf_add(ov->tcp, which, data, remaining) < 0) {
        return ARKIME_PARSER_UNREGISTER;
    }

    while (ov->tcp->len[which] >= 2) {
        BSB bsb;
        BSB_INIT(bsb, ov->tcp->buf[which], ov->tcp->len[which]);

        uint16_t rlen = 0;
        BSB_IMPORT_u16(bsb, rlen);
        if (BSB_IS_ERROR(bsb) || rlen == 0) {
            return ARKIME_PARSER_UNREGISTER;
        }
        if (BSB_REMAINING(bsb) < rlen)
            return 0;

        const uint8_t *rec;
        BSB_IMPORT_ptr(bsb, rec, rlen);
        if (BSB_IS_ERROR(bsb)) {
            return ARKIME_PARSER_UNREGISTER;
        }

        if (openvpn_process_record(ov, session, rec, rlen, which))
            return ARKIME_PARSER_UNREGISTER;
        arkime_parser_buf_del(ov->tcp, which, 2 + rlen);
    }
    return 0;
}
/******************************************************************************/
LOCAL void openvpn_free(ArkimeSession_t UNUSED(*session), void *uw)
{
    OpenVPN_t *ov = uw;
    if (ov->tls)
        arkime_parser_buf_free(ov->tls);
    if (ov->tcp)
        arkime_parser_buf_free(ov->tcp);
    ARKIME_TYPE_FREE(OpenVPN_t, ov);
}
/******************************************************************************/
LOCAL void openvpn_register(ArkimeSession_t *session, int isTcp, int hmacSize)
{
    if (arkime_parsers_has_registered(session, openvpn_parser))
        return;
    OpenVPN_t *ov = ARKIME_TYPE_ALLOC0(OpenVPN_t);
    ov->isTcp    = (uint8_t)isTcp;
    ov->hmacSize = (uint8_t)hmacSize;
    ov->tls      = arkime_parser_buf_create2(2048, OPENVPN_BUF_MAX);
    if (isTcp)
        ov->tcp = arkime_parser_buf_create2(2048, OPENVPN_BUF_MAX);
    arkime_parsers_register(session, openvpn_parser, ov, openvpn_free);
}
/******************************************************************************/
LOCAL void openvpn_udp_classify(ArkimeSession_t *session, const uint8_t *data,
                                int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "openvpn"))
        return;
    int hmac = openvpn_validate_client_hardreset(data, len);
    if (hmac < 0)
        return;
    arkime_session_add_protocol(session, "openvpn");
    openvpn_register(session, 0, hmac);
}
/******************************************************************************/
LOCAL void openvpn_tcp_classify(ArkimeSession_t *session, const uint8_t *data,
                                int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "openvpn"))
        return;
    if (len < 3)
        return;
    uint16_t rlen = ((uint16_t)data[0] << 8) | data[1];
    if (rlen + 2 > len)
        return;
    int hmac = openvpn_validate_client_hardreset(data + 2, rlen);
    if (hmac < 0)
        return;
    arkime_session_add_protocol(session, "openvpn");
    openvpn_register(session, 1, hmac);
}
/******************************************************************************/
void arkime_parser_init()
{
    /* UDP HARD_RESET_CLIENT_V2 (opcode 7, key_id 0). The classifier function
     * does the rest of the validation (length must be 14/38/42/54/86, last 5
     * bytes zero). */
    arkime_parsers_classifier_register_udp("openvpn", NULL, 0,
                                           (const uint8_t *)"\x38", 1,
                                           openvpn_udp_classify);

    /* TCP: 2-byte length prefix followed by HARD_RESET_CLIENT_V2. Register a
     * tight prefix for each possible hard-reset record length so we trigger
     * port-independently with minimal false positives. */
    arkime_parsers_classifier_register_tcp("openvpn", NULL, 0,
                                           (const uint8_t *)"\x00\x0e\x38", 3,
                                           openvpn_tcp_classify);
    arkime_parsers_classifier_register_tcp("openvpn", NULL, 0,
                                           (const uint8_t *)"\x00\x26\x38", 3,
                                           openvpn_tcp_classify);
    arkime_parsers_classifier_register_tcp("openvpn", NULL, 0,
                                           (const uint8_t *)"\x00\x2a\x38", 3,
                                           openvpn_tcp_classify);
    arkime_parsers_classifier_register_tcp("openvpn", NULL, 0,
                                           (const uint8_t *)"\x00\x36\x38", 3,
                                           openvpn_tcp_classify);
    arkime_parsers_classifier_register_tcp("openvpn", NULL, 0,
                                           (const uint8_t *)"\x00\x56\x38", 3,
                                           openvpn_tcp_classify);

    tls_process_client_hello_func       = arkime_parsers_get_named_func("tls_process_client_hello");
    tls_process_server_hello_func       = arkime_parsers_get_named_func("tls_process_server_hello");
    tls_process_server_certificate_func = arkime_parsers_get_named_func("tls_process_server_certificate");
}
