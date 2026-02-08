/* s7comm.c
 *
 * Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * S7comm/S7comm-plus parser for Siemens S7 PLC communication
 * Protocol stack: TCP -> TPKT -> COTP -> S7comm/S7comm-plus
 */
#include <string.h>
#include "arkimeconfig.h"
#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL  int rosctrField;
LOCAL  int funcField;
LOCAL  int pduRefField;
LOCAL  int errorClassField;
LOCAL  int errorCodeField;
LOCAL  int s7plusOpcodeField;

/* TPKT header: version(1) + reserved(1) + length(2) = 4 bytes */
#define TPKT_HDR_LEN    4

/* Minimum S7comm header: protocol_id(1) + rosctr(1) + redundancy(2) + pdu_ref(2) + param_len(2) + data_len(2) = 10 */
#define S7COMM_HDR_LEN  10

/* S7comm protocol IDs */
#define S7COMM_PROTOCOL_ID      0x32
#define S7COMM_PLUS_PROTOCOL_ID 0x72

/******************************************************************************/
LOCAL void s7comm_parse_pdu(ArkimeSession_t *session, BSB *bsb)
{
    uint8_t protocolId = 0;
    BSB_IMPORT_u08(*bsb, protocolId);

    if (BSB_IS_ERROR(*bsb))
        return;

    if (protocolId == S7COMM_PROTOCOL_ID) {
        if (!arkime_session_has_protocol(session, "s7comm"))
            arkime_session_add_protocol(session, "s7comm");

        if (BSB_REMAINING(*bsb) < S7COMM_HDR_LEN - 1)
            return;

        uint8_t rosctr = 0;
        BSB_IMPORT_u08(*bsb, rosctr);
        arkime_field_int_add(rosctrField, session, rosctr);

        BSB_IMPORT_skip(*bsb, 2); // redundancy identification

        uint16_t pduRef = 0;
        BSB_IMPORT_u16(*bsb, pduRef);
        arkime_field_int_add(pduRefField, session, pduRef);

        uint16_t paramLen = 0;
        BSB_IMPORT_u16(*bsb, paramLen);
        BSB_IMPORT_skip(*bsb, 2); // data length

        if (BSB_IS_ERROR(*bsb))
            return;

        // Ack (0x02) and Ack_Data (0x03) have error class and error code
        if (rosctr == 0x02 || rosctr == 0x03) {
            uint8_t errorClass = 0, errorCode = 0;
            BSB_IMPORT_u08(*bsb, errorClass);
            BSB_IMPORT_u08(*bsb, errorCode);
            if (errorClass != 0)
                arkime_field_int_add(errorClassField, session, errorClass);
            if (errorCode != 0)
                arkime_field_int_add(errorCodeField, session, errorCode);
        }

        // Parse function code from parameter area
        if (paramLen >= 1 && BSB_REMAINING(*bsb) >= 1) {
            uint8_t funcCode = 0;
            BSB_IMPORT_u08(*bsb, funcCode);
            arkime_field_int_add(funcField, session, funcCode);
        }
    } else if (protocolId == S7COMM_PLUS_PROTOCOL_ID) {
        if (!arkime_session_has_protocol(session, "s7comm-plus"))
            arkime_session_add_protocol(session, "s7comm-plus");

        if (BSB_REMAINING(*bsb) < 4)
            return;

        BSB_IMPORT_skip(*bsb, 1); // version
        BSB_IMPORT_skip(*bsb, 2); // length

        uint8_t digestLen = 0;
        BSB_IMPORT_u08(*bsb, digestLen);

        if (BSB_IS_ERROR(*bsb))
            return;

        BSB_IMPORT_skip(*bsb, digestLen);

        if (BSB_REMAINING(*bsb) >= 1) {
            uint8_t opcode = 0;
            BSB_IMPORT_u08(*bsb, opcode);
            arkime_field_int_add(s7plusOpcodeField, session, opcode);
        }
    }
}
/******************************************************************************/
LOCAL int s7comm_tcp_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int which)
{
    ArkimeParserBuf_t *buf = uw;

    arkime_parser_buf_add(buf, which, data, len);

    while (buf->len[which] >= TPKT_HDR_LEN) {
        BSB bsb;
        BSB_INIT(bsb, buf->buf[which], buf->len[which]);

        uint8_t version = 0;
        BSB_IMPORT_u08(bsb, version);

        if (version != 3) {
            return ARKIME_PARSER_UNREGISTER;
        }

        BSB_IMPORT_skip(bsb, 1); // reserved

        uint16_t tpktLen = 0;
        BSB_IMPORT_u16(bsb, tpktLen);

        if (tpktLen < TPKT_HDR_LEN || tpktLen > 65535) {
            return ARKIME_PARSER_UNREGISTER;
        }

        if (tpktLen > buf->len[which]) {
            return 0; // Need more data
        }

        // Parse COTP header
        if (BSB_REMAINING(bsb) < 2) {
            arkime_parser_buf_del(buf, which, tpktLen);
            continue;
        }

        uint8_t cotpLen = 0;
        BSB_IMPORT_u08(bsb, cotpLen);

        uint8_t cotpPduType = 0;
        BSB_IMPORT_u08(bsb, cotpPduType);
        cotpPduType >>= 4;

        // Skip rest of COTP header
        if (cotpLen > 1) {
            BSB_IMPORT_skip(bsb, cotpLen - 1);
        }

        if (BSB_IS_ERROR(bsb)) {
            arkime_parser_buf_del(buf, which, tpktLen);
            continue;
        }

        // Only parse S7comm data for DT Data PDUs (0x0f)
        if (cotpPduType == 0x0f && BSB_REMAINING(bsb) >= 1) {
            s7comm_parse_pdu(session, &bsb);
        }

        arkime_parser_buf_del(buf, which, tpktLen);
    }

    return 0;
}
/******************************************************************************/
LOCAL void s7comm_tcp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void UNUSED(*uw))
{
    if (arkime_session_has_protocol(session, "s7comm") || arkime_session_has_protocol(session, "s7comm-plus")) {
        return;
    }

    /* TPKT header starts with version=3, reserved=0, then 2-byte length */
    if (len < TPKT_HDR_LEN + 3) {
        return;
    }

    if (data[0] != 0x03 || data[1] != 0x00) {
        return;
    }

    uint16_t tpktLen = (data[2] << 8) | data[3];
    if (tpktLen < 7 || tpktLen > len) {
        return;
    }

    /* COTP header follows TPKT */
    uint8_t cotpLen = data[4];
    uint8_t cotpPduType = data[5] >> 4;

    if (cotpPduType == 0x0e || cotpPduType == 0x0d) {
        /* COTP Connection Request/Confirm - check for TSAP parameters (c1/c2)
         * which distinguish S7comm COTP from RDP COTP */
        int offset = 4 + 2 + 5; // TPKT(4) + pdu_type+len(2) + dst_ref(2)+src_ref(2)+class(1)
        if (offset >= len)
            return;
        gboolean found = FALSE;
        while (offset + 2 < len && offset < 4 + 1 + cotpLen) {
            uint8_t paramCode = data[offset];
            uint8_t paramLen = data[offset + 1];
            if (paramCode == 0xc1 || paramCode == 0xc2) {
                found = TRUE;
                break;
            }
            offset += 2 + paramLen;
        }
        if (!found)
            return;
    } else if (cotpPduType == 0x0f) {
        /* DT Data - verify S7comm or S7comm-plus protocol ID */
        int s7offset = 4 + 1 + cotpLen;
        if (s7offset >= len)
            return;
        if (data[s7offset] != S7COMM_PROTOCOL_ID && data[s7offset] != S7COMM_PLUS_PROTOCOL_ID)
            return;
    } else {
        return;
    }

    ArkimeParserBuf_t *info = arkime_parser_buf_create();
    arkime_parsers_register(session, s7comm_tcp_parser, info, arkime_parser_buf_session_free);
}
/******************************************************************************/
void arkime_parser_init()
{
    /* TPKT always starts with 0x03 0x00 */
    arkime_parsers_classifier_register_tcp("s7comm", NULL, 0, (const uint8_t *)"\x03\x00", 2, s7comm_tcp_classify);

    rosctrField = arkime_field_define("s7comm", "integer",
                                      "s7comm.rosctr", "S7comm ROSCTR", "s7comm.rosctr",
                                      "S7comm Remote Operating Service Control (1=Job, 2=Ack, 3=Ack_Data, 7=Userdata)",
                                      ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                      (char *)NULL);

    funcField = arkime_field_define("s7comm", "integer",
                                    "s7comm.func", "S7comm Function Code", "s7comm.func",
                                    "S7comm Function Codes",
                                    ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                    (char *)NULL);

    pduRefField = arkime_field_define("s7comm", "integer",
                                      "s7comm.pduref", "S7comm PDU Reference", "s7comm.pduref",
                                      "S7comm PDU Reference IDs",
                                      ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                      (char *)NULL);

    errorClassField = arkime_field_define("s7comm", "integer",
                                          "s7comm.errorClass", "S7comm Error Class", "s7comm.errorClass",
                                          "S7comm Error Classes",
                                          ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                          (char *)NULL);

    errorCodeField = arkime_field_define("s7comm", "integer",
                                         "s7comm.errorCode", "S7comm Error Code", "s7comm.errorCode",
                                         "S7comm Error Codes",
                                         ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                         (char *)NULL);

    s7plusOpcodeField = arkime_field_define("s7comm", "integer",
                                            "s7comm.opcode", "S7comm-plus Opcode", "s7comm.opcode",
                                            "S7comm-plus Opcodes (0x31=Request, 0x32=Response, 0x33=Notification)",
                                            ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                            (char *)NULL);
}
