/******************************************************************************/
/* chad.c  -- CHAD implementation as plugin
 *
 * Copyright Yahoo Inc.
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
#include <string.h>
#include <ctype.h>
#include "moloch.h"

extern MolochConfig_t        config;

static char     CHAD_ORDER_ARR[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVSXYZ1234567890=";


static char     CHAD_HTTP_CONFIG[] = "host;accept;accept-encoding;accept-language;accept-charset;te;connection;referer;user-agent;cookie;content-encoding;keep-alive;ua-cpu;pragma;content-type;content-length;if-modified-since;trailer;transfer-encoding;via;x-forwarded-for;proxy-connection;userip;upgrade;authorization;expect;if-match;if-none-match;if-range;if-unmodified-since;max-forwards;proxy-authorization;range;server;warning;cache-control";

static char     CHAD_HTTP_IGNORE[] = "X-IPINTELL;rpauserdata;rspauth;x-novinet;x-is-aol;x-lb-client-ip;x-lb-client-ssl;x-ssl-offload;dnt;X-CHAD;X-QS-CHAD;X-POST-CHAD;X-OREO-CHAD";


static char     CHAD_SMTP_CONFIG[]="received;message-id;reply-to;from;to;subject;date;mime-version;content-transfer-encoding;x-priority;x-msmail-priority;x-mailer;x-mimeole;content-type;content-disposition;user-agent;dkim-signature;domainkey-signature;cc;sender;delivered-to;errors-to;precedence;importance;X-Virus-Scanned";

static char     CHAD_SMTP_IGNORE[]="x-freebsd-cvs-branch;x-beenthere;x-mailman-version;list-unsubscribe;list-subscribe;list-id;list-archive;list-post;list-help;x-return-path-hint;x-roving-id;x-lumos-senderid;x-roving-campaignid;x-roving-streamid;x-server-id;x-antiabuse;x-aol-ip;x-originalarrivaltime";

int chad_plugin_num;
int chad_http_num;
int chad_email_num;

/******************************************************************************/
typedef struct chad_token {
    struct chad_token    *c_next, *c_prev;
    char                 *str;
    uint32_t              c_hash;
    char                  letter;
    short                 c_bucket;
    int                   c_count;
} ChadToken_t;


HASH_VAR(c_, chadTokens, ChadToken_t, 151);
HASH_VAR(c_, chadSMTPTokens, ChadToken_t, 151);


/******************************************************************************/
ChadToken_t *chad_token_add(char *item, char letter, gboolean http)
{
    ChadToken_t *token = MOLOCH_TYPE_ALLOC(ChadToken_t);

    while (isspace(*item))
        item++;
    g_strchomp(item);

    token->str = g_ascii_strdown(item, -1);
    token->letter = letter;
    if (http)
        HASH_ADD(c_, chadTokens, token->str, token);
    else
        HASH_ADD(c_, chadSMTPTokens, token->str, token);
    return token;
}

/******************************************************************************/
void chad_on_header_field(MolochSession_t *session, http_parser *UNUSED(hp), const char *field, size_t UNUSED(field_len))
{
    ChadToken_t *token;
    char letter;

    HASH_FIND(c_, chadTokens, field, token);
    if (token) {
        letter = token->letter;
    } else {
        letter = '_';
    }

    if (letter == 0)
        return;

    if (!session->pluginData[chad_plugin_num]) {
        session->pluginData[chad_plugin_num] = g_string_new_len(&letter, 1);
    } else {
        g_string_append_len(session->pluginData[chad_plugin_num], &letter, 1);
    }

}

