/* ssdp.c -- SSDP (UPnP discovery) classifier + header extractor.
 *
 * Owns the ssdp protocol classification (moved here from misc.c) and
 * extracts a handful of HTTP-style headers that are useful for passive
 * device fingerprinting:
 *
 *   - SERVER       → ssdp.server
 *   - USER-AGENT   → ssdp.userAgent
 *   - ST           → ssdp.st   (M-SEARCH search target)
 *   - NT           → ssdp.nt   (NOTIFY notification type)
 *   - USN          → ssdp.usn  (unique service name / UUID)
 *   - LOCATION     → ssdp.location
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <ctype.h>

extern ArkimeConfig_t        config;

LOCAL int serverField    = -1;
LOCAL int userAgentField = -1;
LOCAL int stField        = -1;
LOCAL int ntField        = -1;
LOCAL int usnField       = -1;
LOCAL int locationField  = -1;

/******************************************************************************/
/* Trim leading whitespace, copy through end-of-line into out (NUL-terminated).
 * Returns the number of characters written (not counting NUL), or 0.
 */
LOCAL int ssdp_copy_header_value(const char *p, int remaining, char *out, int outLen)
{
    while (remaining > 0 && (*p == ' ' || *p == '\t')) {
        p++;
        remaining--;
    }
    int i = 0;
    while (remaining > 0 && *p != '\r' && *p != '\n' && i < outLen - 1) {
        out[i++] = *p++;
        remaining--;
    }
    out[i] = '\0';
    while (i > 0 && (out[i - 1] == ' ' || out[i - 1] == '\t')) {
        out[--i] = '\0';
    }
    return i;
}

/******************************************************************************/
LOCAL void ssdp_emit(int fieldId, ArkimeSession_t *session, const char *val, int vlen)
{
    if (fieldId < 0 || vlen <= 0) return;
    arkime_field_string_add(fieldId, session, val, vlen, TRUE);
}

/******************************************************************************/
/* Walk HTTP-style header lines from data[0..len), extracting the headers we
 * care about. Tolerates LF-only and CRLF terminators.
 */
LOCAL void ssdp_extract(ArkimeSession_t *session, const uint8_t *data, int len)
{
    const char *p = (const char *)data;
    const char *end = p + len;

    // Skip the first line (request-line or status-line).
    while (p < end && *p != '\n') p++;
    if (p < end) p++;

    while (p < end) {
        const char *lineStart = p;
        while (p < end && *p != '\n') p++;
        int lineLen = p - lineStart;
        if (lineLen > 0 && lineStart[lineLen - 1] == '\r') lineLen--;
        if (lineLen == 0) break; // end of headers

        const char *colon = memchr(lineStart, ':', lineLen);
        if (colon) {
            int nameLen = colon - lineStart;
            const char *val = colon + 1;
            int remaining = lineLen - nameLen - 1;
            char vbuf[1024];
            int vlen;

            // Case-insensitive header name match.
            if (nameLen == 6 && g_ascii_strncasecmp(lineStart, "SERVER", 6) == 0) {
                vlen = ssdp_copy_header_value(val, remaining, vbuf, sizeof(vbuf));
                ssdp_emit(serverField, session, vbuf, vlen);
            } else if (nameLen == 10 && g_ascii_strncasecmp(lineStart, "USER-AGENT", 10) == 0) {
                vlen = ssdp_copy_header_value(val, remaining, vbuf, sizeof(vbuf));
                ssdp_emit(userAgentField, session, vbuf, vlen);
            } else if (nameLen == 2 && g_ascii_strncasecmp(lineStart, "ST", 2) == 0) {
                vlen = ssdp_copy_header_value(val, remaining, vbuf, sizeof(vbuf));
                ssdp_emit(stField, session, vbuf, vlen);
            } else if (nameLen == 2 && g_ascii_strncasecmp(lineStart, "NT", 2) == 0) {
                vlen = ssdp_copy_header_value(val, remaining, vbuf, sizeof(vbuf));
                ssdp_emit(ntField, session, vbuf, vlen);
            } else if (nameLen == 3 && g_ascii_strncasecmp(lineStart, "USN", 3) == 0) {
                vlen = ssdp_copy_header_value(val, remaining, vbuf, sizeof(vbuf));
                ssdp_emit(usnField, session, vbuf, vlen);
            } else if (nameLen == 8 && g_ascii_strncasecmp(lineStart, "LOCATION", 8) == 0) {
                vlen = ssdp_copy_header_value(val, remaining, vbuf, sizeof(vbuf));
                ssdp_emit(locationField, session, vbuf, vlen);
            }
        }

        if (p < end) p++; // consume LF
    }
}

