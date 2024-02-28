/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

/******************************************************************************/
typedef struct socksinfo {
    char     *user;
    char     *host;
    uint32_t  ip;
    uint16_t  port;
    uint16_t  userlen;
    uint16_t  hostlen;
    uint8_t   which;
    uint8_t   state4;
    uint8_t   state5[2];
} SocksInfo_t;

LOCAL  int ipField;
LOCAL  int portField;
LOCAL  int userField;
LOCAL  int hostField;

//#define SOCKSDEBUG

/******************************************************************************/
#define SOCKS4_STATE_REPLY        0
#define SOCKS4_STATE_DATA         1
LOCAL int socks4_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int remaining, int which)
{
    SocksInfo_t            *socks          = uw;

    switch (socks->state4) {
    case SOCKS4_STATE_REPLY:
        if (which == socks->which)
            return 0;
        if (remaining >= 8 && data[0] == 0 && data[1] >= 0x5a && data[1] <= 0x5d) {
            if (socks->ip)
                arkime_field_ip4_add(ipField, session, socks->ip);
            arkime_field_int_add(portField, session, socks->port);
            arkime_session_add_protocol(session, "socks");

            if (socks->user) {
                if (!arkime_field_string_add(userField, session, socks->user, socks->userlen, FALSE)) {
                    g_free(socks->user);
                }
                socks->user = 0;
            }
            if (socks->host) {
                if (!arkime_field_string_add(hostField, session, socks->host, socks->hostlen, FALSE)) {
                    g_free(socks->host);
                }
                socks->host = 0;
            }
            arkime_parsers_classify_tcp(session, data + 8, remaining - 8, which);
            socks->state4 = SOCKS4_STATE_DATA;
            return 8;
        }
        break;
    case SOCKS4_STATE_DATA:
        /*if (which != socks->which)
            return 0;*/
        arkime_parsers_classify_tcp(session, data, remaining, which);
        arkime_parsers_unregister(session, uw);
        break;
    }

    return 0;
}
/******************************************************************************/
#define SOCKS5_STATE_VER_REQUEST    1
#define SOCKS5_STATE_VER_REPLY      2
#define SOCKS5_STATE_USER_REQUEST   3
#define SOCKS5_STATE_USER_REPLY     4
#define SOCKS5_STATE_CONN_REQUEST   5
#define SOCKS5_STATE_CONN_REPLY     6
#define SOCKS5_STATE_CONN_DATA      7
LOCAL int socks5_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int remaining, int which)
{
    SocksInfo_t            *socks          = uw;
    int                     consumed;

    //LOG("%d %d %d", which, socks->which, socks->state5[which]);
    //arkime_print_hex_string(data, remaining);

    switch (socks->state5[which]) {
    case SOCKS5_STATE_VER_REQUEST:
        if (remaining < 3) {
            arkime_parsers_unregister(session, uw);
            return 0;
        }

        if (data[2] == 0) {
            socks->state5[which] = SOCKS5_STATE_CONN_REQUEST;
        } else {
            socks->state5[which] = SOCKS5_STATE_USER_REQUEST;
        }
        socks->state5[(which + 1) % 2] = SOCKS5_STATE_VER_REPLY;
        break;
    case SOCKS5_STATE_VER_REPLY:
        if (remaining != 2 || data[0] != 5 || data[1] > 2) {
            arkime_parsers_unregister(session, uw);
            return 0;
        }

        arkime_session_add_protocol(session, "socks");

        if (socks->state5[socks->which] == SOCKS5_STATE_CONN_DATA) {
            // Other side of connection already in data state
            socks->state5[which] = SOCKS5_STATE_CONN_REPLY;
        } else if (data[1] == 0) {
            socks->state5[socks->which] = SOCKS5_STATE_CONN_REQUEST;
            socks->state5[which] = SOCKS5_STATE_CONN_REPLY;
        } else if (data[1] == 2) {
            socks->state5[socks->which] = SOCKS5_STATE_USER_REQUEST;
            socks->state5[which] = SOCKS5_STATE_USER_REPLY;
        } else {
            // We don't handle other auth methods
            arkime_parsers_unregister(session, uw);
        }


        return 2;
    case SOCKS5_STATE_USER_REQUEST:
        if (remaining < 2 || (3 + data[1] > (int)remaining) || (2 + data[1] + 1 + data[data[1] + 2]  > (int)remaining)) {
            arkime_parsers_unregister(session, uw);
            return 0;
        }

        arkime_field_string_add(userField, session, (char *)data + 2, data[1], TRUE);
        arkime_session_add_tag(session, "socks:password");
        socks->state5[which] = SOCKS5_STATE_CONN_REQUEST;
        return data[1] + 1 + data[data[1] + 2];
    case SOCKS5_STATE_USER_REPLY:
        socks->state5[which] = SOCKS5_STATE_CONN_REPLY;
        return 2;
    case SOCKS5_STATE_CONN_REQUEST:
        if (remaining < 6 || data[0] != 5 || data[1] != 1 || data[2] != 0) {
            arkime_parsers_unregister(session, uw);
            return 0;
        }

        socks->state5[which] = SOCKS5_STATE_CONN_DATA;
        if (data[3] == 1) { // IPV4
            if (remaining < 10) {
                arkime_parsers_unregister(session, uw);
                return 0;
            }
            socks->port = (data[8] & 0xff) << 8 | (data[9] & 0xff);
            memcpy(&socks->ip, data + 4, 4);
            arkime_field_ip4_add(ipField, session, socks->ip);
            arkime_field_int_add(portField, session, socks->port);
            consumed = 4 + 4 + 2;
        } else if (data[3] == 3) { // Domain Name
            if (remaining < data[4] + 7) {
                arkime_parsers_unregister(session, uw);
                return 0;
            }
            socks->port = (data[5 + data[4]] & 0xff) << 8 | (data[6 + data[4]] & 0xff);

            arkime_field_string_add_lower(hostField, session, (char *)data + 5, data[4]);
            arkime_field_int_add(portField, session, socks->port);
            consumed = 4 + 1 + data[4] + 2;
        } else if (data[3] == 4) { // IPV6
            if (remaining < 22) {
                arkime_parsers_unregister(session, uw);
                return 0;
            }
            consumed = 4 + 16 + 2;
        } else {
            break;
        }

        arkime_parsers_classify_tcp(session, data + consumed, remaining - consumed, which);
        return consumed;
    case SOCKS5_STATE_CONN_REPLY: {
        if (remaining < 6) {
            arkime_parsers_unregister(session, uw);
            return 0;
        }

        socks->state5[which] = SOCKS5_STATE_CONN_DATA;
        if (data[3] == 1) { // IPV4
            consumed = 4 + 4 + 2;
        } else if (data[3] == 3) { // Domain Name
            consumed = 4 + 1 + data[4] + 2;
        } else if (data[3] == 4) { // IPV6
            consumed = 4 + 16 + 2;
        } else {
            break;
        }

        if (remaining < consumed) {
            arkime_parsers_unregister(session, uw);
            return 0;
        }

        arkime_parsers_classify_tcp(session, data + consumed, remaining - consumed, which);
        return consumed;
    }
    case SOCKS5_STATE_CONN_DATA:
        arkime_parsers_classify_tcp(session, data, remaining, which);
        arkime_parsers_unregister(session, uw);
        return 0;
    default:
        arkime_parsers_unregister(session, uw);
    }

    return 0;
}

