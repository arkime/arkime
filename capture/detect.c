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

/******************************************************************************/
extern MolochConfig_t        config;
extern gchar                *nodeName;
extern gchar                *extraTag;
extern gboolean              debug;
static gchar                 nodeTag[100];
static gchar                 classTag[100];

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

    if (extraTag)
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, extraTag);

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
    int buflen = 0;
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
                buflen += sprintf(buf, "%d.%d", value/40, value % 40);
            else /* one value in first byte */
                buflen += sprintf(buf, "%d", value);
        } else {
            buflen += sprintf(buf+buflen, ".%d", value);
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
            if (strcmp(lastOid, "2.5.4.3") == 0) {
                MolochString_t *element = malloc(sizeof(*element));
                element->str = g_ascii_strdown((char*)value, alen);
                DLL_PUSH_TAIL(s_, &ci->commonName, element);
            } else if (strcmp(lastOid, "2.5.4.10") == 0) {
                if (ci->orgName) {
                    LOG("Multiple orgName %s => %.*s", ci->orgName, alen, value);
                    free(ci->orgName);
                }
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
                if (debug)
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

    if (memcmp("SSH", data, 3) == 0)
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:ssh");

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
    int remaining = hlf->count_new;
    char *data    = hlf->data + (hlf->count - hlf->offset - hlf->count_new);

    while (remaining > 0) {
        if ((session->wParsers & (1 << session->which)) == 0) {
            if (hlf->offset == 0) {
                http_parser_init(&session->parsers[session->which], HTTP_BOTH);
                session->wParsers |= (1 << session->which);
                session->parsers[session->which].data = session;
            } else {
                break;
            }
        }

        int len = http_parser_execute(&session->parsers[session->which], &parserSettings, data, remaining);
        if (len <= 0) {
            session->wParsers &= ~(1 << session->which);
            break;
        }
        data += len;
        remaining -= len;
    }
}

/******************************************************************************/
int
moloch_hp_cb_on_message_begin (http_parser *parser)
{
    MolochSession_t *session = parser->data;

    session->inValue &= ~(1 << session->which);
    session->inBody  &= ~(1 << session->which);

    return 0;
}
/******************************************************************************/
int
moloch_hp_cb_on_url (http_parser *parser, const char *at, size_t length)
{
    MolochSession_t *session = parser->data;

    if (!session->urlString)
        session->urlString = g_string_new_len(at, length);
    else
        g_string_append_len(session->urlString, at, length);

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

    if (!(session->inBody & (1 << session->which))) {
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
        session->inBody |= (1 << session->which);
    }

    return 0;
}

/******************************************************************************/
int
moloch_hp_cb_on_message_complete (http_parser *parser)
{
    MolochSession_t *session = parser->data;

    char tag[200];

    if (parser->status_code == 0) {
        snprintf(tag, sizeof(tag), "http:method:%s", http_method_str(parser->method));
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, tag);
    } else {
        snprintf(tag, sizeof(tag), "http:statuscode:%d", parser->status_code);
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, tag);
    }

    session->header[0][0] = session->header[1][0] = 0;

    if (session->urlString) {
        char *ch = session->urlString->str;
        while (*ch) {
            if (*ch < 32) {
                moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "http:control-char");
                break;
            }
            ch++;
        }
    }

    if (session->hostString) {
        g_string_ascii_down(session->hostString);
    }

    if (session->urlString && session->hostString) {
        MolochString_t *hstring;

        HASH_FIND(s_, session->hosts, session->hostString->str+2, hstring);
        if (!hstring) {
            hstring = malloc(sizeof(*hstring));
            hstring->str = g_strdup(session->hostString->str+2);
            HASH_ADD(s_, session->hosts, hstring->str, hstring);
        }

        if (session->urlString->str[0] != '/') {
            char *result = strstr(session->urlString->str, session->hostString->str+2);

            /* If the host header is in the first 8 bytes of url then just use the url */
            if (result && result - session->urlString->str <= 8) {
                g_ptr_array_add(session->urlArray, g_string_free(session->urlString, FALSE));
                g_string_free(session->hostString, TRUE);
            } else {
                /* Host header doesn't match the url */
                g_string_append(session->hostString, ";");
                g_string_append(session->hostString, session->urlString->str);
                g_ptr_array_add(session->urlArray, g_string_free(session->hostString, FALSE));
                g_string_free(session->urlString, TRUE);
            }
        } else {
            /* Normal case, url starts with /, so no extra host in url */
            g_string_append(session->hostString, session->urlString->str);
            g_ptr_array_add(session->urlArray, g_string_free(session->hostString, FALSE));
            g_string_free(session->urlString, TRUE);
        }

        if (session->urlArray->len == 1)
            moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:http");

        session->urlString = NULL;
        session->hostString = NULL;
    } else if (session->urlString) {
        g_ptr_array_add(session->urlArray, g_string_free(session->urlString, FALSE));
        if (session->urlArray->len == 1)
            moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:http");

        session->urlString = NULL;
    } else if (session->hostString) {
        MolochString_t *hstring;

        HASH_FIND(s_, session->hosts, session->hostString->str+2, hstring);
        if (!hstring) {
            hstring = malloc(sizeof(*hstring));
            hstring->str = g_strdup(session->hostString->str+2);
            HASH_ADD(s_, session->hosts, hstring->str, hstring);
        }

        g_string_free(session->hostString, TRUE);
        session->hostString = NULL;
    }

    if (session->uaString) {
        MolochString_t *string;

        HASH_FIND(s_, session->userAgents, session->uaString->str, string);
        if (!string) {
            string = malloc(sizeof(*string));
            string->str = g_strdup(session->uaString->str);
            HASH_ADD(s_, session->userAgents, string->str, string);
        }

        g_string_free(session->uaString, TRUE);
        session->uaString = NULL;
    }

    if (session->xffString) {
        int i;
        gchar **parts = g_strsplit(session->xffString->str, ",", 0);

        for (i = 0; parts[i]; i++) {
            gchar *ip = parts[i];
            while (*ip == ' ')
                ip++;

            in_addr_t ia = inet_addr(ip);
            if (ia == 0 || ia == 0xffffffff) {
                moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "http:bad-xff");
                LOG("ERROR - Didn't understand ip: %s %s %d", session->xffString->str, ip, ia);
                continue;
            }

            MolochInt_t *mi;

            HASH_FIND(i_, session->xffs, (void*)(long)ia, mi);
            if (!mi) {
                mi = malloc(sizeof(*mi));
                mi->i = ia;
                HASH_ADD(i_, session->xffs, (void *)(long)mi->i, mi);
            }
        }

        g_strfreev(parts);
        g_string_free(session->xffString, TRUE);
        session->xffString = NULL;
    }
    return 0;
}

