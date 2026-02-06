/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Basic IMAP parser - parses email addresses from FETCH responses
 */
#include "arkime.h"
#include <ctype.h>

extern ArkimeConfig_t   config;

LOCAL  int srcField;
LOCAL  int dstField;
LOCAL  int subjectField;
LOCAL  int folderField;

typedef struct {
    GString  *line;
    uint8_t   serverWhich;
    uint8_t   inFetch;
    uint8_t   inHeaders;
} IMAPInfo_t;

/******************************************************************************/
LOCAL void imap_parse_email_address(int field, ArkimeSession_t *session, const char *data, int len)
{
    const char *end = data + len;

    while (data < end) {
        while (data < end && isspace(*data)) data++;
        const char *start = data;

        /* Handle quoted strings */
        if (data < end && *data == '"') {
            data++;
            while (data < end && *data != '"') data++;
            data++;
            while (data < end && isspace(*data)) data++;
            start = data;
        }

        /* Find angle bracket or comma */
        while (data < end && *data != '<' && *data != ',' && *data != '\r' && *data != '\n') data++;

        if (data < end && *data == '<') {
            data++;
            start = data;
            while (data < end && *data != '>') data++;
        }

        if (data > start) {
            arkime_field_string_add_lower(field, session, start, data - start);
        }

        while (data < end && *data != ',' && *data != '\r' && *data != '\n') data++;
        if (data < end) data++;
    }
}
/******************************************************************************/
LOCAL void imap_parse_header_line(IMAPInfo_t UNUSED(*imap), ArkimeSession_t *session, const char *line, int len)
{
    if (len < 3) return;

    /* From: header */
    if (len > 5 && strncasecmp(line, "From:", 5) == 0) {
        imap_parse_email_address(srcField, session, line + 5, len - 5);
    }
    /* To: header */
    else if (len > 3 && strncasecmp(line, "To:", 3) == 0) {
        imap_parse_email_address(dstField, session, line + 3, len - 3);
    }
    /* Cc: header */
    else if (len > 3 && strncasecmp(line, "Cc:", 3) == 0) {
        imap_parse_email_address(dstField, session, line + 3, len - 3);
    }
    /* Subject: header */
    else if (len > 8 && strncasecmp(line, "Subject:", 8) == 0) {
        const char *s = line + 8;
        const char *end = line + len;
        while (s < end && isspace(*s)) s++;
        arkime_field_string_add(subjectField, session, s, len - (s - line), TRUE);
    }
}
/******************************************************************************/
LOCAL int imap_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int remaining, int which)
{
    IMAPInfo_t *imap = uw;

    while (remaining > 0) {
        /* Find end of line */
        const uint8_t *lineEnd = memchr(data, '\n', remaining);
        int lineLen;

        if (lineEnd) {
            lineLen = lineEnd - data;
            /* Strip CR if present */
            if (lineLen > 0 && data[lineLen - 1] == '\r') {
                lineLen--;
            }
        } else {
            /* Incomplete line, buffer it */
            g_string_append_len(imap->line, (const char *)data, remaining);
            return 0;
        }

        /* Combine with any buffered data */
        if (imap->line->len > 0) {
            g_string_append_len(imap->line, (const char *)data, lineLen);
            const char *line = imap->line->str;
            int len = imap->line->len;

            /* Process the complete line - same logic as unbuffered */
            if (which == imap->serverWhich) {
                /* Check for FETCH response */
                if (len > 6 && line[0] == '*' && line[1] == ' ') {
                    if (arkime_memcasestr(line, len, "fetch", 5)) {
                        imap->inFetch = TRUE;
                        imap->inHeaders = TRUE;
                    }
                }
                /* End of FETCH - closing paren */
                else if (imap->inFetch && len > 0 && line[0] == ')') {
                    imap->inFetch = FALSE;
                    imap->inHeaders = FALSE;
                }
                /* Process headers within FETCH */
                else if (imap->inFetch && imap->inHeaders) {
                    if (len == 0) {
                        imap->inHeaders = FALSE;
                    } else {
                        imap_parse_header_line(imap, session, line, len);
                    }
                }
            }

            g_string_truncate(imap->line, 0);
        } else {
            const char *line = (const char *)data;
            int len = lineLen;

            /* Server responses */
            if (which == imap->serverWhich) {
                /* Check for FETCH response */
                if (len > 6 && line[0] == '*' && line[1] == ' ') {
                    if (arkime_memcasestr(line, len, "fetch", 5)) {
                        imap->inFetch = TRUE;
                        imap->inHeaders = TRUE;
                    }
                }
                /* End of FETCH - closing paren or new tagged response */
                else if (imap->inFetch && len > 0 && line[0] == ')') {
                    imap->inFetch = FALSE;
                    imap->inHeaders = FALSE;
                }
                /* Process headers within FETCH */
                else if (imap->inFetch && imap->inHeaders) {
                    if (len == 0) {
                        imap->inHeaders = FALSE;
                    } else {
                        imap_parse_header_line(imap, session, line, len);
                    }
                }
            }
            /* Client commands */
            else {
                /* Check for SELECT/EXAMINE to capture folder name */
                if (len > 7) {
                    const char *cmd = line;
                    /* Skip tag */
                    while (cmd < line + len && !isspace(*cmd)) cmd++;
                    while (cmd < line + len && isspace(*cmd)) cmd++;

                    if (strncasecmp(cmd, "SELECT ", 7) == 0 || strncasecmp(cmd, "EXAMINE ", 8) == 0) {
                        const char *folder = cmd + (strncasecmp(cmd, "SELECT", 6) == 0 ? 7 : 8);
                        while (folder < line + len && isspace(*folder)) folder++;

                        /* Remove quotes if present */
                        const char *folderEnd = line + len;
                        if (*folder == '"') {
                            folder++;
                            folderEnd = memchr(folder, '"', folderEnd - folder);
                            if (!folderEnd) folderEnd = line + len;
                        } else {
                            while (folderEnd > folder && isspace(folderEnd[-1])) folderEnd--;
                        }

                        if (folderEnd > folder) {
                            arkime_field_string_add(folderField, session, folder, folderEnd - folder, TRUE);
                        }
                    }
                }
            }
        }

        /* Move past this line */
        remaining -= (lineEnd - data) + 1;
        data = lineEnd + 1;
    }

    return 0;
}
/******************************************************************************/
LOCAL void imap_free(ArkimeSession_t UNUSED(*session), void *uw)
{
    IMAPInfo_t *imap = uw;

    g_string_free(imap->line, TRUE);
    ARKIME_TYPE_FREE(IMAPInfo_t, imap);
}
/******************************************************************************/
LOCAL void imap_classify(ArkimeSession_t *session, const uint8_t *data, int len, int which, void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "imap"))
        return;

    if (len < 10)
        return;

    /* Check for "* OK " followed by IMAP somewhere */
    if (memcmp(data, "* OK ", 5) != 0)
        return;

    if (!arkime_memcasestr((const char *)data + 5, len - 5, "imap", 4))
        return;

    arkime_session_add_protocol(session, "imap");

    IMAPInfo_t *imap = ARKIME_TYPE_ALLOC0(IMAPInfo_t);
    imap->line = g_string_sized_new(256);
    imap->serverWhich = which;

    arkime_parsers_register(session, imap_parser, imap, imap_free);
}
/******************************************************************************/
void arkime_parser_init()
{
    srcField = arkime_field_define("email", "lotermfield",
                                   "email.src", "Sender", "email.src",
                                   "Email from address",
                                   ARKIME_FIELD_TYPE_STR_HASH, ARKIME_FIELD_FLAG_CNT,
                                   "requiredRight", "emailSearch",
                                   "category", "user",
                                   (char *)NULL);

    dstField = arkime_field_define("email", "lotermfield",
                                   "email.dst", "Receiver", "email.dst",
                                   "Email to address",
                                   ARKIME_FIELD_TYPE_STR_HASH, ARKIME_FIELD_FLAG_CNT,
                                   "requiredRight", "emailSearch",
                                   "category", "user",
                                   (char *)NULL);

    subjectField = arkime_field_define("email", "termfield",
                                       "email.subject", "Subject", "email.subject",
                                       "Email subject header",
                                       ARKIME_FIELD_TYPE_STR_HASH, ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_FORCE_UTF8,
                                       "requiredRight", "emailSearch",
                                       (char *)NULL);

    folderField = arkime_field_define("email", "termfield",
                                      "email.folder", "Folder", "email.folder",
                                      "Email folder/mailbox name",
                                      ARKIME_FIELD_TYPE_STR_HASH, ARKIME_FIELD_FLAG_CNT,
                                      (char *)NULL);

    arkime_parsers_classifier_register_tcp("imap", NULL, 0, (const uint8_t *)"* OK ", 5, imap_classify);
}
