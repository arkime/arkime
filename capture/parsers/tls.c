/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include "tls-cipher.h"
#include "openssl/objects.h"

extern ArkimeConfig_t        config;
LOCAL  int                   hostField;
LOCAL  int                   verField;
LOCAL  int                   cipherField;
LOCAL  int                   ja3Field;
LOCAL  int                   ja3sField;
LOCAL  int                   srcIdField;
LOCAL  int                   dstIdField;
LOCAL  int                   ja3StrField;
LOCAL  int                   ja3sStrField;
LOCAL  int                   ja4Field;
LOCAL  int                   ja4RawField;

LOCAL  gboolean              ja4Raw;

typedef struct {
    uint8_t             buf[8192];
    uint16_t            len;
    char                which;
} TLSInfo_t;

extern uint8_t    arkime_char_to_hexstr[256][3];

LOCAL GChecksum *checksums256[ARKIME_MAX_PACKET_THREADS];

LOCAL uint32_t tls_process_client_hello_func;
LOCAL uint32_t tls_process_server_hello_func;
LOCAL uint32_t tls_process_server_certificate_func;

/******************************************************************************/
// https://tools.ietf.org/html/draft-davidben-tls-grease-00
LOCAL int tls_is_grease_value(uint32_t val)
{
    if ((val & 0x0f) != 0x0a)
        return 0;

    if ((val & 0xff) != ((val >> 8) & 0xff))
        return 0;

    return 1;
}
/******************************************************************************/
LOCAL void tls_session_version(ArkimeSession_t *session, uint16_t ver)
{
    char str[100];

    switch (ver) {
    case 0x0100:
        arkime_field_string_add(verField, session, "SSLv1", 5, TRUE);
        break;
    case 0x0200:
        arkime_field_string_add(verField, session, "SSLv2", 5, TRUE);
        break;
    case 0x0300:
        arkime_field_string_add(verField, session, "SSLv3", 5, TRUE);
        break;
    case 0x0301:
        arkime_field_string_add(verField, session, "TLSv1", 5, TRUE);
        break;
    case 0x0302:
        arkime_field_string_add(verField, session, "TLSv1.1", 7, TRUE);
        break;
    case 0x0303:
        arkime_field_string_add(verField, session, "TLSv1.2", 7, TRUE);
        break;
    case 0x0304:
        arkime_field_string_add(verField, session, "TLSv1.3", 7, TRUE);
        break;
    case 0x7f00 ... 0x7fff:
        snprintf(str, sizeof(str), "TLSv1.3-draft-%02d", ver & 0xff);
        arkime_field_string_add(verField, session, str, -1, TRUE);
        break;
    default:
        snprintf(str, sizeof(str), "0x%04x", ver);
        arkime_field_string_add(verField, session, str, 6, TRUE);
    }
}
/******************************************************************************/
LOCAL void tls_ja4_version(uint16_t ver, char vstr[3])
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
    /*    case 0x7f00 ... 0x7fff:
            memcpy(vstr, "13", 3);
            break;*/
    default:
        memcpy(vstr, "00", 3);
        break;
    }
}
/******************************************************************************/
LOCAL uint32_t tls_process_server_hello(ArkimeSession_t *session, const uint8_t *data, int len, void UNUSED(*uw))
{
    BSB bsb;
    BSB_INIT(bsb, data, len);

    uint16_t ver = 0;
    BSB_IMPORT_u16(bsb, ver);
    BSB_IMPORT_skip(bsb, 32);     // Random

    if (BSB_IS_ERROR(bsb))
        return -1;

    int  add12Later = FALSE;

    // If ver is 0x303 that means there should be an extended header with actual version
    if (ver != 0x0303)
        tls_session_version(session, ver);
    else
        add12Later = TRUE;

    /* Parse sessionid, only for SSLv3 - TLSv1.2 */
    if (ver >= 0x0300 && ver <= 0x0303) {
        int skiplen = 0;
        BSB_IMPORT_u08(bsb, skiplen);   // Session Id Length
        if (skiplen > 0 && BSB_REMAINING(bsb) > skiplen) {
            const uint8_t *ptr = BSB_WORK_PTR(bsb);
            char sessionId[513];
            int  i;
            for (i = 0; i < skiplen; i++) {
                sessionId[i * 2] = arkime_char_to_hexstr[ptr[i]][0];
                sessionId[i * 2 + 1] = arkime_char_to_hexstr[ptr[i]][1];
            }
            sessionId[skiplen * 2] = 0;
            arkime_field_string_add(dstIdField, session, sessionId, skiplen * 2, TRUE);
        }
        BSB_IMPORT_skip(bsb, skiplen);  // Session Id
    }

    uint16_t cipher = 0;
    BSB_IMPORT_u16(bsb, cipher);

    /* Parse cipher */
    const char *cipherStr = ciphers[cipher >> 8][cipher & 0xff];
    if (cipherStr)
        arkime_field_string_add(cipherField, session, cipherStr, -1, TRUE);
    else {
        char str[100];
        snprintf(str, sizeof(str), "0x%04x", cipher);
        arkime_field_string_add(cipherField, session, str, 6, TRUE);
    }

    /* Thanks wireshark - No compression with TLS 1.3 before draft -22 */
    if (ver < 0x0700 || ver >= 0x7f16) {
        BSB_IMPORT_skip(bsb, 1);
    }


    char ja3[30000];
    BSB ja3bsb;
    char eja3[10000];
    BSB eja3bsb;

    BSB_INIT(ja3bsb, ja3, sizeof(ja3));
    BSB_INIT(eja3bsb, eja3, sizeof(eja3));

    if (BSB_REMAINING(bsb) > 2) {
        int etotlen = 0;
        BSB_IMPORT_u16(bsb, etotlen);  // Extensions Length

        etotlen = MIN(etotlen, BSB_REMAINING(bsb));

        BSB ebsb;
        BSB_INIT(ebsb, BSB_WORK_PTR(bsb), etotlen);

        while (BSB_REMAINING(ebsb) > 0) {
            int etype = 0, elen = 0;

            BSB_IMPORT_u16 (ebsb, etype);
            BSB_IMPORT_u16 (ebsb, elen);

            BSB_EXPORT_sprintf(eja3bsb, "%d-", etype);

            if (elen > BSB_REMAINING(ebsb))
                break;

            if (etype == 0x2b && elen == 2) { // etype 0x2b is supported version
                uint16_t supported_version = 0;
                BSB_IMPORT_u16(ebsb, supported_version);

                if (supported_version == 0x0304) {
                    tls_session_version(session, supported_version);
                    add12Later = FALSE;
                }
                continue; // Already processed ebsb above
            }

            if (etype == 0x10) { // etype 0x10 is alpn
                if (elen == 5 && BSB_REMAINING(ebsb) >= 5 && memcmp(BSB_WORK_PTR(ebsb), "\x00\x03\x02\x68\x32", 5) == 0) {
                    arkime_session_add_protocol(session, "http2");
                }
            }

            BSB_IMPORT_skip (ebsb, elen);
        }
        BSB_EXPORT_rewind(eja3bsb, 1); // Remove last -
    }

    if (add12Later)
        tls_session_version(session, 0x303);

    BSB_EXPORT_sprintf(ja3bsb, "%d,%d,%.*s", ver, cipher, (int)BSB_LENGTH(eja3bsb), eja3);

    if (config.ja3Strings) {
        arkime_field_string_add(ja3sStrField, session, ja3, strlen(ja3), TRUE);
    }

    gchar *md5 = g_compute_checksum_for_data(G_CHECKSUM_MD5, (guchar *)ja3, BSB_LENGTH(ja3bsb));
    if (config.debug > 1) {
        LOG("JA3s: %s => %s", ja3, md5);
    }
    if (!arkime_field_string_add(ja3sField, session, md5, 32, FALSE)) {
        g_free(md5);
    }
    return 0;
}

