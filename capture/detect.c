/* detect.c  -- Functions for dealing with detection
 *
 * Copyright 2012-2013 AOL Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include "glib.h"
#include "nids.h"
#include "pcap.h"
#include "magic.h"
#include "moloch.h"
#include "bsb.h"

//#define HTTPDEBUG
//#define EMAILDEBUG
//#define SMBDEBUG

/******************************************************************************/
extern MolochConfig_t        config;
extern gchar               **extraTags;
static gchar                 nodeTag[100];
static gchar                 classTag[100];
extern uint32_t              pluginsCbs;

static http_parser_settings  parserSettings;
static magic_t               cookie;

static char                 *qclasses[256];
static char                 *qtypes[256];

extern MolochStringHashStd_t httpReqHeaders;
extern MolochStringHashStd_t httpResHeaders;
extern MolochStringHashStd_t emailHeaders;


/******************************************************************************/
void moloch_detect_initial_tag(MolochSession_t *session)
{
    int i;

    moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, nodeTag);
    if (config.nodeClass)
        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, classTag);

    if (extraTags) {
        for (i = 0; extraTags[i]; i++) {
            moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, extraTags[i]);
        }
    }

    switch(session->protocol) {
    case IPPROTO_TCP:
        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "tcp");
        break;
    case IPPROTO_UDP:
        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "udp");
        break;
    case IPPROTO_ICMP:
        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "ICMP");
        break;
    }
}


/*############################## ASN ##############################*/

/******************************************************************************/
unsigned char *
moloch_detect_asn_get_tlv(BSB *bsb, int *apc, int *atag, int *alen)
{

    if (BSB_REMAINING(*bsb) < 2)
        goto get_tlv_error;

    u_char ch = 0;
    BSB_IMPORT_u08(*bsb, ch);

    *apc = (ch >> 5) & 0x1;

    if ((ch & 0x1f) ==  0x1f) {
        while (BSB_REMAINING(*bsb)) {
            BSB_IMPORT_u08(*bsb, ch);
            (*atag) = ((*atag) << 7) | ch;
            if ((ch & 0x80) == 0)
                break;
        }
    } else {
        *atag = ch & 0x1f;
        BSB_IMPORT_u08(*bsb, ch);
    }

    if (BSB_IS_ERROR(*bsb) || ch == 0x80) {
        goto get_tlv_error;
    }

    if (ch & 0x80) {
        int cnt = ch & 0x7f;
        (*alen) = 0;
        while (cnt > 0 && BSB_REMAINING(*bsb)) {
            BSB_IMPORT_u08(*bsb, ch);
            (*alen) = ((*alen) << 8) | ch;
            cnt--;
        }
    } else {
        (*alen) = ch;
    }

    if (*alen < 0)
        goto get_tlv_error;

    if (*alen > BSB_REMAINING(*bsb))
        *alen = BSB_REMAINING(*bsb);

    unsigned char *value;
    BSB_IMPORT_ptr(*bsb, value, *alen);
    if (BSB_IS_ERROR(*bsb)) {
        goto get_tlv_error;
    }

    return value;

get_tlv_error:
    (*apc)  = 0;
    (*alen) = 0;
    (*atag) = 0;
    return 0;
}
/******************************************************************************/
char *moloch_detect_asn_decode_oid(unsigned char *oid, int len) {
    static char buf[1000];
    uint32_t buflen = 0;
    int pos = 0;
    int first = TRUE;
    int value = 0;

    for (pos = 0; pos < len; pos++) {
        value = (value << 7) | (oid[pos] & 0x7f);
        if (oid[pos] & 0x80) {
            continue;
        } 

        if (first) {
            first = FALSE;
            if (value > 40) /* two values in first byte */
                buflen = snprintf(buf, sizeof(buf), "%d.%d", value/40, value % 40);
            else /* one value in first byte */
                buflen = snprintf(buf, sizeof(buf), "%d", value);
        } else if (buflen < sizeof(buf)) {
            buflen += snprintf(buf+buflen, sizeof(buf)-buflen, ".%d", value);
        }

        value = 0;
    }

    return buf;
}

/*############################## TLS ##############################*/

/******************************************************************************/
void
moloch_detect_tls_certinfo_process(MolochCertInfo_t *ci, BSB *bsb)
{
    int apc, atag, alen;
    char *lastOid = NULL;

    while (BSB_REMAINING(*bsb)) {
        unsigned char *value = moloch_detect_asn_get_tlv(bsb, &apc, &atag, &alen);
        if (!value)
            return;

        if (apc) {
            BSB tbsb;
            BSB_INIT(tbsb, value, alen);
            moloch_detect_tls_certinfo_process(ci, &tbsb);
        } else if (atag  == 6)  {
            lastOid = moloch_detect_asn_decode_oid(value, alen);
        } else if (lastOid && (atag == 20 || atag == 19 || atag == 12))  {
            /* 20 == BER_UNI_TAG_TeletexString
             * 19 == BER_UNI_TAG_PrintableString
             * 12 == BER_UNI_TAG_UTF8String
             */
            if (strcmp(lastOid, "2.5.4.3") == 0) {
                MolochString_t *element = MOLOCH_TYPE_ALLOC(MolochString_t);
                element->utf8 = atag == 12;
                if (element->utf8)
                    element->str = g_utf8_strdown((char*)value, alen);
                else
                    element->str = g_ascii_strdown((char*)value, alen);
                DLL_PUSH_TAIL(s_, &ci->commonName, element);
            } else if (strcmp(lastOid, "2.5.4.10") == 0) {
                if (ci->orgName) {
                    LOG("Multiple orgName %s => %.*s", ci->orgName, alen, value);
                    free(ci->orgName);
                }
                ci->orgUtf8 = atag == 12;
                ci->orgName = g_strndup((char*)value, alen);
            }
        }
    }
}
/******************************************************************************/
void
moloch_detect_tls_alt_names(MolochCertsInfo_t *certs, BSB *bsb)
{
    int apc, atag, alen;
    static char *lastOid = NULL;

    while (BSB_REMAINING(*bsb) >= 2) {
        unsigned char *value = moloch_detect_asn_get_tlv(bsb, &apc, &atag, &alen);

        if (!value)
            return;

        if (apc) {
            BSB tbsb;
            BSB_INIT(tbsb, value, alen);
            moloch_detect_tls_alt_names(certs, &tbsb);
            if (certs->alt.s_count > 0) {
                return;
            }
        } else if (atag == 6)  {
            lastOid = moloch_detect_asn_decode_oid(value, alen);
            if (strcmp(lastOid, "2.5.29.17") != 0)
                lastOid = NULL;
        } else if (lastOid && atag == 4) {
            BSB tbsb;
            BSB_INIT(tbsb, value, alen);
            moloch_detect_tls_alt_names(certs, &tbsb);
            return;
        } else if (lastOid && atag == 2) {
            MolochString_t *element = MOLOCH_TYPE_ALLOC0(MolochString_t);
            element->str = g_ascii_strdown((char*)value, alen);
            DLL_PUSH_TAIL(s_, &certs->alt, element);
        }
    }
    return;
}
/******************************************************************************/
void
moloch_detect_tls_process(MolochSession_t *session, unsigned char *data, int len)
{
    BSB sslbsb;

    BSB_INIT(sslbsb, data, len);

    while (BSB_REMAINING(sslbsb) > 5) {
        unsigned char *ssldata = BSB_WORK_PTR(sslbsb);
        int            ssllen = MIN(BSB_REMAINING(sslbsb) - 5, ssldata[3] << 8 | ssldata[4]);

        BSB pbsb;
        BSB_INIT(pbsb, ssldata+5, ssllen);

        while (BSB_REMAINING(pbsb) > 7) {
            unsigned char *pdata = BSB_WORK_PTR(pbsb);
            int            plen = MIN(BSB_REMAINING(pbsb) - 4, pdata[2] << 8 | pdata[3]);

            if (pdata[0] != 0x0b) {
                BSB_IMPORT_skip(pbsb, plen + 4);
                continue;
            }

            BSB cbsb;
            BSB_INIT(cbsb, pdata+7, plen-3); // The - 4 for plen is done above, confusing

            while(BSB_REMAINING(cbsb) > 3) {
                int            badreason = 0;
                unsigned char *cdata = BSB_WORK_PTR(cbsb);
                int            clen = MIN(BSB_REMAINING(cbsb) - 3, (cdata[0] << 16 | cdata[1] << 8 | cdata[2]));

                MolochCertsInfo_t *certs = MOLOCH_TYPE_ALLOC0(MolochCertsInfo_t);
                DLL_INIT(s_, &certs->alt);
                DLL_INIT(s_, &certs->subject.commonName);
                DLL_INIT(s_, &certs->issuer.commonName);

                int            atag, alen, apc;
                unsigned char *value;

                BSB            bsb;
                BSB_INIT(bsb, cdata + 3, clen);

                /* Certificate */
                if (!(value = moloch_detect_asn_get_tlv(&bsb, &apc, &atag, &alen)))
                    {badreason = 1; goto bad_cert;}
                BSB_INIT(bsb, value, alen);

                /* signedCertificate */
                if (!(value = moloch_detect_asn_get_tlv(&bsb, &apc, &atag, &alen)))
                    {badreason = 2; goto bad_cert;}
                BSB_INIT(bsb, value, alen);

                /* serialNumber or version*/
                if (!(value = moloch_detect_asn_get_tlv(&bsb, &apc, &atag, &alen)))
                    {badreason = 3; goto bad_cert;}

                if (apc) {
                    if (!(value = moloch_detect_asn_get_tlv(&bsb, &apc, &atag, &alen)))
                        {badreason = 4; goto bad_cert;}
                }
                certs->serialNumberLen = alen;
                certs->serialNumber = malloc(alen);
                memcpy(certs->serialNumber, value, alen);

                /* signature */
                if (!(value = moloch_detect_asn_get_tlv(&bsb, &apc, &atag, &alen)))
                    {badreason = 5; goto bad_cert;}

                /* issuer */
                if (!(value = moloch_detect_asn_get_tlv(&bsb, &apc, &atag, &alen)))
                    {badreason = 6; goto bad_cert;}
                BSB tbsb;
                BSB_INIT(tbsb, value, alen);
                moloch_detect_tls_certinfo_process(&certs->issuer, &tbsb);

                /* validity */
                if (!(value = moloch_detect_asn_get_tlv(&bsb, &apc, &atag, &alen)))
                    {badreason = 7; goto bad_cert;}

                /* subject */
                if (!(value = moloch_detect_asn_get_tlv(&bsb, &apc, &atag, &alen)))
                    {badreason = 8; goto bad_cert;}
                BSB_INIT(tbsb, value, alen);
                moloch_detect_tls_certinfo_process(&certs->subject, &tbsb);

                /* subjectPublicKeyInfo */
                if (!(value = moloch_detect_asn_get_tlv(&bsb, &apc, &atag, &alen)))
                    {badreason = 9; goto bad_cert;}

                /* extensions */
                if (BSB_REMAINING(bsb)) {
                    if (!(value = moloch_detect_asn_get_tlv(&bsb, &apc, &atag, &alen)))
                        {badreason = 10; goto bad_cert;}
                    BSB tbsb;
                    BSB_INIT(tbsb, value, alen);
                    moloch_detect_tls_alt_names(certs, &tbsb);
                }

                MolochCertsInfo_t *element;
                HASH_FIND(t_, session->certs, certs, element);
                if (element) {
                    moloch_nids_certs_free(certs);
                } else {
                    HASH_ADD(t_, session->certs, certs, certs);
                }

                BSB_IMPORT_skip(cbsb, clen + 3);

                continue;

            bad_cert:
                if (config.debug)
                    LOG("bad cert %d - %d %.*s", badreason, clen, clen, cdata);
                moloch_nids_certs_free(certs);
                break;
            }

            BSB_IMPORT_skip(pbsb, plen + 4);
        }

        BSB_IMPORT_skip(sslbsb, ssllen + 5);
    }

    session->certJsonSize += len*2;
}

