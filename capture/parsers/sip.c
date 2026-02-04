/* Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SIP (Session Initiation Protocol) parser
 * RFC 3261
 */
#include "arkime.h"

extern ArkimeConfig_t config;

LOCAL int methodField;
LOCAL int statusCodeField;
LOCAL int callIdField;
LOCAL int fromField;
LOCAL int toField;
LOCAL int userAgentField;
LOCAL int viaField;
LOCAL int contactField;
LOCAL int userField;

/******************************************************************************/
// Find end of line, return length including CRLF or -1 if not found
LOCAL int sip_find_line(const uint8_t *data, int len, int *lineLen)
{
    for (int i = 0; i < len; i++) {
        if (data[i] == '\r' && i + 1 < len && data[i + 1] == '\n') {
            *lineLen = i;
            return i + 2;
        }
        if (data[i] == '\n') {
            *lineLen = i;
            return i + 1;
        }
    }
    return -1;
}

/******************************************************************************/
// Extract user part from SIP URI: sip:user@host or "Name" <sip:user@host>
LOCAL void sip_extract_user(ArkimeSession_t *session, const char *value, int valueLen)
{
    // Find sip: or sips:
    const char *sipUri = arkime_memcasestr(value, valueLen, "sip:", 4);
    if (!sipUri) {
        sipUri = arkime_memcasestr(value, valueLen, "sips:", 5);
        if (!sipUri)
            return;
        sipUri += 5;
    } else {
        sipUri += 4;
    }

    int remaining = valueLen - (sipUri - value);
    if (remaining <= 0)
        return;

    // Find @ to get user part
    int userLen = 0;
    for (int i = 0; i < remaining; i++) {
        if (sipUri[i] == '@') {
            userLen = i;
            break;
        }
        if (sipUri[i] == '>' || sipUri[i] == ';' || sipUri[i] == ' ')
            break;
    }

    if (userLen > 0) {
        arkime_field_string_add(userField, session, sipUri, userLen, TRUE);
    }
}

/******************************************************************************/
// Parse SIP header, return Content-Length value if found, -1 otherwise
LOCAL int sip_parse_header(ArkimeSession_t *session, const uint8_t *line, int lineLen)
{
    // Find colon
    int colonPos = -1;
    for (int i = 0; i < lineLen; i++) {
        if (line[i] == ':') {
            colonPos = i;
            break;
        }
    }

    if (colonPos <= 0)
        return -1;

    // Skip whitespace after colon
    int valueStart = colonPos + 1;
    while (valueStart < lineLen && (line[valueStart] == ' ' || line[valueStart] == '\t'))
        valueStart++;

    // Check for empty value
    if (valueStart >= lineLen)
        return -1;

    int valueLen = lineLen - valueStart;

    const char *name = (char *)line;
    const char *value = (char *)line + valueStart;

    // Match headers (including compact forms from RFC 3261)
    if ((colonPos == 7 && strncasecmp(name, "Call-ID", 7) == 0) ||
        (colonPos == 1 && (*name == 'i' || *name == 'I'))) {
        arkime_field_string_add(callIdField, session, value, valueLen, TRUE);
    } else if ((colonPos == 4 && strncasecmp(name, "From", 4) == 0) ||
               (colonPos == 1 && (*name == 'f' || *name == 'F'))) {
        arkime_field_string_add(fromField, session, value, valueLen, TRUE);
        sip_extract_user(session, value, valueLen);
    } else if ((colonPos == 2 && strncasecmp(name, "To", 2) == 0) ||
               (colonPos == 1 && (*name == 't' || *name == 'T'))) {
        arkime_field_string_add(toField, session, value, valueLen, TRUE);
        sip_extract_user(session, value, valueLen);
    } else if (colonPos == 10 && strncasecmp(name, "User-Agent", 10) == 0) {
        arkime_field_string_add(userAgentField, session, value, valueLen, TRUE);
    } else if ((colonPos == 3 && strncasecmp(name, "Via", 3) == 0) ||
               (colonPos == 1 && (*name == 'v' || *name == 'V'))) {
        arkime_field_string_add(viaField, session, value, valueLen, TRUE);
    } else if ((colonPos == 7 && strncasecmp(name, "Contact", 7) == 0) ||
               (colonPos == 1 && (*name == 'm' || *name == 'M'))) {
        arkime_field_string_add(contactField, session, value, valueLen, TRUE);
    } else if (colonPos == 13 && strncasecmp(name, "Authorization", 13) == 0) {
        // Extract username from Digest auth
        const char *userPtr = arkime_memcasestr(value, valueLen, "username=\"", 10);
        if (userPtr) {
            userPtr += 10;
            int remaining = valueLen - (userPtr - value);
            for (int i = 0; i < remaining; i++) {
                if (userPtr[i] == '"') {
                    arkime_field_string_add(userField, session, userPtr, i, TRUE);
                    break;
                }
            }
        }
    } else if ((colonPos == 14 && strncasecmp(name, "Content-Length", 14) == 0) ||
               (colonPos == 1 && (*name == 'l' || *name == 'L'))) {
        return atoi(value);
    }

    return -1;
}

