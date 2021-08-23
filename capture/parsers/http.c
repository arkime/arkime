/* Copyright 2012-2017 AOL Inc. All rights reserved.
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
#include "moloch.h"
#include <sys/socket.h>
#include <arpa/inet.h>

//#define HTTPDEBUG 1

#define MAX_URL_LENGTH 4096

typedef struct {
    MolochSession_t *session;
    GString         *urlString;
    GString         *hostString;
    GString         *cookieString;
    GString         *authString;
    GString         *proxyAuthString;

    GString         *valueString[2];

    char             header[2][40];
    short            pos[2];
    http_parser      parsers[2];

    GChecksum       *checksum[4];
    const char      *magicString[2];

    uint16_t         wParsers:2;
    uint16_t         inHeader:2;
    uint16_t         inValue:2;
    uint16_t         inBody:2;
    uint16_t         urlWhich:1;
    uint16_t         which:1;
    uint16_t         isConnect:2; // Keep track of each side that is CONNECT and completed headers
    uint16_t         reclassify:2; // Keep track of each side that needs to reclassify still
    uint16_t         http2Upgrade:1;
} HTTPInfo_t;

extern MolochConfig_t        config;
LOCAL  http_parser_settings  parserSettings;
extern uint32_t              pluginsCbs;
LOCAL  MolochStringHashStd_t httpReqHeaders;
LOCAL  MolochStringHashStd_t httpResHeaders;

LOCAL  int cookieKeyField;
LOCAL  int cookieValueField;
LOCAL  int hostField;
LOCAL  int userField;
LOCAL  int atField;
LOCAL  int urlsField;
LOCAL  int xffField;
LOCAL  int uaField;
LOCAL  int tagsReqField;
LOCAL  int tagsResField;
LOCAL  int md5Field;
LOCAL  int sha256Field;
LOCAL  int verReqField;
LOCAL  int verResField;
LOCAL  int pathField;
LOCAL  int keyField;
LOCAL  int valueField;
LOCAL  int magicField;
LOCAL  int statuscodeField;
LOCAL  int methodField;
LOCAL  int reqBodyField;
LOCAL  int headerReqField;
LOCAL  int headerReqValue;
LOCAL  int headerResField;
LOCAL  int headerResValue;

/******************************************************************************/
void http_common_parse_cookie(MolochSession_t *session, char *cookie, int len)
{
    char *start = cookie;
    char *end = cookie + len;
    while (1) {
        while (isspace(*start) && start < end) start++;
        char *equal = memchr(start, '=', end - start);
        if (!equal)
            break;
        moloch_field_string_add(cookieKeyField, session, start, equal-start, TRUE);
        start = memchr(equal+1, ';', end - (equal+1));
        if (config.parseCookieValue) {
            equal++;
            while (isspace(*equal) && equal < end) equal++;
            if (equal < end && equal != start)
                moloch_field_string_add(cookieValueField, session, equal, start?start-equal:end-equal, TRUE);
        }

        if(!start)
            break;
        start++;
    }
}
/******************************************************************************/
void http_common_add_header_value(MolochSession_t *session, int pos, const char *s, int l)
{
    while (isspace(*s)) {
        s++;
        l--;
    }

    switch (config.fields[pos]->type) {
    case MOLOCH_FIELD_TYPE_INT:
    case MOLOCH_FIELD_TYPE_INT_ARRAY:
    case MOLOCH_FIELD_TYPE_INT_HASH:
    case MOLOCH_FIELD_TYPE_INT_GHASH:
        moloch_field_int_add(pos, session, atoi(s));
        break;
    case MOLOCH_FIELD_TYPE_FLOAT:
    case MOLOCH_FIELD_TYPE_FLOAT_ARRAY:
    case MOLOCH_FIELD_TYPE_FLOAT_GHASH:
        moloch_field_float_add(pos, session, atof(s));
        break;
    case MOLOCH_FIELD_TYPE_STR:
    case MOLOCH_FIELD_TYPE_STR_ARRAY:
    case MOLOCH_FIELD_TYPE_STR_HASH:
    case MOLOCH_FIELD_TYPE_STR_GHASH:
        if (pos == headerReqValue || pos == headerResValue)
            moloch_field_string_add_lower(pos, session, s, MIN(l, 1024));
        else
            moloch_field_string_add(pos, session, s, l, TRUE);
        break;
    case MOLOCH_FIELD_TYPE_IP:
    case MOLOCH_FIELD_TYPE_IP_GHASH:
    {
        int i;
        gchar **parts = g_strsplit(s, ",", 0);

        for (i = 0; parts[i]; i++) {
            moloch_field_ip_add_str(pos, session, parts[i]);

            /* Add back maybe
            if (ia == 0 || ia == 0xffffffff) {
                moloch_session_add_tag(session, "http:bad-xff");
                if (config.debug)
                    LOG("INFO - Didn't understand ip: %s %s %d", s, ip, ia);
                continue;
            }
            */
        }

        g_strfreev(parts);
        break;
    }
    case MOLOCH_FIELD_TYPE_CERTSINFO:
        // Unsupported
        break;
    } /* SWITCH */
}
/******************************************************************************/
void http_common_add_header(MolochSession_t *session, int pos, int isReq, const char *name, int namelen, const char *value, int valuelen)
{
    MolochString_t        *hstring;

    char *lower = g_ascii_strdown(name, namelen);

    if (isReq)
        moloch_field_string_add(tagsReqField, session, (const char *)lower, namelen, TRUE);
    else
        moloch_field_string_add(tagsResField, session, (const char *)lower, namelen, TRUE);

    if (pos == 0) {
        if (isReq)
            HASH_FIND(s_, httpReqHeaders, lower, hstring);
        else
            HASH_FIND(s_, httpResHeaders, lower, hstring);

        if (hstring) {
            pos = (long)hstring->uw;
        } else if (isReq && config.parseHTTPHeaderRequestAll) { // Header in request
            moloch_field_string_add(headerReqField, session, lower, -1, TRUE);
            pos = headerReqValue;
        }
        else if (!isReq && config.parseHTTPHeaderResponseAll) { // Header in response
            moloch_field_string_add(headerResField, session, lower, -1, TRUE);
            pos = headerResValue;
        }
    }

    g_free(lower);

    if (pos == 0)
        return;

    http_common_add_header_value(session, pos, (char *)value, valuelen);
}
/******************************************************************************/
void http_common_parse_url(MolochSession_t *session, char *url, int len)
{
    char *end = url + len;
    char *question = memchr(url, '?', len);

    if (question) {
        moloch_field_string_add(pathField, session, url, question - url, TRUE);
        char *start = question+1;
        char *ch;
        int   field = keyField;
        for (ch = start; ch < end; ch++) {
            if (*ch == '&') {
                if (ch != start && (config.parseQSValue || field == keyField)) {
                    char *str = g_uri_unescape_segment(start, ch, NULL);
                    if (!str) {
                        moloch_field_string_add(field, session, start, ch-start, TRUE);
                    } else if (!moloch_field_string_add(field, session, str, -1, FALSE)) {
                        g_free(str);
                    }
                }
                start = ch+1;
                field = keyField;
                continue;
            } else if (*ch == '=') {
                if (ch != start && (config.parseQSValue || field == keyField)) {
                    char *str = g_uri_unescape_segment(start, ch, NULL);
                    if (!str) {
                        moloch_field_string_add(field, session, start, ch-start, TRUE);
                    } else if (!moloch_field_string_add(field, session, str, -1, FALSE)) {
                        g_free(str);
                    }
                }
                start = ch+1;
                field = valueField;
            }
        }
        if (config.parseQSValue && field == valueField && ch > start) {
            char *str = g_uri_unescape_segment(start, ch, NULL);
            if (!str) {
                moloch_field_string_add(field, session, start, ch-start, TRUE);
            } else if (!moloch_field_string_add(field, session, str, -1, FALSE)) {
                g_free(str);
            }
        }
    } else {
        moloch_field_string_add(pathField, session, url, len, TRUE);
    }
}
/******************************************************************************/
LOCAL int moloch_hp_cb_on_message_begin (http_parser *parser)
{
    HTTPInfo_t            *http = parser->data;
    MolochSession_t       *session = http->session;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which: %d", http->which);
#endif

    http->magicString[http->which] = NULL;
    http->inHeader &= ~(1 << http->which);
    http->inValue  &= ~(1 << http->which);
    http->inBody   &= ~(1 << http->which);
    g_checksum_reset(http->checksum[http->which]);
    if (config.supportSha256) {
        g_checksum_reset(http->checksum[http->which+2]);
    }

    if (pluginsCbs & MOLOCH_PLUGIN_HP_OMB)
        moloch_plugins_cb_hp_omb(session, parser);

    return 0;
}
/******************************************************************************/
LOCAL int moloch_hp_cb_on_url (http_parser *parser, const char *at, size_t length)
{
    HTTPInfo_t            *http = parser->data;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which:%d url %.*s", http->which, (int)length, at);
#endif

    if (!http->urlString) {
        http->urlString = g_string_new_len(at, length);
        http->urlWhich = http->which;
    } else
        g_string_append_len(http->urlString, at, length);

    return 0;
}

