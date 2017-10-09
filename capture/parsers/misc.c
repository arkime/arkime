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

extern MolochConfig_t        config;

static int userField;

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
void misc_add_protocol_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len), int UNUSED(which), void *uw)
{
    moloch_session_add_protocol(session, uw);
}
/******************************************************************************/
void ntp_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int len, int UNUSED(which), void *UNUSED(uw))
{

    if (session->port2 != 123 ||  // ntp port
            len < 48 ||           // min length
            data[1] > 16          // max stratum
       ) {
        return;
    }
    moloch_session_add_protocol(session, "ntp");
}
/******************************************************************************/
void snmp_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    uint32_t apc, atag, alen;
    BSB bsb;

    BSB_INIT(bsb, data, len);
    unsigned char *value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value || atag != 16 || alen < 16)
        return;

    BSB_INIT(bsb, value, alen);

    value = moloch_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (!value || atag != 2 || alen != 1 || value[0] > 3)
        return;

    moloch_session_add_protocol(session, "snmp");
}
/******************************************************************************/
void syslog_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int len, int UNUSED(which), void *UNUSED(uw))
{
    int i;
    for (i = 2; i < len; i++) {
        if (data[i] == '>') {
            moloch_session_add_protocol(session, "syslog");
            return;
        }

        if (!isdigit(data[i]))
            return;
    }
}
/******************************************************************************/
void stun_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int len, int UNUSED(which), void *UNUSED(uw))
{
    if (20 + data[3] != len)
        return;

    if (memcmp(data+4, "\x21\x12\xa4\x42", 4) == 0) {
        moloch_session_add_protocol(session, "stun");
        return;
    }

    if (data[1] == 1 && len > 25 && data[23] + 24 == len) {
        moloch_session_add_protocol(session, "stun");
        return;
    }

}
/******************************************************************************/
void stun_rsp_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (moloch_memstr((const char *)data+7, len-7, "STUN", 4))
        moloch_session_add_protocol(session, "stun");
}
/******************************************************************************/
void flap_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 6)
        return;

    int flen = 6 + ((data[4] << 8) | data[5]);

    if (len < flen)
        return;

    // lenght matches or there is another flap frame in the packet
    if (len == flen || (data[flen] == '*'))
        moloch_session_add_protocol(session, "flap");
}
/******************************************************************************/
void tacacs_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    if (session->port1 == 49 || session->port2 == 49)
        moloch_session_add_protocol(session, "tacacs");
}
/******************************************************************************/
void dropbox_lan_sync_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (moloch_memstr((const char *)data+1, len-1, "host_int", 8)) {
        moloch_session_add_protocol(session, "dropbox-lan-sync");
    }
}
/******************************************************************************/
void kafka_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 50 || data[4] != 0 || data[5] > 6|| data[7] != 0 || data[8] != 0)
        return;

    int flen = 4 + ((data[2] << 8) | data[3]);

    if (len != flen)
        return;

    moloch_session_add_protocol(session, "kafka");
}
/******************************************************************************/
void thrift_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len > 20 && data[4] == 0x80 && data[5] == 0x01 && data[6] == 0)
    moloch_session_add_protocol(session, "thrift");
}
/******************************************************************************/
void rip_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    if (session->port2 != 520 &&  session->port1 != 520)
        return;
    moloch_session_add_protocol(session, "rip");
}
/******************************************************************************/
#define PARSERS_CLASSIFY_BOTH(_name, _uw, _offset, _str, _len, _func) \
    moloch_parsers_classifier_register_tcp(_name, _uw, _offset, (unsigned char*)_str, _len, _func); \
    moloch_parsers_classifier_register_udp(_name, _uw, _offset, (unsigned char*)_str, _len, _func);

