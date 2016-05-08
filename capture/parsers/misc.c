/* Copyright 2012-2016 AOL Inc. All rights reserved.
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

extern MolochConfig_t        config;

static int userField;

/******************************************************************************/
void bt_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    moloch_session_add_protocol(session, "bittorrent");
}
/******************************************************************************/
void rdp_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{

    if (len > 5 && data[3] <= len && data[4] == (data[3] - 5) && data[5] == 0xe0) {
        moloch_session_add_protocol(session, "rdp");
        if (len > 30 && memcmp(data+11, "Cookie: mstshash=", 17) == 0) {
            char *end = g_strstr_len((char *)data+28, len-28, "\r\n");
            if (end)
                moloch_field_string_add(userField, session, (char*)data+28, end - (char *)data - 28, TRUE);
        }
    }
}
/******************************************************************************/
void imap_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (moloch_memstr((const char *)data+5, len-5, "IMAP", 4)) {
        moloch_session_add_protocol(session, "imap");
    }
}
/******************************************************************************/
void pop3_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    moloch_session_add_protocol(session, "pop3");
}
/******************************************************************************/
void gh0st_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (data[13] == 0x78 &&  
        (((data[8] == 0) && (data[7] == 0) && (((data[6]&0xff) << (uint32_t)8 | (data[5]&0xff)) == len)) ||  // Windows
         ((data[5] == 0) && (data[6] == 0) && (((data[7]&0xff) << (uint32_t)8 | (data[8]&0xff)) == len)))) { // Mac
        moloch_session_add_protocol(session, "gh0st");
    }

    if (data[7] == 0 && data[8] == 0 && data[11] == 0 && data[12] == 0 && data[13] == 0x78 && data[14] == 0x9c) {
        moloch_session_add_protocol(session, "gh0st");
    }
}
/******************************************************************************/
void other220_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (g_strstr_len((char *)data, len, "LMTP") != NULL) {
        moloch_session_add_protocol(session, "lmtp");
    }
    else if (g_strstr_len((char *)data, len, "SMTP") == NULL && g_strstr_len((char *)data, len, " TLS") == NULL) {
        moloch_session_add_protocol(session, "ftp");
    }
}
/******************************************************************************/
void vnc_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len >= 12 && data[7] == '.' && data[11] == 0xa)
        moloch_session_add_protocol(session, "vnc");
}
/******************************************************************************/
void redis_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    moloch_session_add_protocol(session, "redis");
}
/******************************************************************************/
void mongo_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (data[12] == 0xd4 && data[13] == 0x07 && g_strstr_len((gchar*)data+20, len-20, ".$cmd") != NULL)
        moloch_session_add_protocol(session, "mongo");
}
/******************************************************************************/
void sip_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    moloch_session_add_protocol(session, "sip");
}
/******************************************************************************/
void jabber_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (g_strstr_len((gchar*)data+5, len-5, "jabber") != NULL)
        moloch_session_add_protocol(session, "jabber");
}
/******************************************************************************/
void user_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    //If a USER packet must have not NICK or +iw with it so we don't pickup IRC
    if (len <= 5 || moloch_memstr((char *)data, len, "\nNICK ", 6) || moloch_memstr((char *)data, len, " +iw ", 5)) {
        return;
    }
    int i;
    for (i = 5; i < len; i++) {
        if (isspace(data[i]))
            break;
    }

    moloch_field_string_add(userField, session, (char*)data+5, i-5, TRUE);
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_parsers_classifier_register_tcp("bt", NULL, 0, (unsigned char*)"\x13" "BitTorrent protocol", 20, bt_classify);
    moloch_parsers_classifier_register_tcp("rdp", NULL, 0, (unsigned char*)"\x03\x00", 2, rdp_classify);
    moloch_parsers_classifier_register_tcp("imap", NULL, 0, (unsigned char*)"* OK ", 5, imap_classify);
    moloch_parsers_classifier_register_tcp("pop3", NULL, 0, (unsigned char*)"+OK POP3 ", 9, pop3_classify);
    moloch_parsers_classifier_register_tcp("gh0st", NULL, 14, 0, 0, gh0st_classify);
    moloch_parsers_classifier_register_tcp("other220", NULL, 0, (unsigned char*)"220 ", 4, other220_classify);
    moloch_parsers_classifier_register_tcp("vnc", NULL, 0, (unsigned char*)"RFB 0", 5, vnc_classify);

    moloch_parsers_classifier_register_tcp("redis", NULL, 0, (unsigned char*)"+PONG", 5, redis_classify);
    moloch_parsers_classifier_register_tcp("redis", NULL, 0, (unsigned char*)"\x2a\x31\x0d\x0a\x24", 5, redis_classify);
    moloch_parsers_classifier_register_tcp("redis", NULL, 0, (unsigned char*)"\x2a\x32\x0d\x0a\x24", 5, redis_classify);
    moloch_parsers_classifier_register_tcp("redis", NULL, 0, (unsigned char*)"\x2a\x33\x0d\x0a\x24", 5, redis_classify);
    moloch_parsers_classifier_register_tcp("redis", NULL, 0, (unsigned char*)"\x2a\x34\x0d\x0a\x24", 5, redis_classify);
    moloch_parsers_classifier_register_tcp("redis", NULL, 0, (unsigned char*)"\x2a\x35\x0d\x0a\x24", 5, redis_classify);

    moloch_parsers_classifier_register_udp("bt", NULL, 0, (unsigned char*)"d1:a", 4, bt_classify);
    moloch_parsers_classifier_register_udp("bt", NULL, 0, (unsigned char*)"d1:r", 4, bt_classify);
    moloch_parsers_classifier_register_udp("bt", NULL, 0, (unsigned char*)"d1:q", 4, bt_classify);

    moloch_parsers_classifier_register_tcp("mongo", NULL, 0, (unsigned char*)"\x35\x00\x00\x00", 4, mongo_classify);
    moloch_parsers_classifier_register_tcp("mongo", NULL, 0, (unsigned char*)"\x36\x00\x00\x00", 4, mongo_classify);
    moloch_parsers_classifier_register_tcp("mongo", NULL, 0, (unsigned char*)"\x37\x00\x00\x00", 4, mongo_classify);
    moloch_parsers_classifier_register_tcp("mongo", NULL, 0, (unsigned char*)"\x38\x00\x00\x00", 4, mongo_classify);
    moloch_parsers_classifier_register_tcp("mongo", NULL, 0, (unsigned char*)"\x39\x00\x00\x00", 4, mongo_classify);
    moloch_parsers_classifier_register_tcp("mongo", NULL, 0, (unsigned char*)"\x3a\x00\x00\x00", 4, mongo_classify);
    moloch_parsers_classifier_register_tcp("mongo", NULL, 0, (unsigned char*)"\x3b\x00\x00\x00", 4, mongo_classify);
    moloch_parsers_classifier_register_tcp("mongo", NULL, 0, (unsigned char*)"\x3c\x00\x00\x00", 4, mongo_classify);
    moloch_parsers_classifier_register_tcp("mongo", NULL, 0, (unsigned char*)"\x3d\x00\x00\x00", 4, mongo_classify);
    moloch_parsers_classifier_register_tcp("mongo", NULL, 0, (unsigned char*)"\x3e\x00\x00\x00", 4, mongo_classify);
    moloch_parsers_classifier_register_tcp("mongo", NULL, 0, (unsigned char*)"\x3f\x00\x00\x00", 4, mongo_classify);

    moloch_parsers_classifier_register_tcp("sip", NULL, 0, (unsigned char*)"SIP/2.0", 7, sip_classify);
    moloch_parsers_classifier_register_tcp("sip", NULL, 0, (unsigned char*)"REGISTER sip:", 13, sip_classify);

    moloch_parsers_classifier_register_tcp("jabber", NULL, 0, (unsigned char*)"<?xml", 5, jabber_classify);

    moloch_parsers_classifier_register_tcp("user", NULL, 0, (unsigned char*)"USER ", 5, user_classify);

    userField = moloch_field_by_db("user");
}