/******************************************************************************/
LOCAL int moloch_hp_cb_on_body (http_parser *parser, const char *at, size_t length)
{
    HTTPInfo_t            *http = parser->data;
    MolochSession_t       *session = http->session;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which: %d", http->which);
#endif

    if (!(http->inBody & (1 << http->which))) {
        if (moloch_memcasestr(at, length, "password=", 9) ||
            moloch_memcasestr(at, length, "passwd=", 7) ||
            moloch_memcasestr(at, length, "pass=", 5)
           ) {
            moloch_session_add_tag(session, "http:password");
        }

        http->magicString[http->which] = moloch_parsers_magic(session, magicField, at, length);
        http->inBody |= (1 << http->which);

        /* Put small requests in a field. */
        if (http->which == http->urlWhich && length <= config.maxReqBody && length > 0) {
            if (!config.reqBodyOnlyUtf8 || g_utf8_validate(at, length, NULL) == TRUE) {
                moloch_field_string_add(reqBodyField, session, at, length, TRUE);
            }
        }

    }

    g_checksum_update(http->checksum[http->which], (guchar *)at, length);
    if (config.supportSha256) {
        g_checksum_update(http->checksum[http->which+2], (guchar *)at, length);
    }

    if (pluginsCbs & MOLOCH_PLUGIN_HP_OB)
        moloch_plugins_cb_hp_ob(session, parser, at, length);

    return 0;
}