/******************************************************************************/
void chad_on_header_complete (MolochSession_t *session, http_parser *hp)
{
    if (session->pluginData[chad_plugin_num]) {
        if (hp->status_code == 0) {
            moloch_field_string_add(chad_http_num, session, ((GString *)session->pluginData[chad_plugin_num])->str, ((GString *)session->pluginData[chad_plugin_num])->len, TRUE);
        }
        g_string_truncate(session->pluginData[chad_plugin_num], 0);
    }
}
/******************************************************************************/
void chad_smtp_on_header(MolochSession_t *session, const char *field, size_t UNUSED(field_len), const char *UNUSED(value), size_t UNUSED(value_len))
{
    ChadToken_t *token;
    char letter;

    HASH_FIND(c_, chadSMTPTokens, field, token);
    if (token) {
        letter = token->letter;
    } else {
        letter = '_';
    }

    if (letter == 0)
        return;

    if (!session->pluginData[chad_plugin_num]) {
        session->pluginData[chad_plugin_num] = g_string_new_len(&letter, 1);
    } else {
        g_string_append_len(session->pluginData[chad_plugin_num], &letter, 1);
    }

}
/******************************************************************************/
void chad_smtp_on_header_complete (MolochSession_t *session)
{
    if (session->pluginData[chad_plugin_num]) {
        moloch_field_string_add(chad_email_num, session, ((GString *)session->pluginData[chad_plugin_num])->str, ((GString *)session->pluginData[chad_plugin_num])->len, TRUE);
        g_string_truncate(session->pluginData[chad_plugin_num], 0);
    }
}

/******************************************************************************/
void chad_plugin_save(MolochSession_t *session, int UNUSED(final))
{
    if (session->pluginData[chad_plugin_num]) {
        g_string_free(session->pluginData[chad_plugin_num], TRUE);
        session->pluginData[chad_plugin_num] = 0;
    }
}
/******************************************************************************/
void chad_plugin_init(char **chads, char **ignores, gboolean http)
{
    int i;
    int p;
    
    for (i = 0, p = 0; chads[i] && p < 64; i++) {
        ChadToken_t *token;
        if(http)
            HASH_FIND(c_, chadTokens, chads[i], token);
        else
            HASH_FIND(c_, chadSMTPTokens, chads[i], token);
        if (token)
            continue;
        chad_token_add(chads[i], CHAD_ORDER_ARR[p], http);
        p++;
    }

    g_strfreev(chads);

    for (i = 0, p = 0; ignores[i] && p < 64; i++) {
        ChadToken_t *token;
        if(http)
            HASH_FIND(c_, chadTokens, ignores[i], token);
        else
            HASH_FIND(c_, chadSMTPTokens, ignores[i], token);
        if (token)
            continue;
        chad_token_add(ignores[i], 0, http);
        p++;
    }

    g_strfreev(ignores);
}
/******************************************************************************/
void moloch_plugin_init()
{
    chad_plugin_num = moloch_plugins_register("chad", TRUE);

    chad_http_num = moloch_field_by_exp("http.chad");
    if (chad_http_num == -1) {
        CONFIGEXIT("Add 'chad=type:string;count:true;' to the '[headers-http-request]' section of the config.ini file");
    }

    chad_email_num = moloch_field_by_exp("email.chad");
    if (chad_email_num == -1) {
        CONFIGEXIT("Add 'chad=type:string;count:true;' to the '[headers-email]' section of the config.ini file");
    }

    LOG("chad plugin num = %d", chad_plugin_num);

    HASH_INIT(c_, chadTokens, moloch_string_hash, moloch_string_cmp);
    chad_plugin_init(moloch_config_str_list(NULL, "chadHTTPItems", CHAD_HTTP_CONFIG),
                     moloch_config_str_list(NULL, "chadHTTPIgnores", CHAD_HTTP_IGNORE),
                     1);


    HASH_INIT(c_, chadSMTPTokens, moloch_string_hash, moloch_string_cmp);
    chad_plugin_init(moloch_config_str_list(NULL, "chadSMTPItems", CHAD_SMTP_CONFIG),
                     moloch_config_str_list(NULL, "chadSMTPIgnores", CHAD_SMTP_IGNORE),
                     0);

    moloch_plugins_set_cb("chad",
      NULL,
      NULL,
      NULL,
      NULL,
      chad_plugin_save,
      NULL,
      NULL,
      NULL
    );

    moloch_plugins_set_http_cb("chad",
      NULL,
      NULL,
      chad_on_header_field,
      NULL,
      chad_on_header_complete,
      NULL,
      NULL
    );

    moloch_plugins_set_smtp_cb("chad",
      chad_smtp_on_header,
      chad_smtp_on_header_complete
    );
}