/******************************************************************************/
// Check if valid SIP method, switch on first char for efficiency
LOCAL int sip_is_method(const char *method, int methodLen)
{
    switch (method[0]) {
    case 'A':
        if (methodLen == 3 && memcmp(method, "ACK", 3) == 0)
            return 1;
        break;
    case 'B':
        if (methodLen == 3 && memcmp(method, "BYE", 3) == 0)
            return 1;
        break;
    case 'C':
        if (methodLen == 6 && memcmp(method, "CANCEL", 6) == 0)
            return 1;
        break;
    case 'I':
        if (methodLen == 6 && memcmp(method, "INVITE", 6) == 0)
            return 1;
        if (methodLen == 4 && memcmp(method, "INFO", 4) == 0)
            return 1;
        break;
    case 'M':
        if (methodLen == 7 && memcmp(method, "MESSAGE", 7) == 0)
            return 1;
        break;
    case 'N':
        if (methodLen == 6 && memcmp(method, "NOTIFY", 6) == 0)
            return 1;
        break;
    case 'O':
        if (methodLen == 7 && memcmp(method, "OPTIONS", 7) == 0)
            return 1;
        break;
    case 'P':
        if (methodLen == 5 && memcmp(method, "PRACK", 5) == 0)
            return 1;
        break;
    case 'R':
        if (methodLen == 8 && memcmp(method, "REGISTER", 8) == 0)
            return 1;
        if (methodLen == 5 && memcmp(method, "REFER", 5) == 0)
            return 1;
        break;
    case 'S':
        if (methodLen == 9 && memcmp(method, "SUBSCRIBE", 9) == 0)
            return 1;
        break;
    case 'U':
        if (methodLen == 6 && memcmp(method, "UPDATE", 6) == 0)
            return 1;
        break;
    }
    return 0;
}

/******************************************************************************/
// Parse SIP request line: "METHOD uri SIP/2.0"
LOCAL int sip_parse_request(ArkimeSession_t *session, const uint8_t *line, int lineLen)
{
    // Find first space
    int methodEnd = -1;
    for (int i = 0; i < lineLen && i < 20; i++) {
        if (line[i] == ' ') {
            methodEnd = i;
            break;
        }
    }

    if (methodEnd <= 0)
        return -1;

    const char *method = (char *)line;
    if (!sip_is_method(method, methodEnd))
        return -1;

    // Verify SIP/2.0 at end
    if (lineLen < methodEnd + 10)
        return -1;

    if (!arkime_memstr((char *)line, lineLen, "SIP/2.0", 7))
        return -1;

    arkime_field_string_add(methodField, session, method, methodEnd, TRUE);
    return 0;
}

/******************************************************************************/
// Parse SIP response line: "SIP/2.0 200 OK"
LOCAL int sip_parse_response(ArkimeSession_t *session, const uint8_t *line, int lineLen)
{
    if (lineLen < 12)
        return -1;

    if (memcmp(line, "SIP/2.0 ", 8) != 0)
        return -1;

    int statusCode = atoi((char *)line + 8);
    if (statusCode >= 100 && statusCode < 700) {
        arkime_field_int_add(statusCodeField, session, statusCode);
    }

    return 0;
}

/******************************************************************************/
// Process SIP message headers, return Content-Length (0 if not found), set isResponse
LOCAL int sip_process(ArkimeSession_t *session, const uint8_t *data, int len, int *isResponse)
{
    int offset = 0;
    int isFirst = 1;
    int contentLength = 0;
    *isResponse = 0;

    while (offset < len) {
        int lineLen = 0;
        int consumed = sip_find_line(data + offset, len - offset, &lineLen);

        if (consumed < 0)
            break;

        if (lineLen == 0) {
            // Empty line - end of headers
            break;
        }

        if (isFirst) {
            isFirst = 0;
            if (lineLen >= 7 && memcmp(data + offset, "SIP/2.0", 7) == 0) {
                sip_parse_response(session, data + offset, lineLen);
                *isResponse = 1;
            } else {
                sip_parse_request(session, data + offset, lineLen);
            }
        } else {
            int cl = sip_parse_header(session, data + offset, lineLen);
            if (cl >= 0)
                contentLength = cl;
        }

        offset += consumed;
    }

    return contentLength;
}

/******************************************************************************/
LOCAL int sip_udp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    int isResponse;
    sip_process(session, data, len, &isResponse);
    return 0;
}