/*############################## HTTP ##############################*/

/******************************************************************************/
void moloch_detect_parse_http(MolochSession_t *session, unsigned char *data, uint32_t remaining, int initial)
{
    MolochSessionHttp_t   *http = session->http;
#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: enter %d initial %d - %d %.*s", session->which, initial, remaining, remaining, data);
#endif

    if (!http) {
        if (initial) {
            moloch_nids_new_session_http(session);
            http = session->http;

            http_parser_init(&http->parsers[0], HTTP_BOTH);
            http_parser_init(&http->parsers[1], HTTP_BOTH);
            http->wParsers = 3;
            http->parsers[0].data = session;
            http->parsers[1].data = session;
        }
        else {
            return;
        }
    } else if ((http->wParsers & (1 << session->which)) == 0) {
        return;
    }

    while (remaining > 0) {
        int len = http_parser_execute(&http->parsers[session->which], &parserSettings, (char *)data, remaining);
#ifdef HTTPDEBUG
            LOG("HTTPDEBUG: parse result: %d input: %d errno: %d", len, remaining, http->parsers[session->which].http_errno);
#endif
        if (len <= 0) {
            http->wParsers &= ~(1 << session->which);
            if (http->wParsers) {
                moloch_nids_free_session_http(session);
            }
            break;
        }
        data += len;
        remaining -= len;
    }
}

/******************************************************************************/
void moloch_detect_parse_ssh(MolochSession_t *session, unsigned char *data, uint32_t remaining)
{
    if (memcmp("SSH", data, 3) == 0)
        return;

    while (remaining >= 6) {
        if (session->sshLen == 0) {
            session->sshLen = (data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]) + 4;
            session->sshCode = data[5];
            if (session->sshLen == 0) {
                break;
            }
        }

        if (session->sshCode == 33 && remaining > 8) {
            uint32_t keyLen = data[6] << 24 | data[7] << 16 | data[8] << 8 | data[9];
            session->isSsh = 0;
            if (remaining > keyLen + 8) {
                char *str = g_base64_encode(data+10, keyLen);

                if (!moloch_field_string_add(MOLOCH_FIELD_SSH_KEY, session, str, (keyLen/3+1)*4, FALSE)) {
                    g_free(str);
                }
            }
            break;
        }

        if (remaining > session->sshLen) {
            remaining -= session->sshLen;
            session->sshLen = 0;
            continue;
        } else {
            session->sshLen -= remaining;
            remaining = 0;
            continue;
        }
    }
}

/******************************************************************************/
void moloch_detect_parse_irc(MolochSession_t *session, unsigned char *data, uint32_t remaining)
{
    while (remaining) {
        if (session->ircState & 0x1) {
            unsigned char *newline = memchr(data, '\n', remaining);
            if (newline) {
                remaining -= (newline - data) +1;
                data = newline+1;
                session->ircState &= ~ 0x1;
            } else {
                return;
            }
        }

        if (remaining > 5 && memcmp("JOIN ", data, 5) == 0) {
            unsigned char *end = data + remaining;
            unsigned char *ptr = data + 5;

            while (ptr < end && *ptr != ' ' && *ptr != '\r' && *ptr != '\n') {
                ptr++;
            }

            moloch_field_string_add(MOLOCH_FIELD_IRC_CHANNELS, session, (char*)data + 5, ptr - data - 5, TRUE);
        }

        if (remaining > 5 && memcmp("NICK ", data, 5) == 0) {
            unsigned char *end = data + remaining;
            unsigned char *ptr = data + 5;

            while (ptr < end && *ptr != ' ' && *ptr != '\r' && *ptr != '\n') {
                ptr++;
            }

            moloch_field_string_add(MOLOCH_FIELD_IRC_NICK, session, (char*)data + 5, ptr - data - 5, TRUE);
        }

        if (remaining > 0) {
            session->ircState |=  0x1;
        }
    }
}

/******************************************************************************/
int
moloch_hp_cb_on_message_begin (http_parser *parser)
{
    MolochSession_t       *session = parser->data;
    MolochSessionHttp_t   *http = session->http;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which: %d", session->which);
#endif

    http->inHeader &= ~(1 << session->which);
    http->inValue  &= ~(1 << session->which);
    http->inBody   &= ~(1 << session->which);
    g_checksum_reset(http->checksum[session->which]);

    if (pluginsCbs & MOLOCH_PLUGIN_HP_OMB)
        moloch_plugins_cb_hp_omb(session, parser);

    return 0;
}
/******************************************************************************/
int
moloch_hp_cb_on_url (http_parser *parser, const char *at, size_t length)
{
    MolochSession_t       *session = parser->data;
    MolochSessionHttp_t   *http = session->http;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which:%d url %.*s", session->which, (int)length, at);
#endif

    if (!http->urlString)
        http->urlString = g_string_new_len(at, length);
    else
        g_string_append_len(http->urlString, at, length);

    return 0;
}

/******************************************************************************/
const char *moloch_memstr(const char *haystack, int haysize, const char *needle, int needlesize)
{
    const char *p;
    const char *end = haystack + haysize - needlesize;

    for (p = haystack; p <= end; p++)
    {
        if (p[0] == needle[0] && memcmp(p+1, needle+1, needlesize-1) == 0)
            return p;
    }
    return NULL;
}
/******************************************************************************/
const char *moloch_memcasestr(const char *haystack, int haysize, const char *needle, int needlesize)
{
    const char *p;
    const char *end = haystack + haysize - needlesize;
    int i;

    for (p = haystack; p <= end; p++)
    {
        for (i = 0; i < needlesize; i++) {
            if (tolower(p[i]) != needle[i]) {
                goto memcasestr_outer;
            }
        }
        return p;

        memcasestr_outer: ;
    }
    return NULL;
}
/******************************************************************************/
int
moloch_hp_cb_on_body (http_parser *parser, const char *at, size_t length)
{
    MolochSession_t     *session = parser->data;
    MolochSessionHttp_t *http = session->http;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which: %d", session->which);
#endif

    if (!(http->inBody & (1 << session->which))) {
        if (moloch_memstr(at, length, "password=", 9)) {
            moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "http:password");
        }

        const char *m = magic_buffer(cookie, at, length);
        if (m) {
            char tmp[500];
            snprintf(tmp, sizeof(tmp), "http:content:%s", m);
            char *semi = strchr(tmp, ';');
            if (semi) {
                *semi = 0;
            } 
            moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, tmp);
        }
        http->inBody |= (1 << session->which);
    }

    g_checksum_update(http->checksum[session->which], (guchar *)at, length);

    if (pluginsCbs & MOLOCH_PLUGIN_HP_OB)
        moloch_plugins_cb_hp_ob(session, parser, at, length);

    return 0;
}

