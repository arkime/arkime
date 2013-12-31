#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "moloch.h"

typedef struct {
    MolochSession_t *session;
    GString         *urlString;
    GString         *hostString;

    GString         *valueString[2];

    char             header[2][40];
    short            pos[2];
    http_parser      parsers[2];

    GChecksum       *checksum[2];

    uint16_t         wParsers:2;
    uint16_t         inHeader:2;
    uint16_t         inValue:2;
    uint16_t         inBody:2;
} HTTPInfo_t;

extern MolochConfig_t        config;
static http_parser_settings  parserSettings;
extern uint32_t              pluginsCbs;
extern MolochStringHashStd_t httpReqHeaders;
extern MolochStringHashStd_t httpResHeaders;

/******************************************************************************/
int
moloch_hp_cb_on_message_begin (http_parser *parser)
{
    HTTPInfo_t            *http = parser->data;
    MolochSession_t       *session = http->session;

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
    HTTPInfo_t            *http = parser->data;

#ifdef HTTPDEBUG
    MolochSession_t       *session = http->session;
    LOG("HTTPDEBUG: which:%d url %.*s", session->which, (int)length, at);
#endif

    if (!http->urlString)
        http->urlString = g_string_new_len(at, length);
    else
        g_string_append_len(http->urlString, at, length);

    return 0;
}

/******************************************************************************/
int
moloch_hp_cb_on_body (http_parser *parser, const char *at, size_t length)
{
    HTTPInfo_t            *http = parser->data;
    MolochSession_t       *session = http->session;

#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: which: %d", session->which);
#endif

    if (!(http->inBody & (1 << session->which))) {
        if (moloch_memstr(at, length, "password=", 9)) {
            moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "http:password");
        }

        moloch_parsers_magic_tag(session, "http:content", at, length);
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
    HTTPInfo_t            *http = parser->data;
    MolochSession_t       *session = http->session;

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
http_add_value(MolochSession_t *session, HTTPInfo_t *http)
{
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
    HTTPInfo_t            *http = parser->data;
    MolochSession_t       *session = http->session;

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
            http_add_value(session, http);
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
    HTTPInfo_t            *http = parser->data;
    MolochSession_t       *session = http->session;
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

        http->pos[session->which] = (hstring?hstring->uw:0);

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
    HTTPInfo_t            *http = parser->data;
    MolochSession_t       *session = http->session;
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
        http_add_value(session, http);
    }

    if (pluginsCbs & MOLOCH_PLUGIN_HP_OHC)
        moloch_plugins_cb_hp_ohc(session, parser);

    return 0;
}

/*############################## SHARED ##############################*/
/******************************************************************************/
int http_parse(MolochSession_t *session, void *uw, const unsigned char *data, int remaining)
{
    HTTPInfo_t            *http          = uw;
#ifdef HTTPDEBUG
    LOG("HTTPDEBUG: enter %d initial %d - %d %.*s", session->which, initial, remaining, remaining, data);
#endif

    if ((http->wParsers & (1 << session->which)) == 0) {
        return 0;
    }

    while (remaining > 0) {
        int len = http_parser_execute(&http->parsers[session->which], &parserSettings, (char *)data, remaining);
#ifdef HTTPDEBUG
            LOG("HTTPDEBUG: parse result: %d input: %d errno: %d", len, remaining, http->parsers[session->which].http_errno);
#endif
        if (len <= 0) {
            http->wParsers &= ~(1 << session->which);
            if (http->wParsers) {
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
void http_free(MolochSession_t UNUSED(*session), void *uw)
{
    HTTPInfo_t            *http          = uw;

    if (http->urlString)
        g_string_free(http->urlString, TRUE);
    if (http->hostString)
        g_string_free(http->hostString, TRUE);
    if (http->valueString[0])
        g_string_free(http->valueString[0], TRUE);
    if (http->valueString[1])
        g_string_free(http->valueString[1], TRUE);

    g_checksum_free(http->checksum[0]);
    g_checksum_free(http->checksum[1]);

    MOLOCH_TYPE_FREE(HTTPInfo_t, http);
}
/******************************************************************************/
void http_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len))
{
    if (moloch_nids_has_tag(session, MOLOCH_FIELD_TAGS, "protocol:http"))
        return;

    moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:http");

    HTTPInfo_t            *http          = MOLOCH_TYPE_ALLOC0(HTTPInfo_t);

    http->checksum[0] = g_checksum_new(G_CHECKSUM_MD5);
    http->checksum[1] = g_checksum_new(G_CHECKSUM_MD5);

    http_parser_init(&http->parsers[0], HTTP_BOTH);
    http_parser_init(&http->parsers[1], HTTP_BOTH);
    http->wParsers = 3;
    http->parsers[0].data = http;
    http->parsers[1].data = http;

    http->session = session;

    moloch_parsers_register(session, http_parse, http, http_free);
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

    int i;
    for (i = 0; method_strings[i]; i++) {
        moloch_parsers_classifier_register_tcp("http", 0, (unsigned char*)method_strings[i], strlen(method_strings[i]), http_classify);
    }

    memset(&parserSettings, 0, sizeof(parserSettings));
    parserSettings.on_message_begin = moloch_hp_cb_on_message_begin;
    parserSettings.on_url = moloch_hp_cb_on_url;
    parserSettings.on_body = moloch_hp_cb_on_body;
    parserSettings.on_headers_complete = moloch_hp_cb_on_headers_complete;
    parserSettings.on_message_complete = moloch_hp_cb_on_message_complete;
    parserSettings.on_header_field = moloch_hp_cb_on_header_field;
    parserSettings.on_header_value = moloch_hp_cb_on_header_value;
}

