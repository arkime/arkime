/* SPDX-License-Identifier: Apache-2.0
 *
 * Minimal POP3 parser - captures the USER name and decodes NTLMSSP blobs
 * sent via SASL AUTH NTLM (RFC 1734 / RFC 5034).
 */
#include "arkime.h"

extern ArkimeConfig_t   config;
extern int              userField;

typedef struct {
    GString  *line[2];
    uint8_t   serverWhich;
    uint8_t   inNtlmAuth;
    uint8_t   ntlmCount;
    uint8_t   done;
} POP3Info_t;

/******************************************************************************/
LOCAL void pop3_process_line(POP3Info_t *pop3, ArkimeSession_t *session, const char *line, int len, int which)
{
    /* Client: "AUTH NTLM [<base64>]" */
    if (which != pop3->serverWhich && len >= 9 &&
        strncasecmp(line, "AUTH NTLM", 9) == 0) {
        arkime_session_add_tag(session, "pop3:authntlm");
        pop3->inNtlmAuth = 1;
        if (len > 10 && line[9] == ' ') {
            if (arkime_parsers_ntlm_decode_base64(session, line + 10, len - 10))
                pop3->ntlmCount++;
        }
        return;
    }

    if (pop3->inNtlmAuth) {
        const char *b = line;
        int blen = len;
        /* Server continuation: "+ TlRMTVN..." */
        if (which == pop3->serverWhich && blen > 2 && b[0] == '+' && b[1] == ' ') {
            b += 2;
            blen -= 2;
        }
        if (arkime_parsers_ntlm_decode_base64(session, b, blen)) {
            pop3->ntlmCount++;
            /* After Type 3 (third message from client) we're done. */
            if (pop3->ntlmCount >= 3)
                pop3->done = 1;
            return;
        }
    }

    /* Client: "USER <name>" */
    if (which != pop3->serverWhich && len > 5 &&
        strncasecmp(line, "USER ", 5) == 0) {
        const char *u = line + 5;
        int ulen = len - 5;
        while (ulen > 0 && isspace((unsigned char) * u)) {
            u++;
            ulen--;
        }
        while (ulen > 0 && isspace((unsigned char)u[ulen - 1])) ulen--;
        if (ulen > 0)
            arkime_field_string_add_lower(userField, session, u, ulen);
        pop3->done = 1;
    }
}
/******************************************************************************/
LOCAL int pop3_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int remaining, int which)
{
    POP3Info_t *pop3 = uw;
    GString *line = pop3->line[which];

    while (remaining > 0) {
        const uint8_t *lineEnd = memchr(data, '\n', remaining);
        int lineLen;

        if (lineEnd) {
            lineLen = lineEnd - data;
            if (lineLen > 0 && data[lineLen - 1] == '\r')
                lineLen--;
        } else {
            if (line->len + remaining > 16384) {
                arkime_session_add_tag(session, "pop3:line-too-long");
                return ARKIME_PARSER_UNREGISTER;
            }
            g_string_append_len(line, (const char *)data, remaining);
            return 0;
        }

        if (line->len > 0) {
            if (line->len + lineLen > 16384) {
                arkime_session_add_tag(session, "pop3:line-too-long");
                return ARKIME_PARSER_UNREGISTER;
            }
            g_string_append_len(line, (const char *)data, lineLen);
            pop3_process_line(pop3, session, line->str, line->len, which);
            g_string_truncate(line, 0);
        } else {
            pop3_process_line(pop3, session, (const char *)data, lineLen, which);
        }

        remaining -= (lineEnd - data) + 1;
        data = lineEnd + 1;

        if (pop3->done)
            return ARKIME_PARSER_UNREGISTER;
    }

    return 0;
}
/******************************************************************************/
LOCAL void pop3_free(ArkimeSession_t UNUSED(*session), void *uw)
{
    POP3Info_t *pop3 = uw;
    g_string_free(pop3->line[0], TRUE);
    g_string_free(pop3->line[1], TRUE);
    ARKIME_TYPE_FREE(POP3Info_t, pop3);
}
/******************************************************************************/
LOCAL void pop3_classify(ArkimeSession_t *session, const uint8_t *data, int len, int which, void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "pop3"))
        return;

    if (len < 8)
        return;

    /* Greeting "+OK ..." with "POP" somewhere in it */
    if (memcmp(data, "+OK ", 4) != 0)
        return;

    if (!arkime_memcasestr((const char *)data + 4, len - 4, "pop", 3))
        return;

    arkime_session_add_protocol(session, "pop3");

    POP3Info_t *pop3 = ARKIME_TYPE_ALLOC0(POP3Info_t);
    pop3->line[0] = g_string_sized_new(256);
    pop3->line[1] = g_string_sized_new(256);
    pop3->serverWhich = which;

    arkime_parsers_register(session, pop3_parser, pop3, pop3_free);
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_parsers_classifier_register_tcp("pop3", NULL, 0, (const uint8_t *)"+OK ", 4, pop3_classify);
}
