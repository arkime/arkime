/* enip.c
 *
 * Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * EtherNet/IP (ENIP) and Common Industrial Protocol (CIP) parser.
 *
 * ENIP runs on TCP/UDP 44818 (explicit messaging) and UDP 2222 (implicit/IO).
 * The encapsulation header is 24 bytes, little-endian:
 *   uint16 command, uint16 length, uint32 session_handle, uint32 status,
 *   uint64 sender_context, uint32 options
 *
 * This first version decodes the encapsulation header for every packet and
 * extracts a small number of CIP details (service code + class id) from
 * SendRRData / SendUnitData, plus the product name and identity attributes
 * from a ListIdentity response. It deliberately does not try to decode every
 * CIP object class.
 */
#include <string.h>
#include "arkimeconfig.h"
#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL  int commandField;
LOCAL  int statusField;
LOCAL  int serviceField;
LOCAL  int classField;
LOCAL  int vendorField;
LOCAL  int productField;
LOCAL  int deviceTypeField;

#define ENIP_HEADER_LEN 24

/******************************************************************************/
LOCAL const char *enip_command_name(uint16_t cmd)
{
    switch (cmd) {
    case 0x0000:
        return "NOP";
    case 0x0004:
        return "ListServices";
    case 0x0063:
        return "ListIdentity";
    case 0x0064:
        return "ListInterfaces";
    case 0x0065:
        return "RegisterSession";
    case 0x0066:
        return "UnRegisterSession";
    case 0x006F:
        return "SendRRData";
    case 0x0070:
        return "SendUnitData";
    case 0x0072:
        return "IndicateStatus";
    case 0x0073:
        return "Cancel";
    default:
        return NULL;
    }
}
/******************************************************************************/
LOCAL int enip_command_known(uint16_t cmd)
{
    return enip_command_name(cmd) != NULL;
}
/******************************************************************************/
LOCAL const char *cip_service_name(uint8_t svc)
{
    /* mask off response bit before lookup */
    switch (svc & 0x7f) {
    case 0x01:
        return "Get_Attributes_All";
    case 0x02:
        return "Set_Attributes_All";
    case 0x03:
        return "Get_Attribute_List";
    case 0x04:
        return "Set_Attribute_List";
    case 0x05:
        return "Reset";
    case 0x06:
        return "Start";
    case 0x07:
        return "Stop";
    case 0x08:
        return "Create";
    case 0x09:
        return "Delete";
    case 0x0A:
        return "Multiple_Service_Packet";
    case 0x0D:
        return "Apply_Attributes";
    case 0x0E:
        return "Get_Attribute_Single";
    case 0x10:
        return "Set_Attribute_Single";
    case 0x11:
        return "Find_Next_Object_Instance";
    case 0x14:
        return "Error_Response";
    case 0x15:
        return "Restore";
    case 0x16:
        return "Save";
    case 0x17:
        return "NoOp";
    case 0x18:
        return "Get_Member";
    case 0x19:
        return "Set_Member";
    case 0x1A:
        return "Insert_Member";
    case 0x1B:
        return "Remove_Member";
    case 0x1C:
        return "GroupSync";
    case 0x4B:
        return "Execute_PCCC";
    case 0x4C:
        return "Read_Tag";
    case 0x4D:
        return "Write_Tag";
    case 0x4E:
        return "Forward_Close";
    case 0x52:
        return "Read_Tag_Fragmented";
    case 0x53:
        return "Write_Tag_Fragmented";
    case 0x54:
        return "Forward_Open";
    case 0x55:
        return "Get_Connection_Owner";
    case 0x5B:
        return "Large_Forward_Open";
    default:
        return NULL;
    }
}
/******************************************************************************/
LOCAL const char *cip_class_name(uint32_t cls)
{
    switch (cls) {
    case 0x01:
        return "Identity";
    case 0x02:
        return "Message Router";
    case 0x03:
        return "DeviceNet";
    case 0x04:
        return "Assembly";
    case 0x05:
        return "Connection";
    case 0x06:
        return "Connection Manager";
    case 0x07:
        return "Register";
    case 0x08:
        return "Discrete Input Point";
    case 0x09:
        return "Discrete Output Point";
    case 0x0A:
        return "Analog Input Point";
    case 0x0B:
        return "Analog Output Point";
    case 0x0E:
        return "Presence Sensing";
    case 0x0F:
        return "Parameter";
    case 0x10:
        return "Parameter Group";
    case 0x1D:
        return "Discrete Input Group";
    case 0x1E:
        return "Discrete Output Group";
    case 0x1F:
        return "Discrete Group";
    case 0x20:
        return "Analog Input Group";
    case 0x21:
        return "Analog Output Group";
    case 0x22:
        return "Analog Group";
    case 0x23:
        return "Position Sensor";
    case 0x24:
        return "Position Controller Supervisor";
    case 0x25:
        return "Position Controller";
    case 0x26:
        return "Block Sequencer";
    case 0x27:
        return "Command Block";
    case 0x28:
        return "Motor Data";
    case 0x29:
        return "Control Supervisor";
    case 0x2A:
        return "AC/DC Drive";
    case 0x2B:
        return "Acknowledge Handler";
    case 0x2C:
        return "Overload";
    case 0x2D:
        return "Softstart";
    case 0x2E:
        return "Selection";
    case 0x30:
        return "S-Device Supervisor";
    case 0x31:
        return "S-Analog Sensor";
    case 0x32:
        return "S-Analog Actuator";
    case 0x33:
        return "S-Single Stage Controller";
    case 0x34:
        return "S-Gas Calibration";
    case 0x35:
        return "Trip Point";
    case 0x37:
        return "File";
    case 0x38:
        return "S-Partial Pressure";
    case 0x39:
        return "Safety Supervisor";
    case 0x3A:
        return "Safety Validator";
    case 0x3B:
        return "Safety Discrete Output Point";
    case 0x3C:
        return "Safety Discrete Output Group";
    case 0x3D:
        return "Safety Discrete Input Point";
    case 0x3E:
        return "Safety Discrete Input Group";
    case 0x3F:
        return "Safety Dual Channel Output";
    case 0x40:
        return "S-Sensor Calibration";
    case 0x41:
        return "Event Log";
    case 0x42:
        return "Motion Device Axis";
    case 0x43:
        return "Time Sync";
    case 0x44:
        return "Modbus";
    case 0xF0:
        return "ControlNet";
    case 0xF1:
        return "ControlNet Keeper";
    case 0xF2:
        return "ControlNet Scheduling";
    case 0xF3:
        return "Connection Configuration";
    case 0xF4:
        return "Port";
    case 0xF5:
        return "TCP/IP Interface";
    case 0xF6:
        return "EtherNet Link";
    case 0xF7:
        return "CompoNet";
    case 0xF8:
        return "CompoNet Repeater";
    default:
        return NULL;
    }
}
/******************************************************************************/
LOCAL const char *enip_vendor_name(uint16_t v)
{
    switch (v) {
    case 0x0001:
        return "Rockwell Automation/Allen-Bradley";
    case 0x0002:
        return "Namco Controls Corp.";
    case 0x0005:
        return "Schneider Electric";
    case 0x000C:
        return "Festo";
    case 0x0017:
        return "OMRON";
    case 0x001A:
        return "Hitachi";
    case 0x002A:
        return "Mitsubishi Electric";
    case 0x0031:
        return "Siemens";
    case 0x0036:
        return "Bosch Rexroth";
    case 0x004D:
        return "Lenze";
    case 0x005A:
        return "Beckhoff";
    case 0x0064:
        return "Yaskawa";
    case 0x0090:
        return "Pilz";
    case 0x0100:
        return "Phoenix Contact";
    case 0x011B:
        return "WAGO";
    case 0x012E:
        return "Rockwell Automation";
    default:
        return NULL;
    }
}
/******************************************************************************/
/* Decode a CIP request path's first class segment.
 * path is in 16-bit words; segments are byte-oriented.
 * Returns class id or 0xffffffff if not found. */
