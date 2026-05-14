/* Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Basic FTP parser - extracts user, password, commands, filenames, response codes, banner
 */
#include "arkime.h"
#include <ctype.h>

extern ArkimeConfig_t   config;

LOCAL  int userField;
LOCAL  int commandField;
LOCAL  int filenameField;
LOCAL  int responseCodeField;
LOCAL  int bannerField;

#define FTP_LINE_MAX 4096

typedef struct {
    GString  *line[2];
    uint8_t   serverWhich;
    uint8_t   sawBanner;
} FTPInfo_t;

/******************************************************************************/
LOCAL int ftp_process_client_line(ArkimeSession_t *session, const char *line, int len)
{
    while (len > 0 && (*line == ' ' || *line == '\t')) {
        line++;
        len--;
    }
    if (len < 3)
        return 0;

    const char *space = memchr(line, ' ', len);
    int cmdLen = space ? (int)(space - line) : len;
    if (cmdLen < 3 || cmdLen > 8)
        return 0;

    char cmd[16];
    int i;
    for (i = 0; i < cmdLen; i++) {
        if (!isalpha((uint8_t)line[i]))
            return 0;
        cmd[i] = toupper((uint8_t)line[i]);
    }
    cmd[cmdLen] = 0;

    if (strcmp(cmd, "HELO") == 0 || strcmp(cmd, "EHLO") == 0 || strcmp(cmd, "LHLO") == 0)
        return 1;

    const char *arg = space ? space + 1 : NULL;
    int argLen = space ? (len - cmdLen - 1) : 0;
    while (argLen > 0 && (*arg == ' ' || *arg == '\t')) {
        arg++;
        argLen--;
    }
    while (argLen > 0 && (arg[argLen - 1] == '\r' || arg[argLen - 1] == ' ' || arg[argLen - 1] == '\t')) {
        argLen--;
    }

    arkime_field_string_add(commandField, session, cmd, cmdLen, TRUE);

    if (argLen <= 0)
        return 0;

    if (strcmp(cmd, "USER") == 0) {
        arkime_field_string_add_lower(userField, session, arg, argLen);
    } else if (strcmp(cmd, "PASS") == 0) {
        arkime_session_add_tag(session, "ftp:password");
    } else if (strcmp(cmd, "RETR") == 0 || strcmp(cmd, "STOR") == 0 ||
               strcmp(cmd, "STOU") == 0 || strcmp(cmd, "APPE") == 0 ||
               strcmp(cmd, "DELE") == 0 || strcmp(cmd, "RNFR") == 0 ||
               strcmp(cmd, "RNTO") == 0 || strcmp(cmd, "MKD")  == 0 ||
               strcmp(cmd, "RMD")  == 0 || strcmp(cmd, "SIZE") == 0 ||
               strcmp(cmd, "MDTM") == 0) {
        arkime_field_string_add(filenameField, session, arg, argLen, TRUE);
    }
    return 0;
}
/******************************************************************************/
LOCAL void ftp_process_server_line(FTPInfo_t *ftp, ArkimeSession_t *session, const char *line, int len)
{
    if (len < 4)
        return;
    if (!isdigit((uint8_t)line[0]) || !isdigit((uint8_t)line[1]) || !isdigit((uint8_t)line[2]))
        return;

    int code = (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');
    arkime_field_int_add(responseCodeField, session, code);

    if (!ftp->sawBanner && code == 220) {
        const char *msg = line + 4;
        int msgLen = len - 4;
        while (msgLen > 0 && (*msg == '-' || *msg == ' ' || *msg == '\t')) {
            msg++;
            msgLen--;
        }
        while (msgLen > 0 && (msg[msgLen - 1] == '\r' || msg[msgLen - 1] == ' ' || msg[msgLen - 1] == '\t' || msg[msgLen - 1] == '-')) {
            msgLen--;
        }
        if (msgLen > 0) {
            arkime_field_string_add(bannerField, session, msg, msgLen, TRUE);
            ftp->sawBanner = 1;
        }
    }
}
/******************************************************************************/
LOCAL int ftp_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int remaining, int which)
{
    FTPInfo_t *ftp = uw;
    GString *buf = ftp->line[which];
    int isServer = (which == ftp->serverWhich);

    while (remaining > 0) {
        const uint8_t *lineEnd = memchr(data, '\n', remaining);
        int chunkLen;

        if (!lineEnd) {
            if (buf->len + remaining > FTP_LINE_MAX) {
                g_string_truncate(buf, 0);
                return ARKIME_PARSER_UNREGISTER;
            }
            g_string_append_len(buf, (const char *)data, remaining);
            return 0;
        }

        chunkLen = lineEnd - data;
        if (buf->len + chunkLen > FTP_LINE_MAX) {
            g_string_truncate(buf, 0);
            return ARKIME_PARSER_UNREGISTER;
        }
        g_string_append_len(buf, (const char *)data, chunkLen);

        int lineLen = buf->len;
        if (lineLen > 0 && buf->str[lineLen - 1] == '\r')
            lineLen--;

        if (lineLen > 0) {
            if (isServer) {
                ftp_process_server_line(ftp, session, buf->str, lineLen);
            } else if (ftp_process_client_line(session, buf->str, lineLen)) {
                arkime_field_free_one(session, bannerField);
                arkime_field_free_one(session, commandField);
                arkime_field_free_one(session, filenameField);
                arkime_field_free_one(session, responseCodeField);
                arkime_session_rm_protocol(session, "ftp");
                g_string_truncate(buf, 0);
                return ARKIME_PARSER_UNREGISTER;
            }
        }

        g_string_truncate(buf, 0);

        remaining -= chunkLen + 1;
        data = lineEnd + 1;
    }

    return 0;
}
/******************************************************************************/
LOCAL void ftp_free(ArkimeSession_t UNUSED(*session), void *uw)
{
    FTPInfo_t *ftp = uw;
    g_string_free(ftp->line[0], TRUE);
    g_string_free(ftp->line[1], TRUE);
    ARKIME_TYPE_FREE(FTPInfo_t, ftp);
}
/******************************************************************************/
LOCAL void ftp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int which, void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "ftp"))
        return;

    if (g_strstr_len((const char *)data, len, "LMTP") != NULL)
        return;
    if (g_strstr_len((const char *)data, len, "SMTP") != NULL)
        return;
    if (g_strstr_len((const char *)data, len, " TLS") != NULL)
        return;

    arkime_session_add_protocol(session, "ftp");

    FTPInfo_t *ftp = ARKIME_TYPE_ALLOC0(FTPInfo_t);
    ftp->line[0] = g_string_sized_new(128);
    ftp->line[1] = g_string_sized_new(128);
    ftp->serverWhich = which;

    arkime_parsers_register(session, ftp_parser, ftp, ftp_free);
}
/******************************************************************************/
void arkime_parser_init()
{
    userField = arkime_field_by_db("user");

    commandField = arkime_field_define("ftp", "uptermfield",
                                       "ftp.command", "Command", "ftp.command",
                                       "FTP command",
                                       ARKIME_FIELD_TYPE_STR_HASH, ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    filenameField = arkime_field_define("ftp", "termfield",
                                        "ftp.filename", "Filename", "ftp.filename",
                                        "FTP filename argument (RETR/STOR/DELE/RNFR/RNTO/MKD/RMD/SIZE/MDTM)",
                                        ARKIME_FIELD_TYPE_STR_HASH, ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);

    responseCodeField = arkime_field_define("ftp", "integer",
                                            "ftp.responseCode", "Response Code", "ftp.responseCode",
                                            "FTP response code",
                                            ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                            (char *)NULL);

    bannerField = arkime_field_define("ftp", "termfield",
                                      "ftp.banner", "Banner", "ftp.banner",
                                      "FTP server welcome banner",
                                      ARKIME_FIELD_TYPE_STR_HASH, ARKIME_FIELD_FLAG_CNT,
                                      (char *)NULL);

    arkime_parsers_classifier_register_tcp("ftp", NULL, 0, (const uint8_t *)"220 ", 4, ftp_classify);
    arkime_parsers_classifier_register_tcp("ftp", NULL, 0, (const uint8_t *)"220-", 4, ftp_classify);
}