/******************************************************************************/
/* @data the data inside the record layer
 * @len  the length of data inside record layer
 */
LOCAL int tls_process_server_handshake_record(ArkimeSession_t *session, const uint8_t *data, int len)
{
    BSB rbsb;

    BSB_INIT(rbsb, data, len);

    while (BSB_REMAINING(rbsb) >= 4) {
        const uint8_t *hdata = BSB_WORK_PTR(rbsb);
        int hlen = MIN(BSB_REMAINING(rbsb), (hdata[1] << 16 | hdata[2] << 8 | hdata[3]) + 4);

        switch (hdata[0]) {
        case 2:
            arkime_parsers_call_named_func(tls_process_server_hello_func, session, hdata + 4, hlen - 4, NULL);
            break;
        case 11:
            arkime_parsers_call_named_func(tls_process_server_certificate_func, session, hdata + 4, hlen - 4, NULL);
            break;
        case 14:
            return 1;
        }

        BSB_IMPORT_skip(rbsb, hlen);
    }
    return 0;
}
/******************************************************************************/
// Comparison function for qsort
LOCAL int compare_uint16_t(const void *a, const void *b)
{
    return (*(const uint16_t *)a < * (const uint16_t *)b ? -1 : * (const uint16_t *)a > *(const uint16_t *)b);
}
/******************************************************************************/
uint32_t tls_process_client_hello_data(ArkimeSession_t *session, const uint8_t *data, int len, void UNUSED(*uw))
{
    if (len < 7)
        return -1;

    char ja3[30000];
    BSB ja3bsb;
    char ecfja3[1000];
    BSB ecfja3bsb;
    char eja3[10000];
    BSB eja3bsb;
    char ecja3[10000];
    BSB ecja3bsb;

    char     ja4HasSNI = 'i';
    uint16_t ja4Ciphers[256];
    uint8_t  ja4NumCiphers = 0;
    uint8_t  ja4NumExtensions = 0;
    uint16_t ja4Extensions[256];
    uint8_t  ja4NumExtensionsSome = 0;
    uint8_t  ja4NumAlgos = 0;
    uint16_t ja4Algos[256];
    uint8_t  ja4ALPN[2] = {'0', '0'};

    BSB_INIT(ja3bsb, ja3, sizeof(ja3));
    BSB_INIT(ecja3bsb, ecja3, sizeof(ecja3));
    BSB_INIT(ecfja3bsb, ecfja3, sizeof(ecfja3));
    BSB_INIT(eja3bsb, eja3, sizeof(eja3));

    BSB pbsb;
    BSB_INIT(pbsb, data, len);

    uint8_t *pdata = BSB_WORK_PTR(pbsb);
    int      plen = MIN(BSB_REMAINING(pbsb) - 4, pdata[2] << 8 | pdata[3]);

    uint16_t ver = 0;
    BSB_IMPORT_skip(pbsb, 4); // type + len
    BSB_IMPORT_u16(pbsb, ver);

    BSB_EXPORT_sprintf(ja3bsb, "%d,", ver);


    BSB cbsb;
    BSB_INIT(cbsb, pdata + 6, plen - 2); // The - 4 for plen is done above, confusing

    if (BSB_REMAINING(cbsb) > 32) {
        BSB_IMPORT_skip(cbsb, 32);     // Random

        int skiplen = 0;
        BSB_IMPORT_u08(cbsb, skiplen);   // Session Id Length
        if (skiplen > 0 && BSB_REMAINING(cbsb) > skiplen) {
            const uint8_t *ptr = BSB_WORK_PTR(cbsb);
            char sessionId[513];
            int  i;

            for (i = 0; i < skiplen; i++) {
                sessionId[i * 2] = arkime_char_to_hexstr[ptr[i]][0];
                sessionId[i * 2 + 1] = arkime_char_to_hexstr[ptr[i]][1];
            }
            sessionId[skiplen * 2] = 0;
            arkime_field_string_add(srcIdField, session, sessionId, skiplen * 2, TRUE);
        }
        BSB_IMPORT_skip(cbsb, skiplen);  // Session Id

        BSB_IMPORT_u16(cbsb, skiplen);   // Ciper Suites Length
        while (BSB_NOT_ERROR(cbsb) && skiplen > 0) {
            uint16_t c = 0;
            BSB_IMPORT_u16(cbsb, c);
            if (!tls_is_grease_value(c)) {
                BSB_EXPORT_sprintf(ja3bsb, "%d-", c);
                ja4Ciphers[ja4NumCiphers] = c;
                ja4NumCiphers++;
            }
            skiplen -= 2;
        }
        BSB_EXPORT_rewind(ja3bsb, 1); // Remove last -
        BSB_EXPORT_u08(ja3bsb, ',');

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

                if (tls_is_grease_value(etype)) {
                    BSB_IMPORT_skip (ebsb, elen);
                    continue;
                }

                ja4NumExtensions++;
                ja4Extensions[ja4NumExtensionsSome] = etype;
                ja4NumExtensionsSome++;

                BSB_EXPORT_sprintf(eja3bsb, "%d-", etype);

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

                    arkime_field_string_add(hostField, session, (char *)BSB_WORK_PTR(bsb), sni, TRUE);
                    ja4HasSNI = 'd';
                } else if (etype == 0x000a) { // Elliptic Curves
                    BSB bsb;
                    BSB_IMPORT_bsb (ebsb, bsb, elen);

                    uint16_t llen = 0;
                    BSB_IMPORT_u16(bsb, llen); // list len
                    while (llen > 0 && !BSB_IS_ERROR(bsb)) {
                        uint16_t c = 0;
                        BSB_IMPORT_u16(bsb, c);
                        if (!tls_is_grease_value(c)) {
                            BSB_EXPORT_sprintf(ecja3bsb, "%d-", c);
                        }
                        llen -= 2;
                    }
                    BSB_EXPORT_rewind(ecja3bsb, 1); // Remove last -
                } else if (etype == 0x000b) { // Elliptic Curves point formats
                    BSB bsb;
                    BSB_IMPORT_bsb (ebsb, bsb, elen);

                    uint16_t llen = 0;
                    BSB_IMPORT_u08(bsb, llen); // list len
                    while (llen > 0 && !BSB_IS_ERROR(bsb)) {
                        uint8_t c = 0;
                        BSB_IMPORT_u08(bsb, c);
                        BSB_EXPORT_sprintf(ecfja3bsb, "%d-", c);
                        llen -= 1;
                    }
                    BSB_EXPORT_rewind(ecfja3bsb, 1); // Remove last -
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
                        if (!tls_is_grease_value(supported_version)) {
                            ver = MAX(supported_version, ver);
                        }
                        llen--;
                    }
                } else if (etype == 0xffce) { // esni
                    arkime_session_add_tag(session, "tls:has_esni");
                    BSB_IMPORT_skip (ebsb, elen);
                } else if (etype == 0xfe0d) { // encrypted_client_hello
                    arkime_session_add_tag(session, "tls:has_ech");
                    BSB_IMPORT_skip (ebsb, elen);
                } else {
                    BSB_IMPORT_skip (ebsb, elen);
                }
            }
            BSB_EXPORT_rewind(eja3bsb, 1); // Remove last -
        }
    }

    if (BSB_LENGTH(ja3bsb) > 0 && BSB_NOT_ERROR(ja3bsb) && BSB_NOT_ERROR(ecja3bsb) && BSB_NOT_ERROR(eja3bsb) && BSB_NOT_ERROR(ecfja3bsb)) {
        BSB_EXPORT_sprintf(ja3bsb, "%.*s,%.*s,%.*s", (int)BSB_LENGTH(eja3bsb), eja3, (int)BSB_LENGTH(ecja3bsb), ecja3, (int)BSB_LENGTH(ecfja3bsb), ecfja3);

        if (config.ja3Strings) {
            arkime_field_string_add(ja3StrField, session, ja3, strlen(ja3), TRUE);
        }

        gchar *md5 = g_compute_checksum_for_data(G_CHECKSUM_MD5, (guchar *)ja3, BSB_LENGTH(ja3bsb));

        if (config.debug > 1) {
            LOG("JA3: %s => %s", ja3, md5);
        }
        if (!arkime_field_string_add(ja3Field, session, md5, 32, FALSE)) {
            g_free(md5);
        }
    }

    char vstr[3];
    tls_ja4_version(ver, vstr);

    char ja4_r[4096];
    BSB ja4_rbsb;
    BSB_INIT(ja4_rbsb, ja4_r, sizeof(ja4_r));

    char ja4[37];
    ja4[36] = 0;
    ja4[0] = (session->ipProtocol == IPPROTO_TCP) ? 't' : 'q';
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
LOCAL void tls_process_client(ArkimeSession_t *session, const uint8_t *data, int len)
{
    BSB sslbsb;

    BSB_INIT(sslbsb, data, len);

    if (BSB_REMAINING(sslbsb) > 5) {
        const uint8_t *ssldata = BSB_WORK_PTR(sslbsb);
        int            ssllen = MIN(BSB_REMAINING(sslbsb) - 5, ssldata[3] << 8 | ssldata[4]);


        arkime_parsers_call_named_func(tls_process_client_hello_func, session, ssldata + 5, ssllen, NULL);
    }
}