LOCAL uint32_t cip_extract_class(const uint8_t *path, int pathBytes)
{
    BSB bsb;
    BSB_INIT(bsb, path, pathBytes);

    while (BSB_REMAINING(bsb) > 0 && !BSB_IS_ERROR(bsb)) {
        uint8_t seg = 0;
        BSB_IMPORT_u08(bsb, seg);
        uint8_t segType = seg & 0xE0;     // top 3 bits
        uint8_t logFmt  = seg & 0x03;     // bottom 2 for logical
        uint8_t logKind = seg & 0x1C;     // logical type bits

        if (segType == 0x20) {            // Logical Segment
            if (logKind == 0x00) {        // Class ID
                if (logFmt == 0x00) {     // 8-bit
                    uint8_t v = 0;
                    BSB_IMPORT_u08(bsb, v);
                    return v;
                } else if (logFmt == 0x01) { // 16-bit (skip pad)
                    BSB_IMPORT_skip(bsb, 1);
                    uint16_t v = 0;
                    BSB_LIMPORT_u16(bsb, v);
                    return v;
                } else if (logFmt == 0x02) { // 32-bit
                    BSB_IMPORT_skip(bsb, 1);
                    uint32_t v = 0;
                    BSB_LIMPORT_u32(bsb, v);
                    return v;
                } else {
                    return 0xffffffff;
                }
            } else {
                /* skip non-class logical segment */
                if (logFmt == 0x00) {
                    BSB_IMPORT_skip(bsb, 1);
                } else if (logFmt == 0x01) {
                    BSB_IMPORT_skip(bsb, 3);
                } else if (logFmt == 0x02) {
                    BSB_IMPORT_skip(bsb, 5);
                } else {
                    return 0xffffffff;
                }
            }
        } else if (segType == 0x80) {     // Data Segment (e.g. ANSI extended symbol)
            uint8_t dlen = 0;
            BSB_IMPORT_u08(bsb, dlen);
            int skip = dlen + (dlen & 1);
            BSB_IMPORT_skip(bsb, skip);
        } else if (segType == 0x00) {     // Port segment
            uint8_t linkLen = (seg & 0x0F) ? 1 : 0; /* simplified */
            BSB_IMPORT_skip(bsb, linkLen);
        } else {
            /* unknown / unsupported segment, stop */
            return 0xffffffff;
        }
    }
    return 0xffffffff;
}
/******************************************************************************/
LOCAL void enip_record_cip_service(ArkimeSession_t *session, uint8_t svcByte, const uint8_t *path, int pathBytes)
{
    if (!arkime_session_has_protocol(session, "cip")) {
        arkime_session_add_protocol(session, "cip");
    }

    const char *svcName = cip_service_name(svcByte);
    if (svcName) {
        arkime_field_string_add(serviceField, session, svcName, -1, TRUE);
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "Service-0x%02x", svcByte & 0x7f);
        arkime_field_string_add(serviceField, session, buf, -1, TRUE);
    }

    if (path && pathBytes > 0) {
        uint32_t cls = cip_extract_class(path, pathBytes);
        if (cls != 0xffffffff) {
            const char *cname = cip_class_name(cls);
            if (cname) {
                arkime_field_string_add(classField, session, cname, -1, TRUE);
            } else {
                char buf[32];
                snprintf(buf, sizeof(buf), "Class-0x%02x", cls);
                arkime_field_string_add(classField, session, buf, -1, TRUE);
            }
        }
    }
}
/******************************************************************************/
/* Parse the body of a SendRRData / SendUnitData command.
 * Layout: uint32 interface_handle, uint16 timeout, then CPF:
 *   uint16 item_count, then item_count * { uint16 type, uint16 length, data[length] }.
 * For Connected (0xB1) / Unconnected (0xB2) Data items we look for the CIP
 * service byte and request path. Connected data items start with a 2-byte
 * sequence number that we skip.
 */