/******************************************************************************/
LOCAL void moloch_http_parse_authorization(MolochSession_t *session, char *str)
{
    gsize olen;

    while (isspace(*str)) str++;

    char *space = strchr(str, ' ');

    if (!space)
        return;

    moloch_field_string_add_lower(atField, session, str, space-str);

    if (strncasecmp("basic", str, 5) == 0) {
        str += 5;
        while (isspace(*str)) str++;

        int len = strlen(str);
        if (len < 2)
            return;

        // Yahoo reused Basic
        if (len < 6 || memcmp("token=", str, 6) != 0) {
            g_base64_decode_inplace(str, &olen);
            char *colon = strchr(str, ':');
            if (colon)
                *colon = 0;
            moloch_field_string_add(userField, session, str, -1, TRUE);
        }
    } else if (strncasecmp("digest", str, 6) == 0) {
        str += 5;
        while (isspace(*str)) str++;

        char *username = strstr(str, "username");
        if (!username) return;
        str = username + 8;
        while (isspace(*str)) str++;
        if (*str != '=') return;
        str++; // equal
        while (isspace(*str)) str++;

        int quote = 0;
        if (*str == '"') {
            quote = 1;
            str++;
        }
        char *end = str;
        while (*end && (*end != '"' || !quote) && (*end != ',' || quote)) {
            end++;
        }
        moloch_field_string_add(userField, session, str, end - str, TRUE);
    }
}
/******************************************************************************/
LOCAL int moloch_hp_cb_on_message_complete (http_parser *parser)
{
    HTTPInfo_t            *http = parser->data;
    MolochSession_t       *session = http->session;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which: %d", http->which);
#endif

    if (pluginsCbs & MOLOCH_PLUGIN_HP_OMC)
        moloch_plugins_cb_hp_omc(session, parser);

    if (http->inBody & (1 << http->which)) {
        const char *md5 = g_checksum_get_string(http->checksum[http->which]);
        moloch_field_string_uw_add(md5Field, session, (char*)md5, 32, (gpointer)http->magicString[http->which], TRUE);
        if (config.supportSha256) {
            const char *sha256 = g_checksum_get_string(http->checksum[http->which+2]);
            moloch_field_string_uw_add(sha256Field, session, (char*)sha256, 64, (gpointer)http->magicString[http->which], TRUE);
        }
    }

    return 0;
}

