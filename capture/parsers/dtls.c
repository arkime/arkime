/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

//#define DTLSDEBUG 1

extern ArkimeConfig_t        config;

LOCAL uint32_t tls_process_server_certificate_func;
LOCAL uint32_t dtls_process_server_hello_func;
LOCAL uint32_t dtls_process_client_hello_func;

LOCAL GChecksum *checksums256[ARKIME_MAX_PACKET_THREADS];

LOCAL  int                   ja4Field;
LOCAL  int                   ja4RawField;

LOCAL  gboolean              ja4Raw;

/******************************************************************************/
// https://tools.ietf.org/html/draft-davidben-tls-grease-00
LOCAL int dtls_is_grease_value(uint32_t val)
{
    if ((val & 0x0f) != 0x0a)
        return 0;

    if ((val & 0xff) != ((val >> 8) & 0xff))
        return 0;

    return 1;
}
/******************************************************************************/
LOCAL void dtls_ja4_version(uint16_t ver, char vstr[3])
{
    switch (ver) {
    case 0x0100:
        memcpy(vstr, "s1", 3);
        break;
    case 0x0200:
        memcpy(vstr, "s2", 3);
        break;
    case 0x0300:
        memcpy(vstr, "s3", 3);
        break;
    case 0x0301:
        memcpy(vstr, "10", 3);
        break;
    case 0x0302:
        memcpy(vstr, "11", 3);
        break;
    case 0x0303:
        memcpy(vstr, "12", 3);
        break;
    case 0x0304:
        memcpy(vstr, "13", 3);
        break;
    case 0xfeff:
        memcpy(vstr, "d1", 3);
        break;
    case 0xfefd:
        memcpy(vstr, "d2", 3);
        break;
    case 0xfefc:
        memcpy(vstr, "d3", 3);
        break;
    default:
        memcpy(vstr, "00", 3);
        break;
    }
}
/******************************************************************************/
// Comparison function for qsort
LOCAL int compare_uint16_t(const void *a, const void *b)
{
    return (*(const uint16_t *)a < * (const uint16_t *)b ? -1 : * (const uint16_t *)a > *(const uint16_t *)b);
}
/******************************************************************************/
LOCAL uint32_t dtls_process_client_hello(ArkimeSession_t *session, const uint8_t *data, int len, void UNUSED(*uw))
{
    char     ja4HasSNI = 'i';
    uint16_t ja4Ciphers[256];
    uint8_t  ja4NumCiphers = 0;
    uint8_t  ja4NumExtensions = 0;
    uint16_t ja4Extensions[256];
    uint8_t  ja4NumExtensionsSome = 0;
    uint8_t  ja4NumAlgos = 0;
    uint16_t ja4Algos[256];
    uint8_t  ja4ALPN[2] = {'0', '0'};

    BSB cbsb;
    BSB_INIT(cbsb, data, len);

    uint16_t ver = 0;
    BSB_IMPORT_u16(cbsb, ver);


    if (BSB_IS_ERROR(cbsb) || BSB_REMAINING(cbsb) <= 32) {
        return -1;
    }

    BSB_IMPORT_skip(cbsb, 32);     // Random

    int skiplen = 0;
    BSB_IMPORT_u08(cbsb, skiplen);   // Session Id Length
    BSB_IMPORT_skip(cbsb, skiplen);  // Session Id

    BSB_IMPORT_u08(cbsb, skiplen);   // Cookie Length
    BSB_IMPORT_skip(cbsb, skiplen);  // Cookie

    BSB_IMPORT_u16(cbsb, skiplen);   // Ciper Suites Length
    while (BSB_NOT_ERROR(cbsb) && skiplen > 0) {
        uint16_t c = 0;
        BSB_IMPORT_u16(cbsb, c);
        if (!dtls_is_grease_value(c)) {
            ja4Ciphers[ja4NumCiphers] = c;
            ja4NumCiphers++;
        }
        skiplen -= 2;
    }
    BSB_IMPORT_u08(cbsb, skiplen);   // Compression Length
    BSB_IMPORT_skip(cbsb, skiplen);  // Compressions

    if (BSB_REMAINING(cbsb) >= 6) {
        int etotlen = 0;
        BSB_IMPORT_u16(cbsb, etotlen);  // Extensions Length

        etotlen = MIN(etotlen, BSB_REMAINING(cbsb));

        BSB ebsb;
        BSB_INIT(ebsb, BSB_WORK_PTR(cbsb), etotlen);

        while (BSB_REMAINING(ebsb) >= 4) {
            uint16_t etype = 0, elen = 0;

            BSB_IMPORT_u16 (ebsb, etype);
            BSB_IMPORT_u16 (ebsb, elen);

            if (dtls_is_grease_value(etype)) {
                BSB_IMPORT_skip (ebsb, elen);
                continue;
            }

            ja4NumExtensions++;
            ja4Extensions[ja4NumExtensionsSome] = etype;
            ja4NumExtensionsSome++;

            if (elen > BSB_REMAINING(ebsb))
                break;

            if (etype == 0) { // SNI
                ja4NumExtensionsSome--;
                BSB bsb;
                BSB_IMPORT_bsb (ebsb, bsb, elen);

                int sni = 0;
                BSB_IMPORT_u16(bsb, sni); // list len
                if (sni != BSB_REMAINING(bsb))
                    continue;

                BSB_IMPORT_u08(bsb, sni); // type
                if (sni != 0)
                    continue;

                BSB_IMPORT_u16(bsb, sni); // len
                if (sni != BSB_REMAINING(bsb))
                    continue;

                //arkime_field_string_add(hostField, session, (char *)BSB_WORK_PTR(bsb), sni, TRUE);
                ja4HasSNI = 'd';
            } else if (etype == 0x000d) { // Signature Algorithms
                BSB bsb;
                BSB_IMPORT_bsb (ebsb, bsb, elen);

                uint16_t llen = 0;
                BSB_IMPORT_u16(bsb, llen); // list len
                while (llen > 0 && !BSB_IS_ERROR(bsb)) {
                    uint16_t a = 0;
                    BSB_IMPORT_u16(bsb, a);
                    ja4Algos[ja4NumAlgos++] = a;
                    llen -= 2;
                }
            } else if (etype == 0x10) { // ALPN
                ja4NumExtensionsSome--;
                BSB bsb;
                BSB_IMPORT_bsb (ebsb, bsb, elen);

                BSB_IMPORT_skip (bsb, 2); // len
                uint8_t alen = 0;
                BSB_IMPORT_u08 (bsb, alen); // len
                const uint8_t *astr = NULL;
                BSB_IMPORT_ptr (bsb, astr, alen);
                if (alen > 0 && astr && !BSB_IS_ERROR(bsb)) {
                    ja4ALPN[0] = astr[0];
                    ja4ALPN[1] = astr[alen - 1];
                }
            } else if (etype == 0x2b) { // etype 0x2b is supported version
                BSB bsb;
                BSB_IMPORT_bsb (ebsb, bsb, elen);

                uint16_t llen = 0;
                BSB_IMPORT_u08(bsb, llen); // list len
                while (llen > 0 && !BSB_IS_ERROR(bsb)) {
                    uint16_t supported_version = 0;
                    BSB_IMPORT_u16(bsb, supported_version);
                    if (!dtls_is_grease_value(supported_version)) {
                        ver = MAX(supported_version, ver);
                    }
                    llen--;
                }
            } else {
                BSB_IMPORT_skip (ebsb, elen);
            }
        }
    }

    char vstr[3];
    dtls_ja4_version(ver, vstr);

    char ja4_r[4096];
    BSB ja4_rbsb;
    BSB_INIT(ja4_rbsb, ja4_r, sizeof(ja4_r));

    char ja4[37];
    ja4[36] = 0;
    ja4[0] = 'd';
    ja4[1] = vstr[0];
    ja4[2] = vstr[1];
    ja4[3] = ja4HasSNI;
    ja4[4] = (ja4NumCiphers / 10) + '0';
    ja4[5] = (ja4NumCiphers % 10) + '0';
    ja4[6] = (ja4NumExtensions / 10) + '0';
    ja4[7] = (ja4NumExtensions % 10) + '0';
    ja4[8] = ja4ALPN[0];
    ja4[9] = ja4ALPN[1];
    ja4[10] = '_';

    BSB_EXPORT_ptr(ja4_rbsb, ja4, 11);

    char tmpBuf[5 * 256];
    BSB tmpBSB;

    // Sort ciphers, convert to hex, first 12 bytes of sha256
    qsort(ja4Ciphers, ja4NumCiphers, 2, compare_uint16_t);
    BSB_INIT(tmpBSB, tmpBuf, sizeof(tmpBuf));
    for (int i = 0; i < ja4NumCiphers; i++) {
        BSB_EXPORT_sprintf(tmpBSB, "%04x,", ja4Ciphers[i]);
    }
    if (ja4NumCiphers > 0) {
        BSB_EXPORT_rewind(tmpBSB, 1); // Remove last ,
        BSB_EXPORT_ptr(ja4_rbsb, tmpBuf, BSB_LENGTH(tmpBSB));
    }

    BSB_EXPORT_u08(ja4_rbsb, '_');

    GChecksum *const checksum = checksums256[session->thread];

    if (BSB_LENGTH(tmpBSB) > 0) {
        g_checksum_update(checksum, (guchar *)tmpBuf, BSB_LENGTH(tmpBSB));
        memcpy(ja4 + 11, g_checksum_get_string(checksum), 12);
        g_checksum_reset(checksum);
    } else {
        memcpy(ja4 + 11, "000000000000", 12);
    }

    ja4[23] = '_';

    // Sort the extensions, convert to hex, add unsorted Algos, first 12 bytes of sha256
    qsort(ja4Extensions, ja4NumExtensionsSome, 2, compare_uint16_t);
    BSB_INIT(tmpBSB, tmpBuf, sizeof(tmpBuf));
    for (int i = 0; i < ja4NumExtensionsSome; i++) {
        BSB_EXPORT_sprintf(tmpBSB, "%04x,", ja4Extensions[i]);
    }
    if (ja4NumExtensionsSome > 0) {
        BSB_EXPORT_rewind(tmpBSB, 1); // Remove last ,
    }
    if (ja4NumAlgos > 0) {
        BSB_EXPORT_u08(tmpBSB, '_');
        for (int i = 0; i < ja4NumAlgos; i++) {
            BSB_EXPORT_sprintf(tmpBSB, "%04x,", ja4Algos[i]);
        }
        BSB_EXPORT_rewind(tmpBSB, 1); // Remove last ,
    }

    BSB_EXPORT_ptr(ja4_rbsb, tmpBuf, BSB_LENGTH(tmpBSB));
    BSB_EXPORT_u08(ja4_rbsb, 0);

    if (BSB_LENGTH(tmpBSB) > 0) {
        g_checksum_update(checksum, (guchar *)tmpBuf, BSB_LENGTH(tmpBSB));
        memcpy(ja4 + 24, g_checksum_get_string(checksum), 12);
        g_checksum_reset(checksum);
    } else {
        memcpy(ja4 + 24, "000000000000", 12);
    }

    // Add the field
    arkime_field_string_add(ja4Field, session, ja4, 36, TRUE);
    if (ja4Raw) {
        arkime_field_string_add(ja4RawField, session, ja4_r, BSB_LENGTH(ja4_rbsb), TRUE);
    }
    return 0;
}