LOCAL void enip_parse_send_data(ArkimeSession_t *session, BSB *bsb)
{
    BSB_IMPORT_skip(*bsb, 4);   // interface handle
    BSB_IMPORT_skip(*bsb, 2);   // timeout

    uint16_t itemCount = 0;
    BSB_LIMPORT_u16(*bsb, itemCount);

    if (BSB_IS_ERROR(*bsb) || itemCount == 0 || itemCount > 16) {
        return;
    }

    for (uint16_t i = 0; i < itemCount && !BSB_IS_ERROR(*bsb); i++) {
        uint16_t itemType = 0, itemLen = 0;
        BSB_LIMPORT_u16(*bsb, itemType);
        BSB_LIMPORT_u16(*bsb, itemLen);

        if (BSB_IS_ERROR(*bsb) || itemLen > BSB_REMAINING(*bsb)) {
            return;
        }

        const uint8_t *itemData = BSB_WORK_PTR(*bsb);
        BSB_IMPORT_skip(*bsb, itemLen);

        if (itemType == 0xB1 || itemType == 0xB2) {
            BSB ibsb;
            BSB_INIT(ibsb, itemData, itemLen);
            if (itemType == 0xB1) {
                /* connected data item starts with sequence number */
                BSB_IMPORT_skip(ibsb, 2);
            }
            uint8_t svc = 0;
            BSB_IMPORT_u08(ibsb, svc);
            if (BSB_IS_ERROR(ibsb)) {
                continue;
            }

            if (svc & 0x80) {
                /* response: service + reserved + status + size */
                arkime_field_string_add(serviceField, session,
                                        cip_service_name(svc) ? cip_service_name(svc) : "Reply",
                                        -1, TRUE);
                if (!arkime_session_has_protocol(session, "cip")) {
                    arkime_session_add_protocol(session, "cip");
                }
            } else {
                uint8_t pathSize = 0; // in 16-bit words
                BSB_IMPORT_u08(ibsb, pathSize);
                int pathBytes = pathSize * 2;
                if (pathBytes > BSB_REMAINING(ibsb)) {
                    pathBytes = BSB_REMAINING(ibsb);
                }
                const uint8_t *path = BSB_WORK_PTR(ibsb);
                enip_record_cip_service(session, svc, path, pathBytes);
            }
        }
    }
}
/******************************************************************************/
/* Parse a ListIdentity response body. Layout is a CPF list with one item of
 * type 0x000C whose data starts with: uint16 protocol_version, sockaddr_in (16
 * bytes), uint16 vendor_id, uint16 device_type, uint16 product_code, uint8
 * revision_major, uint8 revision_minor, uint16 status, uint32 serial, uint8
 * product_name_length, char product_name[N], uint8 state. */