/******************************************************************************/
int
moloch_hp_cb_on_message_complete (http_parser *parser)
{
    MolochSession_t       *session = parser->data;
    MolochSessionHttp_t   *http = session->http;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which: %d", session->which);
#endif

    if (pluginsCbs & MOLOCH_PLUGIN_HP_OMC)
        moloch_plugins_cb_hp_omc(session, parser);

    http->header[0][0] = http->header[1][0] = 0;

    if (http->urlString) {
        char *ch = http->urlString->str;
        while (*ch) {
            if (*ch < 32) {
                moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "http:control-char");
                break;
            }
            ch++;
        }
    }

    if (http->hostString) {
        g_string_ascii_down(http->hostString);
    }

    if (http->urlString && http->hostString) {
        char *colon = strchr(http->hostString->str+2, ':');
        if (colon) {
            moloch_field_string_add(MOLOCH_FIELD_HTTP_HOST, session, http->hostString->str+2, colon - http->hostString->str-2, TRUE);
        } else {
            moloch_field_string_add(MOLOCH_FIELD_HTTP_HOST, session, http->hostString->str+2, http->hostString->len-2, TRUE);
        }

        char *question = strchr(http->urlString->str, '?');
        if (question) {
            moloch_field_string_add(MOLOCH_FIELD_HTTP_PATH, session, http->urlString->str, question - http->urlString->str, TRUE);
            char *start = question+1;
            char *ch;
            int   field = MOLOCH_FIELD_HTTP_KEY;
            for (ch = start; *ch; ch++) {
                if (*ch == '&') {
                    if (ch != start && (config.parseQSValue || field == MOLOCH_FIELD_HTTP_KEY)) {
                        char *str = g_uri_unescape_segment(start, ch, NULL);
                        if (!str) {
                            moloch_field_string_add(field, session, start, ch-start, TRUE);
                        } else if (!moloch_field_string_add(field, session, str, strlen(str), FALSE)) {
                            g_free(str);
                        }
                    }
                    start = ch+1;
                    field = MOLOCH_FIELD_HTTP_KEY;
                    continue;
                } else if (*ch == '=') {
                    if (ch != start && (config.parseQSValue || field == MOLOCH_FIELD_HTTP_KEY)) {
                        char *str = g_uri_unescape_segment(start, ch, NULL);
                        if (!str) {
                            moloch_field_string_add(field, session, start, ch-start, TRUE);
                        } else if (!moloch_field_string_add(field, session, str, strlen(str), FALSE)) {
                            g_free(str);
                        }
                    }
                    start = ch+1;
                    field = MOLOCH_FIELD_HTTP_VALUE;
                }
            }
        } else {
            moloch_field_string_add(MOLOCH_FIELD_HTTP_PATH, session, http->urlString->str, http->urlString->len, TRUE);
        }

        if (http->urlString->str[0] != '/') {
            char *result = strstr(http->urlString->str, http->hostString->str+2);

            /* If the host header is in the first 8 bytes of url then just use the url */
            if (result && result - http->urlString->str <= 8) {
                moloch_field_string_add(MOLOCH_FIELD_HTTP_URLS, session, http->urlString->str, http->urlString->len, FALSE);
                g_string_free(http->urlString, FALSE);
                g_string_free(http->hostString, TRUE);
            } else {
                /* Host header doesn't match the url */
                g_string_append(http->hostString, ";");
                g_string_append(http->hostString, http->urlString->str);
                moloch_field_string_add(MOLOCH_FIELD_HTTP_URLS, session, http->hostString->str, http->hostString->len, FALSE);
                g_string_free(http->urlString, TRUE);
                g_string_free(http->hostString, FALSE);
            }
        } else {
            /* Normal case, url starts with /, so no extra host in url */
            g_string_append(http->hostString, http->urlString->str);
            moloch_field_string_add(MOLOCH_FIELD_HTTP_URLS, session, http->hostString->str, http->hostString->len, FALSE);
            g_string_free(http->urlString, TRUE);
            g_string_free(http->hostString, FALSE);
        }

        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:http");

        http->urlString = NULL;
        http->hostString = NULL;
    } else if (http->urlString) {
        moloch_field_string_add(MOLOCH_FIELD_HTTP_URLS, session, http->urlString->str, http->urlString->len, FALSE);
        g_string_free(http->urlString, FALSE);

        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:http");

        http->urlString = NULL;
    } else if (http->hostString) {
        char *colon = strchr(http->hostString->str+2, ':');
        if (colon) {
            moloch_field_string_add(MOLOCH_FIELD_HTTP_HOST, session, http->hostString->str+2, colon - http->hostString->str-2, TRUE);
        } else {
            moloch_field_string_add(MOLOCH_FIELD_HTTP_HOST, session, http->hostString->str+2, http->hostString->len-2, TRUE);
        }

        g_string_free(http->hostString, TRUE);
        http->hostString = NULL;
    }

    if (http->inBody & (1 << session->which)) {
        const char *md5 = g_checksum_get_string(http->checksum[session->which]);
        moloch_field_string_add(MOLOCH_FIELD_HTTP_MD5, session, (char*)md5, 32, TRUE);
    }

    return 0;
}

/******************************************************************************/
void
moloch_detect_http_add_value(MolochSession_t *session)
{
    MolochSessionHttp_t   *http = session->http;
    int                    pos  = http->pos[session->which];
    char                  *s    = http->valueString[session->which]->str;
    int                    l    = http->valueString[session->which]->len;

    while (isspace(*s)) {
        s++;
        l--;
    }


    switch (config.fields[pos]->type) {
    case MOLOCH_FIELD_TYPE_INT:
    case MOLOCH_FIELD_TYPE_INT_ARRAY:
    case MOLOCH_FIELD_TYPE_INT_HASH:
        moloch_field_int_add(pos, session, atoi(s));
        g_string_free(http->valueString[session->which], TRUE);
        break;
    case MOLOCH_FIELD_TYPE_STR:
    case MOLOCH_FIELD_TYPE_STR_ARRAY:
    case MOLOCH_FIELD_TYPE_STR_HASH:
        moloch_field_string_add(pos, session, s, l, TRUE);
        g_string_free(http->valueString[session->which], TRUE);
        break;
    case MOLOCH_FIELD_TYPE_IP_HASH:
    {
        int i;
        gchar **parts = g_strsplit(http->valueString[session->which]->str, ",", 0);

        for (i = 0; parts[i]; i++) {
            gchar *ip = parts[i];
            while (*ip == ' ')
                ip++;

            in_addr_t ia = inet_addr(ip);
            if (ia == 0 || ia == 0xffffffff) {
                moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "http:bad-xff");
                LOG("ERROR - Didn't understand ip: %s %s %d", http->valueString[session->which]->str, ip, ia);
                continue;
            }

            moloch_field_int_add(pos, session, ia);
        }

        g_strfreev(parts);
        g_string_free(http->valueString[session->which], TRUE);
        break;
    }
    } /* SWITCH */


    http->valueString[session->which] = 0;
    http->pos[session->which] = 0;
}
/******************************************************************************/
int
moloch_hp_cb_on_header_field (http_parser *parser, const char *at, size_t length)
{
    MolochSession_t       *session = parser->data;
    MolochSessionHttp_t   *http = session->http;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which: %d field: %.*s", session->which, (int)length, at);
#endif

    if ((http->inHeader & (1 << session->which)) == 0) {
        http->inValue |= (1 << session->which);
        if (http->urlString && parser->status_code == 0 && pluginsCbs & MOLOCH_PLUGIN_HP_OU) {
            moloch_plugins_cb_hp_ou(session, parser, http->urlString->str, http->urlString->len);
        }
    }

    if (http->inValue & (1 << session->which)) {
        http->inValue &= ~(1 << session->which);

        http->header[session->which][0] = 0;

        if (http->pos[session->which]) {
            moloch_detect_http_add_value(session);
        }
    }

    size_t remaining = sizeof(http->header[session->which]) - strlen(http->header[session->which]) - 1;
    if (remaining > 0)
        strncat(http->header[session->which], at, MIN(length, remaining));

    return 0;
}

/******************************************************************************/
int
moloch_hp_cb_on_header_value (http_parser *parser, const char *at, size_t length)
{
    MolochSession_t       *session = parser->data;
    MolochSessionHttp_t   *http = session->http;
    char                   header[200];
    MolochString_t        *hstring = 0;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which: %d value: %.*s", session->which, (int)length, at);
#endif

    if ((http->inValue & (1 << session->which)) == 0) {
        http->inValue |= (1 << session->which);

        char *lower = g_ascii_strdown(http->header[session->which], -1);
        moloch_plugins_cb_hp_ohf(session, parser, lower, strlen(lower));

        if (session->which == 0)
            HASH_FIND(s_, httpReqHeaders, lower, hstring);
        else
            HASH_FIND(s_, httpResHeaders, lower, hstring);

        http->pos[session->which] = (hstring?hstring->len:0);

        snprintf(header, sizeof(header), "http:header:%s", lower);
        g_free(lower);
        moloch_nids_add_tag(session, MOLOCH_FIELD_HTTP_TAGS_REQ+session->which, header);
    }

    moloch_plugins_cb_hp_ohv(session, parser, at, length);

    if (parser->method && strcasecmp("host", http->header[session->which]) == 0) {
        if (!http->hostString)
            http->hostString = g_string_new_len("//", 2);
        g_string_append_len(http->hostString, at, length);
    } 

    if (http->pos[session->which]) {
        if (!http->valueString[session->which])
            http->valueString[session->which] = g_string_new_len(at, length);
        else
            g_string_append_len(http->valueString[session->which], at, length);
    }

    return 0;
}
/******************************************************************************/
int
moloch_hp_cb_on_headers_complete (http_parser *parser)
{
    MolochSession_t       *session = parser->data;
    MolochSessionHttp_t   *http = session->http;
    char                   tag[200];
    char                   version[20];


#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which: %d code: %d method: %d", session->which, parser->status_code, parser->method);
#endif

    int len = snprintf(version, sizeof(version), "%d.%d", parser->http_major, parser->http_minor);

    if (parser->status_code == 0) {
        snprintf(tag, sizeof(tag), "http:method:%s", http_method_str(parser->method));
        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, tag);
        moloch_field_string_add(MOLOCH_FIELD_HTTP_VER_REQ, session, version, len, TRUE);
    } else {
        snprintf(tag, sizeof(tag), "http:statuscode:%d", parser->status_code);
        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, tag);
        moloch_field_string_add(MOLOCH_FIELD_HTTP_VER_RES, session, version, len, TRUE);
    }

    if (http->inValue & (1 << session->which) && http->pos[session->which]) {
        moloch_detect_http_add_value(session);
    }

    if (pluginsCbs & MOLOCH_PLUGIN_HP_OHC)
        moloch_plugins_cb_hp_ohc(session, parser);

    return 0;
}


/*############################## DNS ##############################*/