void moloch_parser_init()
{
    moloch_parsers_classifier_register_tcp("bt", "bittorrent", 0, (unsigned char*)"\x13" "BitTorrent protocol", 20, misc_add_protocol_classify);
    moloch_parsers_classifier_register_tcp("bt", "bittorrent", 0, (unsigned char*)"BSYNC\x00", 6, misc_add_protocol_classify);
    moloch_parsers_classifier_register_tcp("rdp", NULL, 0, (unsigned char*)"\x03\x00", 2, rdp_classify);
    moloch_parsers_classifier_register_tcp("imap", NULL, 0, (unsigned char*)"* OK ", 5, imap_classify);
    moloch_parsers_classifier_register_tcp("pop3", "pop3", 0, (unsigned char*)"+OK POP3 ", 9, misc_add_protocol_classify);
    moloch_parsers_classifier_register_tcp("gh0st", NULL, 14, 0, 0, gh0st_classify);
    moloch_parsers_classifier_register_tcp("other220", NULL, 0, (unsigned char*)"220 ", 4, other220_classify);
    moloch_parsers_classifier_register_tcp("vnc", NULL, 0, (unsigned char*)"RFB 0", 5, vnc_classify);

    moloch_parsers_classifier_register_tcp("redis", "redis", 0, (unsigned char*)"+PONG", 5, misc_add_protocol_classify);
    moloch_parsers_classifier_register_tcp("redis", "redis", 0, (unsigned char*)"\x2a\x31\x0d\x0a\x24", 5, misc_add_protocol_classify);
    moloch_parsers_classifier_register_tcp("redis", "redis", 0, (unsigned char*)"\x2a\x32\x0d\x0a\x24", 5, misc_add_protocol_classify);
    moloch_parsers_classifier_register_tcp("redis", "redis", 0, (unsigned char*)"\x2a\x33\x0d\x0a\x24", 5, misc_add_protocol_classify);
    moloch_parsers_classifier_register_tcp("redis", "redis", 0, (unsigned char*)"\x2a\x34\x0d\x0a\x24", 5, misc_add_protocol_classify);
    moloch_parsers_classifier_register_tcp("redis", "redis", 0, (unsigned char*)"\x2a\x35\x0d\x0a\x24", 5, misc_add_protocol_classify);

    moloch_parsers_classifier_register_udp("bt", "bittorrent", 0, (unsigned char*)"d1:a", 4, misc_add_protocol_classify);
    moloch_parsers_classifier_register_udp("bt", "bittorrent", 0, (unsigned char*)"d1:r", 4, misc_add_protocol_classify);
    moloch_parsers_classifier_register_udp("bt", "bittorrent", 0, (unsigned char*)"d1:q", 4, misc_add_protocol_classify);

    moloch_parsers_classifier_register_tcp("mongo", "mongo", 8, (unsigned char*)"\x00\x00\x00\x00\xd4\x07\x00\x00", 8, misc_add_protocol_classify);
    moloch_parsers_classifier_register_tcp("mongo", "mongo", 8, (unsigned char*)"\xff\xff\xff\xff\xd4\x07\x00\x00", 8, misc_add_protocol_classify);

    PARSERS_CLASSIFY_BOTH("sip", "sip", 0, "SIP/2.0", 7, misc_add_protocol_classify);
    PARSERS_CLASSIFY_BOTH("sip", "sip", 0, "REGISTER sip:", 13, misc_add_protocol_classify);
    PARSERS_CLASSIFY_BOTH("sip", "sip", 0, "NOTIFY sip:", 11, misc_add_protocol_classify);

    moloch_parsers_classifier_register_tcp("jabber", NULL, 0, (unsigned char*)"<?xml", 5, jabber_classify);

    moloch_parsers_classifier_register_tcp("user", NULL, 0, (unsigned char*)"USER ", 5, user_classify);

    moloch_parsers_classifier_register_tcp("thrift", "thrift", 0, (unsigned char*)"\x80\x01\x00\x01\x00\x00\x00", 7, misc_add_protocol_classify);
    moloch_parsers_classifier_register_tcp("thrift", NULL, 0, (unsigned char*)"\x00\x00", 2, thrift_classify);

    moloch_parsers_classifier_register_tcp("aerospike", "aerospike", 0, (unsigned char*)"\x02\x01\x00\x00\x00\x00\x00\x4e\x6e\x6f\x64\x65", 12, misc_add_protocol_classify);
    moloch_parsers_classifier_register_tcp("aerospike", "aerospike", 0, (unsigned char*)"\x02\x01\x00\x00\x00\x00\x00\x23\x6e\x6f\x64\x65", 12, misc_add_protocol_classify);

    moloch_parsers_classifier_register_tcp("cassandra", "cassandra", 0, (unsigned char*)"\x00\x00\x00\x25\x80\x01\x00\x01\x00\x00\x00\x0c\x73\x65\x74\x5f", 16, misc_add_protocol_classify);
    moloch_parsers_classifier_register_tcp("cassandra", "cassandra", 0, (unsigned char*)"\x00\x00\x00\x1d\x80\x01\x00\x01\x00\x00\x00\x10\x64\x65\x73\x63", 16, misc_add_protocol_classify);

    moloch_parsers_classifier_register_udp("ntp", NULL, 0, (unsigned char*)"\x19", 1, ntp_classify);
    moloch_parsers_classifier_register_udp("ntp", NULL, 0, (unsigned char*)"\x1a", 1, ntp_classify);
    moloch_parsers_classifier_register_udp("ntp", NULL, 0, (unsigned char*)"\x1b", 1, ntp_classify);
    moloch_parsers_classifier_register_udp("ntp", NULL, 0, (unsigned char*)"\x1c", 1, ntp_classify);
    moloch_parsers_classifier_register_udp("ntp", NULL, 0, (unsigned char*)"\x23", 1, ntp_classify);
    moloch_parsers_classifier_register_udp("ntp", NULL, 0, (unsigned char*)"\x24", 1, ntp_classify);
    moloch_parsers_classifier_register_udp("ntp", NULL, 0, (unsigned char*)"\xd9", 1, ntp_classify);
    moloch_parsers_classifier_register_udp("ntp", NULL, 0, (unsigned char*)"\xdb", 1, ntp_classify);
    moloch_parsers_classifier_register_udp("ntp", NULL, 0, (unsigned char*)"\xe3", 1, ntp_classify);

    moloch_parsers_classifier_register_udp("snmp", NULL, 0, (unsigned char*)"\x30", 1, snmp_classify);

    moloch_parsers_classifier_register_udp("bjnp", "bjnp", 0, (unsigned char*)"BJNP", 4, misc_add_protocol_classify);

    PARSERS_CLASSIFY_BOTH("syslog", NULL, 0, (unsigned char*)"<1", 2, syslog_classify);
    PARSERS_CLASSIFY_BOTH("syslog", NULL, 0, (unsigned char*)"<2", 2, syslog_classify);
    PARSERS_CLASSIFY_BOTH("syslog", NULL, 0, (unsigned char*)"<3", 2, syslog_classify);
    PARSERS_CLASSIFY_BOTH("syslog", NULL, 0, (unsigned char*)"<4", 2, syslog_classify);
    PARSERS_CLASSIFY_BOTH("syslog", NULL, 0, (unsigned char*)"<5", 2, syslog_classify);
    PARSERS_CLASSIFY_BOTH("syslog", NULL, 0, (unsigned char*)"<6", 2, syslog_classify);
    PARSERS_CLASSIFY_BOTH("syslog", NULL, 0, (unsigned char*)"<7", 2, syslog_classify);
    PARSERS_CLASSIFY_BOTH("syslog", NULL, 0, (unsigned char*)"<8", 2, syslog_classify);
    PARSERS_CLASSIFY_BOTH("syslog", NULL, 0, (unsigned char*)"<9", 2, syslog_classify);

    PARSERS_CLASSIFY_BOTH("stun", NULL, 0, (unsigned char*)"RSP/", 4,stun_rsp_classify);

    moloch_parsers_classifier_register_udp("stun", NULL, 0, (unsigned char*)"\x00\x01\x00", 3, stun_classify);
    moloch_parsers_classifier_register_udp("stun", NULL, 0, (unsigned char*)"\x00\x03\x00", 3, stun_classify);
    moloch_parsers_classifier_register_udp("stun", NULL, 0, (unsigned char*)"\x01\x01\x00", 3, stun_classify);

    moloch_parsers_classifier_register_tcp("flap", NULL, 0, (unsigned char*)"\x2a\x01", 2, flap_classify);

    moloch_parsers_classifier_register_tcp("nsclient", "nsclient", 0, (unsigned char*)"NSClient", 8, misc_add_protocol_classify);
    moloch_parsers_classifier_register_tcp("nsclient", "nsclient", 0, (unsigned char*)"None&", 5, misc_add_protocol_classify);

    moloch_parsers_classifier_register_udp("ssdp", "ssdp", 0, (unsigned char*)"M-SEARCH ", 9, misc_add_protocol_classify);

    moloch_parsers_classifier_register_tcp("zabbix", "zabbix", 0, (unsigned char*)"ZBXD\x01", 5, misc_add_protocol_classify);

    moloch_parsers_classifier_register_tcp("rmi", "rmi", 0, (unsigned char*)"\x4a\x52\x4d\x49\x00\x02\x4b", 7, misc_add_protocol_classify);

    PARSERS_CLASSIFY_BOTH("tacacs", NULL, 0, (unsigned char*)"\xc0\x01\x01", 3, tacacs_classify);
    PARSERS_CLASSIFY_BOTH("tacacs", NULL, 0, (unsigned char*)"\xc0\x01\x02", 3, tacacs_classify);
    PARSERS_CLASSIFY_BOTH("tacacs", NULL, 0, (unsigned char*)"\xc0\x02\x01", 3, tacacs_classify);
    PARSERS_CLASSIFY_BOTH("tacacs", NULL, 0, (unsigned char*)"\xc0\x03\x01", 3, tacacs_classify);
    PARSERS_CLASSIFY_BOTH("tacacs", NULL, 0, (unsigned char*)"\xc0\x03\x02", 3, tacacs_classify);
    PARSERS_CLASSIFY_BOTH("tacacs", NULL, 0, (unsigned char*)"\xc1\x01\x01", 3, tacacs_classify);
    PARSERS_CLASSIFY_BOTH("tacacs", NULL, 0, (unsigned char*)"\xc1\x01\x02", 3, tacacs_classify);

    moloch_parsers_classifier_register_tcp("flash-policy", "flash-policy", 0, (unsigned char*)"<policy-file-request/>", 22, misc_add_protocol_classify);

    moloch_parsers_classifier_register_port("dropbox-lan-sync",  NULL, 17500, MOLOCH_PARSERS_PORT_UDP, dropbox_lan_sync_classify);

    moloch_parsers_classifier_register_tcp("kafka", NULL, 0, (unsigned char*)"\x00\x00", 2, kafka_classify);

    moloch_parsers_classifier_register_udp("steam-friends", "steam-friends", 0, (unsigned char*)"VS01", 4, misc_add_protocol_classify);
    moloch_parsers_classifier_register_udp("value-a2s", "value-a2s", 0, (unsigned char*)"\xff\xff\xff\xff\x54\x53\x6f\x75", 8, misc_add_protocol_classify);
    moloch_parsers_classifier_register_tcp("stream-ihscp", "stream-ihscp", 0, (unsigned char*)"\xa4\x00\x00\x00\x56\x54\x30\x31", 8, misc_add_protocol_classify);

    moloch_parsers_classifier_register_tcp("honeywell-tcc", "honeywell-tcc", 0, (unsigned char*)"\x43\x42\x4b\x50\x50\x52\x05\x50", 8, misc_add_protocol_classify);

    moloch_parsers_classifier_register_tcp("pjl", "pjl", 0, (unsigned char*)"\x1b\x25\x2d\x31\x32\x33\x34\x35", 8, misc_add_protocol_classify);
    moloch_parsers_classifier_register_tcp("pjl", "pjl", 0, (unsigned char*)"\x40\x50\x4a\x4c\x20", 5, misc_add_protocol_classify);

    moloch_parsers_classifier_register_tcp("dcerpc", "dcerpc", 0, (unsigned char*)"\x05\x00\x0b", 3, misc_add_protocol_classify);

    moloch_parsers_classifier_register_udp("rip", NULL, 0, (unsigned char*)"\x01\x01\x00\x00", 4, rip_classify);
    moloch_parsers_classifier_register_udp("rip", NULL, 0, (unsigned char*)"\x01\x02\x00\x00", 4, rip_classify);
    moloch_parsers_classifier_register_udp("rip", NULL, 0, (unsigned char*)"\x02\x01\x00\x00", 4, rip_classify);
    moloch_parsers_classifier_register_udp("rip", NULL, 0, (unsigned char*)"\x02\x02\x00\x00", 4, rip_classify);

    moloch_parsers_classifier_register_tcp("nzsql", "nzsql", 0, (unsigned char*)"\x00\x00\x00\x08\x00\x01\x00\x03", 8, misc_add_protocol_classify);

    userField = moloch_field_by_db("user");
}

