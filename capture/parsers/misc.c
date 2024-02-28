/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL  int userField;

/******************************************************************************/
LOCAL void rdp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{

    if (len > 5 && data[3] <= len && data[4] == (data[3] - 5) && data[5] == 0xe0) {
        arkime_session_add_protocol(session, "rdp");
        if (len > 30 && memcmp(data + 11, "Cookie: mstshash=", 17) == 0) {
            const char *end = g_strstr_len((char *)data + 28, len - 28, "\r\n");
            if (end)
                arkime_field_string_add_lower(userField, session, (char * )data + 28, end - (char *)data - 28);
        }
    }
}
/******************************************************************************/
LOCAL void imap_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_memstr((const char *)data + 5, len - 5, "IMAP", 4)) {
        arkime_session_add_protocol(session, "imap");
    }
}
/******************************************************************************/
LOCAL void gh0st_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 15)
        return;

    if (data[13] == 0x78 && data[14] == 0x9c &&
        (((data[8] == 0) && (data[7] == 0) && (((data[6] & 0xff) << (uint32_t)8 | (data[5] & 0xff)) == len)) || // Windows
         ((data[5] == 0) && (data[6] == 0) && (((data[7] & 0xff) << (uint32_t)8 | (data[8] & 0xff)) == len)))) { // Mac
        arkime_session_add_protocol(session, "gh0st");
    }

    if (data[7] == 0 && data[8] == 0 && data[11] == 0 && data[12] == 0 && data[13] == 0x78 && data[14] == 0x9c) {
        arkime_session_add_protocol(session, "gh0st");
    }
}
/******************************************************************************/
LOCAL void other220_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (g_strstr_len((char *)data, len, "LMTP") != NULL) {
        arkime_session_add_protocol(session, "lmtp");
    } else if (g_strstr_len((char *)data, len, "SMTP") == NULL && g_strstr_len((char *)data, len, " TLS") == NULL) {
        arkime_session_add_protocol(session, "ftp");
    }
}
/******************************************************************************/
LOCAL void vnc_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len >= 12 && data[7] == '.' && data[11] == 0xa)
        arkime_session_add_protocol(session, "vnc");
}
/******************************************************************************/
LOCAL void jabber_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (g_strstr_len((gchar * )data + 5, len - 5, "jabber") != NULL)
        arkime_session_add_protocol(session, "jabber");
}
/******************************************************************************/
LOCAL void user_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    //If a USER packet must have not NICK or +iw with it so we don't pickup IRC
    if (len <= 5 || arkime_memstr((char *)data, len, "\nNICK ", 6) || arkime_memstr((char *)data, len, " +iw ", 5)) {
        return;
    }
    int i;
    for (i = 5; i < len; i++) {
        if (isspace(data[i]))
            break;
    }

    arkime_field_string_add_lower(userField, session, (char *)data + 5, i - 5);
}
/******************************************************************************/
LOCAL void misc_add_protocol_classify(ArkimeSession_t *session, const uint8_t *UNUSED(data), int UNUSED(len), int UNUSED(which), void *uw)
{
    arkime_session_add_protocol(session, uw);
}
/******************************************************************************/
LOCAL void ntp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{

    if ((session->port1 != 123 && session->port2 != 123) ||  // ntp port
        len < 48 ||                                         // min length
        data[1] > 16                                        // max stratum
       ) {
        return;
    }
    arkime_session_add_protocol(session, "ntp");
}
/******************************************************************************/
LOCAL void syslog_classify(ArkimeSession_t *session, const uint8_t *UNUSED(data), int len, int UNUSED(which), void *UNUSED(uw))
{
    int i;
    for (i = 2; i < len; i++) {
        if (data[i] == '>') {
            arkime_session_add_protocol(session, "syslog");
            return;
        }

        if (!isdigit(data[i]))
            return;
    }
}
/******************************************************************************/
LOCAL void stun_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 20 || 20 + data[3] != len)
        return;

    if (memcmp(data + 4, "\x21\x12\xa4\x42", 4) == 0) {
        arkime_session_add_protocol(session, "stun");
        return;
    }

    if (data[1] == 1 && len > 25 && data[23] + 24 == len) {
        arkime_session_add_protocol(session, "stun");
        return;
    }
}
/******************************************************************************/
LOCAL void stun_rsp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_memstr((const char *)data + 7, len - 7, "STUN", 4))
        arkime_session_add_protocol(session, "stun");
}
/******************************************************************************/
LOCAL void flap_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 6)
        return;

    int flen = 6 + ((data[4] << 8) | data[5]);

    if (len < flen)
        return;

    // lenght matches or there is another flap frame in the packet
    if (len == flen || (data[flen] == '*'))
        arkime_session_add_protocol(session, "flap");
}
/******************************************************************************/
LOCAL void tacacs_classify(ArkimeSession_t *session, const uint8_t *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    if (session->port1 == 49 || session->port2 == 49)
        arkime_session_add_protocol(session, "tacacs");
}
/******************************************************************************/
LOCAL void dropbox_lan_sync_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_memstr((const char *)data + 1, len - 1, "host_int", 8)) {
        arkime_session_add_protocol(session, "dropbox-lan-sync");
    }
}
/******************************************************************************/
LOCAL void kafka_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 10 || data[4] != 0 || data[5] > 6 || data[7] != 0)
        return;

    int flen = 4 + ((data[2] << 8) | data[3]);

    if (len != flen)
        return;

    arkime_session_add_protocol(session, "kafka");
}
/******************************************************************************/
LOCAL void thrift_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len > 20 && data[4] == 0x80 && data[5] == 0x01 && data[6] == 0)
        arkime_session_add_protocol(session, "thrift");
}
/******************************************************************************/
LOCAL void rip_classify(ArkimeSession_t *session, const uint8_t *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    if (session->port2 != 520 &&  session->port1 != 520)
        return;
    arkime_session_add_protocol(session, "rip");
}
/******************************************************************************/
LOCAL void isakmp_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 18 ||
        (data[16] != 1 && data[16] != 8 && data[16] != 33 && data[16] != 46) ||
        (data[17] != 0x10 && data[17] != 0x20 && data[17] != 0x02)) {
        return;
    }
    arkime_session_add_protocol(session, "isakmp");
}
/******************************************************************************/
LOCAL void aruba_papi_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 20 || data[0] != 0x49 || data[1] != 0x72) {
        return;
    }
    arkime_session_add_protocol(session, "aruba-papi");
}
/******************************************************************************/
LOCAL void sccp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len > 20 && len >= data[0] + 8 && memcmp(data + 1, "\0\0\0\0\0\0\0", 7) == 0) {
        arkime_session_add_protocol(session, "sccp");
    }
}
/******************************************************************************/
LOCAL void wudo_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 15)
        return;

    if (memcmp(data, "\x00\x00\x00\x00", 4) == 0) {
        arkime_session_add_protocol(session, "wudo");
    } else if (memcmp(data, "\x0eSwarm protocol", 15) == 0) {
        arkime_session_add_protocol(session, "wudo");
    }
}
/******************************************************************************/
LOCAL void mqtt_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 30 || memcmp("MQ", data + 4, 2) != 0)
        return;

    arkime_session_add_protocol(session, "mqtt");

    BSB bsb;

    BSB_INIT(bsb, data, len);
    BSB_IMPORT_skip(bsb, 2);

    int nameLen = 0;
    BSB_IMPORT_u16(bsb, nameLen);
    BSB_IMPORT_skip(bsb, nameLen);

    BSB_IMPORT_skip(bsb, 1); // version

    int flags = 0;
    BSB_IMPORT_u08(bsb, flags);

    BSB_IMPORT_skip(bsb, 2); // keep alive

    int idLen = 0;
    BSB_IMPORT_u16(bsb, idLen);
    BSB_IMPORT_skip(bsb, idLen);

    if (flags & 0x04) { // will
        int skiplen = 0;

        BSB_IMPORT_u16(bsb, skiplen);
        BSB_IMPORT_skip(bsb, skiplen);

        BSB_IMPORT_u16(bsb, skiplen);
        BSB_IMPORT_skip(bsb, skiplen);
    }

    if (flags & 0x80) {
        int      userLen = 0;
        uint8_t *user = 0;
        BSB_IMPORT_u16(bsb, userLen);
        BSB_IMPORT_ptr(bsb, user, userLen);

        if (BSB_NOT_ERROR(bsb)) {
            arkime_field_string_add_lower(userField, session, (char *)user, userLen);
        }
    }
}
/******************************************************************************/
LOCAL void hdfs_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 10 || data[5] != 0xa)
        return;
    arkime_session_add_protocol(session, "hdfs");
}
/******************************************************************************/
LOCAL void hsrp_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (session->port1 != session->port2 || len < 3)
        return;

    if (data[0] == 0 && data[1] == 3)
        arkime_session_add_protocol(session, "hsrp");
    else if (data[0] == 1 && data[1] == 40 && data[2] == 2)
        arkime_session_add_protocol(session, "hsrpv2");
}
/******************************************************************************/
LOCAL void safet_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 24 || data[2] != len)
        return;
    arkime_session_add_protocol(session, "safet");
}
/******************************************************************************/
LOCAL void telnet_tcp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 3 || data[0] != 0xff || data[1] < 0xfa)
        return;
    arkime_session_add_protocol(session, "telnet");
}
/******************************************************************************/
LOCAL void netflow_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    // Exclude DNS and small packets
    if (session->port1 == 53 || session->port2 == 53 || len < 24)
        return;

    BSB bsb;
    uint16_t count = 0;
    uint32_t systime = 0;

    BSB_INIT(bsb, data, len);
    BSB_IMPORT_skip(bsb, 2); //version
    BSB_IMPORT_u16(bsb, count);
    BSB_IMPORT_skip(bsb, 4); // sys_uptime
    BSB_IMPORT_u32(bsb, systime);

    // Make sure valid count and time
    if (count == 0 || count > 200 || count * 16 > len || systime < 1000000000 /*Sep 2001*/)
        return;

    arkime_session_add_protocol(session, "netflow");
}
/******************************************************************************/

