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

//#define EMAILDEBUG

extern MolochConfig_t   config;
extern char            *moloch_char_to_hex;
extern unsigned char    moloch_char_to_hexstr[256][3];
extern unsigned char    moloch_hex_to_char[256][256];
extern uint32_t         pluginsCbs;

LOCAL  MolochStringHashStd_t emailHeaders;

LOCAL  int receivedField;
LOCAL  int idField;
LOCAL  int ipField;
LOCAL  int hostField;
LOCAL  int srcField;
LOCAL  int dstField;
extern int userField;
LOCAL  int hhField;
LOCAL  int subField;
LOCAL  int ctField;
LOCAL  int md5Field;
LOCAL  int sha256Field;
LOCAL  int fnField;
LOCAL  int uaField;
LOCAL  int mvField;
LOCAL  int fctField;
LOCAL  int magicField;
LOCAL  int headerField;
LOCAL  int headerValue;
LOCAL  int helloField;

typedef struct {
    MolochStringHead_t boundaries;
    char               state[2];
    char               needStatus[2];
    GString           *line[2];
    gint               state64[2];
    guint              save64[2];
    guint              bdatRemaining[2];
    GChecksum         *checksum[4];

    uint16_t           base64Decode:2;
    uint16_t           firstInContent:2;
    uint16_t           seenHeaders:2;
    uint16_t           inBDAT:2;
} SMTPInfo_t;