/******************************************************************************/
LOCAL int sip_tcp_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int which)
{
    ArkimeParserBuf_t *sip = uw;

    arkime_parser_buf_add(sip, which, data, len);

    // Find double CRLF to detect end of message headers
    while (sip->len[which] > 4) {
        // Look for end of headers
        int endPos = -1;
        for (int i = 0; i < sip->len[which] - 3; i++) {
            if (sip->buf[which][i] == '\r' && sip->buf[which][i + 1] == '\n' &&
                sip->buf[which][i + 2] == '\r' && sip->buf[which][i + 3] == '\n') {
                endPos = i + 4;
                break;
            }
        }

        if (endPos < 0)
            break;

        int isResponse = 0;
        int contentLength = sip_process(session, sip->buf[which], endPos, &isResponse);

        // Track serverWhich - server sends responses
        if (isResponse) {
            sip->serverWhich = which;
        }

        // Delete headers
        arkime_parser_buf_del(sip, which, endPos);

        // Skip body if present
        if (contentLength > 0) {
            arkime_parser_buf_skip(sip, which, contentLength);
        }
    }

    // Limit parsing
    sip->version++;
    if (sip->version > 200) {
        return ARKIME_PARSER_UNREGISTER;
    }

    return 0;
}

/******************************************************************************/
LOCAL void sip_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "sip"))
        return;

    if (len < 12)
        return;

    // Verify SIP/2.0 present
    if (!arkime_memstr((char *)data, MIN(len, 200), "SIP/2.0", 7))
        return;

    arkime_session_add_protocol(session, "sip");
    arkime_parsers_register(session, sip_udp_parser, NULL, NULL);
}

/******************************************************************************/
LOCAL void sip_tcp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "sip"))
        return;

    if (len < 12)
        return;

    // Verify SIP/2.0 present
    if (!arkime_memstr((char *)data, MIN(len, 200), "SIP/2.0", 7))
        return;

    arkime_session_add_protocol(session, "sip");

    ArkimeParserBuf_t *sip = arkime_parser_buf_create();
    arkime_parsers_register(session, sip_tcp_parser, sip, arkime_parser_buf_session_free);
}

/******************************************************************************/
void arkime_parser_init()
{
    methodField = arkime_field_define("sip", "termfield",
                                      "sip.method", "Method", "sip.method",
                                      "SIP method (INVITE, BYE, REGISTER, etc.)",
                                      ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                      (char *)NULL);

    statusCodeField = arkime_field_define("sip", "integer",
                                          "sip.statuscode", "Status Code", "sip.statuscode",
                                          "SIP response status codes",
                                          ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                          (char *)NULL);

    callIdField = arkime_field_define("sip", "termfield",
                                      "sip.callid", "Call ID", "sip.callid",
                                      "SIP Call-ID header",
                                      ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                      (char *)NULL);

    fromField = arkime_field_define("sip", "termfield",
                                    "sip.from", "From", "sip.from",
                                    "SIP From header",
                                    ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                    (char *)NULL);

    toField = arkime_field_define("sip", "termfield",
                                  "sip.to", "To", "sip.to",
                                  "SIP To header",
                                  ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                  (char *)NULL);

    userAgentField = arkime_field_define("sip", "termfield",
                                         "sip.user-agent", "User-Agent", "sip.useragent",
                                         "SIP User-Agent header",
                                         ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                         (char *)NULL);

    viaField = arkime_field_define("sip", "termfield",
                                   "sip.via", "Via", "sip.via",
                                   "SIP Via headers",
                                   ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                   (char *)NULL);

    contactField = arkime_field_define("sip", "termfield",
                                       "sip.contact", "Contact", "sip.contact",
                                       "SIP Contact header",
                                       ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    userField = arkime_field_define("sip", "termfield",
                                    "sip.user", "User", "sip.user",
                                    "SIP user from URI or auth",
                                    ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                    "category", "user",
                                    (char *)NULL);

    // UDP classifiers
    arkime_parsers_classifier_register_udp("sip", NULL, 0, (uint8_t *)"SIP/2.0", 7, sip_udp_classify);
    arkime_parsers_classifier_register_udp("sip", NULL, 0, (uint8_t *)"INVITE sip:", 11, sip_udp_classify);
    arkime_parsers_classifier_register_udp("sip", NULL, 0, (uint8_t *)"REGISTER sip:", 13, sip_udp_classify);
    arkime_parsers_classifier_register_udp("sip", NULL, 0, (uint8_t *)"OPTIONS sip:", 12, sip_udp_classify);
    arkime_parsers_classifier_register_udp("sip", NULL, 0, (uint8_t *)"NOTIFY sip:", 11, sip_udp_classify);

    // TCP classifiers
    arkime_parsers_classifier_register_tcp("sip", NULL, 0, (uint8_t *)"SIP/2.0", 7, sip_tcp_classify);
    arkime_parsers_classifier_register_tcp("sip", NULL, 0, (uint8_t *)"INVITE sip:", 11, sip_tcp_classify);
    arkime_parsers_classifier_register_tcp("sip", NULL, 0, (uint8_t *)"REGISTER sip:", 13, sip_tcp_classify);
    arkime_parsers_classifier_register_tcp("sip", NULL, 0, (uint8_t *)"NOTIFY sip:", 11, sip_tcp_classify);
}