/******************************************************************************/
LOCAL int dtls_udp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    BSB bbuf;

    // 22 is handshake
    if (data[0] != 22) {
        arkime_parsers_unregister(session, uw);
        return 0;
    }

    BSB_INIT(bbuf, data, len);

    while (BSB_NOT_ERROR(bbuf) && BSB_REMAINING(bbuf) > 11) {
        BSB_IMPORT_skip(bbuf, 11);
        uint16_t tlen = 0;
        BSB_IMPORT_u16(bbuf, tlen);

        if (tlen > BSB_REMAINING(bbuf))
            return 0;

        BSB msgBuf;
        BSB_INIT(msgBuf, BSB_WORK_PTR(bbuf), tlen);
        BSB_IMPORT_skip(bbuf, tlen);

        while (BSB_NOT_ERROR(bbuf) && BSB_NOT_ERROR(msgBuf) && BSB_REMAINING(msgBuf) > 12) {
            uint8_t handshakeType = 0;
            BSB_IMPORT_u08(msgBuf, handshakeType);
            uint32_t handshakeLen = 0;
            BSB_IMPORT_u24(msgBuf, handshakeLen);
            BSB_IMPORT_skip(msgBuf, 2); // msgSeq
            uint32_t frameOffset = 0;
            BSB_IMPORT_u24(msgBuf, frameOffset);
            BSB_IMPORT_skip(msgBuf, 3); // frameLength
            // ALW fix - don't handle fragmented packets yet
            if (frameOffset != 0) {
                BSB_IMPORT_skip(msgBuf, handshakeLen);
                continue;
            }

            // Not enough data left
            if (BSB_IS_ERROR(msgBuf) || handshakeLen > BSB_REMAINING(msgBuf))
                break;

            switch (handshakeType) {
            case 1: // client hello
                arkime_parsers_call_named_func(dtls_process_client_hello_func, session, BSB_WORK_PTR(msgBuf), handshakeLen, NULL);
                break;
            case 2: // server hello
                arkime_parsers_call_named_func(dtls_process_server_hello_func, session, BSB_WORK_PTR(msgBuf), handshakeLen, NULL);
                break;
            case 11: // Certificate
                arkime_parsers_call_named_func(tls_process_server_certificate_func, session, BSB_WORK_PTR(msgBuf), handshakeLen, NULL);
                break;
            }
            BSB_IMPORT_skip(msgBuf, handshakeLen);
        }
    }

    return 0;
}
/******************************************************************************/
LOCAL void dtls_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 100 || data[13] != 1)
        return;
    arkime_session_add_protocol(session, "dtls");
    arkime_parsers_register(session, dtls_udp_parser, uw, 0);
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_parsers_classifier_register_udp("dtls", NULL, 0, (const uint8_t *)"\x16\x01\x00", 3, dtls_udp_classify);
    arkime_parsers_classifier_register_udp("dtls", NULL, 0, (const uint8_t *)"\x16\xfe\xff", 3, dtls_udp_classify);
    arkime_parsers_classifier_register_udp("dtls", NULL, 0, (const uint8_t *)"\x16\xfe\xfe", 3, dtls_udp_classify);
    arkime_parsers_classifier_register_udp("dtls", NULL, 0, (const uint8_t *)"\x16\xfe\xfd", 3, dtls_udp_classify);

    tls_process_server_certificate_func = arkime_parsers_get_named_func("tls_process_server_certificate");

    int t;
    for (t = 0; t < config.packetThreads; t++) {
        checksums256[t] = g_checksum_new(G_CHECKSUM_SHA256);
    }

    ja4Raw = arkime_config_boolean(NULL, "ja4Raw", FALSE);

    ja4Field = arkime_field_define("tls", "lotermfield",
                                   "tls.ja4", "JA4", "tls.ja4",
                                   "SSL/TLS JA4 field",
                                   ARKIME_FIELD_TYPE_STR_GHASH,  ARKIME_FIELD_FLAG_CNT,
                                   (char *)NULL);

    ja4RawField = arkime_field_define("tls", "lotermfield",
                                      "tls.ja4_r", "JA4_r", "tls.ja4_r",
                                      "SSL/TLS JA4_r field",
                                      ARKIME_FIELD_TYPE_STR_GHASH,  ARKIME_FIELD_FLAG_CNT,
                                      (char *)NULL);

    dtls_process_server_hello_func = arkime_parsers_get_named_func("dtls_process_server_hello");
    dtls_process_client_hello_func = arkime_parsers_add_named_func("dtls_process_client_hello", dtls_process_client_hello);
}