/******************************************************************************/
int moloch_detect_dns_name_element(BSB *nbsb, BSB *bsb)
{
    int nlen = 0;
    BSB_IMPORT_u08(*bsb, nlen);

    if (nlen == 0 || nlen > BSB_REMAINING(*bsb)) {
        return 1;
    }

    int j;
    for (j = 0; j < nlen; j++) {
        register u_char c = 0;
        BSB_IMPORT_u08(*bsb, c);

        if (!isascii(c)) {
            BSB_EXPORT_u08(*nbsb, 'M');
            BSB_EXPORT_u08(*nbsb, '-');
            c = toascii(c);
        }
        if (!isprint(c)) {
            BSB_EXPORT_u08(*nbsb, '^');
            c ^= 0x40;
        } 

        BSB_EXPORT_u08(*nbsb, c);
    }

    return 0;
}
/******************************************************************************/
unsigned char *moloch_detect_dns_name(unsigned char *full, int fulllen, BSB *inbsb, int *namelen)
{
    static unsigned char  name[8000];
    BSB  nbsb;
    int  didPointer = 0;
    BSB  tmpbsb;
    BSB *curbsb;

    BSB_INIT(nbsb, name, sizeof(name));

    curbsb = inbsb;

    while (BSB_REMAINING(*curbsb)) {
        unsigned char ch = 0;
        BSB_IMPORT_u08(*curbsb, ch);

        if (ch == 0)
            break;

        BSB_EXPORT_rewind(*curbsb, 1);

        if (ch & 0xc0) {
            if (didPointer > 5)
                return 0;
            didPointer++;
            int tpos = 0;
            BSB_IMPORT_u16(*curbsb, tpos);
            tpos &= 0x3fff;

            BSB_INIT(tmpbsb, full+tpos, fulllen - tpos);
            curbsb = &tmpbsb;
            continue;
        } 

        if (BSB_LENGTH(nbsb)) {
            BSB_EXPORT_u08(nbsb, '.');
        }

        if (moloch_detect_dns_name_element(&nbsb, curbsb) && BSB_LENGTH(nbsb))
            BSB_EXPORT_rewind(nbsb, 1); // Remove last .
    }
    *namelen = BSB_LENGTH(nbsb);
    BSB_EXPORT_u08(nbsb, 0);
    return name;
}
/******************************************************************************/
void moloch_detect_dns(MolochSession_t *session, unsigned char *data, int len) 
{

    if (len < 18)
        return;

    int qr      = (data[2] >> 7) & 0x1;
    int opcode  = (data[2] >> 3) & 0xf;

    if (opcode != 0)
        return;

    int qdcount = (data[4] << 8) | data[5];
    int ancount = (data[6] << 8) | data[7];

    if (qdcount > 10 || qdcount <= 0)
        return;

    BSB bsb;
    BSB_INIT(bsb, data + 12, len - 12);

    /* QD Section */
    int i;
    for (i = 0; BSB_NOT_ERROR(bsb) && i < qdcount; i++) {
        int namelen;
        unsigned char *name = moloch_detect_dns_name(data, len, &bsb, &namelen);

        if (!namelen || BSB_IS_ERROR(bsb))
            break;

        unsigned short qtype = 0 , qclass = 0 ;
        BSB_IMPORT_u16(bsb, qtype);
        BSB_IMPORT_u16(bsb, qclass);

        char *lower = g_ascii_strdown((char*)name, namelen);

        if (qclass <= 255 && qclasses[qclass]) {
            moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, qclasses[qclass]);
        }

        if (qtype <= 255 && qtypes[qtype]) {
            moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, qtypes[qtype]);
        }

        if (lower && !moloch_field_string_add(MOLOCH_FIELD_DNS_HOST, session, lower, namelen, FALSE)) {
            g_free(lower);
        }
    }
    moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:dns");

    if (qr == 0)
        return;

    for (i = 0; BSB_NOT_ERROR(bsb) && i < ancount; i++) {
        int namelen;
        moloch_detect_dns_name(data, len, &bsb, &namelen);

        if (BSB_IS_ERROR(bsb))
            break;


        uint16_t antype = 0;
        BSB_IMPORT_u16 (bsb, antype);
        uint16_t anclass = 0;
        BSB_IMPORT_u16 (bsb, anclass);
        BSB_IMPORT_skip(bsb, 4); // ttl
        uint16_t rdlength = 0;
        BSB_IMPORT_u16 (bsb, rdlength);

        if (antype == 1 && anclass == 1 && rdlength == 4 && BSB_REMAINING(bsb) >= 4) {
            struct in_addr in;
            unsigned char *ptr = BSB_WORK_PTR(bsb);
            in.s_addr = ptr[3] << 24 | ptr[2] << 16 | ptr[1] << 8 | ptr[0];

            moloch_field_int_add(MOLOCH_FIELD_DNS_IP, session, in.s_addr);
        } else if (antype == 5 && anclass == 1 && BSB_REMAINING(bsb) >= rdlength) {
            BSB rdbsb;
            BSB_INIT(rdbsb, BSB_WORK_PTR(bsb), rdlength);

            int namelen;
            unsigned char *name = moloch_detect_dns_name(data, len, &rdbsb, &namelen);

            if (!namelen || BSB_IS_ERROR(rdbsb))
                continue;

            char *lower = g_ascii_strdown((char*)name, namelen);

            if (lower && !moloch_field_string_add(MOLOCH_FIELD_DNS_HOST, session, lower, namelen, FALSE)) {
                g_free(lower);
            }
        }
        BSB_IMPORT_skip(bsb, rdlength);
    }
}

/*############################## EMAIL ##############################*/