LOCAL void enip_parse_list_identity(ArkimeSession_t *session, BSB *bsb)
{
    uint16_t itemCount = 0;
    BSB_LIMPORT_u16(*bsb, itemCount);

    if (BSB_IS_ERROR(*bsb) || itemCount == 0 || itemCount > 8) {
        return;
    }

    for (uint16_t i = 0; i < itemCount && !BSB_IS_ERROR(*bsb); i++) {
        uint16_t itemType = 0, itemLen = 0;
        BSB_LIMPORT_u16(*bsb, itemType);
        BSB_LIMPORT_u16(*bsb, itemLen);
        if (BSB_IS_ERROR(*bsb) || itemLen > BSB_REMAINING(*bsb)) {
            return;
        }

        const uint8_t *itemData = BSB_WORK_PTR(*bsb);
        BSB_IMPORT_skip(*bsb, itemLen);

        if (itemType != 0x000C) {
            continue;
        }

        BSB ibsb;
        BSB_INIT(ibsb, itemData, itemLen);

        BSB_IMPORT_skip(ibsb, 2);   // protocol version
        BSB_IMPORT_skip(ibsb, 16);  // sockaddr_in

        uint16_t vendorId = 0, deviceType = 0;
        uint8_t  nameLen = 0;

        BSB_LIMPORT_u16(ibsb, vendorId);
        BSB_LIMPORT_u16(ibsb, deviceType);
        BSB_IMPORT_skip(ibsb, 2);    // product code
        BSB_IMPORT_skip(ibsb, 2);    // revision major + minor
        BSB_IMPORT_skip(ibsb, 2);    // status
        BSB_IMPORT_skip(ibsb, 4);    // serial
        BSB_IMPORT_u08(ibsb, nameLen);

        if (BSB_IS_ERROR(ibsb)) {
            return;
        }

        const char *vname = enip_vendor_name(vendorId);
        if (vname) {
            arkime_field_string_add(vendorField, session, vname, -1, TRUE);
        } else {
            char vbuf[32];
            snprintf(vbuf, sizeof(vbuf), "Vendor-%u", vendorId);
            arkime_field_string_add(vendorField, session, vbuf, -1, TRUE);
        }

        char dbuf[32];
        snprintf(dbuf, sizeof(dbuf), "DeviceType-0x%04x", deviceType);
        arkime_field_string_add(deviceTypeField, session, dbuf, -1, TRUE);

        if (nameLen > 0 && nameLen <= BSB_REMAINING(ibsb)) {
            const char *name = (const char *)BSB_WORK_PTR(ibsb);
            arkime_field_string_add(productField, session, name, nameLen, TRUE);
        }
    }
}
/******************************************************************************/
LOCAL void enip_parse_message(ArkimeSession_t *session, const uint8_t *data, int len)
{
    BSB bsb;
    BSB_INIT(bsb, data, len);

    uint16_t command = 0, length = 0;
    uint32_t status = 0;
    BSB_LIMPORT_u16(bsb, command);
    BSB_LIMPORT_u16(bsb, length);
    BSB_IMPORT_skip(bsb, 4);          // session handle
    BSB_LIMPORT_u32(bsb, status);
    BSB_IMPORT_skip(bsb, 8);          // sender context
    BSB_IMPORT_skip(bsb, 4);          // options

    if (BSB_IS_ERROR(bsb)) {
        return;
    }

    const char *cname = enip_command_name(command);
    if (cname) {
        arkime_field_string_add(commandField, session, cname, -1, TRUE);
    }

    arkime_field_int_add(statusField, session, (int)status);

    if (length == 0 || length > BSB_REMAINING(bsb)) {
        return;
    }

    BSB body;
    BSB_INIT(body, BSB_WORK_PTR(bsb), length);

    switch (command) {
    case 0x0063:   // ListIdentity
        if (status == 0 && length > 0) {
            enip_parse_list_identity(session, &body);
        }
        break;
    case 0x006F:   // SendRRData
    case 0x0070:   // SendUnitData
        enip_parse_send_data(session, &body);
        break;
    default:
        break;
    }
}
/******************************************************************************/
LOCAL int enip_tcp_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int which)
{
    ArkimeParserBuf_t *enip = uw;

    arkime_parser_buf_add(enip, which, data, len);

    while (enip->len[which] >= ENIP_HEADER_LEN) {
        const uint8_t *buf = enip->buf[which];
        uint16_t command = buf[0] | (buf[1] << 8);
        uint16_t length  = buf[2] | (buf[3] << 8);

        if (!enip_command_known(command)) {
            return ARKIME_PARSER_UNREGISTER;
        }

        uint32_t total = (uint32_t)ENIP_HEADER_LEN + length;
        if (total > (uint32_t)enip->len[which]) {
            return 0;  // need more data
        }

        enip_parse_message(session, buf, total);
        arkime_parser_buf_del(enip, which, total);
    }

    return 0;
}
/******************************************************************************/
LOCAL int enip_udp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    if (len < ENIP_HEADER_LEN) {
        return 0;
    }
    enip_parse_message(session, data, len);
    return 0;
}
/******************************************************************************/
LOCAL int enip_looks_valid(const uint8_t *data, int len)
{
    if (len < ENIP_HEADER_LEN) {
        return 0;
    }
    uint16_t command = data[0] | (data[1] << 8);
    if (!enip_command_known(command)) {
        return 0;
    }
    uint16_t length = data[2] | (data[3] << 8);
    /* For datagrams: header + length must fit in the packet.
     * For TCP we accept any plausible length under 64k. */
    if ((int)length + ENIP_HEADER_LEN > 65535) {
        return 0;
    }
    return 1;
}
/******************************************************************************/
LOCAL void enip_tcp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void UNUSED(*uw))
{
    if (arkime_session_has_protocol(session, "enip")) {
        return;
    }

    if (!enip_looks_valid(data, len)) {
        return;
    }

    arkime_session_add_protocol(session, "enip");

    ArkimeParserBuf_t *info = arkime_parser_buf_create();
    arkime_parsers_register(session, enip_tcp_parser, info, arkime_parser_buf_session_free);
}
/******************************************************************************/
LOCAL void enip_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void UNUSED(*uw))
{
    if (arkime_session_has_protocol(session, "enip")) {
        return;
    }

    if (!enip_looks_valid(data, len)) {
        return;
    }

    /* For UDP the encapsulation length should match exactly. */
    uint16_t length = data[2] | (data[3] << 8);
    if (ENIP_HEADER_LEN + length != len) {
        return;
    }

    arkime_session_add_protocol(session, "enip");

    arkime_parsers_register(session, enip_udp_parser, NULL, NULL);
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_parsers_classifier_register_port("enip", NULL, 44818, ARKIME_PARSERS_PORT_TCP, enip_tcp_classify);
    arkime_parsers_classifier_register_port("enip", NULL, 44818, ARKIME_PARSERS_PORT_UDP, enip_udp_classify);
    arkime_parsers_classifier_register_port("enip", NULL, 2222,  ARKIME_PARSERS_PORT_UDP, enip_udp_classify);

    commandField = arkime_field_define("enip", "termfield",
                                       "enip.command", "ENIP Command", "enip.command",
                                       "ENIP encapsulation command (e.g. ListIdentity, RegisterSession, SendRRData)",
                                       ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    statusField = arkime_field_define("enip", "integer",
                                      "enip.status", "ENIP Status", "enip.status",
                                      "ENIP encapsulation status codes",
                                      ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                      (char *)NULL);

    serviceField = arkime_field_define("enip", "termfield",
                                       "enip.service", "CIP Service", "enip.service",
                                       "CIP service name (e.g. Get_Attribute_Single, Read_Tag, Forward_Open)",
                                       ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    classField = arkime_field_define("enip", "termfield",
                                     "enip.class", "CIP Class", "enip.class",
                                     "CIP object class name (e.g. Identity, Message Router)",
                                     ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                     (char *)NULL);

    vendorField = arkime_field_define("enip", "termfield",
                                      "enip.vendor", "ENIP Vendor", "enip.vendor",
                                      "ENIP vendor name (looked up from vendor id, or Vendor-N if unknown)",
                                      ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                      (char *)NULL);

    productField = arkime_field_define("enip", "termfield",
                                       "enip.product", "ENIP Product", "enip.product",
                                       "ENIP product name string from a ListIdentity response",
                                       ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    deviceTypeField = arkime_field_define("enip", "termfield",
                                          "enip.device-type", "ENIP Device Type", "enip.deviceType",
                                          "ENIP device type from ListIdentity",
                                          ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                          (char *)NULL);
}
