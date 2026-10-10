/* Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <inttypes.h>

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

extern ArkimeConfig_t        config;

LOCAL  int versionField;
LOCAL  int modeField;
LOCAL  int stratumField;
LOCAL  int refIdField;
LOCAL  int stratumNumField;
LOCAL  int pollField;
LOCAL  int precisionField;
LOCAL  int rootDelayField;
LOCAL  int rootDispersionField;
LOCAL  int timeFields[4];
LOCAL  int extensionsField;

#define NTP_UNIX_OFFSET 2208988800ULL

/******************************************************************************/
// NTP 64 bit timestamp to "unixseconds.microseconds", nothing is added for an unset (zero) timestamp
LOCAL void ntp_add_time(ArkimeSession_t *session, int field, BSB *bsb)
{
    uint32_t secs = 0, frac = 0;
    BSB_IMPORT_u32(*bsb, secs);
    BSB_IMPORT_u32(*bsb, frac);
    if (BSB_IS_ERROR(*bsb) || (secs == 0 && frac == 0))
        return;

    // Era 0 wraps in 2036, treat small values as era 1
    uint64_t unix = secs >= NTP_UNIX_OFFSET ? secs - NTP_UNIX_OFFSET : secs + (0x100000000ULL - NTP_UNIX_OFFSET);
    uint32_t usec = (uint32_t)(((uint64_t)frac * 1000000ULL) >> 32);
    char str[32];
    int len = snprintf(str, sizeof(str), "%" PRIu64 ".%06u", unix, usec);
    arkime_field_string_add(field, session, str, len, TRUE);
}

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
    if (mode < ARRAY_LEN(ntpModes) && ntpModes[mode]) {
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

    // Modes 6 and 7 (control, private) use a different layout after the first byte
    if (mode >= 6) {
        return 0;
    }

    arkime_field_int_add(stratumNumField, session, stratum);

    int8_t poll = 0, precision = 0;
    BSB_IMPORT_u08(bsb, poll);
    BSB_IMPORT_u08(bsb, precision);
    arkime_field_int_add(pollField, session, poll);
    arkime_field_int_add(precisionField, session, precision);

    // Root delay and dispersion are 16.16 fixed point seconds
    uint32_t rootDelay = 0, rootDispersion = 0;
    BSB_IMPORT_u32(bsb, rootDelay);
    BSB_IMPORT_u32(bsb, rootDispersion);
    arkime_field_float_add(rootDelayField, session, rootDelay / 65536.0f);
    arkime_field_float_add(rootDispersionField, session, rootDispersion / 65536.0f);

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
                    arkime_sprint_hex_string(refStr, refId, 4);
                    arkime_field_string_add(refIdField, session, refStr, 8, TRUE);
                }
            }
        } else if (stratum <= 15) {
            // For stratum 2+: IPv4 address of upstream server, or first 4 bytes of MD5 hash for IPv6
            // Display as hex since we can't distinguish IPv4 from IPv6 hash
            char refStr[12];
            arkime_sprint_hex_string(refStr, refId, 4);
            arkime_field_string_add(refIdField, session, refStr, 8, TRUE);
        }
        BSB_IMPORT_skip(bsb, 4);
    }

    // Reference, origin, receive and transmit timestamps
    for (int i = 0; i < 4 && BSB_REMAINING(bsb) >= 8; i++) {
        ntp_add_time(session, timeFields[i], &bsb);
    }

    // Extension fields (RFC 7822): u16 type, u16 length including the header, padded to 4 bytes,
    // minimum 16 bytes, anything shorter left over is the MAC
    int extensions = 0;
    while (BSB_REMAINING(bsb) >= 16) {
        const uint8_t *ext = BSB_WORK_PTR(bsb);
        uint32_t elen = (ext[2] << 8) | ext[3];
        if (elen < 16 || (elen & 3) || elen > BSB_REMAINING(bsb))
            break;
        BSB_IMPORT_skip(bsb, elen);
        extensions++;
    }
    arkime_field_int_add(extensionsField, session, extensions);

    return 0;
}
/******************************************************************************/
LOCAL void ntp_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "ntp"))
        return;

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

    stratumNumField = arkime_field_define("ntp", "integer",
                                          "ntp.stratumNum", "Stratum Number", "ntp.stratumNum",
                                          "NTP stratum as a number",
                                          ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                          (char *)NULL);

    pollField = arkime_field_define("ntp", "integer",
                                    "ntp.poll", "Poll", "ntp.poll",
                                    "NTP poll interval exponent, seconds are 2^poll",
                                    ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                    (char *)NULL);

    precisionField = arkime_field_define("ntp", "integer",
                                         "ntp.precision", "Precision", "ntp.precision",
                                         "NTP clock precision exponent, seconds are 2^precision",
                                         ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                         (char *)NULL);

    rootDelayField = arkime_field_define("ntp", "float",
                                         "ntp.rootDelay", "Root Delay", "ntp.rootDelay",
                                         "NTP root delay in seconds",
                                         ARKIME_FIELD_TYPE_FLOAT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                         (char *)NULL);

    rootDispersionField = arkime_field_define("ntp", "float",
                                              "ntp.rootDispersion", "Root Dispersion", "ntp.rootDispersion",
                                              "NTP root dispersion in seconds",
                                              ARKIME_FIELD_TYPE_FLOAT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                              (char *)NULL);

    timeFields[0] = arkime_field_define("ntp", "termfield",
                                        "ntp.refTime", "Reference Time", "ntp.refTime",
                                        "NTP reference timestamp as unix seconds.microseconds",
                                        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);

    timeFields[1] = arkime_field_define("ntp", "termfield",
                                        "ntp.orgTime", "Origin Time", "ntp.orgTime",
                                        "NTP origin timestamp as unix seconds.microseconds",
                                        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);

    timeFields[2] = arkime_field_define("ntp", "termfield",
                                        "ntp.recTime", "Receive Time", "ntp.recTime",
                                        "NTP receive timestamp as unix seconds.microseconds",
                                        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);

    timeFields[3] = arkime_field_define("ntp", "termfield",
                                        "ntp.xmtTime", "Transmit Time", "ntp.xmtTime",
                                        "NTP transmit timestamp as unix seconds.microseconds",
                                        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);

    extensionsField = arkime_field_define("ntp", "integer",
                                          "ntp.extensions", "Extension Fields", "ntp.extensions",
                                          "Number of NTP extension fields in a packet",
                                          ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                          (char *)NULL);

    // NTP uses UDP port 123. Register by port and let ntp_udp_classify
    // validate version/mode/stratum so all valid first-byte combinations
    // are covered (LI 0-3 x VN 1-4 x Mode 1-7).
    arkime_parsers_classifier_register_port("ntp", NULL, 123, ARKIME_PARSERS_PORT_UDP, ntp_udp_classify);
}