/******************************************************************************/
#define EMAIL_CMD                  0
#define EMAIL_CMD_RETURN           1
#define EMAIL_DATA_HEADER          2
#define EMAIL_DATA_HEADER_RETURN   3
#define EMAIL_DATA_HEADER_DONE     4
#define EMAIL_DATA                 5
#define EMAIL_DATA_RETURN          6
#define EMAIL_IGNORE               7
#define EMAIL_TLS_OK               8
#define EMAIL_TLS_OK_RETURN        9
#define EMAIL_TLS                 10
#define EMAIL_MIME                11
#define EMAIL_MIME_RETURN         12
#define EMAIL_MIME_DONE           13
#define EMAIL_MIME_DATA           14
#define EMAIL_MIME_DATA_RETURN    15
/******************************************************************************/
char *moloch_detect_remove_matching(char *str, char start, char stop) 
{
    while (isspace(*str))
        str++;

    if (*str == start)
        str++;

    char *startstr = str;

    while (*str && *str != stop)
        str++;
    *str = 0;

    return startstr;
}
/******************************************************************************/
void
moloch_detect_email_add_value(MolochSession_t *session, int pos, char *s, int l)
{
    while (isspace(*s)) {
        s++;
        l--;
    }

    switch (config.fields[pos]->type) {
    case MOLOCH_FIELD_TYPE_INT:
    case MOLOCH_FIELD_TYPE_INT_ARRAY:
    case MOLOCH_FIELD_TYPE_INT_HASH:
        moloch_field_int_add(pos, session, atoi(s));
        break;
    case MOLOCH_FIELD_TYPE_STR:
    case MOLOCH_FIELD_TYPE_STR_ARRAY:
    case MOLOCH_FIELD_TYPE_STR_HASH:
        moloch_field_string_add(pos, session, s, l, TRUE);
        break;
    case MOLOCH_FIELD_TYPE_IP_HASH:
    {
        int i;
        gchar **parts = g_strsplit(s, ",", 0);

        for (i = 0; parts[i]; i++) {
            gchar *ip = parts[i];
            while (*ip == ' ')
                ip++;

            in_addr_t ia = inet_addr(ip);
            if (ia == 0 || ia == 0xffffffff) {
                moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "http:bad-xff");
                LOG("ERROR - Didn't understand ip: %s %s %d", s, ip, ia);
                continue;
            }

            moloch_field_int_add(pos, session, ia);
        }

        g_strfreev(parts);
        break;
    }
    } /* SWITCH */
}
/******************************************************************************/
void moloch_detect_parse_email_addresses(int field, MolochSession_t *session, char *data, int len)
{
    char *end = data+len;

    while (data < end) {
        while (data < end && isspace(*data)) data++;
        char *start = data;

        /* Starts with quote is easy */
        if (data < end && *data == '"') {
            data++;
            while (data < end && *data != '"') data++;
            data++;
            while (data < end && isspace(*data)) data++;
            start = data;
        }

        while (data < end && *data != '<' && *data != ',') data++;

        if (*data == '<') {
            data++;
            start = data;
            while (data < end && *data != '>') data++;
        }

        char *lower = g_ascii_strdown(start, data - start);
        if (!moloch_field_string_add(field, session, lower, data - start, FALSE)) {
            g_free(lower);
        }

        while (data < end && *data != ',') data++;
        if (data < end && *data == ',') data++;
    }
}
/******************************************************************************/
void moloch_detect_parse_email(MolochSession_t *session, unsigned char *data, uint32_t remaining)
{
#ifdef EMAILDEBUG
    LOG("EMAILDEBUG: enter %d", session->which);
#endif

    MolochSessionEmail_t *email        = session->email;
    GString              *line         = email->line[session->which];
    char                 *state        = &email->state[session->which];
    MolochString_t       *emailHeader  = 0;

    while (remaining > 0) {
        switch (*state) {
        case EMAIL_CMD: {
            if (*data == '\r') {
                *state = EMAIL_CMD_RETURN;
                break;
            }
            g_string_append_c(line, *data);
            break;
        }
        case EMAIL_CMD_RETURN: {
#ifdef EMAILDEBUG
            printf("%d %d cmd => %s\n", session->which, *state, line->str);
#endif
            if (strncasecmp(line->str, "MAIL FROM:", 10) == 0) {
                *state = EMAIL_CMD;
                char *lower = g_ascii_strdown(moloch_detect_remove_matching(line->str+11, '<', '>'), -1);
                if (!moloch_field_string_add(MOLOCH_FIELD_EMAIL_SRC, session, lower, -1, FALSE)) {
                    g_free(lower);
                }
            } else if (strncasecmp(line->str, "RCPT TO:", 8) == 0) {
                char *lower = g_ascii_strdown(moloch_detect_remove_matching(line->str+9, '<', '>'), -1);
                if (!moloch_field_string_add(MOLOCH_FIELD_EMAIL_DST, session, lower, -1, FALSE)) {
                    g_free(lower);
                }
                *state = EMAIL_CMD;
            } else if (strncasecmp(line->str, "DATA", 4) == 0) {
                *state = EMAIL_DATA_HEADER;
            } else if (strncasecmp(line->str, "STARTTLS", 8) == 0) {
                *state = EMAIL_IGNORE;
                email->state[(session->which+1)%2] = EMAIL_TLS_OK;
                return;
            } else {
                *state = EMAIL_CMD;
            }

            g_string_truncate(line, 0);
            if (*data != '\n')
                continue;
            break;
        }
        case EMAIL_DATA_HEADER: {
            if (*data == '\r') {
                *state = EMAIL_DATA_HEADER_RETURN;
                break;
            }
            g_string_append_c(line, *data);
            break;
        }
        case EMAIL_DATA_HEADER_RETURN: {
#ifdef EMAILDEBUG
            printf("%d %d header => %s\n", session->which, *state, line->str);
#endif
            if (strcmp(line->str, ".") == 0) {
                *state = EMAIL_CMD;
            } else if (*line->str == 0) {
                *state = EMAIL_DATA;
                if (pluginsCbs & MOLOCH_PLUGIN_SMTP_OHC) {
                    moloch_plugins_cb_smtp_ohc(session);
                }
            } else {
                *state = EMAIL_DATA_HEADER_DONE;
            }

            if (*data != '\n')
                continue;
            break;
        }
        case EMAIL_DATA_HEADER_DONE: {
#ifdef EMAILDEBUG
            printf("%d %d header done => %s (%c)\n", session->which, *state, line->str, *data);
#endif
            *state = EMAIL_DATA_HEADER;

            if (*data == ' ' || *data == '\t') {
                g_string_append_c(line, *data);
                break;
            }

            char *colon = strchr(line->str, ':');
            if (!colon) {
                g_string_truncate(line, 0);
                break;
            }

            char *lower = g_ascii_strdown(line->str, colon - line->str);
            HASH_FIND(s_, emailHeaders, lower, emailHeader);

            if (emailHeader) {
                int cpos = colon - line->str + 1;
                moloch_detect_email_add_value(session, emailHeader->len, line->str + cpos , line->len - cpos);
            } else if (strcmp(lower, "cc") == 0) {
                moloch_detect_parse_email_addresses(MOLOCH_FIELD_EMAIL_DST, session, line->str+3, line->len-3);
            } else if (strcmp(lower, "to") == 0) {
                moloch_detect_parse_email_addresses(MOLOCH_FIELD_EMAIL_DST, session, line->str+3, line->len-3);
            } else if (strcmp(lower, "from") == 0) {
                moloch_detect_parse_email_addresses(MOLOCH_FIELD_EMAIL_SRC, session, line->str+5, line->len-5);
            } else if (strcmp(lower, "message-id") == 0) {
                moloch_field_string_add(MOLOCH_FIELD_EMAIL_ID, session, moloch_detect_remove_matching(line->str+11, '<', '>'), -1, TRUE);
            } else if (strcmp(lower, "content-type") == 0) {
                char *s = line->str + 13;
                while(isspace(*s)) s++;

                moloch_field_string_add(MOLOCH_FIELD_EMAIL_CT, session, s, -1, TRUE);
                char *boundary = (char *)moloch_memcasestr(s, line->len - (s - line->str), "boundary=", 9);
                if (boundary) {
                    MolochString_t *string = MOLOCH_TYPE_ALLOC0(MolochString_t);
                    string->str = g_strdup(moloch_detect_remove_matching(boundary+9, '"', '"'));
                    string->len = strlen(string->str);
                    DLL_PUSH_TAIL(s_, &email->boundaries, string);
                }
            } else {
                int i;
                for (i = 0; config.smtpIpHeaders && config.smtpIpHeaders[i]; i++) {
                    if (strcasecmp(lower, config.smtpIpHeaders[i]) == 0) {
                        int l = strlen(config.smtpIpHeaders[i]);
                        char *ip = moloch_detect_remove_matching(line->str+l, '[', ']');
                        in_addr_t ia = inet_addr(ip);
                        if (ia == 0 || ia == 0xffffffff)
                            break;
                        moloch_field_int_add(MOLOCH_FIELD_EMAIL_IP, session, ia);
                    }
                }
            }

            if (pluginsCbs & MOLOCH_PLUGIN_SMTP_OH) {
                moloch_plugins_cb_smtp_oh(session, lower, colon - line->str, colon + 1, line->len - (colon - line->str) - 1);
            }

            g_free(lower);

            g_string_truncate(line, 0);
            if (*data != '\n')
                continue;
            break;
        }
        case EMAIL_MIME_DATA:
        case EMAIL_DATA: {
            if (*data == '\r') {
                (*state)++;
                break;
            }
            g_string_append_c(line, *data);
            break;
        }
        case EMAIL_MIME_DATA_RETURN:
        case EMAIL_DATA_RETURN: {
#ifdef EMAILDEBUG
            printf("%d %d %sdata => %s\n", session->which, *state, (*state == EMAIL_MIME_DATA_RETURN?"mime ": ""), line->str);
#endif
            if (strcmp(line->str, ".") == 0) {
                *state = EMAIL_CMD;
            } else {
                MolochString_t *string;
                gboolean        found = FALSE;

                if (line->str[0] == '-') {
                    DLL_FOREACH(s_,&email->boundaries,string) {
                        if ((int)line->len >= (int)(string->len + 2) && memcmp(line->str+2, string->str, string->len) == 0) {
                            found = TRUE;
                            break;
                        }
                    }
                }

                if (found) {
                    if (email->base64Decode & (1 << session->which)) {
                        const char *md5 = g_checksum_get_string(email->checksum[session->which]);
                        moloch_field_string_add(MOLOCH_FIELD_EMAIL_MD5, session, (char*)md5, 32, TRUE);
                    }
                    email->base64Decode &= ~(1 << session->which);
                    email->state64[session->which] = 0;
                    email->save64[session->which] = 0;
                    g_checksum_reset(email->checksum[session->which]);
                    *state = EMAIL_MIME;
                } else if (*state == EMAIL_MIME_DATA_RETURN) {
                    if (email->base64Decode & (1 << session->which)) {
                        guchar buf[20000];
                        if (sizeof(buf) > line->len) {
                            gsize  b = g_base64_decode_step (line->str, line->len, buf, 
                                                            &(email->state64[session->which]),
                                                            &(email->save64[session->which]));
                            g_checksum_update(email->checksum[session->which], buf, b);
                        }

                    }
                    *state = EMAIL_MIME_DATA;
                } else {
                    *state = EMAIL_DATA;
                }
            }

            g_string_truncate(line, 0);
            if (*data != '\n')
                continue;
            break;
        }
        case EMAIL_IGNORE: {
            return;
        }
        case EMAIL_TLS_OK: {
            if (*data == '\r') {
                *state = EMAIL_TLS_OK_RETURN;
                break;
            }
            g_string_append_c(line, *data);
            break;
        }
        case EMAIL_TLS_OK_RETURN: {
#ifdef EMAILDEBUG
            printf("%d %d tls => %s\n", session->which, *state, line->str);
#endif
            *state = EMAIL_TLS;
            if (*data != '\n')
                continue;
            break;
        }
        case EMAIL_TLS: {
            *state = EMAIL_IGNORE;
            moloch_detect_tls_process(session, (unsigned char*)data, remaining);
            moloch_nids_free_session_email(session);
            return;
        }
        case EMAIL_MIME: {

            if (*data == '\r') {
                *state = EMAIL_MIME_RETURN;
                break;
            }
            g_string_append_c(line, *data);
            break;
        }
        case EMAIL_MIME_RETURN: {
#ifdef EMAILDEBUG
            printf("%d %d mime => %s\n", session->which, *state, line->str);
#endif
            if (*line->str == 0) {
                *state = EMAIL_MIME_DATA;
            } else {
                *state = EMAIL_MIME_DONE;
            }
            
            if (*data != '\n')
                continue;
            break;
        }
        case EMAIL_MIME_DONE: {
#ifdef EMAILDEBUG
            printf("%d %d mime done => %s (%c)\n", session->which, *state, line->str, *data);
#endif
            *state = EMAIL_MIME;

            if (*data == ' ' || *data == '\t') {
                g_string_append_c(line, *data);
                break;
            }

            if (strncasecmp(line->str, "content-type:", 13) == 0) {
                char *s = line->str + 13;
                while(isspace(*s)) s++;
                char *boundary = (char *)moloch_memcasestr(s, line->len - (s - line->str), "boundary=", 9);
                if (boundary) {
                    MolochString_t *string = MOLOCH_TYPE_ALLOC0(MolochString_t);
                    string->str = g_strdup(moloch_detect_remove_matching(boundary+9, '"', '"'));
                    string->len = strlen(string->str);
                    DLL_PUSH_TAIL(s_, &email->boundaries, string);
                }
            } else if (strncasecmp(line->str, "content-disposition:", 20) == 0) {
                char *s = line->str + 13;
                while(isspace(*s)) s++;
                char *filename = (char *)moloch_memcasestr(s, line->len - (s - line->str), "filename=", 9);
                if (filename) {
                    moloch_field_string_add(MOLOCH_FIELD_EMAIL_FN, session, moloch_detect_remove_matching(filename+9, '"', '"'), -1, TRUE);
                }
            } else if (strncasecmp(line->str, "content-transfer-encoding:", 26) == 0) {
                if(moloch_memcasestr(line->str+26, line->len - 26, "base64", 6)) {
                    email->base64Decode |= (1 << session->which);
                }
            }

            g_string_truncate(line, 0);
            if (*data != '\n')
                continue;
            break;
        }
        }
        data++;
        remaining--;
    }
}

/*############################## SMB ##############################*/

// States
#define SMB_NETBIOS             0
#define SMB_SMBHEADER           1
#define SMB_SKIP                2
#define SMB1_TREE_CONNECT_ANDX 10
#define SMB1_DELETE            11
#define SMB1_OPEN_ANDX         12
#define SMB1_CREATE_ANDX       13
#define SMB1_SETUP_ANDX        14

#define SMB2_TREE_CONNECT      20
#define SMB2_CREATE            21

// SMB1 Flags
#define SMB1_FLAGS_REPLY       0x80
#define SMB1_FLAGS2_UNICODE    0x8000

// SMB2 Flags
#define SMB2_FLAGS_SERVER_TO_REDIR 0x00000001