/******************************************************************************/
LOCAL void http_add_value(MolochSession_t *session, HTTPInfo_t *http)
{
    int                     pos  = http->pos[http->which];
    char                    *s   = http->valueString[http->which]->str;
    int                     l    = http->valueString[http->which]->len;

    http_common_add_header_value(session, pos, s, l);

    g_string_truncate(http->valueString[http->which], 0);
    http->pos[http->which] = 0;
}
/******************************************************************************/
LOCAL int moloch_hp_cb_on_header_field (http_parser *parser, const char *at, size_t length)
{
    HTTPInfo_t            *http = parser->data;
    MolochSession_t       *session = http->session;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which: %d field: %.*s", http->which, (int)length, at);
#endif

    if (http->inValue & (1 << http->which)) {
        http->inValue &= ~(1 << http->which);

        http->header[http->which][0] = 0;

        if (http->pos[http->which]) {
            http_add_value(session, http);
        }
    }

    if ((http->inHeader & (1 << http->which)) == 0) {
        http->inHeader |= (1 << http->which);
        if (http->urlString && parser->status_code == 0 && pluginsCbs & MOLOCH_PLUGIN_HP_OU) {
            moloch_plugins_cb_hp_ou(session, parser, http->urlString->str, http->urlString->len);
        }
    }

    int len = strlen(http->header[http->which]);
    size_t remaining = sizeof(http->header[http->which]) - len;
    if (remaining > 1) {
        int copy = MIN(length, remaining - 1);
        memcpy(http->header[http->which] + len, at, copy);
        http->header[http->which][len + copy] = 0;
    }

    return 0;
}