/******************************************************************************/
int
moloch_hp_cb_on_header_field (http_parser *parser, const char *at, size_t length)
{
    MolochSession_t *session = parser->data;

    if (session->inValue & (1 << session->which)) {
        session->inValue &= ~(1 << session->which);
        session->header[session->which][0] = 0;
    }

    size_t remaining = sizeof(session->header[session->which]) - strlen(session->header[session->which]) - 1;
    if (remaining > 0)
        strncat(session->header[session->which], at, MIN(length, remaining));

    return 0;
}

/******************************************************************************/
int
moloch_hp_cb_on_header_value (http_parser *parser, const char *at, size_t length)
{
    MolochSession_t *session = parser->data;
    char header[200];

    if ((session->inValue & (1 << session->which)) == 0) {
        session->inValue |= (1 << session->which);

        char *lower = g_ascii_strdown(session->header[session->which], -1);
        snprintf(header, sizeof(header), "http:header:%s", lower);
        g_free(lower);
        moloch_nids_add_tag(session, MOLOCH_TAG_HTTP_REQUEST+session->which, header);
    }

    if (parser->method && strcasecmp("host", session->header[session->which]) == 0) {
        if (!session->hostString)
            session->hostString = g_string_new_len("//", 2);
        g_string_append_len(session->hostString, at, length);
    } 

    if (parser->method && strcasecmp("user-agent", session->header[session->which]) == 0) {
        if (!session->uaString)
            session->uaString = g_string_new_len(at, length);
        else
            g_string_append_len(session->uaString, at, length);
    } 

    if (parser->method && strcasecmp("x-forwarded-for", session->header[session->which]) == 0) {
        if (!session->xffString)
            session->xffString = g_string_new_len(at, length);
        else
            g_string_append_len(session->xffString, at, length);
    } 

    return 0;
}
/******************************************************************************/
void moloch_detect_dns(MolochSession_t *session, unsigned char *data, int len) 
{

    if (len < 18)
        return;

    int qr      = (data[2] >> 7) & 0x1;

    if (qr != 0)
        return;
    
    int qdcount = (data[4] << 8) | data[5];

    unsigned char *ptr = data + 12;
    unsigned char  name[8000];

    if (qdcount > 10 || qdcount <= 0)
        return;

    int i;
    for (i = 0; (ptr < data + len) && i < qdcount; i++) {
        unsigned char *nptr = name;

        while (ptr < data + len) {
            int len = *ptr;
            ptr++;
            if (len == 0)
                break;
            if (nptr != name) {
                *nptr = '.';
                nptr++;
            }

            int j;
            for (j = 0; j < len; j++) {
                register u_char c = ptr[j];

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
            ptr  += len;
        }
        ptr += 4; // qtype && qclass
        *nptr = 0;

        if (name == nptr)
            break;

        MolochString_t *hstring;

        HASH_FIND(s_, session->hosts, name, hstring);
        if (!hstring) {
            hstring = malloc(sizeof(*hstring));
            hstring->str = (char *)g_strdup((char *)name);
            HASH_ADD(s_, session->hosts, hstring->str, hstring);
        }
    }
    moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, "protocol:dns");
}

/******************************************************************************/
void moloch_detect_init()
{
    snprintf(nodeTag, sizeof(nodeTag), "node:%s", nodeName);
    moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, nodeTag, NULL);

    if (config.nodeClass) {
        snprintf(classTag, sizeof(classTag), "node:%s", config.nodeClass);
        moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, classTag, NULL);
    }

    if (extraTag) {
        moloch_db_get_tag(NULL, MOLOCH_TAG_TAGS, extraTag, NULL);
    }

    memset(&parserSettings, 0, sizeof(parserSettings));
    parserSettings.on_message_begin = moloch_hp_cb_on_message_begin;
    parserSettings.on_url = moloch_hp_cb_on_url;
    parserSettings.on_body = moloch_hp_cb_on_body;
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