/******************************************************************************/
LOCAL int tls_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int remaining, int which)
{
    TLSInfo_t            *tls          = uw;

    // If not the server half ignore
    if (which != tls->which)
        return 0;

    // Copy the data we have
    memcpy(tls->buf + tls->len, data, MIN(remaining, (int)sizeof(tls->buf) - tls->len));
    tls->len += MIN(remaining, (int)sizeof(tls->buf) - tls->len);

    // Make sure we have header
    if (tls->len < 5)
        return 0;

    // Not handshake protocol, stop looking
    if (tls->buf[0] != 0x16) {
        tls->len = 0;
        arkime_parsers_unregister(session, uw);
        return 0;
    }

    // Need the whole record
    int need = ((tls->buf[3] << 8) | tls->buf[4]) + 5;
    if (need > tls->len)
        return 0;

    if (tls_process_server_handshake_record(session, tls->buf + 5, need - 5)) {
        tls->len = 0;
        arkime_parsers_unregister(session, uw);
        return 0;
    }
    tls->len -= need;

    // Still more data to process
    if (tls->len) {
        memmove(tls->buf, tls->buf + need, tls->len);
        return 0;
    }

    return 0;
}
/******************************************************************************/
LOCAL void tls_save(ArkimeSession_t *session, void *uw, int UNUSED(final))
{
    TLSInfo_t            *tls          = uw;

    if (tls->len > 5 && tls->buf[0] == 0x16) {
        tls_process_server_handshake_record(session, tls->buf + 5, tls->len - 5);
        tls->len = 0;
    }
}
/******************************************************************************/
LOCAL void tls_free(ArkimeSession_t *UNUSED(session), void *uw)
{
    TLSInfo_t            *tls          = uw;

    ARKIME_TYPE_FREE(TLSInfo_t, tls);
}
/******************************************************************************/
LOCAL void tls_classify(ArkimeSession_t *session, const uint8_t *data, int len, int which, void *UNUSED(uw))
{
    if (len < 6 || data[2] > 0x03)
        return;

    if (arkime_session_has_protocol(session, "tls"))
        return;


    /* 1 Content Type - 0x16
     * 2 Version 0x0301 - 0x03 - 03
     * 2 Length
     * 1 Message Type 1 - Client Hello, 2 Server Hello
     */
    if (data[2] <= 0x03 && (data[5] == 1 || data[5] == 2)) {
        arkime_session_add_protocol(session, "tls");

        TLSInfo_t  *tls = ARKIME_TYPE_ALLOC(TLSInfo_t);
        tls->len        = 0;

        arkime_parsers_register2(session, tls_parser, tls, tls_free, tls_save);

        if (data[5] == 1) {
            tls_process_client(session, data, (int)len);
            tls->which      = (which + 1) % 2;
        } else {
            tls->which      = which;
        }
    }
}
/******************************************************************************/
void arkime_parser_init()
{
    ja4Raw = arkime_config_boolean(NULL, "ja4Raw", FALSE);

    hostField = arkime_field_by_exp("host.http");

    verField = arkime_field_define("tls", "termfield",
                                   "tls.version", "Version", "tls.version",
                                   "SSL/TLS version field",
                                   ARKIME_FIELD_TYPE_STR_GHASH,  ARKIME_FIELD_FLAG_CNT,
                                   (char *)NULL);

    cipherField = arkime_field_define("tls", "uptermfield",
                                      "tls.cipher", "Cipher", "tls.cipher",
                                      "SSL/TLS cipher field",
                                      ARKIME_FIELD_TYPE_STR_GHASH,  ARKIME_FIELD_FLAG_CNT,
                                      (char *)NULL);

    ja3Field = arkime_field_define("tls", "lotermfield",
                                   "tls.ja3", "JA3", "tls.ja3",
                                   "SSL/TLS JA3 field",
                                   ARKIME_FIELD_TYPE_STR_GHASH,  ARKIME_FIELD_FLAG_CNT,
                                   (char *)NULL);

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

    ja3sField = arkime_field_define("tls", "lotermfield",
                                    "tls.ja3s", "JA3S", "tls.ja3s",
                                    "SSL/TLS JA3S field",
                                    ARKIME_FIELD_TYPE_STR_GHASH,  ARKIME_FIELD_FLAG_CNT,
                                    (char *)NULL);

    dstIdField = arkime_field_define("tls", "lotermfield",
                                     "tls.sessionid.dst", "Dst Session Id", "tls.dstSessionId",
                                     "SSL/TLS Dst Session Id",
                                     ARKIME_FIELD_TYPE_STR_GHASH,  0,
                                     (char *)NULL);

    srcIdField = arkime_field_define("tls", "lotermfield",
                                     "tls.sessionid.src", "Src Session Id", "tls.srcSessionId",
                                     "SSL/TLS Src Session Id",
                                     ARKIME_FIELD_TYPE_STR_GHASH,  0,
                                     (char *)NULL);

    arkime_field_define("general", "lotermfield",
                        "tls.sessionid", "Src or Dst Session Id", "tlsidall",
                        "Shorthand for tls.sessionid.src or tls.sessionid.dst",
                        0,  ARKIME_FIELD_FLAG_FAKE,
                        "regex", "^tls\\\\.sessionid\\\\.(?:(?!\\\\.cnt$).)*$",
                        (char *)NULL);

    if (config.ja3Strings) {
        ja3sStrField = arkime_field_define("tls", "lotermfield",
                                           "tls.ja3sstring", "JA3SSTR", "tls.ja3sstring",
                                           "SSL/TLS JA3S String field",
                                           ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                           (char *)NULL);

        ja3StrField = arkime_field_define("tls", "lotermfield",
                                          "tls.ja3string", "JA3STR", "tls.ja3string",
                                          "SSL/TLS JA3 String field",
                                          ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                          (char *)NULL);
    }

    arkime_parsers_classifier_register_tcp("tls", NULL, 0, (uint8_t *)"\x16\x03", 2, tls_classify);

    int t;
    for (t = 0; t < config.packetThreads; t++) {
        checksums256[t] = g_checksum_new(G_CHECKSUM_SHA256);
    }

    tls_process_client_hello_func = arkime_parsers_add_named_func("tls_process_client_hello", tls_process_client_hello_data);
    tls_process_server_hello_func = arkime_parsers_add_named_func("tls_process_server_hello", tls_process_server_hello);
    tls_process_server_certificate_func = arkime_parsers_get_named_func("tls_process_server_certificate");
}

