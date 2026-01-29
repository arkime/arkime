/* modbus.c
 *
 * Copyright 2020 Sqooba AG. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <ctype.h>
#include "arkimeconfig.h"
#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL  int unitIdField;
LOCAL  int transactionIdField;
LOCAL  int protocolIdField;
LOCAL  int functionCodeField;
LOCAL  int exceptionCodeField;

#define MODBUS_TCP_HEADER_LEN 7

/******************************************************************************/
LOCAL int modbus_tcp_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int which)
{
    ArkimeParserBuf_t *modbus = uw;

    modbus->state[which]++;

    arkime_parser_buf_add(modbus, which, data, len);

    while (modbus->len[which] >= MODBUS_TCP_HEADER_LEN) {
        BSB bsb;
        BSB_INIT(bsb, modbus->buf[which], modbus->len[which]);

        uint16_t transactionId = 0;
        BSB_IMPORT_u16(bsb, transactionId);

        uint16_t protocolId = 0;
        BSB_IMPORT_u16(bsb, protocolId);

        if (protocolId != 0) {
            // Protocol ID should always be 0
            return ARKIME_PARSER_UNREGISTER;
        }

        uint16_t modbusLen = 0;
        BSB_IMPORT_u16(bsb, modbusLen);

        if (modbusLen < 2 || modbusLen > 255) {
            return 0;
        }

        if (modbusLen > BSB_REMAINING(bsb)) {
            return 0;
        }

        uint8_t unitId = 0;
        BSB_IMPORT_u08(bsb, unitId);

        uint8_t functionCode = 0;
        BSB_IMPORT_u08(bsb, functionCode);

        if (which == 1 && functionCode & 0x80) {
            functionCode = functionCode & 0x7f;
            uint8_t exceptionCode = 0;
            BSB_IMPORT_u08(bsb, exceptionCode);
            arkime_field_int_add(exceptionCodeField, session, exceptionCode);
        }

        if (which == 0) {
            arkime_field_int_add(transactionIdField, session, transactionId);
            arkime_field_int_add(functionCodeField, session, functionCode);
        }

        if (modbus->state[which] == 1) {
            arkime_field_int_add(unitIdField, session, unitId);
            arkime_field_int_add(protocolIdField, session, protocolId);
        }

        arkime_parser_buf_del(modbus, which, 6 + modbusLen);
    }

    return 0;
}
/******************************************************************************/
LOCAL void modbus_tcp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void UNUSED(*uw))
{
    // Checks that the Modbus data has at least the length of a full header
    if (len < MODBUS_TCP_HEADER_LEN) {
        return;
    }

    BSB bsb;
    BSB_INIT(bsb, data, len);

    uint16_t protocolId = 0, modbusLen = 0;
    BSB_IMPORT_skip(bsb, 2);
    BSB_IMPORT_u16(bsb, protocolId);
    BSB_IMPORT_u16(bsb, modbusLen);

    if (protocolId != 0 || (modbusLen - 1) != (len - 7)) {
        return;
    }

    if (arkime_session_has_protocol(session, "modbus")) {
        return;
    }
    arkime_session_add_protocol(session, "modbus");

    ArkimeParserBuf_t *info = arkime_parser_buf_create();
    arkime_parsers_register(session, modbus_tcp_parser, info, arkime_parser_buf_session_free);
}
/******************************************************************************/
void arkime_parser_init()
{
    // All packets to/from port 502 likely to be Modbus TCP, can cause false positives
    arkime_parsers_classifier_register_port("modbus", NULL, 502, ARKIME_PARSERS_PORT_TCP, modbus_tcp_classify);

    unitIdField = arkime_field_define("modbus", "integer",
                                      "modbus.unitid", "Modbus Unit ID", "modbus.unitid",
                                      "Modbus Unit ID",
                                      ARKIME_FIELD_TYPE_INT, 0,
                                      (char *) NULL);

    transactionIdField = arkime_field_define("modbus", "integer",
                                             "modbus.transactionid", "Modbus Transaction IDs", "modbus.transactionid",
                                             "Modbus Transaction IDs",
                                             ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                             (char *) NULL);

    protocolIdField = arkime_field_define("modbus", "integer",
                                          "modbus.protocolid", "Modbus Protocol ID", "modbus.protocolid",
                                          "Modbus Protocol ID (should always be 0)",
                                          ARKIME_FIELD_TYPE_INT, 0,
                                          (char *) NULL);

    functionCodeField = arkime_field_define("modbus", "integer",
                                            "modbus.funccode", "Modbus Function Code", "modbus.funccode",
                                            "Modbus Function Codes",
                                            ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                            (char *)NULL);

    exceptionCodeField = arkime_field_define("modbus", "integer",
                                             "modbus.exccode", "Modbus Exception Code", "modbus.exccode",
                                             "Modbus Exception Codes",
                                             ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                             (char *)NULL);
}
