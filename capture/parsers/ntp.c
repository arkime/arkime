/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

/*
 * NTP (Network Time Protocol) - RFC 5905
 *
 * NTP Packet (48 bytes minimum):
 *   Byte 0: LI (2 bits) | VN (3 bits) | Mode (3 bits)
 *   Byte 1: Stratum
 *   Byte 2: Poll
 *   Byte 3: Precision
 *   Bytes 4-7: Root Delay
 *   Bytes 8-11: Root Dispersion
 *   Bytes 12-15: Reference ID
 *   Bytes 16-23: Reference Timestamp
 *   Bytes 24-31: Origin Timestamp
 *   Bytes 32-39: Receive Timestamp
 *   Bytes 40-47: Transmit Timestamp
 */

// NTP Modes
#define NTP_MODE_RESERVED       0
#define NTP_MODE_SYMM_ACTIVE    1
#define NTP_MODE_SYMM_PASSIVE   2
#define NTP_MODE_CLIENT         3
#define NTP_MODE_SERVER         4
#define NTP_MODE_BROADCAST      5
#define NTP_MODE_CONTROL        6
#define NTP_MODE_PRIVATE        7

LOCAL const char *ntpModes[] = {
    [0] = "reserved",
    [1] = "symmetric-active",
    [2] = "symmetric-passive",
    [3] = "client",
    [4] = "server",
    [5] = "broadcast",
    [6] = "control",
    [7] = "private"
};

// Stratum values
#define NTP_STRATUM_UNSPEC      0
#define NTP_STRATUM_PRIMARY     1
#define NTP_STRATUM_SECONDARY   2   // 2-15
#define NTP_STRATUM_UNSYNCED    16

extern ArkimeConfig_t        config;
LOCAL  int versionField;
LOCAL  int modeField;
LOCAL  int stratumField;
LOCAL  int refIdField;