// 2.2.13 AUTHENTICATE_MESSAGE from  http://download.microsoft.com/download/9/5/E/95EF66AF-9026-4BB0-A41D-A4F81802D92C/[MS-NLMP].pdf
void moloch_detect_smb_security_blob(MolochSession_t *session, unsigned char *data, int len)
{
    BSB bsb;

    BSB_INIT(bsb, data, len);

    int apc, atag, alen;
    unsigned char *value = moloch_detect_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (atag != 1)
        return;

    BSB_INIT(bsb, value, alen);
    value = moloch_detect_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (atag != 16)
        return;

    BSB_INIT(bsb, value, alen);
    value = moloch_detect_asn_get_tlv(&bsb, &apc, &atag, &alen);
    if (atag != 2)
        return;

    BSB_INIT(bsb, value, alen);
    value = moloch_detect_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (atag != 4 || memcmp("NTLMSSP", value, 7) != 0)
        return;

    /* Woot, have the part we need to decode */
    BSB_INIT(bsb, value, alen);
    BSB_IMPORT_skip(bsb, 8); 

    int type = 0;
    BSB_LIMPORT_u32(bsb, type);

    if (type != 3) // auth type
        return;

    gsize bread, bwritten;
    int lens[6], offsets[6];
    int i;
    for (i = 0; i < 6; i++) {
        BSB_LIMPORT_u16(bsb, lens[i]);
        BSB_IMPORT_skip(bsb, 2);
        BSB_LIMPORT_u32(bsb, offsets[i]);
    }

    if (BSB_IS_ERROR(bsb))
        return;

    if (lens[1]) {
        GError      *error = 0;
        char *out = g_convert((char *)value + offsets[2], lens[2], "utf8", "ucs2", &bread, &bwritten, &error);
        if (error)
            LOG("ERROR %s", error->message);
        else {
            if (!moloch_field_string_add(MOLOCH_FIELD_SMB_DOMAIN, session, out, -1, FALSE)) {
                g_free(out);
            }
        }
    }
    if (lens[2]) {
        GError      *error = 0;
        char *out = g_convert((char *)value + offsets[3], lens[3], "utf8", "ucs2", &bread, &bwritten, &error);
        if (error)
            LOG("ERROR %s", error->message);
        else {
            if (!moloch_field_string_add(MOLOCH_FIELD_SMB_USER, session, out, -1, FALSE)) {
                g_free(out);
            }
        }
    }
    if (lens[3]) {
        GError      *error = 0;
        char *out = g_convert((char *)value + offsets[4], lens[4], "utf8", "ucs2", &bread, &bwritten, &error);
        if (error)
            LOG("ERROR %s", error->message);
        else {
            if (!moloch_field_string_add(MOLOCH_FIELD_SMB_HOST, session, out, -1, FALSE)) {
                g_free(out);
            }
        }
    }
}

int moloch_detect_parse_smb1(MolochSession_t *session, BSB *bsb, char *state, uint32_t *remlen)
{
    MolochSessionSMB_t   *smb          = session->smb;

    unsigned char *start = BSB_WORK_PTR(*bsb);
    unsigned char cmd    = 0;

    switch (*state) {
    case SMB_SMBHEADER: {
        unsigned char flags = 0;
        if (BSB_REMAINING(*bsb) < 32) {
            return 1;
            break;
        }
        BSB_IMPORT_skip(*bsb, 4);
        BSB_IMPORT_u08(*bsb, cmd);
        BSB_IMPORT_skip(*bsb, 4);
        BSB_IMPORT_u08(*bsb, flags);
        BSB_LIMPORT_u16(*bsb, smb->flags2[session->which]);
        BSB_IMPORT_skip(*bsb, 20);
        if ((flags & SMB1_FLAGS_REPLY) == 0) {
            switch (cmd) {
            case 0x06:
                *state = SMB1_DELETE;
                break;
            case 0x2d:
                *state = SMB1_OPEN_ANDX;
                break;
            case 0x73:
                *state = SMB1_SETUP_ANDX;
                break;
            case 0x75:
                *state = SMB1_TREE_CONNECT_ANDX;
                break;
            case 0xa2:
                *state = SMB1_CREATE_ANDX;
                break;
            default:
                *state = SMB_SKIP;
            }
        } else {
            *state = SMB_SKIP;
        }
#ifdef SMBDEBUG
        LOG("%d cmd: %x flags2: %x newstate: %d remlen: %d", session->which, cmd, smb->flags2[session->which], *state, *remlen);
#endif
        break;
    }
    case SMB1_CREATE_ANDX:
    case SMB1_OPEN_ANDX: {
        if (BSB_REMAINING(*bsb) < *remlen) {
            return 1;
        }
        int wordcount = 0;
        BSB_IMPORT_u08(*bsb, wordcount);
        BSB_IMPORT_skip(*bsb, wordcount*2 + 3);
        gsize bread, bwritten;
        GError      *error = 0;
        char *out = g_convert((char*)BSB_WORK_PTR(*bsb), BSB_REMAINING(*bsb), "utf8", "ucs2", &bread, &bwritten, &error);
        if (error)
            LOG("ERROR %s", error->message);
        else {
            if (!moloch_field_string_add(MOLOCH_FIELD_SMB_FN, session, out, -1, FALSE)) {
                g_free(out);
            }
        }
        *state = SMB_SKIP;
        break;
    }
    case SMB1_DELETE: {
        if (BSB_REMAINING(*bsb) < *remlen) {
            return 1;
        }
        int wordcount = 0;
        BSB_IMPORT_u08(*bsb, wordcount);
        BSB_IMPORT_skip(*bsb, wordcount*2+3);
        gsize bread, bwritten;
        GError      *error = 0;
        char *out = g_convert((char*)BSB_WORK_PTR(*bsb), BSB_REMAINING(*bsb), "utf8", "ucs2", &bread, &bwritten, &error);

        if (error)
            LOG("ERROR %s", error->message);
        else {
            if (!moloch_field_string_add(MOLOCH_FIELD_SMB_FN, session, out, -1, FALSE)) {
                g_free(out);
            }
        }
        *state = SMB_SKIP;
        break;
    }
    case SMB1_TREE_CONNECT_ANDX: {
        if (BSB_REMAINING(*bsb) < *remlen) {
            return 1;
        }
        int passlength = 0;
        BSB_IMPORT_skip(*bsb, 6);
        BSB_IMPORT_u16(*bsb, passlength);
        BSB_IMPORT_skip(*bsb, 2 + passlength);

        gsize bread, bwritten;
        GError      *error = 0;

        int offset = ((BSB_WORK_PTR(*bsb) - start) % 2 == 0)?2:1;
        char *out = g_convert((char*)BSB_WORK_PTR(*bsb)+offset, BSB_REMAINING(*bsb), "utf8", "ucs2", &bread, &bwritten, &error);
        if (error)
            LOG("ERROR %s", error->message);
        else {
            if (!moloch_field_string_add(MOLOCH_FIELD_SMB_SHARE, session, out, -1, FALSE)) {
                g_free(out);
            }
        }

        *state = SMB_SKIP;
        break;
    }
    case SMB1_SETUP_ANDX: {
        if (BSB_REMAINING(*bsb) < *remlen) {
            return 1;
        }
        int wordcount = 0;
        BSB_IMPORT_u08(*bsb, wordcount);

        if (wordcount != 12) {
            *state = SMB_SKIP;
            break;
        }

        BSB_IMPORT_skip(*bsb, 7*2);

        int securitylen = 0;
        BSB_LIMPORT_u16(*bsb, securitylen);

        BSB_IMPORT_skip(*bsb, 8);

        BSB_IMPORT_skip(*bsb, 2);

        moloch_detect_smb_security_blob(session, BSB_WORK_PTR(*bsb), securitylen);
        BSB_IMPORT_skip(*bsb, securitylen);

        gsize bread, bwritten;
        GError      *error = 0;
        int          offset;

        offset = ((BSB_WORK_PTR(*bsb) - start) % 2 == 0)?0:1;
        BSB_IMPORT_skip(*bsb, offset);

        char *out = g_convert((char*)BSB_WORK_PTR(*bsb), BSB_REMAINING(*bsb), "utf8", "ucs2", &bread, &bwritten, &error);
        if (error)
            LOG("ERROR %s", error->message);
        else if (!BSB_IS_ERROR(*bsb)) {
            if (*out)
                moloch_field_string_add(MOLOCH_FIELD_SMB_OS, session, out, -1, TRUE);
            char *ver = out + strlen(out) + 1;
            if (*ver)
                moloch_field_string_add(MOLOCH_FIELD_SMB_VER, session, ver, -1, TRUE);
            char *domain = ver + strlen(ver) + 1;
            if (*domain)
                moloch_field_string_add(MOLOCH_FIELD_SMB_DOMAIN, session, domain, -1, TRUE);
            g_free(out);
        } else {
            g_free(out);
        }

        *state = SMB_SKIP;
        break;
    }
    }

    *remlen -= (BSB_WORK_PTR(*bsb) - start);
    return 0;
}
/******************************************************************************/
int moloch_detect_parse_smb2(MolochSession_t *session, BSB *bsb, char *state, uint32_t *remlen)
{
    unsigned char *start = BSB_WORK_PTR(*bsb);

    switch (*state) {
    case SMB_SMBHEADER: {
        uint16_t  flags = 0;
        uint16_t  cmd = 0;

        if (BSB_REMAINING(*bsb) < 64) {
            return 1;
            break;
        }
        BSB_IMPORT_skip(*bsb, 12);
        BSB_LIMPORT_u16(*bsb, cmd);
        BSB_IMPORT_skip(*bsb, 2);
        BSB_LIMPORT_u32(*bsb, flags);
        BSB_IMPORT_skip(*bsb, 44);

        if ((flags & SMB2_FLAGS_SERVER_TO_REDIR) == 0) {
            switch (cmd) {
            case 0x03:
                *state = SMB2_TREE_CONNECT;
                break;
            case 0x05:
                *state = SMB2_CREATE;
                break;
            default:
                *state = SMB_SKIP;
            }
        } else {
            *state = SMB_SKIP;
        }
#ifdef SMBDEBUG
        LOG("%d cmd: %x flags: %x newstate: %d remlen: %d", session->which, cmd, flags, *state, *remlen);
#endif
        *remlen -= (BSB_WORK_PTR(*bsb) - start);
        break;
    }
    case SMB2_TREE_CONNECT: {
        uint16_t  pathoffset = 0;
        uint16_t  pathlen = 0;

        if (BSB_REMAINING(*bsb) < *remlen) {
            return 1;
        }
        BSB_IMPORT_skip(*bsb, 4);
        BSB_LIMPORT_u16(*bsb, pathoffset);
        BSB_LIMPORT_u16(*bsb, pathlen);
        pathoffset -= (64 + 8);
        BSB_IMPORT_skip(*bsb, pathoffset);

        gsize bread, bwritten;
        GError      *error = 0;
        char *out = g_convert((char*)BSB_WORK_PTR(*bsb), pathlen, "utf8", "ucs2", &bread, &bwritten, &error);
        if (error)
            LOG("ERROR %s", error->message);
        else {
            if (!moloch_field_string_add(MOLOCH_FIELD_SMB_SHARE, session, out, -1, FALSE)) {
                g_free(out);
            }
        }

        *remlen -= (BSB_WORK_PTR(*bsb) - start);
        *state = SMB_SKIP;
        break;
    }
    case SMB2_CREATE: {
        uint16_t  nameoffset = 0;
        uint16_t  namelen = 0;

        if (BSB_REMAINING(*bsb) < *remlen) {
            return 1;
        }
        BSB_IMPORT_skip(*bsb, 44);
        BSB_LIMPORT_u16(*bsb, nameoffset);
        BSB_LIMPORT_u16(*bsb, namelen);
        nameoffset -= (64 + 48);
        BSB_IMPORT_skip(*bsb, nameoffset);

        gsize bread, bwritten;
        GError      *error = 0;
        char *out = g_convert((char*)BSB_WORK_PTR(*bsb), namelen, "utf8", "ucs2", &bread, &bwritten, &error);
        if (error)
            LOG("ERROR %s", error->message);
        else {
            if (!moloch_field_string_add(MOLOCH_FIELD_SMB_FN, session, out, -1, FALSE)) {
                g_free(out);
            }
        }

        *remlen -= (BSB_WORK_PTR(*bsb) - start);
        *state = SMB_SKIP;
        break;
    }
    }

    return 0;
}
/******************************************************************************/
void moloch_detect_parse_smb(MolochSession_t *session, unsigned char *data, uint32_t remaining)
{
    MolochSessionSMB_t   *smb          = session->smb;
    char                 *state        = &smb->state[session->which];
    char                 *buf          = smb->buf[session->which];
    short                *buflen       = &smb->buflen[session->which];
    uint32_t             *remlen       = &smb->remlen[session->which];

#ifdef SMBDEBUG
    LOG("ENTER: remaining: %d state: %d buflen: %d remlen: %d", remaining, *state, *buflen, *remlen);
#endif

    while (remaining > 0) {
        BSB bsb;
        int done = 0;

        if (*buflen == 0) {
            BSB_INIT(bsb, data, remaining);
            data += remaining;
            remaining = 0;
        } else {
            int len = MIN(remaining, (uint32_t)MAX_SMB_BUFFER - (*buflen));
            memcpy(buf + (*buflen), data, len);
            (*buflen) += len;
            remaining -= len;
            data += len;
            BSB_INIT(bsb, buf, *buflen);
        }

        if (*state != SMB_SKIP && *remlen > MAX_SMB_BUFFER) {
            LOG("ERROR - Not enough room for SMB packet %d", *remlen);
            moloch_nids_free_session_smb(session);
            return;
        }

        while (!done && BSB_REMAINING(bsb) > 0) {
#ifdef SMBDEBUG
            LOG(" S: bsbremaining: %ld remaining: %d state: %d buflen: %d remlen: %d done: %d", BSB_REMAINING(bsb), remaining, *state, *buflen, *remlen, done);
#endif
            switch (*state) {
            case SMB_NETBIOS:
                if(BSB_REMAINING(bsb) < 5) {
                    done = 1;
                    break;
                }

                BSB_IMPORT_skip(bsb, 1);
                BSB_IMPORT_u24(bsb, *remlen);
                // Peak at SMBHEADER for version
                smb->version[session->which] = *(BSB_WORK_PTR(bsb));
                *state = SMB_SMBHEADER;
                break;
            case SMB_SKIP:
                if (BSB_REMAINING(bsb) < *remlen) {
                    *remlen -= BSB_REMAINING(bsb);
                    BSB_IMPORT_skip(bsb, BSB_REMAINING(bsb));
                } else {
                    BSB_IMPORT_skip(bsb, *remlen);
                    *remlen = 0;
                    *state = SMB_NETBIOS;
                }
                break;
            default:
                if (smb->version[session->which] == 0xff) {
                    done = moloch_detect_parse_smb1(session, &bsb, state, remlen);
                } else {
                    done = moloch_detect_parse_smb2(session, &bsb, state, remlen);
                }
            }

#ifdef SMBDEBUG
            LOG(" E: bsbremaining: %ld remaining: %d state: %d buflen: %d remlen: %d done: %d", BSB_REMAINING(bsb), remaining, *state, *buflen, *remlen, done);
#endif
        }

        if (BSB_IS_ERROR(bsb)) {
            moloch_nids_free_session_smb(session);
            return;
        }

        if (BSB_REMAINING(bsb) > 0 && BSB_WORK_PTR(bsb) != (unsigned char *)buf) {
#ifdef SMBDEBUG
            LOG("  Moving data %ld %s", BSB_REMAINING(bsb), (char*)(long)moloch_friendly_session_id(session->protocol, session->addr1, session->port1, session->addr2, session->port2));
#endif
            if (BSB_REMAINING(bsb) > MAX_SMB_BUFFER) {
                LOG("ERROR - Not enough room for SMB packet %ld", BSB_REMAINING(bsb));
                moloch_nids_free_session_smb(session);
                return;
            }
            memmove(buf, BSB_WORK_PTR(bsb), BSB_REMAINING(bsb));
        }
        *buflen = BSB_REMAINING(bsb);
    }
}

