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
LOCAL int socks4_parser(MolochSession_t *session, void *uw, const unsigned char *data, int remaining, int which)
{
    SocksInfo_t            *socks          = uw;

    switch(socks->state4) {
    case SOCKS4_STATE_REPLY:
        if (which == socks->which)
            return 0;
        if (remaining >= 8 && data[0] == 0 && data[1] >= 0x5a && data[1] <= 0x5d) {
            if (socks->ip)
                moloch_field_ip4_add(ipField, session, socks->ip);
            moloch_field_int_add(portField, session, socks->port);
            moloch_session_add_protocol(session, "socks");

            if (socks->user) {
                if (!moloch_field_string_add(userField, session, socks->user, socks->userlen, FALSE)) {
                    g_free(socks->user);
                }
                socks->user = 0;
            }
            if (socks->host) {
                if (!moloch_field_string_add(hostField, session, socks->host, socks->hostlen, FALSE)) {
                    g_free(socks->host);
                }
                socks->host = 0;
            }
            moloch_parsers_classify_tcp(session, data+8, remaining-8, which);
            socks->state4 = SOCKS4_STATE_DATA;
            return 8;
        }
        break;
    case SOCKS4_STATE_DATA:
        /*if (which != socks->which)
            return 0;*/
        moloch_parsers_classify_tcp(session, data, remaining, which);
        moloch_parsers_unregister(session, uw);
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
LOCAL int socks5_parser(MolochSession_t *session, void *uw, const unsigned char *data, int remaining, int which)
{
    SocksInfo_t            *socks          = uw;
    int                     consumed;

    //LOG("%d %d %d", which, socks->which, socks->state5[which]);
    //moloch_print_hex_string(data, remaining);

    switch(socks->state5[which]) {
    case SOCKS5_STATE_VER_REQUEST:
        if (data[2] == 0) {
            socks->state5[which] = SOCKS5_STATE_CONN_REQUEST;
        } else {
            socks->state5[which] = SOCKS5_STATE_USER_REQUEST;
        }
        socks->state5[(which+1)%2] = SOCKS5_STATE_VER_REPLY;
        break;
    case SOCKS5_STATE_VER_REPLY:
        if (remaining != 2 || data[0] != 5 || data[1] > 2) {
            moloch_parsers_unregister(session, uw);
            return 0;
        }

        moloch_session_add_protocol(session, "socks");

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
            moloch_parsers_unregister(session, uw);
        }


        return 2;
    case SOCKS5_STATE_USER_REQUEST:
        if ((2 + data[1] > (int)remaining) || (2 + data[1] + 1 + data[data[1]+2]  > (int)remaining)) {
            moloch_parsers_unregister(session, uw);
            return 0;
        }

        moloch_field_string_add(userField, session, (char *)data + 2, data[1], TRUE);
        moloch_session_add_tag(session, "socks:password");
        socks->state5[which] = SOCKS5_STATE_CONN_REQUEST;
        return data[1] + 1 + data[data[1]+2];
    case SOCKS5_STATE_USER_REPLY:
        socks->state5[which] = SOCKS5_STATE_CONN_REPLY;
        return 2;
    case SOCKS5_STATE_CONN_REQUEST:
        if (remaining < 6 || data[0] != 5 || data[1] != 1 || data[2] != 0) {
            moloch_parsers_unregister(session, uw);
            return 0;
        }

        socks->state5[which] = SOCKS5_STATE_CONN_DATA;
        if (data[3] == 1) { // IPV4
            socks->port = (data[8]&0xff) << 8 | (data[9]&0xff);
            memcpy(&socks->ip, data+4, 4);
            moloch_field_ip4_add(ipField, session, socks->ip);
            moloch_field_int_add(portField, session, socks->port);
            consumed = 4 + 4 + 2;
        } else if (data[3] == 3) { // Domain Name
            socks->port = (data[5+data[4]]&0xff) << 8 | (data[6+data[4]]&0xff);

            moloch_field_string_add_lower(hostField, session, (char *)data+5, data[4]);
            moloch_field_int_add(portField, session, socks->port);
            consumed = 4 + 1 + data[4] + 2;
        } else if (data[3] == 4) { // IPV6
            consumed = 4 + 16 + 2;
        } else {
            break;
        }

        moloch_parsers_classify_tcp(session, data+consumed, remaining-consumed, which);
        return consumed;
    case SOCKS5_STATE_CONN_REPLY: {
        if (remaining < 6) {
            moloch_parsers_unregister(session, uw);
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
        moloch_parsers_classify_tcp(session, data+consumed, remaining-consumed, which);
        return consumed;
    }
    case SOCKS5_STATE_CONN_DATA:
        moloch_parsers_classify_tcp(session, data, remaining, which);
        moloch_parsers_unregister(session, uw);
        return 0;
    default:
        moloch_parsers_unregister(session, uw);
    }

    return 0;
}

/******************************************************************************/
LOCAL void socks_free(MolochSession_t UNUSED(*session), void *uw)
{
    SocksInfo_t            *socks          = uw;

    if (socks->user)
        g_free(socks->user);
    if (socks->host)
        g_free(socks->host);
    MOLOCH_TYPE_FREE(SocksInfo_t, socks);
}
/******************************************************************************/
LOCAL void socks4_classify(MolochSession_t *session, const unsigned char *data, int len, int which, void *UNUSED(uw))
{
#ifdef SOCKSDEBUG
    LOG("SOCKSDEBUG: enter %d %d", data[0], len);
#endif

    if (data[len-1] == 0)  {
        SocksInfo_t *socks;

        socks = MOLOCH_TYPE_ALLOC0(SocksInfo_t);
        socks->which = which;
        socks->port = (data[2]&0xff) << 8 | (data[3]&0xff);
        if (data[4] == 0 && data[5] == 0 && data[6] == 0 && data[7] != 0) {
            socks->ip = 0;
        } else {
            memcpy(&socks->ip, data+4, 4);
        }

        int i;
        for(i = 8; i < len && data[i]; i++);
        if (i > 8 && i != len ) {
            socks->user = g_strndup((char *)data+8, i-8);
            socks->userlen = i - 8;
        }

        if (socks->ip == 0) {
            i++;
            int start;
            for(start = i; i < len && data[i]; i++);
            if (i > start && i != len ) {
                socks->hostlen = i-start;
                socks->host = g_ascii_strdown((char*)data+start, i-start);
            }
        }

        moloch_parsers_register(session, socks4_parser, socks, socks_free);
    }
}

/******************************************************************************/
LOCAL void socks5_classify(MolochSession_t *session, const unsigned char *data, int len, int which, void *UNUSED(uw))
{
#ifdef SOCKSDEBUG
    LOG("SOCKSDEBUG: enter %d %d", data[0], len);
#endif

    if ((len >=3 && len <= 5) && data[1] == len - 2 && data[2] <= 3) {
        SocksInfo_t *socks;

        socks = MOLOCH_TYPE_ALLOC0(SocksInfo_t);
        socks->which = which;
        socks->state5[which] = SOCKS5_STATE_VER_REQUEST;
        moloch_parsers_register(session, socks5_parser, socks, socks_free);
        return;
    }
    return;
}
/******************************************************************************/
void moloch_parser_init()
{
    ipField = moloch_field_define("socks", "ip",
        "ip.socks", "IP", "socks.ip",
        "SOCKS destination IP",
        MOLOCH_FIELD_TYPE_IP, MOLOCH_FIELD_FLAG_IPPRE, 
        "aliases", "[\"socks.ip\"]",
        "portField", "sockspo", 
        "portField2", "socks.port", 
        NULL);

    hostField = moloch_field_define("socks", "lotermfield",
        "host.socks", "Host", "socks.host",
        "SOCKS destination host",
        MOLOCH_FIELD_TYPE_STR,       0, 
        "aliases", "[\"socks.host\"]", 
        "category", "host",
        NULL);

    portField = moloch_field_define("socks", "integer",
        "port.socks", "Port", "socks.port",
        "SOCKS destination port",
        MOLOCH_FIELD_TYPE_INT,       0, 
        "aliases", "[\"socks.port\"]", 
        "category", "port",
        NULL);

    userField = moloch_field_define("socks", "termfield",
        "socks.user", "User", "socks.user",
        "SOCKS authenticated user",
        MOLOCH_FIELD_TYPE_STR,     0, 
        "aliases", "[\"socksuser\"]", 
        "category", "user",
        NULL);

    moloch_parsers_classifier_register_tcp("socks5", NULL, 0, (unsigned char*)"\005", 1, socks5_classify);
    moloch_parsers_classifier_register_tcp("socks4", NULL, 0, (unsigned char*)"\004\000", 2, socks4_classify);
    moloch_parsers_classifier_register_tcp("socks4", NULL, 0, (unsigned char*)"\004\001", 2, socks4_classify);
}