/******************************************************************************/
LOCAL int moloch_hp_cb_on_header_value (http_parser *parser, const char *at, size_t length)
{
    HTTPInfo_t            *http = parser->data;
    MolochSession_t       *session = http->session;
    MolochString_t        *hstring;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which: %d value: %.*s", http->which, (int)length, at);
#endif

    if ((http->inValue & (1 << http->which)) == 0) {
        http->inValue |= (1 << http->which);

        const char *header = http->header[http->which];
        moloch_plugins_cb_hp_ohfr(session, parser, header, strlen(header));
        char *lower = g_ascii_strdown(header, -1);
        moloch_plugins_cb_hp_ohf(session, parser, lower, strlen(lower));

        if (http->which == http->urlWhich)
            HASH_FIND(s_, httpReqHeaders, lower, hstring);
        else
            HASH_FIND(s_, httpResHeaders, lower, hstring);

        http->pos[http->which] = (long)(hstring?hstring->uw:0);

        if (http->pos[http->which] == 0) { // Header was not defined
            if ((http->which == 0) && config.parseHTTPHeaderRequestAll) { // Header in request
                moloch_field_string_add(headerReqField, session, lower, -1, TRUE);
                http->pos[http->which] = (long) headerReqValue;
            }
            else if ((http->which == 1) && config.parseHTTPHeaderResponseAll) { // Header in response
                moloch_field_string_add(headerResField, session, lower, -1, TRUE);
                http->pos[http->which] = (long) headerResValue;
            }
        }

        if (http->which == http->urlWhich)
            moloch_field_string_add(tagsReqField, session, lower, -1, TRUE);
        else {
            moloch_field_string_add(tagsResField, session, lower, -1, TRUE);
            if (strcmp(lower, "upgrade") == 0 && length >= 3 && memcmp(at, "h2c", 3) == 0) {
                http->http2Upgrade = 1;
            }
        }
        g_free(lower);
    }

    moloch_plugins_cb_hp_ohv(session, parser, at, length);

    // Request side
    if (parser->method) {
        if (strcasecmp("host", http->header[http->which]) == 0) {
            if (!http->hostString)
                http->hostString = g_string_new_len(at, length);
            else
                g_string_append_len(http->hostString, at, length);
        } else if (strcasecmp("cookie", http->header[http->which]) == 0) {
            if (!http->cookieString)
                http->cookieString = g_string_new_len(at, length);
            else
                g_string_append_len(http->cookieString, at, length);
        } else if (strcasecmp("authorization", http->header[http->which]) == 0) {
            if (!http->authString)
                http->authString = g_string_new_len(at, length);
            else
                g_string_append_len(http->authString, at, length);
        } else if (strcasecmp("proxy-authorization", http->header[http->which]) == 0) {
            if (!http->proxyAuthString)
                http->proxyAuthString = g_string_new_len(at, length);
            else 
                g_string_append_len(http->proxyAuthString, at, length);
        }
    }

    if (http->pos[http->which]) {
        if (!http->valueString[http->which])
            http->valueString[http->which] = g_string_new_len(at, length);
        else
            g_string_append_len(http->valueString[http->which], at, length);
    }

    return 0;
}
/******************************************************************************/
LOCAL int moloch_hp_cb_on_headers_complete (http_parser *parser)
{
    HTTPInfo_t            *http = parser->data;
    MolochSession_t       *session = http->session;
    char                   version[20];

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which: %d code: %d method: %d upgrade: %d", http->which, parser->status_code, parser->method, parser->upgrade);
#endif

    if (parser->method == HTTP_CONNECT) {
        http->reclassify |= (1 << http->which);
        http->isConnect |= (1 << http->which);
    }

    int len = snprintf(version, sizeof(version), "%d.%d", parser->http_major, parser->http_minor);

    if (parser->status_code == 0) {
        moloch_field_string_add(methodField, session, http_method_str(parser->method), -1, TRUE);
        moloch_field_string_add(verReqField, session, version, len, TRUE);
    } else {
        moloch_field_int_add(statuscodeField, session, parser->status_code);
        moloch_field_string_add(verResField, session, version, len, TRUE);
    }

    if (http->inValue & (1 << http->which) && http->pos[http->which]) {
        http_add_value(session, http);
    }

    http->header[0][0] = http->header[1][0] = 0;

    if (http->urlString) {
        char *ch = http->urlString->str;
        while (*ch) {
            if (*ch < 32) {
                moloch_session_add_tag(session, "http:control-char");
                break;
            }
            ch++;
        }
    }

    if (http->cookieString && http->cookieString->str[0]) {
        http_common_parse_cookie(session, http->cookieString->str, http->cookieString->len);
        g_string_truncate(http->cookieString, 0);
    }

    if (http->authString && http->authString->str[0]) {
        moloch_http_parse_authorization(session, http->authString->str);
        g_string_truncate(http->authString, 0);
    }

    /* Adding an additional check for proxy-authorization string*/
    if (http->proxyAuthString && http->proxyAuthString->str[0]){
	    moloch_http_parse_authorization(session, http->proxyAuthString->str);
	    g_string_truncate(http->proxyAuthString, 0);
    }

    if (http->hostString) {
        g_string_ascii_down(http->hostString);
    }

    gboolean truncated = FALSE;
    if (http->urlString && http->hostString) {
        char *colon = strchr(http->hostString->str, ':');
        if (colon) {
            moloch_field_string_add(hostField, session, http->hostString->str, colon - http->hostString->str, TRUE);
        } else {
            moloch_field_string_add(hostField, session, http->hostString->str, http->hostString->len, TRUE);
        }

        http_common_parse_url(session, http->urlString->str, http->urlString->len);

        if (http->urlString->str[0] != '/') {
            char *result = strstr(http->urlString->str, http->hostString->str);

            /* If the host header is in the first 8 bytes of url then just use the url */
            if (result && result - http->urlString->str <= 8) {
                if (http->urlString->len > MAX_URL_LENGTH) {
                    truncated = TRUE;
                    g_string_truncate(http->urlString, MAX_URL_LENGTH);
                }
                if (!moloch_field_string_add(urlsField, session, http->urlString->str, http->urlString->len, FALSE))
                    g_free(http->urlString->str);
                g_string_free(http->urlString, FALSE);
                g_string_free(http->hostString, TRUE);
            } else {
                /* Host header doesn't match the url */
                g_string_append(http->hostString, ";");
                g_string_append(http->hostString, http->urlString->str);

                if (http->hostString->len > MAX_URL_LENGTH) {
                    truncated = TRUE;
                    g_string_truncate(http->hostString, MAX_URL_LENGTH);
                }
                if (!moloch_field_string_add(urlsField, session, http->hostString->str, http->hostString->len, FALSE))
                    g_free(http->hostString->str);

                g_string_free(http->urlString, TRUE);
                g_string_free(http->hostString, FALSE);
            }
        } else {
            /* Normal case, url starts with /, so no extra host in url */
            g_string_append(http->hostString, http->urlString->str);

            if (http->hostString->len > MAX_URL_LENGTH) {
                truncated = TRUE;
                g_string_truncate(http->hostString, MAX_URL_LENGTH);
            }
            if (!moloch_field_string_add(urlsField, session, http->hostString->str, http->hostString->len, FALSE))
                g_free(http->hostString->str);
            g_string_free(http->urlString, TRUE);
            g_string_free(http->hostString, FALSE);
        }

        http->urlString = NULL;
        http->hostString = NULL;
    } else if (http->urlString) {

        if (http->urlString->len > MAX_URL_LENGTH) {
            truncated = TRUE;
            g_string_truncate(http->urlString, MAX_URL_LENGTH);
        }
        if (!moloch_field_string_add(urlsField, session, http->urlString->str, http->urlString->len, FALSE))
                g_free(http->urlString->str);
        g_string_free(http->urlString, FALSE);

        http->urlString = NULL;
    } else if (http->hostString) {
        char *colon = strchr(http->hostString->str, ':');
        if (colon) {
            moloch_field_string_add(hostField, session, http->hostString->str, colon - http->hostString->str, TRUE);
        } else {
            moloch_field_string_add(hostField, session, http->hostString->str, http->hostString->len, TRUE);
        }

        g_string_free(http->hostString, TRUE);
        http->hostString = NULL;
    }

    if (truncated)
        moloch_session_add_tag(session, "http:url-truncated");

    moloch_session_add_protocol(session, "http");

    if (pluginsCbs & MOLOCH_PLUGIN_HP_OHC)
        moloch_plugins_cb_hp_ohc(session, parser);

    return 0;
}