/******************************************************************************/
enum {
EMAIL_CMD,
EMAIL_CMD_RETURN,

EMAIL_AUTHLOGIN,
EMAIL_AUTHLOGIN_RETURN,

EMAIL_AUTHPLAIN,
EMAIL_AUTHPLAIN_RETURN,

EMAIL_DATA_HEADER,
EMAIL_DATA_HEADER_RETURN,
EMAIL_DATA_HEADER_DONE,
EMAIL_DATA,
EMAIL_DATA_RETURN,
EMAIL_IGNORE,
EMAIL_TLS_OK,
EMAIL_TLS_OK_RETURN,
EMAIL_TLS,
EMAIL_MIME,
EMAIL_MIME_RETURN,
EMAIL_MIME_DONE,
EMAIL_MIME_DATA,
EMAIL_MIME_DATA_RETURN
};
/******************************************************************************/
LOCAL char *smtp_remove_matching(char *str, char start, char stop)
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
LOCAL void smtp_email_add_value(MolochSession_t *session, int pos, char *s, int l)
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
        moloch_field_string_add(pos, session, s, l, TRUE);
        break;
    case MOLOCH_FIELD_TYPE_IP:
    case MOLOCH_FIELD_TYPE_IP_GHASH:
    {
        int i;
        gchar **parts = g_strsplit(s, ",", 0);

        for (i = 0; parts[i]; i++) {
            moloch_field_ip_add_str(pos, session, parts[i]);
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
LOCAL char * smtp_quoteable_decode_inplace(char *str, gsize *olen)
{
    char *start = str;
    int   ipos = 0;
    int   opos = 0;
    int   done = 0;

    while (str[ipos] && !done) {
        switch(str[ipos]) {
        case '=':
            if (str[ipos+1] && str[ipos+2] && str[ipos+1] != '\n') {
                str[opos] = (char)moloch_hex_to_char[(unsigned char)str[ipos+1]][(unsigned char)str[ipos+2]];
                ipos += 2;
            } else {
                done = 1;
                continue;
            }
            break;
        case '_':
            str[opos] = ' ';
            break;
        case '?':
            if (str[ipos+1] == '=') {
                done = 1;
                continue;
            }
            str[opos] = str[ipos];
            break;
        default:
            str[opos] = str[ipos];
        }
        opos++;
        ipos++;
    }

    *olen = opos;
    str[opos] = 0;
    return start;
}

/******************************************************************************/
LOCAL char *smtp_gformat(char *format)
{
    switch (format[0]) {
    case 'k':
    case 'K':
        if (strcasecmp(format, "ks_c_5601-1987") == 0)
            return "CP949";
        break;
    case 'g':
    case 'G':
        if (strcasecmp(format, "gb2312") == 0)
            return "CP936";
        break;
    case 'w':
    case 'W':
        if (strcasecmp(format, "windows-1251") == 0)
            return "CP1251";
        if (strcasecmp(format, "windows-1252") == 0)
            return "CP1252";
        break;
    }
    return format;
}
/******************************************************************************/
LOCAL void smtp_email_add_encoded(MolochSession_t *session, int pos, char *string, int len)
{
    /* Decode this nightmare - http://www.rfc-editor.org/rfc/rfc2047.txt */
    /* =?charset?encoding?encoded-text?= */

    char  output[0xfff];
    char *str = string;
    char *end = str + len;
    GError  *error = 0;
    gsize    bread, bwritten, olen;

    BSB bsb;
    BSB_INIT(bsb, output, sizeof(output));

    while (str < end) {
        char *startquestion = strstr(str, "=?");

        /* No encoded text, or normal text in front of encoded */
        if (!startquestion || str != startquestion) {
            int extra = 0;
            if (!startquestion)
                startquestion = end;
            else if (str + 1 == startquestion && *str == ' ') {
                // If we have " =?" don't encode space, this helps with "?= =?"
                extra = 1;
            }

            char *out = g_convert((char *)str+extra, startquestion - str-extra, "utf-8", "CP1252", &bread, &bwritten, &error);
            if (error) {
                LOG("ERROR convering %s to utf-8 %s ", "CP1252", error->message);
                moloch_field_string_add(pos, session, string, len, TRUE);
                g_error_free(error);
                return;
            }

            BSB_EXPORT_ptr_some(bsb, out, bwritten);
            g_free(out);

            str = startquestion;
            continue;
        }

        /* Start of encoded token */
        char *question = strchr(str+2, '?');
        if (!question || strlen(question) < 5) { /* ?[qQbB]?<encoded-text>?= support empty text*/
            moloch_field_string_add(pos, session, string, len, TRUE);
            return;
        }

        char *endquestion = strstr(question+3, "?=");
        if (!endquestion) {
            moloch_field_string_add(pos, session, string, len, TRUE);
            return;
        }

        /* str+2 - question         = charset */
        /* question+1               = encoding */
        /* question+3 - endquestion = encoded-text */

        if (question+3 == endquestion) {
            // The encoded text is empty
        } else if (*(question+1) == 'B' || *(question+1) == 'b') {
            *question = 0;
            *endquestion = 0; // g_base64_decode_inplace expected null terminated string

            if (question[3] && question[4]) {
                g_base64_decode_inplace(question+3, &olen);
            } else
                olen = 0;

            char *fmt = smtp_gformat(str+2);
            if (strcasecmp(fmt, "utf-8") == 0) {
                // No need to convert, will validate at the end
                BSB_EXPORT_ptr_some(bsb, question+3, olen);
            } else {
                char *out = g_convert((char *)question+3, olen, "utf-8", fmt, &bread, &bwritten, &error);
                if (error) {
                    LOG("ERROR convering %s to utf-8 %s ", str+2, error->message);
                    moloch_field_string_add(pos, session, string, len, TRUE);
                    g_error_free(error);
                    return;
                }

                BSB_EXPORT_ptr_some(bsb, out, bwritten);
                g_free(out);
            }
        } else if (*(question+1) == 'Q' || *(question+1) == 'q') {
            *question = 0;

            smtp_quoteable_decode_inplace(question+3, &olen);

            char *fmt = smtp_gformat(str+2);
            if (strcasecmp(fmt, "utf-8") == 0) {
                // No need to convert, will validate at the end
                BSB_EXPORT_ptr_some(bsb, question+3, olen);
            } else {
                char *out = g_convert((char *)question+3, strlen(question+3), "utf-8", fmt, &bread, &bwritten, &error);
                if (error) {
                    LOG("ERROR convering %s to utf-8 %s ", str+2, error->message);
                    moloch_field_string_add(pos, session, string, len, TRUE);
                    g_error_free(error);
                    return;
                }

                BSB_EXPORT_ptr_some(bsb, out, bwritten);
                g_free(out);
            }
        } else {
            moloch_field_string_add(pos, session, string, len, TRUE);
            return;
        }
        str = endquestion + 2;
    }

    if (BSB_IS_ERROR(bsb)) {
        // Error is from being too long, just output what we have
        moloch_field_string_add(pos, session, output, sizeof(output), TRUE);
        return;
    }

    gboolean good = g_utf8_validate(output, BSB_LENGTH(bsb), (const char **)&end);

    if (!good) {
        moloch_field_string_add(pos, session, "Error Decoding", 14, TRUE);
        return;
    }
    moloch_field_string_add(pos, session, output, BSB_LENGTH(bsb), TRUE);
}
/******************************************************************************/
LOCAL void smtp_parse_email_addresses(int field, MolochSession_t *session, char *data, int len)
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

        if (data < end && *data == '<') {
            data++;
            start = data;
            while (data < end && *data != '>') data++;
        }

        moloch_field_string_add_lower(field, session, start, data - start);

        while (data < end && *data != ',') data++;
        if (data < end && *data == ',') data++;
    }
}
/******************************************************************************/
LOCAL void smtp_parse_email_received(MolochSession_t *session, char *data, int len)
{
    char *start = data;
    char *end = data+len;

    while (data < end) {
        if (end - data > 10) {
            if (memcmp("from ", data, 5) == 0 && (data == start || data[-1] != '-')) {
                data += 5;
                while(data < end && isspace(*data)) data++;

                if (*data == '[') {
                    data++;
                    char *ipstart = data;
                    while (data < end && *data != ']') data++;
                    *data = 0;
                    data++;
                    moloch_field_ip_add_str(ipField, session, ipstart);
                    continue;
                }

                char *fromstart = data;
                while (data < end && *data != ' ' && *data != ')') {
                    if (*data == '@')
                        fromstart = data+1;
                    data++;
                }

                moloch_field_string_add_lower(hostField, session, (char *)fromstart, data-fromstart);
            } else if (memcmp("by ", data, 3) == 0) {
                data += 3;
                while(data < end && isspace(*data)) data++;
                char *fromstart = data;
                while (data < end && *data != ' ' && *data != ')') {
                    if (*data == '@')
                        fromstart = data+1;
                    data++;
                }
                moloch_field_string_add_lower(hostField, session, (char *)fromstart, data-fromstart);
            }
        }

        if (*data == '[') {
            data++;
            char *ipstart = data;
            while (data < end && *data != ']') data++;
            *data = 0;
            moloch_field_ip_add_str(ipField, session, ipstart);
        }
        data++;
    }
}
/******************************************************************************/
LOCAL int smtp_parser(MolochSession_t *session, void *uw, const unsigned char *data, int remaining, int which)
{
    SMTPInfo_t           *email        = uw;
    GString              *line         = email->line[which];
    char                 *state        = &email->state[which];
    MolochString_t       *emailHeader  = 0;

#ifdef EMAILDEBUG
    LOG("EMAILDEBUG: enter %d %d %d %.*s", which, *state, email->needStatus[(which + 1) % 2], remaining, data);
#endif

    while (remaining > 0) {
        switch (*state) {
        case EMAIL_AUTHPLAIN:
        case EMAIL_AUTHLOGIN:
        case EMAIL_CMD: {
            if (*data == '\r') {
                (*state)++;
                break;
            }
            g_string_append_c(line, *data);
            break;
        }
        case EMAIL_CMD_RETURN: {
#ifdef EMAILDEBUG
            printf("%d %d cmd => %s\n", which, *state, line->str);
#endif
            if (email->needStatus[(which + 1) % 2]) {
                email->needStatus[(which + 1) % 2] = 0;
                char tag[200];
                snprintf(tag, sizeof(tag), "smtp:statuscode:%d", atoi(line->str));
                moloch_session_add_tag(session, tag);
            } else if (strncasecmp(line->str, "MAIL FROM:", 10) == 0) {
                *state = EMAIL_CMD;
                moloch_field_string_add_lower(srcField, session, smtp_remove_matching(line->str+10, '<', '>'), -1);
            } else if (strncasecmp(line->str, "RCPT TO:", 8) == 0) {
                *state = EMAIL_CMD;
                moloch_field_string_add_lower(dstField, session, smtp_remove_matching(line->str+8, '<', '>'), -1);
            } else if (strncasecmp(line->str, "DATA", 4) == 0) {
                *state = EMAIL_DATA_HEADER;
                email->seenHeaders |= (1 << which);
            } else if (strncasecmp(line->str, "BDAT", 4) == 0) {
                email->inBDAT |= (1 << which);
                email->bdatRemaining[which] = atoi(line->str+5)+1;

                if (email->seenHeaders & (1 << which))
                    *state = EMAIL_DATA;
                else {
                    email->seenHeaders |= (1 << which);
                    *state = EMAIL_DATA_HEADER;
                }
            } else if (strncasecmp(line->str, "AUTH LOGIN", 10) == 0) {
                moloch_session_add_tag(session, "smtp:authlogin");
                if (line->len > 11) {
                    gsize out_len = 0;
                    g_base64_decode_inplace(line->str+11, &out_len);
                    if (out_len > 0) {
                        moloch_field_string_add_lower(userField, session, line->str+11, out_len);
                    }
                    *state = EMAIL_CMD;
                } else {
                    *state = EMAIL_AUTHLOGIN;
                }
            } else if (strncasecmp(line->str, "AUTH PLAIN", 10) == 0) {
                moloch_session_add_tag(session, "smtp:authplain");
                if (line->len > 11) {
                    gsize out_len = 0;
                    gsize zation = 0;
                    g_base64_decode_inplace(line->str+11, &out_len);
                    zation = strlen(line->str+11);
                    if (zation < out_len) {
                        gsize cation = strlen(line->str+11+zation+1);
                        if (cation+zation+1 < out_len) {
                            moloch_field_string_add_lower(userField, session, line->str+11+zation+1, cation);
                        }
                    }
                    *state = EMAIL_CMD;
                } else {
                    *state = EMAIL_AUTHPLAIN;
                }
            } else if (strncasecmp(line->str, "STARTTLS", 8) == 0) {
                moloch_session_add_tag(session, "smtp:starttls");
                *state = EMAIL_IGNORE;
                email->state[(which+1)%2] = EMAIL_TLS_OK;
                return 0;
            } else if (strncasecmp(line->str, "HELO ", 5) == 0 ||
                       strncasecmp(line->str, "EHLO ", 5) == 0) {
                moloch_field_string_add_lower(helloField, session, line->str+5, -1);
                *state = EMAIL_CMD;
            } else {
                *state = EMAIL_CMD;
            }

            g_string_truncate(line, 0);
            if (*data != '\n')
                continue;
            break;
        }
        case EMAIL_AUTHLOGIN_RETURN: {
            gsize out_len = 0;
            if (line->len > 1)
                g_base64_decode_inplace(line->str, &out_len);
            if (out_len > 0) {
                moloch_field_string_add_lower(userField, session, line->str, out_len);
            }
            *state = EMAIL_CMD;
            break;
        }
        case EMAIL_AUTHPLAIN_RETURN: {
            gsize out_len = 0;
            gsize zation = 0;
            if (line->len > 1)
                g_base64_decode_inplace(line->str, &out_len);
            zation = strlen(line->str);
            if (zation < out_len) {
                gsize cation = strlen(line->str+zation+1);
                if (cation+zation+1 < out_len) {
                    moloch_field_string_add_lower(userField, session, line->str+zation+1, cation);
                }
            }
            *state = EMAIL_CMD;
            break;
        }
        case EMAIL_DATA_HEADER: {
            if (*data == '\r') {
                *state = EMAIL_DATA_HEADER_RETURN;
                break;
            }
            g_string_append_c(line, (gchar)*data);
            break;
        }
        case EMAIL_DATA_HEADER_RETURN: {
#ifdef EMAILDEBUG
            printf("%d %d header => %s\n", which, *state, line->str);
#endif
            if (strcmp(line->str, ".") == 0) {
                email->needStatus[which] = 1;
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
            printf("%d %d header done => %s (%c)\n", which, *state, line->str, *data);
#endif
            *state = EMAIL_DATA_HEADER;

            if (*data == ' ' || *data == '\t') {
                g_string_append_c(line, ' ');
                break;
            }

            char *colon = strchr(line->str, ':');
            if (!colon) {
                g_string_truncate(line, 0);
                break;
            }

            char *lower = g_ascii_strdown(line->str, colon - line->str);
            HASH_FIND(s_, emailHeaders, lower, emailHeader);

            moloch_field_string_add(hhField, session, lower, colon - line->str, TRUE);

            gboolean is_header_value_consumed = FALSE;

            if (emailHeader) {
                int cpos = colon - line->str + 1;

                if ((long)emailHeader->uw == subField) {
                    if (line->str[8] != ' ') {
                        moloch_session_add_tag(session, "smtp:missing-subject-space");
                        smtp_email_add_encoded(session, subField, line->str+8, line->len-8);
                    } else {
                        smtp_email_add_encoded(session, subField, line->str+9, line->len-9);
                    }
                } else if ((long)emailHeader->uw == dstField) {
                    smtp_parse_email_addresses(dstField, session, line->str+cpos, line->len-cpos);
                } else if ((long)emailHeader->uw == srcField) {
                    smtp_parse_email_addresses(srcField, session, line->str+cpos, line->len-cpos);
                } else if ((long)emailHeader->uw == idField) {
                    moloch_field_string_add(idField, session, smtp_remove_matching(line->str+cpos, '<', '>'), -1, TRUE);
                } else if ((long)emailHeader->uw == receivedField) {
                    smtp_parse_email_received(session, line->str+cpos, line->len-cpos);
                } else if ((long)emailHeader->uw == ctField) {
                    char *s = line->str + 13;
                    while(isspace(*s)) s++;

                    moloch_field_string_add(ctField, session, s, -1, TRUE);
                    char *boundary = (char *)moloch_memcasestr(s, line->len - (s - line->str), "boundary=", 9);
                    if (boundary) {
                        MolochString_t *string = MOLOCH_TYPE_ALLOC0(MolochString_t);
                        string->str = g_strdup(smtp_remove_matching(boundary+9, '"', '"'));
                        string->len = strlen(string->str);
                        DLL_PUSH_TAIL(s_, &email->boundaries, string);
                    }
                } else {
                    smtp_email_add_value(session, (long)emailHeader->uw, line->str + cpos , line->len - cpos);
                }
                is_header_value_consumed = TRUE;
            }

            if (config.smtpIpHeaders && is_header_value_consumed == FALSE) {
                int i;
                for (i = 0; config.smtpIpHeaders && config.smtpIpHeaders[i]; i++) {
                    if (strcasecmp(lower, config.smtpIpHeaders[i]) == 0) {
                        int l = strlen(config.smtpIpHeaders[i]);
                        char *ip = smtp_remove_matching(line->str+l+1, '[', ']');
                        moloch_field_ip_add_str(ipField, session, ip);
                        is_header_value_consumed = TRUE;
                    }
                }
            }

            if (config.parseSMTPHeaderAll && is_header_value_consumed == FALSE) {
                int cpos = colon - line->str + 1;
                moloch_field_string_add(headerField, session, lower, colon - line->str, TRUE);
                smtp_email_add_value(session, (long)headerValue, line->str + cpos , line->len - cpos);
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
            g_string_append_c(line, (gchar)*data);
            break;
        }
        case EMAIL_MIME_DATA_RETURN:
        case EMAIL_DATA_RETURN: {
#ifdef EMAILDEBUG
            printf("%d %d %sdata => %s\n", which, *state, (*state == EMAIL_MIME_DATA_RETURN?"mime ": ""), line->str);
#endif

            // If not in BDAT end DATA on single .
            if (!(email->inBDAT & 1 << which) && (strcmp(line->str, ".") == 0)) {
                email->needStatus[which] = 1;
                *state = EMAIL_CMD;
            } else {
                gboolean        found = FALSE;

                if (line->str[0] == '-') {
                    MolochString_t *string;
                    DLL_FOREACH(s_,&email->boundaries,string) {
                        if ((int)line->len >= (int)(string->len + 2) && memcmp(line->str+2, string->str, string->len) == 0) {
                            found = TRUE;
                            break;
                        }
                    }
                }

                if (found) {
                    if (email->base64Decode & (1 << which)) {
                        const char *md5 = g_checksum_get_string(email->checksum[which]);
                        moloch_field_string_add(md5Field, session, (char*)md5, 32, TRUE);
                        if (config.supportSha256) {
                            const char *sha256 = g_checksum_get_string(email->checksum[which+2]);
                            moloch_field_string_add(sha256Field, session, (char*)sha256, 64, TRUE);
                        }
                    }
                    email->firstInContent |= (1 << which);
                    email->base64Decode &= ~(1 << which);
                    email->state64[which] = 0;
                    email->save64[which] = 0;
                    g_checksum_reset(email->checksum[which]);
                    if (config.supportSha256) {
                        g_checksum_reset(email->checksum[which+2]);
                    }
                    *state = EMAIL_MIME;
                } else if (*state == EMAIL_MIME_DATA_RETURN) {
                    if (email->base64Decode & (1 << which)) {
                        guchar buf[20000];
                        if (sizeof(buf) > line->len) {
                            gsize  b = g_base64_decode_step (line->str, line->len, buf,
                                                            &(email->state64[which]),
                                                            &(email->save64[which]));
                            g_checksum_update(email->checksum[which], buf, b);
                            if (config.supportSha256) {
                                g_checksum_update(email->checksum[which+2], buf, b);
                            }

                            if (email->firstInContent & (1 << which)) {
                                email->firstInContent &= ~(1 << which);
                                moloch_parsers_magic(session, magicField, (char *)buf, b);
                            }
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
            return 0;
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
            printf("%d %d tls => %s\n", which, *state, line->str);
#endif
            *state = EMAIL_TLS;
            if (*data != '\n')
                continue;
            break;
        }
        case EMAIL_TLS: {
            *state = EMAIL_IGNORE;
            moloch_parsers_classify_tcp(session, data, remaining, which);
            moloch_parsers_unregister(session, email);
            return 0;
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
            printf("%d %d mime => %s\n", which, *state, line->str);
#endif
            if (*line->str == 0) {
                *state = EMAIL_MIME_DATA;
            } else if (strcmp(line->str, ".") == 0) {
                email->needStatus[which] = 1;
                *state = EMAIL_CMD;
            } else {
                *state = EMAIL_MIME_DONE;
            }

            if (*data != '\n')
                continue;
            break;
        }
        case EMAIL_MIME_DONE: {
#ifdef EMAILDEBUG
            printf("%d %d mime done => %s (%c)\n", which, *state, line->str, *data);
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
                    string->str = g_strdup(smtp_remove_matching(boundary+9, '"', '"'));
                    string->len = strlen(string->str);
                    DLL_PUSH_TAIL(s_, &email->boundaries, string);
                }
            } else if (strncasecmp(line->str, "content-disposition:", 20) == 0) {
                char *s = line->str + 13;
                while(isspace(*s)) s++;
                char *filename = (char *)moloch_memcasestr(s, line->len - (s - line->str), "filename=", 9);
                if (filename) {
                    char *matching = smtp_remove_matching(filename+9, '"', '"');
                    smtp_email_add_encoded(session, fnField, matching, strlen(matching));
                }
            } else if (strncasecmp(line->str, "content-transfer-encoding:", 26) == 0) {
                if(moloch_memcasestr(line->str+26, line->len - 26, "base64", 6)) {
                    email->base64Decode |= (1 << which);
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

        if (email->inBDAT & 1 << which) {
            email->bdatRemaining[which]--;
            if (email->bdatRemaining[which] == 0) {
#ifdef EMAILDEBUG
                printf("%d %d reseting to CMD %s\n", which, *state, line->str);
#endif
                *state = EMAIL_CMD;
                email->inBDAT &=  ~(1 << which);
            }
        }
    }

    return 0;
}
/******************************************************************************/
LOCAL void smtp_free(MolochSession_t UNUSED(*session), void *uw)
{
    SMTPInfo_t            *email          = uw;

    MolochString_t *string;

    g_string_free(email->line[0], TRUE);
    g_string_free(email->line[1], TRUE);

    g_checksum_free(email->checksum[0]);
    g_checksum_free(email->checksum[1]);
    if (config.supportSha256) {
        g_checksum_free(email->checksum[2]);
        g_checksum_free(email->checksum[3]);
    }

    while (DLL_POP_HEAD(s_, &email->boundaries, string)) {
        g_free(string->str);
        MOLOCH_TYPE_FREE(MolochString_t, string);
    }

    MOLOCH_TYPE_FREE(SMTPInfo_t, email);
}
/******************************************************************************/
LOCAL void smtp_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 5)
        return;

    if (memcmp("HELO ", data, 5) == 0 ||
        memcmp("EHLO ", data, 5) == 0 ||
        (memcmp("220 ", data, 4) == 0 &&
         g_strstr_len((char *)data, len, "SMTP") != 0)) {

        if (moloch_session_has_protocol(session, "smtp"))
            return;

        moloch_session_add_protocol(session, "smtp");

        SMTPInfo_t *email = MOLOCH_TYPE_ALLOC0(SMTPInfo_t);

        email->line[0] = g_string_sized_new(100);
        email->line[1] = g_string_sized_new(100);

        email->checksum[0] = g_checksum_new(G_CHECKSUM_MD5);
        email->checksum[1] = g_checksum_new(G_CHECKSUM_MD5);
        if (config.supportSha256) {
            email->checksum[2] = g_checksum_new(G_CHECKSUM_SHA256);
            email->checksum[3] = g_checksum_new(G_CHECKSUM_SHA256);
        }

        DLL_INIT(s_, &(email->boundaries));

        moloch_parsers_register(session, smtp_parser, email, smtp_free);
    }
}
/******************************************************************************/
void moloch_parser_init()
{
    hostField = moloch_field_define("email", "lotermfield",
        "host.email", "Hostname", "email.host",
        "Email hostnames",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        "aliases", "[\"email.host\"]",
        "requiredRight", "emailSearch",
        "category", "host",
        (char *)NULL);

    moloch_field_define("email", "lotextfield",
        "host.email.tokens", "Hostname Tokens", "email.hostTokens",
        "Email Hostname Tokens",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_FAKE,
        "aliases", "[\"email.host.tokens\"]",
        "requiredRight", "emailSearch",
        (char *)NULL);

    uaField = moloch_field_define("email", "termfield",
        "email.x-mailer", "X-Mailer Header", "email.useragent",
        "Email X-Mailer header",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        "requiredRight", "emailSearch",
        (char *)NULL);

    srcField = moloch_field_define("email", "lotermfield",
        "email.src", "Sender", "email.src",
        "Email from address",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        "requiredRight", "emailSearch",
        "category", "user",
        (char *)NULL);

    dstField = moloch_field_define("email", "lotermfield",
        "email.dst", "Receiver", "email.dst",
        "Email to address",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        "requiredRight", "emailSearch",
        "category", "user",
        (char *)NULL);

    subField = moloch_field_define("email", "termfield",
        "email.subject", "Subject", "email.subject",
        "Email subject header",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_FORCE_UTF8,
        "requiredRight", "emailSearch",
        (char *)NULL);

    idField = moloch_field_define("email", "termfield",
        "email.message-id", "Id", "email.id",
        "Email Message-Id header",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        "requiredRight", "emailSearch",
        (char *)NULL);

    ctField = moloch_field_define("email", "termfield",
        "email.content-type", "Content-Type", "email.contentType",
        "Email content-type header",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    mvField = moloch_field_define("email", "termfield",
        "email.mime-version", "Mime-Version", "email.mimeVersion",
        "Email Mime-Header header",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    fnField = moloch_field_define("email", "termfield",
        "email.fn", "Filenames", "email.filename",
        "Email attachment filenames",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_FORCE_UTF8,
        "requiredRight", "emailSearch",
        (char *)NULL);

    md5Field = moloch_field_define("email", "termfield",
        "email.md5", "Attach MD5s", "email.md5",
        "Email attachment MD5s",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        "requiredRight", "emailSearch",
        "category", "md5",
        (char *)NULL);

    if (config.supportSha256) {
        sha256Field = moloch_field_define("email", "termfield",
            "email.sha256", "Attach SHA256s", "email.sha256",
            "Email attachment SHA256s",
            MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
            "requiredRight", "emailSearch",
            "category", "sha256",
            "disabled", "true",
            (char *)NULL);
    }

    fctField = moloch_field_define("email", "termfield",
        "email.file-content-type", "Attach Content-Type", "email.fileContentType",
        "Email attachment content types",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        "requiredRight", "emailSearch",
        (char *)NULL);

    ipField = moloch_field_define("email", "ip",
        "ip.email", "IP", "email.ip",
        "Email IP address",
        MOLOCH_FIELD_TYPE_IP_GHASH,   MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_IPPRE,
        "requiredRight", "emailSearch",
        "category", "ip",
        (char *)NULL);

    hhField = moloch_field_define("email", "lotermfield",
        "email.has-header", "Header", "email.header",
        "Email has the header set",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        "requiredRight", "emailSearch",
        (char *)NULL);

    headerField = moloch_field_define("email", "termfield",
            "email.has-header.name", "Header Field", "email.headerField", "Email has the header field set",
            MOLOCH_FIELD_TYPE_STR_ARRAY, MOLOCH_FIELD_FLAG_NODB,
            (char *)NULL);

    headerValue = moloch_field_define("email", "termfield",
            "email.has-header.value", "Header Value", "email.headerValue", "Email has the header value",
            MOLOCH_FIELD_TYPE_STR_ARRAY, MOLOCH_FIELD_FLAG_CNT,
            "requiredRight", "emailSearch",
            (char *)NULL);

    magicField = moloch_field_define("email", "termfield",
        "email.bodymagic", "Body Magic", "email.bodyMagic",
        "The content type of body determined by libfile/magic",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    helloField = moloch_field_define("email", "lotermfield",
        "email.smtp-hello", "SMTP Hello", "email.smtpHello",
        "SMTP HELO/EHLO",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    HASH_INIT(s_, emailHeaders, moloch_string_hash, moloch_string_cmp);
    moloch_config_add_header(&emailHeaders, "cc", dstField);
    moloch_config_add_header(&emailHeaders, "to", dstField);
    moloch_config_add_header(&emailHeaders, "from", srcField);
    moloch_config_add_header(&emailHeaders, "message-id", idField);
    moloch_config_add_header(&emailHeaders, "content-type", ctField);
    moloch_config_add_header(&emailHeaders, "subject", subField);
    moloch_config_add_header(&emailHeaders, "x-mailer", uaField);
    moloch_config_add_header(&emailHeaders, "user-agent", uaField);
    moloch_config_add_header(&emailHeaders, "mime-version", mvField);
    moloch_config_add_header(&emailHeaders, "received", receivedField);
    moloch_config_load_header("headers-email", "email", "Email header ", "email.", "email.header-", &emailHeaders, 0);

    moloch_parsers_classifier_register_tcp("smtp", NULL, 0, (unsigned char*)"HELO", 4, smtp_classify);
    moloch_parsers_classifier_register_tcp("smtp", NULL, 0, (unsigned char*)"EHLO", 4, smtp_classify);
    moloch_parsers_classifier_register_tcp("smtp", NULL, 0, (unsigned char*)"220 ", 4, smtp_classify);
}