#define PARSERS_CLASSIFY_BOTH(_name, _uw, _offset, _str, _len, _func) \
    arkime_parsers_classifier_register_tcp(_name, _uw, _offset, (uint8_t *)_str, _len, _func); \
    arkime_parsers_classifier_register_udp(_name, _uw, _offset, (uint8_t *)_str, _len, _func);

#define SIMPLE_CLASSIFY_TCP(name, bytes) arkime_parsers_classifier_register_tcp(name, name, 0, (uint8_t *)bytes, sizeof(bytes) - 1, misc_add_protocol_classify);
#define SIMPLE_CLASSIFY_UDP(name, bytes) arkime_parsers_classifier_register_udp(name, name, 0, (uint8_t *)bytes, sizeof(bytes) - 1, misc_add_protocol_classify);
#define SIMPLE_CLASSIFY_BOTH(name, bytes) PARSERS_CLASSIFY_BOTH(name, name, 0, (uint8_t *)bytes, sizeof(bytes) - 1, misc_add_protocol_classify);

void arkime_parser_init()
{
    SIMPLE_CLASSIFY_TCP("bittorrent", "\x13" "BitTorrent protocol");
    SIMPLE_CLASSIFY_TCP("bittorrent", "BSYNC\x00");
    SIMPLE_CLASSIFY_UDP("bittorrent", "d1:a");
    SIMPLE_CLASSIFY_UDP("bittorrent", "d1:r");
    SIMPLE_CLASSIFY_UDP("bittorrent", "d1:q");

    /* Bitcoin main network */
    SIMPLE_CLASSIFY_TCP("bitcoin", "\xf9\xbe\xb4\xd9");
    /* Bitcoin namecoin fork */
    SIMPLE_CLASSIFY_TCP("bitcoin", "\xf9\xbe\xb4\xfe");

    CLASSIFY_TCP("rdp", 0, "\x03\x00", rdp_classify);
    CLASSIFY_TCP("imap", 0, "* OK ", imap_classify);
    SIMPLE_CLASSIFY_TCP("pop3", "+OK ");
    CLASSIFY_TCP("gh0st", 13, "\x78", gh0st_classify);
    CLASSIFY_TCP("other220", 0, "220 ", other220_classify);
    CLASSIFY_TCP("vnc", 0, "RFB 0", vnc_classify);

    SIMPLE_CLASSIFY_TCP("redis", "+PONG");
    SIMPLE_CLASSIFY_TCP("redis", "\x2a\x31\x0d\x0a\x24");
    SIMPLE_CLASSIFY_TCP("redis", "\x2a\x32\x0d\x0a\x24");
    SIMPLE_CLASSIFY_TCP("redis", "\x2a\x33\x0d\x0a\x24");
    SIMPLE_CLASSIFY_TCP("redis", "\x2a\x34\x0d\x0a\x24");
    SIMPLE_CLASSIFY_TCP("redis", "\x2a\x35\x0d\x0a\x24");
    SIMPLE_CLASSIFY_TCP("redis", "-NOAUTH ");

    CLASSIFY_TCP("mongo", 8, "\x00\x00\x00\x00\xd4\x07\x00\x00", misc_add_protocol_classify);
    CLASSIFY_TCP("mongo", 8, "\xff\xff\xff\xff\xd4\x07\x00\x00", misc_add_protocol_classify);

    SIMPLE_CLASSIFY_BOTH("sip", "SIP/2.0");
    SIMPLE_CLASSIFY_BOTH("sip", "REGISTER sip:");
    SIMPLE_CLASSIFY_BOTH("sip", "NOTIFY sip:");

    CLASSIFY_TCP("jabber", 0, "<?xml", jabber_classify);

    CLASSIFY_TCP("user", 0, "USER ", user_classify);

    SIMPLE_CLASSIFY_TCP("thrift", "\x80\x01\x00\x01\x00\x00\x00");
    CLASSIFY_TCP("thrift", 0, "\x00\x00", thrift_classify);

    SIMPLE_CLASSIFY_TCP("aerospike", "\x02\x01\x00\x00\x00\x00\x00\x4e\x6e\x6f\x64\x65");
    SIMPLE_CLASSIFY_TCP("aerospike", "\x02\x01\x00\x00\x00\x00\x00\x23\x6e\x6f\x64\x65");

    SIMPLE_CLASSIFY_TCP("cassandra", "\x00\x00\x00\x25\x80\x01\x00\x01\x00\x00\x00\x0c\x73\x65\x74\x5f");
    SIMPLE_CLASSIFY_TCP("cassandra", "\x00\x00\x00\x1d\x80\x01\x00\x01\x00\x00\x00\x10\x64\x65\x73\x63");

    CLASSIFY_UDP("ntp", 0, "\x13", ntp_classify);
    CLASSIFY_UDP("ntp", 0, "\x19", ntp_classify);
    CLASSIFY_UDP("ntp", 0, "\x1a", ntp_classify);
    CLASSIFY_UDP("ntp", 0, "\x1b", ntp_classify);
    CLASSIFY_UDP("ntp", 0, "\x1c", ntp_classify);
    CLASSIFY_UDP("ntp", 0, "\x21", ntp_classify);
    CLASSIFY_UDP("ntp", 0, "\x23", ntp_classify);
    CLASSIFY_UDP("ntp", 0, "\x24", ntp_classify);
    CLASSIFY_UDP("ntp", 0, "\xd9", ntp_classify);
    CLASSIFY_UDP("ntp", 0, "\xdb", ntp_classify);
    CLASSIFY_UDP("ntp", 0, "\xe3", ntp_classify);

    SIMPLE_CLASSIFY_UDP("bjnp", "BJNP");

    PARSERS_CLASSIFY_BOTH("syslog", NULL, 0, (uint8_t *)"<1", 2, syslog_classify);
    PARSERS_CLASSIFY_BOTH("syslog", NULL, 0, (uint8_t *)"<2", 2, syslog_classify);
    PARSERS_CLASSIFY_BOTH("syslog", NULL, 0, (uint8_t *)"<3", 2, syslog_classify);
    PARSERS_CLASSIFY_BOTH("syslog", NULL, 0, (uint8_t *)"<4", 2, syslog_classify);
    PARSERS_CLASSIFY_BOTH("syslog", NULL, 0, (uint8_t *)"<5", 2, syslog_classify);
    PARSERS_CLASSIFY_BOTH("syslog", NULL, 0, (uint8_t *)"<6", 2, syslog_classify);
    PARSERS_CLASSIFY_BOTH("syslog", NULL, 0, (uint8_t *)"<7", 2, syslog_classify);
    PARSERS_CLASSIFY_BOTH("syslog", NULL, 0, (uint8_t *)"<8", 2, syslog_classify);
    PARSERS_CLASSIFY_BOTH("syslog", NULL, 0, (uint8_t *)"<9", 2, syslog_classify);

    PARSERS_CLASSIFY_BOTH("stun", NULL, 0, (uint8_t *)"RSP/", 4, stun_rsp_classify);

    CLASSIFY_UDP("stun", 0, "\x00\x01\x00", stun_classify);
    CLASSIFY_UDP("stun", 0, "\x00\x03\x00", stun_classify);
    CLASSIFY_UDP("stun", 0, "\x01\x01\x00", stun_classify);

    CLASSIFY_TCP("flap", 0, "\x2a\x01", flap_classify);

    SIMPLE_CLASSIFY_TCP("nsclient", "NSClient");
    SIMPLE_CLASSIFY_TCP("nsclient", "None&");

    SIMPLE_CLASSIFY_UDP("ssdp", "M-SEARCH ");
    SIMPLE_CLASSIFY_UDP("ssdp", "NOTIFY * ");

    SIMPLE_CLASSIFY_TCP("zabbix", "ZBXD\x01");

    SIMPLE_CLASSIFY_TCP("rmi", "\x4a\x52\x4d\x49\x00\x02\x4b");

    PARSERS_CLASSIFY_BOTH("tacacs", NULL, 0, (uint8_t *)"\xc0\x01\x01", 3, tacacs_classify);
    PARSERS_CLASSIFY_BOTH("tacacs", NULL, 0, (uint8_t *)"\xc0\x01\x02", 3, tacacs_classify);
    PARSERS_CLASSIFY_BOTH("tacacs", NULL, 0, (uint8_t *)"\xc0\x02\x01", 3, tacacs_classify);
    PARSERS_CLASSIFY_BOTH("tacacs", NULL, 0, (uint8_t *)"\xc0\x03\x01", 3, tacacs_classify);
    PARSERS_CLASSIFY_BOTH("tacacs", NULL, 0, (uint8_t *)"\xc0\x03\x02", 3, tacacs_classify);
    PARSERS_CLASSIFY_BOTH("tacacs", NULL, 0, (uint8_t *)"\xc1\x01\x01", 3, tacacs_classify);
    PARSERS_CLASSIFY_BOTH("tacacs", NULL, 0, (uint8_t *)"\xc1\x01\x02", 3, tacacs_classify);

    SIMPLE_CLASSIFY_TCP("flash-policy", "<policy-file-request/>");

    arkime_parsers_classifier_register_port("dropbox-lan-sync",  NULL, 17500, ARKIME_PARSERS_PORT_UDP, dropbox_lan_sync_classify);

    CLASSIFY_TCP("kafka", 0, "\x00\x00", kafka_classify);

    SIMPLE_CLASSIFY_UDP("steam-friends", "VS01");
    SIMPLE_CLASSIFY_UDP("valve-a2s", "\xff\xff\xff\xff\x54\x53\x6f\x75");
    SIMPLE_CLASSIFY_TCP("stream-ihscp", "\xa4\x00\x00\x00\x56\x54\x30\x31");

    SIMPLE_CLASSIFY_TCP("honeywell-tcc", "\x43\x42\x4b\x50\x50\x52\x05\x50");

    SIMPLE_CLASSIFY_TCP("pjl", "\x1b\x25\x2d\x31\x32\x33\x34\x35");
    SIMPLE_CLASSIFY_TCP("pjl", "\x40\x50\x4a\x4c\x20");

    SIMPLE_CLASSIFY_TCP("dcerpc", "\x05\x00\x0b");

    CLASSIFY_UDP("rip", 0, "\x01\x01\x00\x00", rip_classify);
    CLASSIFY_UDP("rip", 0, "\x01\x02\x00\x00", rip_classify);
    CLASSIFY_UDP("rip", 0, "\x02\x01\x00\x00", rip_classify);
    CLASSIFY_UDP("rip", 0, "\x02\x02\x00\x00", rip_classify);

    SIMPLE_CLASSIFY_TCP("nzsql", "\x00\x00\x00\x08\x00\x01\x00\x03");

    SIMPLE_CLASSIFY_TCP("splunk", "--splunk-cooked-mode");
    CLASSIFY_TCP("splunk-replication", 6, "\x00\x06\x00\x00\x00\x05_raw", misc_add_protocol_classify);

    arkime_parsers_classifier_register_port("isakmp",  NULL, 500, ARKIME_PARSERS_PORT_UDP, isakmp_udp_classify);
    arkime_parsers_classifier_register_port("isakmp",  NULL, 4500, ARKIME_PARSERS_PORT_UDP, isakmp_udp_classify);

    arkime_parsers_classifier_register_port("aruba-papi",  NULL, 8211, ARKIME_PARSERS_PORT_UDP, aruba_papi_udp_classify);

    SIMPLE_CLASSIFY_TCP("x11", "\x6c\x00\x0b\x00");

    SIMPLE_CLASSIFY_TCP("memcached", "flush_all");
    SIMPLE_CLASSIFY_TCP("memcached", "STORED\r\n");
    SIMPLE_CLASSIFY_TCP("memcached", "END\r\n");
    SIMPLE_CLASSIFY_TCP("memcached", "VALUE ");

    CLASSIFY_UDP("memcached", 6, "\x00\x00stats", misc_add_protocol_classify);
    CLASSIFY_UDP("memcached", 6, "\x00\x00gets ", misc_add_protocol_classify);

    CLASSIFY_UDP("netflow", 0, "\x00\x05", netflow_classify);
    CLASSIFY_UDP("netflow", 0, "\x00\x07", netflow_classify);
    CLASSIFY_UDP("netflow", 0, "\x00\x09", netflow_classify);

    SIMPLE_CLASSIFY_TCP("hbase", "HBas\x00");

    SIMPLE_CLASSIFY_TCP("hadoop", "hrpc\x09");

    CLASSIFY_TCP("hdfs", 0, "\x00\x1c\x50", hdfs_classify);
    CLASSIFY_TCP("hdfs", 0, "\x00\x1c\x51", hdfs_classify);
    CLASSIFY_TCP("hdfs", 0, "\x00\x1c\x55", hdfs_classify);

    SIMPLE_CLASSIFY_TCP("zookeeper", "zk_version");
    SIMPLE_CLASSIFY_TCP("zookeeper", "mntr\n");
    SIMPLE_CLASSIFY_TCP("zookeeper", "\x00\x00\x00\x2c\x00\x00\x00\x00");
    SIMPLE_CLASSIFY_TCP("zookeeper", "\x00\x00\x00\x2d\x00\x00\x00\x00");

    arkime_parsers_classifier_register_port("sccp",  NULL, 2000, ARKIME_PARSERS_PORT_TCP_DST, sccp_classify);

    arkime_parsers_classifier_register_port("wudo",  NULL, 7680, ARKIME_PARSERS_PORT_TCP_DST, wudo_classify);

    CLASSIFY_TCP("mqtt", 0, "\x10", mqtt_classify);

    arkime_parsers_classifier_register_port("hsrp",  NULL, 1985, ARKIME_PARSERS_PORT_UDP, hsrp_udp_classify);
    arkime_parsers_classifier_register_port("hsrp",  NULL, 2029, ARKIME_PARSERS_PORT_UDP, hsrp_udp_classify);

    SIMPLE_CLASSIFY_TCP("elasticsearch", "ES\x00\x00");

    arkime_parsers_classifier_register_port("safet",  NULL, 23294, ARKIME_PARSERS_PORT_UDP, safet_udp_classify);

    arkime_parsers_classifier_register_port("telnet",  NULL, 23, ARKIME_PARSERS_PORT_TCP_DST, telnet_tcp_classify);

    arkime_parsers_classifier_register_port("whois",  "whois", 43, ARKIME_PARSERS_PORT_TCP_DST, misc_add_protocol_classify);

    arkime_parsers_classifier_register_port("finger",  "finger", 79, ARKIME_PARSERS_PORT_TCP_DST, misc_add_protocol_classify);

    SIMPLE_CLASSIFY_TCP("rtsp", "RTSP/1.0 ");

    SIMPLE_CLASSIFY_TCP("rmi", "JRMI\x00");

    SIMPLE_CLASSIFY_BOTH("dnp3", "\x05\x64");

    userField = arkime_field_by_db("user");
}