/******************************************************************************/
LOCAL void socks_free(ArkimeSession_t UNUSED(*session), void *uw)
{
    SocksInfo_t            *socks          = uw;

    if (socks->user)
        g_free(socks->user);
    if (socks->host)
        g_free(socks->host);
    ARKIME_TYPE_FREE(SocksInfo_t, socks);
}
/******************************************************************************/
LOCAL void socks4_classify(ArkimeSession_t *session, const uint8_t *data, int len, int which, void *UNUSED(uw))
{
#ifdef SOCKSDEBUG
    LOG("SOCKSDEBUG: enter %d %d", data[0], len);
#endif

    if (len < 8 || data[len - 1] != 0)
        return;

    SocksInfo_t *socks;

    socks = ARKIME_TYPE_ALLOC0(SocksInfo_t);
    socks->which = which;
    socks->port = (data[2] & 0xff) << 8 | (data[3] & 0xff);
    if (data[4] == 0 && data[5] == 0 && data[6] == 0 && data[7] != 0) {
        socks->ip = 0;
    } else {
        memcpy(&socks->ip, data + 4, 4);
    }

    int i;
    for (i = 8; i < len && data[i]; i++);
    if (i > 8 && i != len ) {
        socks->user = g_strndup((char *)data + 8, i - 8);
        socks->userlen = i - 8;
    }

    if (socks->ip == 0) {
        i++;
        int start;
        for (start = i; i < len && data[i]; i++);
        if (i > start && i != len ) {
            socks->hostlen = i - start;
            socks->host = g_ascii_strdown((char *)data + start, i - start);
        }
    }

    arkime_parsers_register(session, socks4_parser, socks, socks_free);
}

