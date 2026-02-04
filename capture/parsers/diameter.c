/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL int originHostField;
LOCAL int originRealmField;
LOCAL int destHostField;
LOCAL int destRealmField;
LOCAL int sessionIdField;
LOCAL int userNameField;
LOCAL int cmdCodeField;
LOCAL int appIdField;

// Common Diameter command codes (RFC 6733)
LOCAL const char *diameter_cmd_name(uint32_t code)
{
    switch (code) {
    case 257:
        return "Capabilities-Exchange";
    case 258:
        return "Re-Auth";
    case 271:
        return "Accounting";
    case 272:
        return "Credit-Control";
    case 274:
        return "Abort-Session";
    case 275:
        return "Session-Termination";
    case 280:
        return "Device-Watchdog";
    case 282:
        return "Disconnect-Peer";
    // 3GPP commands
    case 300:
        return "User-Authorization";
    case 301:
        return "Server-Assignment";
    case 302:
        return "Location-Info";
    case 303:
        return "Multimedia-Auth";
    case 304:
        return "Registration-Termination";
    case 305:
        return "Push-Profile";
    case 306:
        return "User-Data";
    case 307:
        return "Profile-Update";
    case 308:
        return "Subscribe-Notifications";
    case 309:
        return "Push-Notification";
    case 316:
        return "Update-Location";
    case 317:
        return "Cancel-Location";
    case 318:
        return "Authentication-Information";
    case 319:
        return "Insert-Subscriber-Data";
    case 320:
        return "Delete-Subscriber-Data";
    case 321:
        return "Purge-UE";
    case 322:
        return "Reset";
    case 323:
        return "Notify";
    default:
        return NULL;
    }
}

// Common Diameter application IDs
LOCAL const char *diameter_app_name(uint32_t appId)
{
    switch (appId) {
    case 0:
        return "Common";
    case 1:
        return "NASREQ";
    case 2:
        return "Mobile-IPv4";
    case 3:
        return "Base-Accounting";
    case 4:
        return "Credit-Control";
    case 5:
        return "EAP";
    case 6:
        return "SIP";
    case 7:
        return "MIP6I";
    case 8:
        return "MIP6A";
    case 9:
        return "QoS";
    case 0xffffffff:
        return "Relay";
    // 3GPP applications
    case 16777216:
        return "3GPP-Cx";
    case 16777217:
        return "3GPP-Sh";
    case 16777236:
        return "3GPP-Rx";
    case 16777238:
        return "3GPP-Gx";
    case 16777251:
        return "3GPP-S6a";
    case 16777252:
        return "3GPP-S13";
    case 16777255:
        return "3GPP-SLg";
    case 16777265:
        return "3GPP-SWm";
    case 16777272:
        return "3GPP-SWx";
    case 16777291:
        return "3GPP-S6b";
    default:
        return NULL;
    }
}

/******************************************************************************/
// Parse AVPs from Diameter message
LOCAL void diameter_parse_avps(ArkimeSession_t *session, const uint8_t *data, int len)
{
    BSB bsb;
    BSB_INIT(bsb, data, len);

    while (BSB_REMAINING(bsb) >= 8) {
        uint32_t avpCode = 0;
        uint8_t flags = 0;
        uint32_t avpLen = 0;

        BSB_IMPORT_u32(bsb, avpCode);
        BSB_IMPORT_u08(bsb, flags);

        // AVP Length is 3 bytes
        uint8_t lenBytes[3];
        BSB_IMPORT_u08(bsb, lenBytes[0]);
        BSB_IMPORT_u08(bsb, lenBytes[1]);
        BSB_IMPORT_u08(bsb, lenBytes[2]);
        avpLen = (lenBytes[0] << 16) | (lenBytes[1] << 8) | lenBytes[2];

        if (avpLen < 8 || BSB_IS_ERROR(bsb))
            break;

        // Skip Vendor-ID if V flag is set
        uint32_t dataLen = avpLen - 8;
        if (flags & 0x80) {
            if (avpLen < 12)
                break;
            BSB_IMPORT_skip(bsb, 4);
            dataLen = avpLen - 12;
        }

        if (BSB_REMAINING(bsb) < (int)dataLen)
            break;

        const uint8_t *avpData = BSB_WORK_PTR(bsb);
        BSB_IMPORT_skip(bsb, dataLen);

        // AVPs are padded to 4-byte boundary
        uint32_t padding = (4 - (avpLen & 3)) & 3;
        BSB_IMPORT_skip(bsb, padding);

        switch (avpCode) {
        case 1:   // User-Name
            arkime_field_string_add(userNameField, session, (char *)avpData, dataLen, TRUE);
            break;
        case 263: // Session-Id
            arkime_field_string_add(sessionIdField, session, (char *)avpData, dataLen, TRUE);
            break;
        case 264: // Origin-Host
            arkime_field_string_add(originHostField, session, (char *)avpData, dataLen, TRUE);
            break;
        case 283: // Destination-Realm
            arkime_field_string_add(destRealmField, session, (char *)avpData, dataLen, TRUE);
            break;
        case 293: // Destination-Host
            arkime_field_string_add(destHostField, session, (char *)avpData, dataLen, TRUE);
            break;
        case 296: // Origin-Realm
            arkime_field_string_add(originRealmField, session, (char *)avpData, dataLen, TRUE);
            break;
        }
    }
}

