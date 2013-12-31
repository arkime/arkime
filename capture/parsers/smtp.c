#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "moloch.h"

//#define EMAILDEBUG

extern MolochConfig_t   config;
extern char            *moloch_char_to_hex;
extern unsigned char    moloch_char_to_hexstr[256][3];
extern unsigned char    moloch_hex_to_char[256][256];
extern uint32_t         pluginsCbs;
extern MolochStringHashStd_t emailHeaders;

typedef struct {
    MolochStringHead_t boundaries;
    char               state[2];
    char               needStatus[2];
    GString           *line[2];
    gint               state64[2];
    guint              save64[2];
    GChecksum         *checksum[2];

    uint16_t           base64Decode:2;
    uint16_t           firstInContent:2;
} SMTPInfo_t;

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
char *smtp_remove_matching(char *str, char start, char stop) 
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
smtp_email_add_value(MolochSession_t *session, int pos, char *s, int l)
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
char *
smtp_quoteable_decode_inplace(char *str, gsize *olen)
{
    char *start = str;
    int   ipos = 0;
    int   opos = 0;
    int   done = 0;

    while (str[ipos] && !done) {
        switch(str[ipos]) {
        case '=':
            if (str[ipos+1] && str[ipos+2] && str[ipos+1] != '\n') {
                str[opos] = moloch_hex_to_char[(unsigned char)str[ipos+1]][(unsigned char)str[ipos+2]];
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
void
smtp_email_add_encoded(MolochSession_t *session, int pos, char *string, int len)
{
    /* Decode this nightmare - http://www.rfc-editor.org/rfc/rfc2047.txt */
    /* =?charset?encoding?encoded-text?= */


    char  output[0xffff];
    int   outputlen = 0;
    char *str = string;
    char *end = str + len;

    while (str < end) {
        if (str[0] != '=' || str[1] != '?') {
            output[outputlen] = *str;
            outputlen++;
            str++;
            continue;
        }

        /* Start of encoded token */
        char *question = strchr(str+2, '?');
        if (!question) {
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

        GError  *error = 0;
        gsize    bread, bwritten, olen;
        if (*(question+1) == 'B' || *(question+1) == 'b') {
            *question = 0;
            *endquestion = 0;

            g_base64_decode_inplace(question+3, &olen);

            char *out = g_convert((char *)question+3, olen, "utf8", str+2, &bread, &bwritten, &error);
            if (error) {
                LOG("ERROR convering %s to utf8 %s ", str+2, error->message);
                moloch_field_string_add(pos, session, string, len, TRUE);
                g_error_free(error);
                return;
            }

            strcpy(output+outputlen, out);
            outputlen += strlen(out);
            g_free(out);
        } else if (*(question+1) == 'Q' || *(question+1) == 'q') {
            *question = 0;

            smtp_quoteable_decode_inplace(question+3, &olen);

            char *out = g_convert((char *)question+3, strlen(question+3), "utf8", str+2, &bread, &bwritten, &error);
            if (error) {
                LOG("ERROR convering %s to utf8 %s ", str+2, error->message);
                moloch_field_string_add(pos, session, string, len, TRUE);
                g_error_free(error);
                return;
            }

            strcpy(output+outputlen, out);
            outputlen += strlen(out);
            g_free(out);
        } else {
            moloch_field_string_add(pos, session, string, len, TRUE);
            return;
        }
        str = endquestion + 3;
    }

    output[outputlen] = 0;
    moloch_field_string_add(pos, session, output, outputlen, TRUE);
}
/******************************************************************************/
void smtp_parse_email_addresses(int field, MolochSession_t *session, char *data, int len)
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
void smtp_parse_email_received(MolochSession_t *session, char *data, int len)
{
    char *end = data+len;

    while (data < end) {
        if (end - data > 10) {
            if (memcmp("from ", data, 5) == 0) {
                data += 5;
                while(data < end && isspace(*data)) data++;
                char *fromstart = data;
                while (data < end && *data != ' ' && *data != ')') {
                    if (*data == '@')
                        fromstart = data+1;
                    data++;
                }
                char *lower = g_ascii_strdown((char*)fromstart, data - fromstart);
                if (!moloch_field_string_add(MOLOCH_FIELD_EMAIL_HOST, session, lower, data - fromstart, FALSE)) {
                    g_free(lower);
                }
            } else if (memcmp("by ", data, 3) == 0) {
                data += 3;
                while(data < end && isspace(*data)) data++;
                char *fromstart = data;
                while (data < end && *data != ' ' && *data != ')') {
                    if (*data == '@')
                        fromstart = data+1;
                    data++;
                }
                char *lower = g_ascii_strdown((char*)fromstart, data - fromstart);
                if (!moloch_field_string_add(MOLOCH_FIELD_EMAIL_HOST, session, lower, data - fromstart, FALSE)) {
                    g_free(lower);
                }
            }
        }

        if (*data == '[') {
            data++;
            char *ipstart = data;
            while (data < end && *data != ']') data++;
            *data = 0;
            in_addr_t ia = inet_addr(ipstart);
            if (ia == 0 || ia == 0xffffffff)
                continue;
            moloch_field_int_add(MOLOCH_FIELD_EMAIL_IP, session, ia);
        }
        data++;
    }
}
/******************************************************************************/
int smtp_parser(MolochSession_t *session, void *uw, const unsigned char *data, int remaining)
{
#ifdef EMAILDEBUG
    LOG("EMAILDEBUG: enter %d", session->which);
#endif

    SMTPInfo_t           *email        = uw;
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
            if (email->needStatus[(session->which + 1) % 2]) {
                email->needStatus[(session->which + 1) % 2] = 0;
                char tag[200];
                snprintf(tag, sizeof(tag), "smtp:statuscode:%d", atoi(line->str));
                moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, tag);
            } else if (strncasecmp(line->str, "MAIL FROM:", 10) == 0) {
                *state = EMAIL_CMD;
                char *lower = g_ascii_strdown(smtp_remove_matching(line->str+11, '<', '>'), -1);
                if (!moloch_field_string_add(MOLOCH_FIELD_EMAIL_SRC, session, lower, -1, FALSE)) {
                    g_free(lower);
                }
            } else if (strncasecmp(line->str, "RCPT TO:", 8) == 0) {
                char *lower = g_ascii_strdown(smtp_remove_matching(line->str+9, '<', '>'), -1);
                if (!moloch_field_string_add(MOLOCH_FIELD_EMAIL_DST, session, lower, -1, FALSE)) {
                    g_free(lower);
                }
                *state = EMAIL_CMD;
            } else if (strncasecmp(line->str, "DATA", 4) == 0) {
                *state = EMAIL_DATA_HEADER;
            } else if (strncasecmp(line->str, "AUTH LOGIN", 8) == 0) {
                moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "smtp:authlogin");
                *state = EMAIL_CMD;
            } else if (strncasecmp(line->str, "STARTTLS", 8) == 0) {
                moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "smtp:starttls");
                *state = EMAIL_IGNORE;
                email->state[(session->which+1)%2] = EMAIL_TLS_OK;
                return 0;
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
                email->needStatus[session->which] = 1;
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
                g_string_append_c(line, ' ');
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
                switch (emailHeader->uw) {
                case MOLOCH_FIELD_EMAIL_SUB:
                    smtp_email_add_encoded(session, MOLOCH_FIELD_EMAIL_SUB, line->str+9, line->len-9);
                    break;
                case MOLOCH_FIELD_EMAIL_DST:
                    smtp_parse_email_addresses(MOLOCH_FIELD_EMAIL_DST, session, line->str+cpos, line->len-cpos);
                    break;
                case MOLOCH_FIELD_EMAIL_SRC:
                    smtp_parse_email_addresses(MOLOCH_FIELD_EMAIL_SRC, session, line->str+cpos, line->len-cpos);
                    break;
                case MOLOCH_FIELD_EMAIL_ID:
                    moloch_field_string_add(MOLOCH_FIELD_EMAIL_ID, session, smtp_remove_matching(line->str+cpos, '<', '>'), -1, TRUE);
                    break;
                case MOLOCH_FIELD_EMAIL_RECEIVED:
                    smtp_parse_email_received(session, line->str+cpos, line->len-cpos);
                    break;
                case MOLOCH_FIELD_EMAIL_CT: {
                    char *s = line->str + 13;
                    while(isspace(*s)) s++;

                    moloch_field_string_add(MOLOCH_FIELD_EMAIL_CT, session, s, -1, TRUE);
                    char *boundary = (char *)moloch_memcasestr(s, line->len - (s - line->str), "boundary=", 9);
                    if (boundary) {
                        MolochString_t *string = MOLOCH_TYPE_ALLOC0(MolochString_t);
                        string->str = g_strdup(smtp_remove_matching(boundary+9, '"', '"'));
                        string->len = strlen(string->str);
                        DLL_PUSH_TAIL(s_, &email->boundaries, string);
                    }
                    break;
                }
                default:
                    smtp_email_add_value(session, emailHeader->uw, line->str + cpos , line->len - cpos);
                } /* switch */
            } else {
                int i;
                for (i = 0; config.smtpIpHeaders && config.smtpIpHeaders[i]; i++) {
                    if (strcasecmp(lower, config.smtpIpHeaders[i]) == 0) {
                        int l = strlen(config.smtpIpHeaders[i]);
                        char *ip = smtp_remove_matching(line->str+l, '[', ']');
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
                email->needStatus[session->which] = 1;
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
                    email->firstInContent |= (1 << session->which);
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

                            if (email->firstInContent & (1 << session->which)) {
                                email->firstInContent &= ~(1 << session->which);
                                moloch_parsers_magic_tag(session, "smtp:content", (char *)buf, b);
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
            printf("%d %d tls => %s\n", session->which, *state, line->str);
#endif
            *state = EMAIL_TLS;
            if (*data != '\n')
                continue;
            break;
        }
        case EMAIL_TLS: {
            *state = EMAIL_IGNORE;
            moloch_parsers_classify_tcp(session, data, remaining);
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
                    string->str = g_strdup(smtp_remove_matching(boundary+9, '"', '"'));
                    string->len = strlen(string->str);
                    DLL_PUSH_TAIL(s_, &email->boundaries, string);
                }
            } else if (strncasecmp(line->str, "content-disposition:", 20) == 0) {
                char *s = line->str + 13;
                while(isspace(*s)) s++;
                char *filename = (char *)moloch_memcasestr(s, line->len - (s - line->str), "filename=", 9);
                if (filename) {
                    moloch_field_string_add(MOLOCH_FIELD_EMAIL_FN, session, smtp_remove_matching(filename+9, '"', '"'), -1, TRUE);
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

    return 0;
}
/******************************************************************************/
void smtp_free(MolochSession_t UNUSED(*session), void *uw)
{
    SMTPInfo_t            *email          = uw;

    MolochString_t *string;

    g_string_free(email->line[0], TRUE);
    g_string_free(email->line[1], TRUE);

    g_checksum_free(email->checksum[0]);
    g_checksum_free(email->checksum[1]);

    while (DLL_POP_HEAD(s_, &email->boundaries, string)) {
        g_free(string->str);
        MOLOCH_TYPE_FREE(MolochString_t, string);
    }

    MOLOCH_TYPE_FREE(SMTPInfo_t, email);
}
/******************************************************************************/
void smtp_classify(MolochSession_t *session, const unsigned char *data, int len)
{
    if (len < 5)
        return;

    if (memcmp("HELO ", data, 5) == 0 ||
        memcmp("EHLO ", data, 5) == 0 ||
        (memcmp("220 ", data, 4) == 0 &&
         g_strstr_len((char *)data, len, "SMTP") != 0)) {

        if (moloch_nids_has_tag(session, MOLOCH_FIELD_TAGS, "protocol:smtp"))
            return;

        moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:smtp");

        SMTPInfo_t *email = MOLOCH_TYPE_ALLOC0(SMTPInfo_t);

        email->line[0] = g_string_sized_new(100);
        email->line[1] = g_string_sized_new(100);

        email->checksum[0] = g_checksum_new(G_CHECKSUM_MD5);
        email->checksum[1] = g_checksum_new(G_CHECKSUM_MD5);

        DLL_INIT(s_, &(email->boundaries));

        moloch_parsers_register(session, smtp_parser, email, smtp_free);
    }
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_HOST,    "eho",    MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_UA,      "eua",    MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_SRC,     "esrc",   MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_DST,     "edst",   MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_SUB,     "esub",   MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_FORCE_UTF8);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_ID,      "eid",    MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_CT,      "ect",    MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_MV,      "emv",    MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_FN,      "efn",    MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_MD5,     "emd5",   MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_FCT,     "efct",   MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_EMAIL_IP,      "eip",    MOLOCH_FIELD_TYPE_IP_HASH,   MOLOCH_FIELD_FLAG_CNT);

    if (config.parseSMTP) {
        moloch_parsers_classifier_register_tcp("smtp", 0, (unsigned char*)"HELO", 4, smtp_classify);
        moloch_parsers_classifier_register_tcp("smtp", 0, (unsigned char*)"EHLO", 4, smtp_classify);
        moloch_parsers_classifier_register_tcp("smtp", 0, (unsigned char*)"220 ", 4, smtp_classify);
    }
}