/*############################## SHARED ##############################*/
/******************************************************************************/
LOCAL int http_parse(MolochSession_t *session, void *uw, const unsigned char *data, int remaining, int which)
{
    HTTPInfo_t            *http          = uw;

    if (http->http2Upgrade) {
        moloch_parsers_classify_tcp(session, data, remaining, which);
        return MOLOCH_PARSER_UNREGISTER;
    }

    http->which = which;
#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: enter %d - %d %.*s", http->which, remaining, remaining, data);
#endif

    if (http->isConnect) {
        // Check if either side needs to be classified
        if (http->reclassify & (1 << which)) {
            http->reclassify &= ~(1 << which);
            moloch_parsers_classify_tcp(session, data, remaining, which);

            // Both sides have been reclassified, remove http parser
            if (http->reclassify == 0 && http->isConnect == 0x3) {
                moloch_parsers_unregister(session, uw);
            }
            return 0;
        }
    }

    if ((http->wParsers & (1 << http->which)) == 0) {
        return 0;
    }

    while (remaining > 0) {
        int len = http_parser_execute(&http->parsers[http->which], &parserSettings, (char *)data, remaining);
#ifdef HTTPDEBUG
            LOG("HTTPDEBUG: parse result: %d input: %d errno: %d", len, remaining, http->parsers[http->which].http_errno);
#endif
        if (len <= 0) {
            http->wParsers &= ~(1 << http->which);
            if (!http->wParsers) {
                moloch_parsers_unregister(session, uw);
            }
            break;
        }
        data += len;
        remaining -= len;
    }
    return 0;
}
/******************************************************************************/
void http_save(MolochSession_t UNUSED(*session), void *uw, int final)
{
    if (!final)
        return;

    HTTPInfo_t            *http          = uw;

#ifdef HTTPDEBUG
    LOG("Save callback %d", final);
#endif
    if (http->wParsers & 0x1) {
        http_parser_execute(&http->parsers[0], &parserSettings, 0, 0);
    }

    if (http->wParsers & 0x2) {
        http_parser_execute(&http->parsers[1], &parserSettings, 0, 0);
    }

}
/******************************************************************************/
LOCAL void http_free(MolochSession_t UNUSED(*session), void *uw)
{
    HTTPInfo_t            *http          = uw;

    if (http->urlString)
        g_string_free(http->urlString, TRUE);
    if (http->hostString)
        g_string_free(http->hostString, TRUE);
    if (http->cookieString)
        g_string_free(http->cookieString, TRUE);
    if (http->authString)
        g_string_free(http->authString, TRUE);
    if (http->proxyAuthString)
        g_string_free(http->proxyAuthString, TRUE);
    if (http->valueString[0])
        g_string_free(http->valueString[0], TRUE);
    if (http->valueString[1])
        g_string_free(http->valueString[1], TRUE);
    g_checksum_free(http->checksum[0]);
    g_checksum_free(http->checksum[1]);
    if (config.supportSha256) {
        g_checksum_free(http->checksum[2]);
        g_checksum_free(http->checksum[3]);
    }

    MOLOCH_TYPE_FREE(HTTPInfo_t, http);
}
/******************************************************************************/
LOCAL void http_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    if (moloch_session_has_protocol(session, "http"))
        return;

    moloch_session_add_protocol(session, "http");

    HTTPInfo_t            *http          = MOLOCH_TYPE_ALLOC0(HTTPInfo_t);

    http->checksum[0] = g_checksum_new(G_CHECKSUM_MD5);
    http->checksum[1] = g_checksum_new(G_CHECKSUM_MD5);
    if (config.supportSha256) {
        http->checksum[2] = g_checksum_new(G_CHECKSUM_SHA256);
        http->checksum[3] = g_checksum_new(G_CHECKSUM_SHA256);
    }

    http_parser_init(&http->parsers[0], HTTP_BOTH);
    http_parser_init(&http->parsers[1], HTTP_BOTH);
    http->wParsers = 3;
    http->parsers[0].data = http;
    http->parsers[1].data = http;

    http->session = session;

    moloch_parsers_register2(session, http_parse, http, http_free, http_save);
}
/******************************************************************************/
void moloch_parser_init()
{
static const char *method_strings[] =
    {
#define XX(num, name, string) #string,
    HTTP_METHOD_MAP(XX)
#undef XX
    0
    };

    hostField = moloch_field_define("http", "lotermfield",
        "host.http", "Hostname", "http.host",
        "HTTP host header field",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        "aliases", "[\"http.host\"]",
        "category", "host",
        (char *)NULL);

    moloch_field_define("http", "lotextfield",
        "host.http.tokens", "Hostname Tokens", "http.hostTokens",
        "HTTP host Tokens header field",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_FAKE,
        "aliases", "[\"http.host.tokens\"]",
        (char *)NULL);

    urlsField = moloch_field_define("http", "termfield",
        "http.uri", "URI", "http.uri",
        "URIs for request",
        MOLOCH_FIELD_TYPE_STR_HASH, MOLOCH_FIELD_FLAG_CNT,
        "category", "[\"url\",\"host\"]",
        (char *)NULL);

    moloch_field_define("http", "lotextfield",
        "http.uri.tokens", "URI Tokens", "http.uriTokens",
        "URIs Tokens for request",
        MOLOCH_FIELD_TYPE_STR_HASH, MOLOCH_FIELD_FLAG_FAKE,
        (char *)NULL);

    xffField = moloch_field_define("http", "ip",
        "ip.xff", "XFF IP", "http.xffIp",
        "X-Forwarded-For Header",
        MOLOCH_FIELD_TYPE_IP_GHASH, MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_IPPRE,
        "category", "ip",
        (char *)NULL);

    uaField = moloch_field_define("http", "termfield",
        "http.user-agent", "Useragent", "http.useragent",
        "User-Agent Header",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    moloch_field_define("http", "lotextfield",
        "http.user-agent.tokens", "Useragent Tokens", "http.useragentTokens",
        "User-Agent Header Tokens",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_FAKE,
        (char *)NULL);

    tagsReqField = moloch_field_define("http", "lotermfield",
        "http.hasheader.src", "Has Src Header", "http.requestHeader",
        "Request has header present",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    tagsResField = moloch_field_define("http", "lotermfield",
        "http.hasheader.dst", "Has Dst Header", "http.responseHeader",
        "Response has header present",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    headerReqField = moloch_field_define("http", "lotermfield",
        "http.header.request.field", "Request Header Fields", "http.requestHeaderField",
        "Contains Request header fields",
        MOLOCH_FIELD_TYPE_STR_ARRAY, MOLOCH_FIELD_FLAG_NODB,
        (char *)NULL);

    headerReqValue = moloch_field_define("http","lotermfield",
        "http.hasheader.src.value", "Request Header Values", "http.requestHeaderValue",
        "Contains request header values",
        MOLOCH_FIELD_TYPE_STR_ARRAY, MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    headerResField = moloch_field_define("http","lotermfield",
        "http.header.response.field","Response Header fields", "http.responseHeaderField",
        "Contains response header fields",
        MOLOCH_FIELD_TYPE_STR_ARRAY, MOLOCH_FIELD_FLAG_NODB,
        (char *)NULL);

    headerResValue = moloch_field_define("http","lotermfield",
        "http.hasheader.dst.value", "Response Header Values", "http.responseHeaderValue",
        "Contains response header values",
        MOLOCH_FIELD_TYPE_STR_ARRAY, MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    moloch_field_define("http", "lotermfield",
        "http.hasheader", "Has Src or Dst Header", "hhall",
        "Shorthand for http.hasheader.src or http.hasheader.dst",
        0,  MOLOCH_FIELD_FLAG_FAKE,
        "regex", "^http\\\\.hasheader\\\\.(?:(?!(cnt|value)$).)*$",
        (char *)NULL);

    moloch_field_define("http", "lotermfield",
        "http.hasheader.value", "Has Value in Src or Dst Header", "hhvalueall",
        "Shorthand for http.hasheader.src.value or http.hasheader.dst.value",
        0,  MOLOCH_FIELD_FLAG_FAKE,
        "regex", "^http\\\\.hasheader\\\\.(src|dst)\\\\.value$",
        (char *)NULL);

    md5Field = moloch_field_define("http", "lotermfield",
        "http.md5", "Body MD5", "http.md5",
        "MD5 of http body response",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        "category", "md5",
        (char *)NULL);

    if (config.supportSha256) {
        sha256Field = moloch_field_define("http", "lotermfield",
            "http.sha256", "Body SHA256", "http.sha256",
            "SHA256 of http body response",
            MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
            "category", "sha256",
            (char *)NULL);
    }

    moloch_field_define("http", "termfield",
        "http.version", "Version", "httpversion",
        "HTTP version number",
        0, MOLOCH_FIELD_FLAG_FAKE,
        "regex", "^http.version.[a-z]+$",
        (char *)NULL);

    verReqField = moloch_field_define("http", "termfield",
        "http.version.src", "Src Version", "http.clientVersion",
        "Request HTTP version number",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    verResField = moloch_field_define("http", "termfield",
        "http.version.dst", "Dst Version", "http.serverVersion",
        "Response HTTP version number",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    pathField = moloch_field_define("http", "termfield",
        "http.uri.path", "URI Path", "http.path",
        "Path portion of URI",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    keyField = moloch_field_define("http", "termfield",
        "http.uri.key", "QS Keys", "http.key",
        "Keys from query string of URI",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    valueField = moloch_field_define("http", "termfield",
        "http.uri.value", "QS Values", "http.value",
        "Values from query string of URI",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    cookieKeyField = moloch_field_define("http", "termfield",
        "http.cookie.key", "Cookie Keys", "http.cookieKey",
        "The keys to cookies sent up in requests",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    cookieValueField = moloch_field_define("http", "termfield",
        "http.cookie.value", "Cookie Values", "http.cookieValue",
        "The values to cookies sent up in requests",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    methodField = moloch_field_define("http", "termfield",
        "http.method", "Request Method", "http.method",
        "HTTP Request Method",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    magicField = moloch_field_define("http", "termfield",
        "http.bodymagic", "Body Magic", "http.bodyMagic",
        "The content type of body determined by libfile/magic",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    userField = moloch_field_define("http", "termfield",
        "http.user", "User", "http.user",
        "HTTP Auth User",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        "category", "user",
        (char *)NULL);

    atField = moloch_field_define("http", "lotermfield",
        "http.authtype", "Auth Type", "http.authType",
        "HTTP Auth Type",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    statuscodeField = moloch_field_define("http", "integer",
        "http.statuscode", "Status Code", "http.statuscode",
        "Response HTTP numeric status code",
        MOLOCH_FIELD_TYPE_INT_GHASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    reqBodyField = moloch_field_define("http", "termfield",
        "http.reqbody", "Request Body", "http.requestBody",
        "HTTP Request Body",
        MOLOCH_FIELD_TYPE_STR_HASH, 0,
        (char *)NULL);

    HASH_INIT(s_, httpReqHeaders, moloch_string_hash, moloch_string_cmp);
    HASH_INIT(s_, httpResHeaders, moloch_string_hash, moloch_string_cmp);

    moloch_config_add_header(&httpReqHeaders, "x-forwarded-for", xffField);
    moloch_config_add_header(&httpReqHeaders, "user-agent", uaField);
    moloch_config_add_header(&httpReqHeaders, "host", hostField);
    moloch_config_load_header("headers-http-request", "http", "Request header ", "http.", "http.request-", &httpReqHeaders, 0);
    moloch_config_load_header("headers-http-response", "http", "Response header ", "http.", "http.response-", &httpResHeaders, 0);

    int i;
    for (i = 0; method_strings[i]; i++) {
        moloch_parsers_classifier_register_tcp("http", NULL, 0, (unsigned char*)method_strings[i], strlen(method_strings[i]), http_classify);
    }

    moloch_parsers_classifier_register_tcp("http", NULL, 0, (unsigned char*)"HTTP", 4, http_classify);

    memset(&parserSettings, 0, sizeof(parserSettings));
    parserSettings.on_message_begin = moloch_hp_cb_on_message_begin;
    parserSettings.on_url = moloch_hp_cb_on_url;
    parserSettings.on_body = moloch_hp_cb_on_body;
    parserSettings.on_headers_complete = moloch_hp_cb_on_headers_complete;
    parserSettings.on_message_complete = moloch_hp_cb_on_message_complete;
    parserSettings.on_header_field = moloch_hp_cb_on_header_field;
    parserSettings.on_header_value = moloch_hp_cb_on_header_value;
}