/******************************************************************************/
LOCAL int diameter_tcp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    // Diameter header is 20 bytes
    if (len < 20)
        return 0;

    BSB bsb;
    BSB_INIT(bsb, data, len);

    while (BSB_REMAINING(bsb) >= 20) {
        const uint8_t *msgStart = BSB_WORK_PTR(bsb);

        // Version (1 byte)
        uint8_t version = 0;
        BSB_IMPORT_u08(bsb, version);
        if (version != 1)
            return ARKIME_PARSER_UNREGISTER;

        // Message Length (3 bytes)
        uint8_t lenBytes[3];
        BSB_IMPORT_u08(bsb, lenBytes[0]);
        BSB_IMPORT_u08(bsb, lenBytes[1]);
        BSB_IMPORT_u08(bsb, lenBytes[2]);
        uint32_t msgLen = (lenBytes[0] << 16) | (lenBytes[1] << 8) | lenBytes[2];

        if (msgLen < 20 || msgLen > 16777215)
            return ARKIME_PARSER_UNREGISTER;

        // Check if we have the full message
        if ((int)msgLen > len - (int)(msgStart - data))
            return 0; // Need more data

        // Command Flags (1 byte) + Command Code (3 bytes)
        uint8_t cmdFlags = 0;
        BSB_IMPORT_u08(bsb, cmdFlags);

        uint8_t cmdBytes[3];
        BSB_IMPORT_u08(bsb, cmdBytes[0]);
        BSB_IMPORT_u08(bsb, cmdBytes[1]);
        BSB_IMPORT_u08(bsb, cmdBytes[2]);
        uint32_t cmdCode = (cmdBytes[0] << 16) | (cmdBytes[1] << 8) | cmdBytes[2];

        // Application-ID (4 bytes)
        uint32_t appId = 0;
        BSB_IMPORT_u32(bsb, appId);

        // Skip Hop-by-Hop and End-to-End identifiers (8 bytes)
        BSB_IMPORT_skip(bsb, 8);

        if (BSB_IS_ERROR(bsb))
            return ARKIME_PARSER_UNREGISTER;

        // R flag (bit 7) indicates request vs answer
        const char *reqAns = (cmdFlags & 0x80) ? "Request" : "Answer";

        // Add command code with name if known
        const char *cmdName = diameter_cmd_name(cmdCode);
        if (cmdName) {
            char cmdStr[80];
            snprintf(cmdStr, sizeof(cmdStr), "%s-%s (%u)", cmdName, reqAns, cmdCode);
            arkime_field_string_add(cmdCodeField, session, cmdStr, -1, TRUE);
        } else {
            char cmdStr[32];
            snprintf(cmdStr, sizeof(cmdStr), "%s (%u)", reqAns, cmdCode);
            arkime_field_string_add(cmdCodeField, session, cmdStr, -1, TRUE);
        }

        // Add application ID with name if known
        const char *appName = diameter_app_name(appId);
        if (appName) {
            char appStr[64];
            snprintf(appStr, sizeof(appStr), "%s (%u)", appName, appId);
            arkime_field_string_add(appIdField, session, appStr, -1, TRUE);
        } else {
            char appStr[16];
            snprintf(appStr, sizeof(appStr), "%u", appId);
            arkime_field_string_add(appIdField, session, appStr, -1, TRUE);
        }

        // Parse AVPs (remaining bytes in message)
        int avpLen = msgLen - 20;
        if (avpLen > 0 && BSB_REMAINING(bsb) >= avpLen) {
            diameter_parse_avps(session, BSB_WORK_PTR(bsb), avpLen);
        }

        // Move to next message
        BSB_INIT(bsb, msgStart + msgLen, len - (int)(msgStart - data) - msgLen);
    }

    return 0;
}