/******************************************************************************/
LOCAL void socks5_classify(ArkimeSession_t *session, const uint8_t *data, int len, int which, void *UNUSED(uw))
{
#ifdef SOCKSDEBUG
    LOG("SOCKSDEBUG: enter %d %d", data[0], len);
#endif

    if ((len >= 3 && len <= 5) && data[1] == len - 2 && data[2] <= 3) {
        SocksInfo_t *socks;

        socks = ARKIME_TYPE_ALLOC0(SocksInfo_t);
        socks->which = which;
        socks->state5[which] = SOCKS5_STATE_VER_REQUEST;
        arkime_parsers_register(session, socks5_parser, socks, socks_free);
        return;
    }
    return;
}
/******************************************************************************/
void arkime_parser_init()
{
    ipField = arkime_field_define("socks", "ip",
                                  "ip.socks", "IP", "socks.ip",
                                  "SOCKS destination IP",
                                  ARKIME_FIELD_TYPE_IP, ARKIME_FIELD_FLAG_IPPRE,
                                  "aliases", "[\"socks.ip\"]",
                                  "portField", "sockspo",
                                  "portField2", "socks.port",
                                  (char *)NULL);

    hostField = arkime_field_define("socks", "lotermfield",
                                    "host.socks", "Host", "socks.host",
                                    "SOCKS destination host",
                                    ARKIME_FIELD_TYPE_STR,       0,
                                    "aliases", "[\"socks.host\"]",
                                    "category", "host",
                                    (char *)NULL);

    arkime_field_define("socks", "lotextfield",
                        "host.socks.tokens", "Hostname Tokens", "socks.hostTokens",
                        "SOCKS Hostname Tokens",
                        ARKIME_FIELD_TYPE_STR,       ARKIME_FIELD_FLAG_FAKE,
                        "aliases", "[\"socks.host.tokens\"]",
                        (char *)NULL);

    portField = arkime_field_define("socks", "integer",
                                    "port.socks", "Port", "socks.port",
                                    "SOCKS destination port",
                                    ARKIME_FIELD_TYPE_INT,       0,
                                    "aliases", "[\"socks.port\"]",
                                    "category", "port",
                                    (char *)NULL);

    userField = arkime_field_define("socks", "termfield",
                                    "socks.user", "User", "socks.user",
                                    "SOCKS authenticated user",
                                    ARKIME_FIELD_TYPE_STR,     0,
                                    "aliases", "[\"socksuser\"]",
                                    "category", "user",
                                    (char *)NULL);

    arkime_parsers_classifier_register_tcp("socks5", NULL, 0, (uint8_t *)"\005", 1, socks5_classify);
    arkime_parsers_classifier_register_tcp("socks4", NULL, 0, (uint8_t *)"\004\000", 2, socks4_classify);
    arkime_parsers_classifier_register_tcp("socks4", NULL, 0, (uint8_t *)"\004\001", 2, socks4_classify);
}

