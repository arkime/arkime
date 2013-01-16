/* detect.c  -- Functions for dealing with detection
 *
 * Copyright 2012 AOL Inc. All rights reserved.
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

//#define HTTPDEBUG

/******************************************************************************/
extern MolochConfig_t        config;
extern gchar               **extraTags;
static gchar                 nodeTag[100];
static gchar                 classTag[100];
extern uint32_t              pluginsCbs;

static http_parser_settings  parserSettings;
static magic_t               cookie;


/******************************************************************************/
void moloch_detect_initial_tag(MolochSession_t *session)
{
    int i;

    for (i = 0; i < MOLOCH_TAG_MAX; i++)
    {
        if (session->tags[i])
            g_hash_table_destroy(session->tags[i]);
        session->tags[i] = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
    }

    moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, nodeTag);
    if (config.nodeClass)
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, classTag);

    if (extraTags) {
        for (i = 0; extraTags[i]; i++) {
            moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, extraTags[i]);
        }
    }

    switch(session->protocol) {
    case IPPROTO_TCP:
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "tcp");
        break;
    case IPPROTO_UDP:
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "udp");
        break;
    case IPPROTO_ICMP:
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "ICMP");
        break;
    }
}

/******************************************************************************/
unsigned char *
moloch_detect_asn_get_tlv(unsigned char **data, int *len, int *apc, int *atag, int *alen)
{
    if ((*len) < 2) {
        goto get_tlv_error;
    }

    int pos = 0;

    *apc = ((*data)[0] >> 5) & 0x1;

    if (((*data)[0] & 0x1f) ==  0x1f) {
        for (pos = 1; pos < *len; pos++) {
            (*atag) = ((*atag) << 7) | (*data)[pos];
            if (((*data)[pos] & 0x80) == 0)
                break;
        }
    } else {
        *atag = (*data)[pos] & 0x1f;
        pos++;
    }

    if (pos >= (*len) || (*data)[pos] == 0x80) {
        goto get_tlv_error;
    }

    if ((*data)[pos] & 0x80) {
        int cnt = (*data)[pos] & 0x7f;
        pos++;
        (*alen) = 0;
        while (cnt > 0 && pos < (*len)) {
            (*alen) = ((*alen) << 8) | (*data)[pos];
            cnt--;
            pos++;
        }
    } else {
        (*alen) = (*data)[pos];
        pos++;
    }

    (*len) -= pos;
    (*data) += pos;
    unsigned char *value = (*data);

    if ((*len) < 0 || (*alen) < 0) {
        goto get_tlv_error;
    }

    if ((*alen) > (*len)) {
        (*alen) = (*len);
    }
    (*data) += (*alen);
    (*len) -= (*alen);

    return value;

get_tlv_error:
    (*apc)  = 0;
    (*alen) = 0;
    (*atag) = 0;
    (*len)  = 0;
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
/******************************************************************************/
void
moloch_detect_tls_certinfo_process(MolochCertInfo_t *ci, unsigned char *data, int len)
{
    int apc, atag, alen;
    char *lastOid = NULL;

    while (len > 0) {
        unsigned char *value = moloch_detect_asn_get_tlv(&data, &len, &apc, &atag, &alen);
        if (apc) {
            moloch_detect_tls_certinfo_process(ci, value, alen);
        } else if (atag  == 6)  {
            lastOid = moloch_detect_asn_decode_oid(value, alen);
        } else if (lastOid && (atag == 20 || atag == 19 || atag == 12))  {
            /* 20 == BER_UNI_TAG_TeletexString
             * 19 == BER_UNI_TAG_PrintableString
             * 12 == BER_UNI_TAG_UTF8String
             */
            if (strcmp(lastOid, "2.5.4.3") == 0) {
                MolochString_t *element = malloc(sizeof(*element));
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
                if (ci->orgUtf8)
                    ci->orgName = g_utf8_strdown((char*)value, alen);
                else
                    ci->orgName = g_ascii_strdown((char*)value, alen);
            }
        }
    }
}
/******************************************************************************/
void
moloch_detect_tls_alt_names(MolochCertsInfo_t *certs, unsigned char *data, int len)
{
    int apc, atag, alen;
    static char *lastOid = NULL;

    while (len >= 2) {
        unsigned char *value = moloch_detect_asn_get_tlv(&data, &len, &apc, &atag, &alen);

        if (!value)
            return;

        if (apc) {
            moloch_detect_tls_alt_names(certs, value, alen);
            if (certs->alt.s_count > 0) {
                return;
            }
        } else if (atag == 6)  {
            lastOid = moloch_detect_asn_decode_oid(value, alen);
            if (strcmp(lastOid, "2.5.29.17") != 0)
                lastOid = NULL;
        } else if (lastOid && atag == 4) {
            moloch_detect_tls_alt_names(certs, value, alen);
            return;
        } else if (lastOid && atag == 2) {
            MolochString_t *element = malloc(sizeof(*element));
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
    unsigned char *ssldata;
    unsigned char *pdata;
    unsigned char *cdata;
    int            ssllen = 0;
    int            plen = 0;
    int            clen = 0;

    for (ssldata = data;
         ssldata < data + len; 
         ssldata += ssllen + 5) {

        ssllen = MIN(data+len-ssldata-5, ((ssldata[3]&0xff) << 8 | (ssldata[4]&0xff)));



        for (pdata = ssldata + 5;
             pdata < data + len && pdata < ssldata + ssllen; 
             pdata += plen + 4) {

            plen = MIN(data+len-pdata-7, ((pdata[2]&0xff) << 8 | (pdata[3]&0xff)));

            if (pdata[0] != 0x0b)
                continue;

            for (cdata = pdata+7;
                 cdata < data + len && cdata < pdata + plen;
                 cdata += clen + 3)
            {
                int badreason = 0;
                clen = MIN(data+len-cdata-3, cdata[0] << 16 | cdata[1] << 8 | cdata[2]);

                MolochCertsInfo_t *certs = malloc(sizeof(MolochCertsInfo_t));
                memset(certs, 0, sizeof(*certs));
                DLL_INIT(s_, &certs->alt);
                DLL_INIT(s_, &certs->subject.commonName);
                DLL_INIT(s_, &certs->issuer.commonName);

                unsigned char *asndata = cdata + 3;
                int            asnlen  = clen;
                int            atag, alen, apc;
                unsigned char *value;

                /* Certificate */
                if (!(value = moloch_detect_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                    {badreason = 1; goto bad_cert;}
                asndata = value;
                asnlen = alen;

                /* signedCertificate */
                if (!(value = moloch_detect_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                    {badreason = 2; goto bad_cert;}
                asndata = value;
                asnlen = alen;

                /* serialNumber or version*/
                if (!(value = moloch_detect_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                    {badreason = 3; goto bad_cert;}

                if (apc) {
                    if (!(value = moloch_detect_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                        {badreason = 4; goto bad_cert;}
                }
                certs->serialNumberLen = alen;
                certs->serialNumber = malloc(alen);
                memcpy(certs->serialNumber, value, alen);

                /* signature */
                if (!(value = moloch_detect_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                    {badreason = 5; goto bad_cert;}

                /* issuer */
                if (!(value = moloch_detect_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                    {badreason = 6; goto bad_cert;}
                moloch_detect_tls_certinfo_process(&certs->issuer, value, alen);

                /* validity */
                if (!(value = moloch_detect_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                    {badreason = 7; goto bad_cert;}

                /* subject */
                if (!(value = moloch_detect_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                    {badreason = 8; goto bad_cert;}
                moloch_detect_tls_certinfo_process(&certs->subject, value, alen);

                /* subjectPublicKeyInfo */
                if (!(value = moloch_detect_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                    {badreason = 9; goto bad_cert;}

                /* extensions */
                if (asnlen) {
                    if (!(value = moloch_detect_asn_get_tlv(&asndata, &asnlen, &apc, &atag, &alen)))
                        {badreason = 10; goto bad_cert;}
                    moloch_detect_tls_alt_names(certs, value, alen);
                }

                MolochCertsInfo_t *element;
                HASH_FIND(t_, session->certs, certs, element);
                if (element) {
                    moloch_nids_certs_free(certs);
                } else {
                    HASH_ADD(t_, session->certs, certs, certs);
                }

                continue;

            bad_cert:
                if (config.debug)
                    LOG("bad cert %d - %d %.*s", badreason, clen, clen, cdata);
                moloch_nids_certs_free(certs);
                break;
            }
        }
    }
}
/******************************************************************************/
void moloch_detect_parse_classify(MolochSession_t *session, struct tcp_stream *UNUSED(a_tcp), struct half_stream *hlf)
{
    unsigned char *data = (unsigned char *)hlf->data;

    if (hlf->offset != 0)
        return;

    if (hlf->count < 3)
        return;

    if (memcmp("SSH", data, 3) == 0) {
        session->isSsh = 1;
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:ssh");
        unsigned char *n = memchr(data, 0x0a, hlf->count);
        if (n && *(n-1) == 0x0d)
            n--;

        if (n) {
            int len = (n - data);

            MolochString_t *hstring;
            char *str = g_ascii_strdown((char *)data, len);

            HASH_FIND(s_, session->sshver, str, hstring);
            if (!hstring) {
                hstring = malloc(sizeof(*hstring));
                hstring->str = str;
                HASH_ADD(s_, session->sshver, hstring->str, hstring);
            } else {
                g_free(str);
            }
        }
    }

    if (hlf->count < 4)
        return;

    if (memcmp("220 ", data, 4) == 0) {
        if (g_strstr_len((char *)data, hlf->count_new, "LMTP") != 0)
            moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:lmtp");
        else if (g_strstr_len((char *)data, hlf->count_new, "SMTP") != 0)
            moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:smtp");
        else
            moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:ftp");
    }

    if (hlf->count < 5)
        return;

    if (memcmp("HELO ", data, 5) == 0)
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:smtp");

    if (memcmp("EHLO ", data, 5) == 0)
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:smtp");



    if (hlf->count < 9)
        return;

    if (memcmp("+OK POP3 ", data, 9) == 0)
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:pop3");


    if (hlf->count < 14)
        return;


    if (data[13] == 0x78 &&  
        (((data[8] == 0) && (data[7] == 0) && (((data[6]&0xff) << 8 | (data[5]&0xff)) == hlf->count)) ||  // Windows
         ((data[5] == 0) && (data[6] == 0) && (((data[7]&0xff) << 8 | (data[8]&0xff)) == hlf->count)))) { // Mac
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:gh0st");
     }else if (data[7] == 0 && data[8] == 0 && data[11] == 0 && data[12] == 0 && data[13] == 0x78 && data[14] == 0x9c) {
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:gh0st-improved");
    }

    if (hlf->count < 19)
        return;

    if (memcmp("BitTorrent protocol", data, 19) == 0)
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:bittorrent");

    if (hlf->count < 30)
        return;

    if (hlf->count != hlf->count_new && data[0] == 0x16 && data[1] == 0x03 && data[2] <= 0x03 && data[5] == 2) {
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:tls");
        moloch_detect_tls_process(session, data, hlf->count);
    }
}
/******************************************************************************/
void moloch_detect_parse_yara(MolochSession_t *session, struct tcp_stream *UNUSED(a_tcp), struct half_stream *hlf)
{
    moloch_yara_execute(session, hlf->data, hlf->count - hlf->offset, (hlf->offset == 0));
}
/******************************************************************************/
void moloch_detect_parse_http(MolochSession_t *session, struct tcp_stream *UNUSED(a_tcp), struct half_stream *hlf)
{
#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: enter %d", session->which);
#endif

    if (!session->http) {
        if (hlf->offset == 0) {
            moloch_nids_new_session_http(session);
            http_parser_init(&session->http->parsers[0], HTTP_BOTH);
            http_parser_init(&session->http->parsers[1], HTTP_BOTH);
            session->http->wParsers = 3;
            session->http->parsers[0].data = session;
            session->http->parsers[1].data = session;
        }
        else {
            return;
        }
    } else if ((session->http->wParsers & (1 << session->which)) == 0) {
        return;
    }

    int remaining = hlf->count_new;
    char *data    = hlf->data + (hlf->count - hlf->offset - hlf->count_new);

    while (remaining > 0) {
        int len = http_parser_execute(&session->http->parsers[session->which], &parserSettings, data, remaining);
#ifdef HTTPDEBUG
            LOG("HTTPDEBUG: parse result: %d input: %d errno: %d", len, remaining, session->http->parsers[session->which].http_errno);
#endif
        if (len <= 0) {
            session->http->wParsers &= ~(1 << session->which);
            if (session->http->wParsers) {
                moloch_nids_free_session_http(session, TRUE);
            }
            break;
        }
        data += len;
        remaining -= len;
    }
}

/******************************************************************************/
void moloch_detect_parse_ssh(MolochSession_t *session, struct tcp_stream *UNUSED(a_tcp), struct half_stream *hlf)
{
    uint32_t remaining = hlf->count_new;
    unsigned char *data   = (unsigned char*)(hlf->data + (hlf->count - hlf->offset - hlf->count_new));

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
                MolochString_t *hstring;
                HASH_FIND(s_, session->sshkey, str, hstring);
                if (!hstring) {
                    hstring = malloc(sizeof(*hstring));
                    hstring->str = str;
                    HASH_ADD(s_, session->sshkey, hstring->str, hstring);
                } else {
                    free(str);
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
int
moloch_hp_cb_on_message_begin (http_parser *parser)
{
    MolochSession_t *session = parser->data;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which: %d", session->which);
#endif

    session->http->inHeader &= ~(1 << session->which);
    session->http->inValue  &= ~(1 << session->which);
    session->http->inBody   &= ~(1 << session->which);

    if (pluginsCbs & MOLOCH_PLUGIN_HP_OMB)
        moloch_plugins_cb_hp_omb(session, parser);

    return 0;
}
/******************************************************************************/
int
moloch_hp_cb_on_url (http_parser *parser, const char *at, size_t length)
{
    MolochSession_t *session = parser->data;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which:%d url %.*s", session->which, (int)length, at);
#endif

    if (!session->http->urlString)
        session->http->urlString = g_string_new_len(at, length);
    else
        g_string_append_len(session->http->urlString, at, length);

    return 0;
}

/******************************************************************************/
const char *moloch_memstr(const char *haystack, int haysize, const char *needle, int needlesize)
{
    const char *p;

    haysize -= needlesize;

    for (p = haystack; p <= (haystack+haysize); p++)
    {
        if (p[0] == needle[0] && memcmp(p+1, needle+1, needlesize-1) == 0)
            return p;
    }
    return NULL;
}
/******************************************************************************/
int
moloch_hp_cb_on_body (http_parser *parser, const char *at, size_t length)
{
    MolochSession_t *session = parser->data;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which: %d", session->which);
#endif

    if (!(session->http->inBody & (1 << session->which))) {
        if (moloch_memstr(at, length, "password=", 9)) {
            moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "http:password");
        }

        const char *m = magic_buffer(cookie, at, length);
        if (m) {
            char tmp[500];
            snprintf(tmp, sizeof(tmp), "http:content:%s", m);
            char *semi = strchr(tmp, ';');
            if (semi) {
                *semi = 0;
            } 
            moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, tmp);
        }
        session->http->inBody |= (1 << session->which);
    }

    if (pluginsCbs & MOLOCH_PLUGIN_HP_OB)
        moloch_plugins_cb_hp_ob(session, parser, at, length);

    return 0;
}

/******************************************************************************/
int
moloch_hp_cb_on_message_complete (http_parser *parser)
{
    MolochSession_t *session = parser->data;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which: %d", session->which);
#endif

    if (pluginsCbs & MOLOCH_PLUGIN_HP_OMC)
        moloch_plugins_cb_hp_omc(session, parser);

    char tag[200];

    if (parser->status_code == 0) {
        snprintf(tag, sizeof(tag), "http:method:%s", http_method_str(parser->method));
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, tag);
    } else {
        snprintf(tag, sizeof(tag), "http:statuscode:%d", parser->status_code);
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, tag);
    }

    session->http->header[0][0] = session->http->header[1][0] = 0;

    if (session->http->urlString) {
        char *ch = session->http->urlString->str;
        while (*ch) {
            if (*ch < 32) {
                moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "http:control-char");
                break;
            }
            ch++;
        }
    }

    if (session->http->hostString) {
        g_string_ascii_down(session->http->hostString);
    }

    if (session->http->urlString && session->http->hostString) {
        MolochString_t *hstring;

        HASH_FIND(s_, session->hosts, session->http->hostString->str+2, hstring);
        if (!hstring) {
            hstring = malloc(sizeof(*hstring));
            hstring->str = g_strdup(session->http->hostString->str+2);
            HASH_ADD(s_, session->hosts, hstring->str, hstring);
        }

        if (session->http->urlString->str[0] != '/') {
            char *result = strstr(session->http->urlString->str, session->http->hostString->str+2);

            /* If the host header is in the first 8 bytes of url then just use the url */
            if (result && result - session->http->urlString->str <= 8) {
                g_ptr_array_add(session->http->urlArray, g_string_free(session->http->urlString, FALSE));
                g_string_free(session->http->hostString, TRUE);
            } else {
                /* Host header doesn't match the url */
                g_string_append(session->http->hostString, ";");
                g_string_append(session->http->hostString, session->http->urlString->str);
                g_ptr_array_add(session->http->urlArray, g_string_free(session->http->hostString, FALSE));
                g_string_free(session->http->urlString, TRUE);
            }
        } else {
            /* Normal case, url starts with /, so no extra host in url */
            g_string_append(session->http->hostString, session->http->urlString->str);
            g_ptr_array_add(session->http->urlArray, g_string_free(session->http->hostString, FALSE));
            g_string_free(session->http->urlString, TRUE);
        }

        if (session->http->urlArray->len == 1)
            moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:http");

        session->http->urlString = NULL;
        session->http->hostString = NULL;
    } else if (session->http->urlString) {
        g_ptr_array_add(session->http->urlArray, g_string_free(session->http->urlString, FALSE));
        if (session->http->urlArray->len == 1)
            moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:http");

        session->http->urlString = NULL;
    } else if (session->http->hostString) {
        MolochString_t *hstring;

        HASH_FIND(s_, session->hosts, session->http->hostString->str+2, hstring);
        if (!hstring) {
            hstring = malloc(sizeof(*hstring));
            hstring->str = g_strdup(session->http->hostString->str+2);
            HASH_ADD(s_, session->hosts, hstring->str, hstring);
        }

        g_string_free(session->http->hostString, TRUE);
        session->http->hostString = NULL;
    }

    if (session->http->uaString) {
        MolochString_t *string;

        HASH_FIND(s_, session->http->userAgents, session->http->uaString->str, string);
        if (!string) {
            string = malloc(sizeof(*string));
            string->str = g_strdup(session->http->uaString->str);
            HASH_ADD(s_, session->http->userAgents, string->str, string);
        }

        g_string_free(session->http->uaString, TRUE);
        session->http->uaString = NULL;
    }

    if (session->http->xffString) {
        int i;
        gchar **parts = g_strsplit(session->http->xffString->str, ",", 0);

        for (i = 0; parts[i]; i++) {
            gchar *ip = parts[i];
            while (*ip == ' ')
                ip++;

            in_addr_t ia = inet_addr(ip);
            if (ia == 0 || ia == 0xffffffff) {
                moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "http:bad-xff");
                LOG("ERROR - Didn't understand ip: %s %s %d", session->http->xffString->str, ip, ia);
                continue;
            }

            MolochInt_t *mi;
            HASH_FIND(i_, session->http->xffs, (void*)(long)ia, mi);
            if (!mi) {
                mi = malloc(sizeof(*mi));
                mi->i = ia;
                HASH_ADD(i_, session->http->xffs, (void *)(long)mi->i, mi);
            }
        }

        g_strfreev(parts);
        g_string_free(session->http->xffString, TRUE);
        session->http->xffString = NULL;
    }
    return 0;
}

/******************************************************************************/
int
moloch_hp_cb_on_header_field (http_parser *parser, const char *at, size_t length)
{
    MolochSession_t *session = parser->data;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which: %d field: %.*s", session->which, (int)length, at);
#endif

    if ((session->http->inHeader & (1 << session->which)) == 0) {
        session->http->inValue |= (1 << session->which);
        if (session->http->urlString && parser->status_code == 0 && pluginsCbs & MOLOCH_PLUGIN_HP_OU) {
            moloch_plugins_cb_hp_ou(session, parser, session->http->urlString->str, session->http->urlString->len);
        }
    }

    if (session->http->inValue & (1 << session->which)) {
        session->http->inValue &= ~(1 << session->which);

        session->http->header[session->which][0] = 0;
    }

    size_t remaining = sizeof(session->http->header[session->which]) - strlen(session->http->header[session->which]) - 1;
    if (remaining > 0)
        strncat(session->http->header[session->which], at, MIN(length, remaining));

    return 0;
}

/******************************************************************************/
int
moloch_hp_cb_on_header_value (http_parser *parser, const char *at, size_t length)
{
    MolochSession_t *session = parser->data;
    char header[200];

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which: %d value: %.*s", session->which, (int)length, at);
#endif

    if ((session->http->inValue & (1 << session->which)) == 0) {
        session->http->inValue |= (1 << session->which);

        char *lower = g_ascii_strdown(session->http->header[session->which], -1);
        moloch_plugins_cb_hp_ohf(session, parser, lower, strlen(lower));

        snprintf(header, sizeof(header), "http:header:%s", lower);
        g_free(lower);
        moloch_nids_add_tag(session, MOLOCH_TAG_HTTP_REQUEST+session->which, header);
    }

    moloch_plugins_cb_hp_ohv(session, parser, at, length);

    if (parser->method && strcasecmp("host", session->http->header[session->which]) == 0) {
        if (!session->http->hostString)
            session->http->hostString = g_string_new_len("//", 2);
        g_string_append_len(session->http->hostString, at, length);
    } 

    if (parser->method && strcasecmp("user-agent", session->http->header[session->which]) == 0) {
        if (!session->http->uaString)
            session->http->uaString = g_string_new_len(at, length);
        else
            g_string_append_len(session->http->uaString, at, length);
    } 

    if (parser->method && strcasecmp("x-forwarded-for", session->http->header[session->which]) == 0) {
        if (!session->http->xffString)
            session->http->xffString = g_string_new_len(at, length);
        else
            g_string_append_len(session->http->xffString, at, length);
    } 

    return 0;
}
/******************************************************************************/
int
moloch_hp_cb_on_headers_complete (http_parser *parser)
{
    MolochSession_t *session = parser->data;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which: %d", session->which);
#endif

    if (pluginsCbs & MOLOCH_PLUGIN_HP_OHC)
        moloch_plugins_cb_hp_ohc(session, parser);

    return 0;
}
/******************************************************************************/
unsigned char *moloch_detect_dns_name_element(unsigned char **data, int *len, unsigned char *nptr)
{
    unsigned char *start = *data;
    int nlen = **data;

    (*data)++;
    if (nlen == 0 || nlen > *len) {
        return 0;
    }

    int j;
    for (j = 0; j < nlen; j++) {
        register u_char c = (*data)[j];

        if (!isascii(c)) {
            *(nptr++) = 'M';
            *(nptr++) = '-';
            c = toascii(c);
        }
        if (!isprint(c)) {
            *(nptr++) = '^';
            c ^= 0x40;
        } 

        *(nptr++) = c;
    }
    (*data) += nlen;

    (*len) -= (*data) - start;
    *nptr  =0;

    return nptr;
}
/******************************************************************************/
unsigned char *moloch_detect_dns_name(unsigned char *full, unsigned char *data, int len, int *olen)
{
    static unsigned char  name[8000];
    unsigned char *nptr = name;
    int didPointer = 0;

    *olen = 0;

    while (len > 0) {
        unsigned char *result;
        if (*data & 0xc0) {
            if (didPointer > 5) {
                return 0;
            }
            didPointer++;
            int tlen = (*data & 0x3f) << 8 | *(data + 1);

            len = data + len - full - tlen;
            data = full + tlen;
            *olen += 2;
            continue;
        } else {
            if (nptr != name) {
                *(nptr++) = '.';
            }
            if (!didPointer) {
                *olen += 1 + *data;
            }
            result = moloch_detect_dns_name_element(&data, &len, nptr);
        }


        if (result == 0) {
            if (nptr != name)
                *(nptr - 1) = 0; /* Remove last . */
            break;
        }
        nptr = result;
    }
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

    unsigned char *ptr = data + 12;

    if (qdcount > 10 || qdcount <= 0)
        return;

    /* QD Section */
    int i;
    for (i = 0; (ptr < data + len) && i < qdcount; i++) {
        int   olen;
        unsigned char *name = moloch_detect_dns_name(data, ptr, (data + len - ptr), &olen);

        if (!name)
            break;
        ptr += olen;
        ptr += 4; // qtype && qclass

        MolochString_t *hstring;

        char *lower = g_ascii_strdown((char*)name, -1);
        HASH_FIND(s_, session->hosts, lower, hstring);
        if (!hstring) {
            hstring = malloc(sizeof(*hstring));
            hstring->str = lower;
            HASH_ADD(s_, session->hosts, hstring->str, hstring);
        } else {
            g_free(lower);
        }
    }
    moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:dns");

    if (qr == 0)
        return;

    for (i = 0; (ptr < data + len) && i < ancount; i++) {
        int   olen;
        unsigned char *name = moloch_detect_dns_name(data, ptr, (data + len - ptr), &olen);

        if (!name)
            break;
        ptr += olen;
        int antype = (ptr[0] << 8) | ptr[1];
        int anclass = (ptr[2] << 8) | ptr[3];
        ptr += 8; // type, class, ttl

        int rdlength = (ptr[0] << 8) | ptr[1];
        ptr += 2;

        if (antype == 1 && anclass == 1 && rdlength == 4) {
            struct in_addr in;
            in.s_addr = ptr[3] << 24 | ptr[2] << 16 | ptr[1] << 8 | ptr[0];

            MolochInt_t *mi;
            HASH_FIND(i_, session->dnsips, (void*)(long)in.s_addr, mi);
            if (!mi) {
                mi = malloc(sizeof(*mi));
                mi->i = in.s_addr;
                HASH_ADD(i_, session->dnsips, (void *)(long)mi->i, mi);
            }
        } else if (antype == 5 && anclass == 1) {
            unsigned char *name = moloch_detect_dns_name(data, ptr, rdlength, &olen);

            MolochString_t *hstring;
            char *lower = g_ascii_strdown((char*)name, -1);
            HASH_FIND(s_, session->hosts, lower, hstring);
            if (!hstring) {
                hstring = malloc(sizeof(*hstring));
                hstring->str = lower;
                HASH_ADD(s_, session->hosts, hstring->str, hstring);
            } else {
                g_free(lower);
            }
        }

        ptr += rdlength;
    }
}

/******************************************************************************/
void moloch_detect_init()
{
    snprintf(nodeTag, sizeof(nodeTag), "node:%s", config.nodeName);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, nodeTag, NULL);

    if (config.nodeClass) {
        snprintf(classTag, sizeof(classTag), "node:%s", config.nodeClass);
        moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, classTag, NULL);
    }

    if (extraTags) {
        int i;
        for (i = 0; extraTags[i]; i++) {
            moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, extraTags[i], NULL);
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
}
/******************************************************************************/
void moloch_detect_exit() {
    magic_close(cookie);
}
/******************************************************************************/