/******************************************************************************/
/*############################## SOCKS ##############################*/

#define SOCKS5_STATE_VER_REPLY      0
#define SOCKS5_STATE_USER_REQUEST   1
#define SOCKS5_STATE_USER_REPLY     2
#define SOCKS5_STATE_CONN_REQUEST   3
#define SOCKS5_STATE_CONN_REPLY     4
void moloch_detect_parse_socks(MolochSession_t *session, unsigned char *data, uint32_t remaining)
{
    MolochSessionSocks_t   *socks          = session->socks;

    if (socks->ver == 4) {
        if (session->which == socks->which)
            return;

        if (remaining >= 8 && data[0] == 0 && data[1] >= 0x5a && data[1] <= 0x5d) {
            moloch_detect_parse_classify(session, data + 8, remaining - 8);
            session->skip[socks->which] = 9 + socks->userlen;
            session->skip[session->which] = 8;
            moloch_field_int_add(MOLOCH_FIELD_SOCKS_IP, session, socks->ip);
            moloch_field_int_add(MOLOCH_FIELD_SOCKS_PORT, session, socks->port);
            moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:socks");

            if (socks->user) {
                if (!moloch_field_string_add(MOLOCH_FIELD_SOCKS_USER, session, socks->user, socks->userlen, TRUE)) {
                    g_free(socks->user);
                }
                socks->user = 0;
            }
        }
        moloch_nids_free_session_socks(session);
        return;
    }

    // Socks 5

    switch(socks->state) {
    case SOCKS5_STATE_VER_REPLY:
        if (session->which == socks->which)
            return;

        if (remaining != 2 || data[0] != 5 || data[1] > 2) {
            moloch_nids_free_session_socks(session);
            return;
        }

        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:socks");

        if (data[1] == 0)
            socks->state = SOCKS5_STATE_CONN_REQUEST;
        else if (data[1] == 2)
            socks->state = SOCKS5_STATE_USER_REQUEST;
        else
            moloch_nids_free_session_socks(session);

        session->skip[session->which] = 2;
        session->skip[socks->which] = 3;

        return;
    case SOCKS5_STATE_USER_REQUEST:
        if (session->which != socks->which)
            return;
        if ((2 + data[1] > (int)remaining) || (2 + data[1] + 1 + data[data[1]+2]  > (int)remaining)) {
            moloch_nids_free_session_socks(session);
            return;
        }

        moloch_field_string_add(MOLOCH_FIELD_SOCKS_USER, session, (char *)data + 2, data[1], TRUE);
        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "socks:password");
        session->skip[session->which] += 2 + data[1] + 1 + data[data[1]+2];
        socks->state = SOCKS5_STATE_USER_REPLY;
        return;
    case SOCKS5_STATE_USER_REPLY:
        if (session->which == socks->which)
            return;

        session->skip[session->which] += 2;
        socks->state = SOCKS5_STATE_CONN_REQUEST;
        return;
    case SOCKS5_STATE_CONN_REQUEST:
        if (session->which != socks->which)
            return;
        if (remaining < 6 || data[0] != 5 || data[1] != 1 || data[2] != 0) {
            moloch_nids_free_session_socks(session);
            return;
        }
        if (data[3] == 1) { // IPV4
            socks->port = (data[8]&0xff) << 8 | (data[9]&0xff);
            memcpy(&socks->ip, data+4, 4);
            moloch_field_int_add(MOLOCH_FIELD_SOCKS_IP, session, socks->ip);
            moloch_field_int_add(MOLOCH_FIELD_SOCKS_PORT, session, socks->port);
            session->skip[socks->which] +=  4 + 4 + 2;
        } else if (data[3] == 3) { // Domain Name
            socks->port = (data[5+data[4]]&0xff) << 8 | (data[6+data[4]]&0xff);
            char *lower = g_ascii_strdown((char*)data+5, data[4]);
            if (!moloch_field_string_add(MOLOCH_FIELD_SOCKS_HOST, session, lower, data[4], FALSE)) {
                g_free(lower);
            }
            moloch_field_int_add(MOLOCH_FIELD_SOCKS_PORT, session, socks->port);
            session->skip[socks->which] += 4 + 1 + data[4] + 2;
        } else if (data[3] == 4) { // IPV6
            session->skip[socks->which] += 4 + 16 + 2;
        }
        socks->state = SOCKS5_STATE_CONN_REPLY;
        return;
    case SOCKS5_STATE_CONN_REPLY:
        if (session->which == socks->which || remaining < 6) {
            moloch_nids_free_session_socks(session);
            return;
        }

        if (data[3] == 1) { // IPV4
            session->skip[session->which] += 4 + 4 + 2;
        } else if (data[3] == 3) { // Domain Name
            session->skip[session->which] += 4 + 1 + data[4] + 2;
        } else if (data[3] == 4) { // IPV6
            session->skip[session->which] += 4 + 16 + 2;
        }
        moloch_nids_free_session_socks(session);
        return;
    }
}