/******************************************************************************/
// Common diameter validation used by both TCP and SCTP classifiers
LOCAL int diameter_validate(const uint8_t *data, int len)
{
    // Diameter header: Version (1) + Length (3) + Flags (1) + Code (3) + AppId (4) + H2H (4) + E2E (4) = 20 bytes
    if (len < 20)
        return 0;

    // Version must be 1
    if (data[0] != 1)
        return 0;

    // Message length (3 bytes, big-endian)
    uint32_t msgLen = (data[1] << 16) | (data[2] << 8) | data[3];

    // Length must be at least 20 and reasonable
    if (msgLen < 20 || msgLen > 16777215)
        return 0;

    // Command flags - check reserved bits are zero
    uint8_t flags = data[4];
    if (flags & 0x0f)
        return 0;

    // Command code must be non-zero
    uint32_t cmdCode = (data[5] << 16) | (data[6] << 8) | data[7];
    if (cmdCode == 0)
        return 0;

    return 1;
}

/******************************************************************************/
LOCAL void diameter_tcp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (!diameter_validate(data, len))
        return;

    arkime_session_add_protocol(session, "diameter");
    arkime_parsers_register(session, diameter_tcp_parser, 0, 0);
}

/******************************************************************************/
LOCAL void diameter_sctp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (!diameter_validate(data, len))
        return;

    arkime_session_add_protocol(session, "diameter");
    arkime_parsers_register(session, diameter_tcp_parser, 0, 0);
}

/******************************************************************************/
void arkime_parser_init()
{
    originHostField = arkime_field_define("diameter", "termfield",
                                          "diameter.origin-host", "Origin Host", "diameter.originHost",
                                          "Diameter Origin-Host AVP",
                                          ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                          (char *)NULL);

    originRealmField = arkime_field_define("diameter", "termfield",
                                           "diameter.origin-realm", "Origin Realm", "diameter.originRealm",
                                           "Diameter Origin-Realm AVP",
                                           ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                           (char *)NULL);

    destHostField = arkime_field_define("diameter", "termfield",
                                        "diameter.dest-host", "Dest Host", "diameter.destHost",
                                        "Diameter Destination-Host AVP",
                                        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);

    destRealmField = arkime_field_define("diameter", "termfield",
                                         "diameter.dest-realm", "Dest Realm", "diameter.destRealm",
                                         "Diameter Destination-Realm AVP",
                                         ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                         (char *)NULL);

    sessionIdField = arkime_field_define("diameter", "termfield",
                                         "diameter.session-id", "Session ID", "diameter.sessionId",
                                         "Diameter Session-Id AVP",
                                         ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                         (char *)NULL);

    userNameField = arkime_field_define("diameter", "termfield",
                                        "diameter.user", "User", "diameter.user",
                                        "Diameter User-Name AVP",
                                        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                        "category", "user",
                                        (char *)NULL);

    cmdCodeField = arkime_field_define("diameter", "termfield",
                                       "diameter.cmd", "Command", "diameter.cmd",
                                       "Diameter Command Code",
                                       ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    appIdField = arkime_field_define("diameter", "termfield",
                                     "diameter.app", "Application", "diameter.app",
                                     "Diameter Application ID",
                                     ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                     (char *)NULL);

    // Register for TCP port 3868
    arkime_parsers_classifier_register_port("diameter", NULL, 3868, ARKIME_PARSERS_PORT_TCP, diameter_tcp_classify);
    // Also register for TLS/TCP port 5658
    arkime_parsers_classifier_register_port("diameter", NULL, 5658, ARKIME_PARSERS_PORT_TCP, diameter_tcp_classify);

    // Register for SCTP port 3868 - Diameter commonly uses SCTP
    arkime_parsers_classifier_register_port("diameter", NULL, 3868, ARKIME_PARSERS_PORT_SCTP, diameter_sctp_classify);
    arkime_parsers_classifier_register_port("diameter", NULL, 5658, ARKIME_PARSERS_PORT_SCTP, diameter_sctp_classify);
}