/******************************************************************************/
LOCAL int ssdp_udp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    if (len < 8) return 0;
    if (data[0] != 'M' && data[0] != 'N' && data[0] != 'H') return 0;

    int isReq = (len >= 9 && memcmp(data, "M-SEARCH ", 9) == 0) ||
                (len >= 9 && memcmp(data, "NOTIFY * ", 9) == 0);
    int isResp = (len >= 7 && memcmp(data, "HTTP/1.", 7) == 0);
    if (!isReq && !isResp) return 0;

    ssdp_extract(session, data, len);
    return 0;
}

/******************************************************************************/
LOCAL void ssdp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    // Already classified (e.g. the other direction fired first): the
    // registered per-session parser handles this packet, don't re-register.
    if (arkime_session_has_protocol(session, "ssdp"))
        return;

    // Quick gate: must look like HTTP-style start (request or status).
    if (len < 8) return;
    if (data[0] != 'M' && data[0] != 'N' && data[0] != 'H') return;

    int isReq = (len >= 9 && memcmp(data, "M-SEARCH ", 9) == 0) ||
                (len >= 9 && memcmp(data, "NOTIFY * ", 9) == 0);
    int isResp = (len >= 7 && memcmp(data, "HTTP/1.", 7) == 0);

    if (!isReq && !isResp) return;

    // For HTTP responses, only accept if it looks UPnP-ish.
    if (isResp) {
        if (!arkime_memstr((const char *)data, len, "\r\nST:", 5) &&
            !arkime_memstr((const char *)data, len, "\r\nUSN:", 6) &&
            !arkime_memstr((const char *)data, len, "UPnP", 4))
            return;
    }

    arkime_session_add_protocol(session, "ssdp");
    ssdp_extract(session, data, len);

    // Register a parser so subsequent packets in this session (e.g. NOTIFY after M-SEARCH,
    // or multiple NOTIFYs) also get header extraction.
    arkime_parsers_register(session, ssdp_udp_parser, NULL, 0);
}

/******************************************************************************/
void arkime_parser_init()
{
    serverField = arkime_field_define("ssdp", "termfield",
                                      "ssdp.server", "Server", "ssdp.server",
                                      "SSDP/UPnP Server header",
                                      ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                      (char *)NULL);

    userAgentField = arkime_field_define("ssdp", "termfield",
                                         "ssdp.userAgent", "User Agent", "ssdp.userAgent",
                                         "SSDP/UPnP User-Agent header",
                                         ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                         (char *)NULL);

    stField = arkime_field_define("ssdp", "termfield",
                                  "ssdp.st", "Search Target", "ssdp.st",
                                  "SSDP M-SEARCH Search Target",
                                  ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                  (char *)NULL);

    ntField = arkime_field_define("ssdp", "termfield",
                                  "ssdp.nt", "Notification Type", "ssdp.nt",
                                  "SSDP NOTIFY Notification Type",
                                  ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                  (char *)NULL);

    usnField = arkime_field_define("ssdp", "termfield",
                                   "ssdp.usn", "USN", "ssdp.usn",
                                   "SSDP Unique Service Name",
                                   ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                   (char *)NULL);

    locationField = arkime_field_define("ssdp", "termfield",
                                        "ssdp.location", "Location", "ssdp.location",
                                        "SSDP/UPnP Location URL",
                                        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);

    // Content-based triggers (formerly in misc.c) so any-port SSDP is both
    // classified and extracted; ssdp_classify re-verifies and guards HTTP
    // responses with the UPnP-ish header check.
    arkime_parsers_classifier_register_udp("ssdp", NULL, 0, (const uint8_t *)"M-SEARCH ", 9, ssdp_classify);
    arkime_parsers_classifier_register_udp("ssdp", NULL, 0, (const uint8_t *)"NOTIFY * ", 9, ssdp_classify);
    arkime_parsers_classifier_register_udp("ssdp", NULL, 0, (const uint8_t *)"HTTP/1.", 7, ssdp_classify);
}