/*############################## SHARED ##############################*/

/******************************************************************************/
void moloch_detect_init()
{
    moloch_field_define_internal(MOLOCH_FIELD_USER,          "user",   MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_CONTINUE);
    moloch_field_define_internal(MOLOCH_FIELD_TAGS,          "ta",     MOLOCH_FIELD_TYPE_INT_HASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_CONTINUE);

    moloch_field_define_internal(MOLOCH_FIELD_HTTP_HOST,     "ho",     MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_HTTP_URLS,     "us",     MOLOCH_FIELD_TYPE_STR_ARRAY, MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_HTTP_XFF,      "xff",    MOLOCH_FIELD_TYPE_IP_HASH,   MOLOCH_FIELD_FLAG_SCNT);
    moloch_field_define_internal(MOLOCH_FIELD_HTTP_UA,       "ua",     MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_HTTP_TAGS_REQ, "hh1",    MOLOCH_FIELD_TYPE_INT_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_HTTP_TAGS_RES, "hh2",    MOLOCH_FIELD_TYPE_INT_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_HTTP_MD5,      "hmd5",   MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_HTTP_VER_REQ,  "hsver",  MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_HTTP_VER_RES,  "hdver",  MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_HTTP_PATH,     "hpath",  MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_HTTP_KEY,      "hkey",   MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_HTTP_VALUE,    "hval",   MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);

    moloch_field_define_internal(MOLOCH_FIELD_SSH_VER,       "sshver", MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_SSH_KEY,       "sshkey", MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);

    moloch_field_define_internal(MOLOCH_FIELD_DNS_IP,        "dnsip",  MOLOCH_FIELD_TYPE_IP_HASH,   MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_DNS_HOST,      "dnsho",  MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);

    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_HOST,    "eho",    MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_UA,      "eua",    MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_SRC,     "esrc",   MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_DST,     "edst",   MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_SUB,     "esub",   MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_ID,      "eid",    MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_CT,      "ect",    MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_MV,      "emv",    MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_FN,      "efn",    MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_MD5,     "emd5",   MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_FCT,     "efct",   MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_IP,      "eip",    MOLOCH_FIELD_TYPE_IP_HASH,   MOLOCH_FIELD_FLAG_CNT);

    moloch_field_define_internal(MOLOCH_FIELD_IRC_NICK,      "ircnck", MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_IRC_CHANNELS,  "ircch",  MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);

    moloch_field_define_internal(MOLOCH_FIELD_SMB_SHARE,     "smbsh",  MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_SMB_FN,        "smbfn",  MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_SMB_OS,        "smbos",  MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_SMB_DOMAIN,    "smbdm",  MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_SMB_VER,       "smbver", MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_SMB_USER,      "smbuser",MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_SMB_HOST,      "smbho",  MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);

    moloch_field_define_internal(MOLOCH_FIELD_SOCKS_IP,      "socksip",MOLOCH_FIELD_TYPE_IP,        0);
    moloch_field_define_internal(MOLOCH_FIELD_SOCKS_HOST,    "socksho",MOLOCH_FIELD_TYPE_STR,       0);
    moloch_field_define_internal(MOLOCH_FIELD_SOCKS_PORT,    "sockspo",MOLOCH_FIELD_TYPE_INT,       0);
    moloch_field_define_internal(MOLOCH_FIELD_SOCKS_USER,    "socksuser",MOLOCH_FIELD_TYPE_STR,     0);

    snprintf(nodeTag, sizeof(nodeTag), "node:%s", config.nodeName);
    moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, nodeTag, NULL);

    if (config.nodeClass) {
        snprintf(classTag, sizeof(classTag), "node:%s", config.nodeClass);
        moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, classTag, NULL);
    }

    if (extraTags) {
        int i;
        for (i = 0; extraTags[i]; i++) {
            moloch_db_get_tag(NULL, MOLOCH_FIELD_TAGS, extraTags[i], NULL);
        }
    }

    memset(&parserSettings, 0, sizeof(parserSettings));
    parserSettings.on_message_begin = moloch_hp_cb_on_message_begin;
    parserSettings.on_url = moloch_hp_cb_on_url;
    parserSettings.on_body = moloch_hp_cb_on_body;
    parserSettings.on_headers_complete = moloch_hp_cb_on_headers_complete;
    parserSettings.on_message_complete = moloch_hp_cb_on_message_complete;
    parserSettings.on_header_field = moloch_hp_cb_on_header_field;
    parserSettings.on_header_value = moloch_hp_cb_on_header_value;

    cookie = magic_open(MAGIC_MIME);
    if (!cookie) {
        LOG("Error with libmagic %s", magic_error(cookie));
    } else {
        magic_load(cookie, NULL);
    }

    qclasses[1]   = "dns:qclass:IN";
    qclasses[2]   = "dns:qclass:CS";
    qclasses[3]   = "dns:qclass:CH";
    qclasses[4]   = "dns:qclass:HS";
    qclasses[255] = "dns:qclass:ANY";

    qtypes[1]   = "dns:qtype:A";
    qtypes[2]   = "dns:qtype:NS";
    qtypes[3]   = "dns:qtype:MD";
    qtypes[4]   = "dns:qtype:MF";
    qtypes[5]   = "dns:qtype:CNAME";
    qtypes[6]   = "dns:qtype:SOA";
    qtypes[7]   = "dns:qtype:MB";
    qtypes[8]   = "dns:qtype:MG";
    qtypes[9]   = "dns:qtype:MR";
    qtypes[10]  = "dns:qtype:NULL";
    qtypes[11]  = "dns:qtype:WKS";
    qtypes[12]  = "dns:qtype:PTR";
    qtypes[13]  = "dns:qtype:HINFO";
    qtypes[14]  = "dns:qtype:MINFO";
    qtypes[15]  = "dns:qtype:MX";
    qtypes[16]  = "dns:qtype:TXT";
    qtypes[252] = "dns:qtype:AXFR";
    qtypes[253] = "dns:qtype:MAILB";
    qtypes[254] = "dns:qtype:MAILA";
    qtypes[255] = "dns:qtype:ANY";
}
/******************************************************************************/
void moloch_detect_exit() {
    magic_close(cookie);
}
/******************************************************************************/
void moloch_print_hex_string(unsigned char* data, unsigned int length)
{
    unsigned int i;
    
    for (i = 0; i < length; i++)
    {
        printf("%02X ", data[i]);
    }

    printf("\n");
}
/******************************************************************************/
void moloch_detect_parse_classify(MolochSession_t *session, unsigned char *data, uint32_t remaining)
{
    if (remaining < 3)
        return;

    if (data[0] == 0x5 && data[1] == 1 && data[2] <= 2 && remaining == 3) {
        moloch_nids_new_session_socks(session, data, remaining);
    }

    if (memcmp("SSH", data, 3) == 0) {
        session->isSsh = 1;
        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:ssh");
        unsigned char *n = memchr(data, 0x0a, remaining);
        if (n && *(n-1) == 0x0d)
            n--;

        if (n) {
            int len = (n - data);

            char *str = g_ascii_strdown((char *)data, len);

            if (!moloch_field_string_add(MOLOCH_FIELD_SSH_VER, session, str, len, FALSE)) {
                free(str);
            }
        }
    }

    if (remaining < 4)
        return;

    if (memcmp("220 ", data, 4) == 0) {
        if (g_strstr_len((char *)data, remaining, "LMTP") != 0)
            moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:lmtp");
        else if (g_strstr_len((char *)data, remaining, "SMTP") != 0) {
            moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:smtp");
            if (!session->email)
                moloch_nids_new_session_email(session);
        }
        else
            moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:ftp");
    }

    if (remaining < 5)
        return;

    if (memcmp("HELO ", data, 5) == 0) {
        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:smtp");
        if (!session->email)
            moloch_nids_new_session_email(session);
    }

    if (memcmp("EHLO ", data, 5) == 0) {
        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:smtp");
        if (!session->email)
            moloch_nids_new_session_email(session);
    }

    if (remaining < 9)
        return;

    if ((data[4] == 0xff || data[4] == 0xfe) && memcmp("SMB", data+5, 3) == 0) {
        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:smb");
        if (!session->smb)
            moloch_nids_new_session_smb(session);
    }

    if (memcmp("+OK POP3 ", data, 9) == 0)
        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:pop3");

    if (data[0] == 0x4 && (data[1] & 0xfe) == 0 && data[remaining-1] == 0)  {
        moloch_nids_new_session_socks(session, data, remaining);
    }

    if (remaining < 11)
        return;

    if ((data[0] == ':' && moloch_memstr((char *)data, remaining, " NOTICE ", 8)) ||
         memcmp("NOTICE AUTH", data, 11) == 0 ||
         memcmp("NICK ", data, 5) == 0 ||
         memcmp("PASS ", data, 5) == 0) {
        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:irc");
        session->isIrc = 1;
    }


    if (remaining < 14)
        return;


    if (data[13] == 0x78 &&  
        (((data[8] == 0) && (data[7] == 0) && (((data[6]&0xff) << (uint32_t)8 | (uint32_t)(data[5]&0xff)) == remaining)) ||  // Windows
         ((data[5] == 0) && (data[6] == 0) && (((data[7]&0xff) << (uint32_t)8 | (uint32_t)(data[8]&0xff)) == remaining)))) { // Mac
        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:gh0st");
     }else if (data[7] == 0 && data[8] == 0 && data[11] == 0 && data[12] == 0 && data[13] == 0x78 && data[14] == 0x9c) {
        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:gh0st-improved");
    }

    if (remaining < 19)
        return;

    if (memcmp("BitTorrent protocol", data, 19) == 0)
        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:bittorrent");

    if (remaining < 30)
        return;

    if (data[0] == 0x16 && data[1] == 0x03 && data[2] <= 0x03 && data[5] == 2) {
        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:tls");
        moloch_detect_tls_process(session, data, remaining);
    }
}
/******************************************************************************/
void moloch_detect_parse_yara(MolochSession_t *session, unsigned char *data, uint32_t remaining, int initial)
{
    moloch_yara_execute(session, (char*)data, remaining, initial);
}