/******************************************************************************/
LOCAL int ntp_udp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    BSB bsb;
    BSB_INIT(bsb, data, len);

    if (BSB_REMAINING(bsb) < 48)
        return 0;

    uint8_t flags = 0;
    BSB_IMPORT_u08(bsb, flags);

    // Extract LI, VN, Mode from flags byte
    uint8_t version = (flags >> 3) & 0x07;
    uint8_t mode = flags & 0x07;

    arkime_field_int_add(versionField, session, version);

    // Add mode
    if (mode < sizeof(ntpModes) / sizeof(ntpModes[0]) && ntpModes[mode]) {
        arkime_field_string_add(modeField, session, ntpModes[mode], -1, TRUE);
    }

    // Stratum
    uint8_t stratum = 0;
    BSB_IMPORT_u08(bsb, stratum);

    if (stratum == 0) {
        arkime_field_string_add(stratumField, session, "unspecified", -1, TRUE);
    } else if (stratum == 1) {
        arkime_field_string_add(stratumField, session, "primary", -1, TRUE);
    } else if (stratum <= 15) {
        arkime_field_string_add(stratumField, session, "secondary", -1, TRUE);
    } else if (stratum == 16) {
        arkime_field_string_add(stratumField, session, "unsynchronized", -1, TRUE);
    }

    // Skip poll and precision
    BSB_IMPORT_skip(bsb, 2);

    // Skip root delay and root dispersion
    BSB_IMPORT_skip(bsb, 8);

    // Reference ID (4 bytes)
    // For stratum 0-1: 4-character ASCII string (e.g., "GPS", "PPS", "LOCL")
    // For stratum 2+: IPv4 address or hash of IPv6
    if (BSB_REMAINING(bsb) >= 4) {
        const uint8_t *refId = BSB_WORK_PTR(bsb);

        if (stratum <= 1) {
            // ASCII reference clock identifier - trim trailing nulls/spaces
            int refLen = 4;
            while (refLen > 0 && (refId[refLen - 1] == 0 || refId[refLen - 1] == ' ')) {
                refLen--;
            }
            if (refLen > 0) {
                // Check if printable ASCII
                int printable = 1;
                for (int i = 0; i < refLen; i++) {
                    if (refId[i] < 0x20 || refId[i] > 0x7e) {
                        printable = 0;
                        break;
                    }
                }
                if (printable) {
                    arkime_field_string_add(refIdField, session, (const char *)refId, refLen, TRUE);
                } else {
                    char refStr[12];
                    snprintf(refStr, sizeof(refStr), "%02x%02x%02x%02x", refId[0], refId[1], refId[2], refId[3]);
                    arkime_field_string_add(refIdField, session, refStr, -1, TRUE);
                }
            }
        } else if (stratum >= 2 && stratum <= 15) {
            // For stratum 2+: IPv4 address of upstream server, or first 4 bytes of MD5 hash for IPv6
            // Display as hex since we can't distinguish IPv4 from IPv6 hash
            char refStr[12];
            snprintf(refStr, sizeof(refStr), "%02x%02x%02x%02x", refId[0], refId[1], refId[2], refId[3]);
            arkime_field_string_add(refIdField, session, refStr, -1, TRUE);
        }
    }

    return 0;
}
/******************************************************************************/
LOCAL void ntp_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    // Check port 123
    if (session->port1 != 123 && session->port2 != 123)
        return;

    // Minimum NTP packet size
    if (len < 48)
        return;

    // Validate stratum (byte 1) - must be <= 16
    if (data[1] > 16)
        return;

    // Extract version and mode from byte 0
    uint8_t version = (data[0] >> 3) & 0x07;
    uint8_t mode = data[0] & 0x07;

    // Valid NTP versions are 1-4
    if (version < 1 || version > 4)
        return;

    // Mode 0 is reserved and shouldn't be used
    if (mode == 0)
        return;

    arkime_session_add_protocol(session, "ntp");
    arkime_parsers_register(session, ntp_udp_parser, 0, 0);
}
/******************************************************************************/
void arkime_parser_init()
{
    versionField = arkime_field_define("ntp", "integer",
                                       "ntp.version", "Version", "ntp.version",
                                       "NTP version",
                                       ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    modeField = arkime_field_define("ntp", "termfield",
                                    "ntp.mode", "Mode", "ntp.mode",
                                    "NTP mode",
                                    ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                    (char *)NULL);

    stratumField = arkime_field_define("ntp", "termfield",
                                       "ntp.stratum", "Stratum", "ntp.stratum",
                                       "NTP stratum",
                                       ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    refIdField = arkime_field_define("ntp", "termfield",
                                     "ntp.ref-id", "Reference ID", "ntp.refId",
                                     "NTP reference identifier",
                                     ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                     (char *)NULL);

    // NTP uses UDP port 123
    // First byte encodes LI (2 bits), Version (3 bits), Mode (3 bits)
    // Common patterns: 0x1b (v3 client), 0x23 (v4 client), 0x24 (v4 server), 0xe3 (v4 broadcast)
    arkime_parsers_classifier_register_udp("ntp", NULL, 0, (const uint8_t *)"\x13", 1, ntp_udp_classify);
    arkime_parsers_classifier_register_udp("ntp", NULL, 0, (const uint8_t *)"\x19", 1, ntp_udp_classify);
    arkime_parsers_classifier_register_udp("ntp", NULL, 0, (const uint8_t *)"\x1a", 1, ntp_udp_classify);
    arkime_parsers_classifier_register_udp("ntp", NULL, 0, (const uint8_t *)"\x1b", 1, ntp_udp_classify);
    arkime_parsers_classifier_register_udp("ntp", NULL, 0, (const uint8_t *)"\x1c", 1, ntp_udp_classify);
    arkime_parsers_classifier_register_udp("ntp", NULL, 0, (const uint8_t *)"\x21", 1, ntp_udp_classify);
    arkime_parsers_classifier_register_udp("ntp", NULL, 0, (const uint8_t *)"\x23", 1, ntp_udp_classify);
    arkime_parsers_classifier_register_udp("ntp", NULL, 0, (const uint8_t *)"\x24", 1, ntp_udp_classify);
    arkime_parsers_classifier_register_udp("ntp", NULL, 0, (const uint8_t *)"\xd9", 1, ntp_udp_classify);
    arkime_parsers_classifier_register_udp("ntp", NULL, 0, (const uint8_t *)"\xdb", 1, ntp_udp_classify);
    arkime_parsers_classifier_register_udp("ntp", NULL, 0, (const uint8_t *)"\xe3", 1, ntp_udp_classify);
}
