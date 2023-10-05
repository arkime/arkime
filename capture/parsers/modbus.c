#include <string.h>
#include <ctype.h>
#include "arkimeconfig.h"
#include "arkime.h"

extern ArkimeConfig_t        config;

#define MAX_MODBUS_BUFFER 0xFF


typedef struct {
    char buf[2][MAX_MODBUS_BUFFER];
    int16_t len[2];
    uint16_t packets[2];
} ModbusInfo_t;

LOCAL  int unitIdField;
LOCAL  int transactionIdField;
LOCAL  int protocolIdField;
LOCAL  int functionCodeField;
LOCAL  int exceptionCodeField;

#define MODBUS_TCP_HEADER_LEN 7

/******************************************************************************/
LOCAL int modbus_tcp_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int which) {

    if (len < MODBUS_TCP_HEADER_LEN || data[3] != 0) {
        return ARKIME_PARSER_UNREGISTER;
    }

    ModbusInfo_t *modbus = uw;

    modbus->packets[which]++;

    memcpy(modbus->buf[which] + modbus->len[which], data, MIN(len, (int)sizeof(modbus->buf[which]) - modbus->len[which]));
    modbus->len[which] += MIN(len, (int)sizeof(modbus->buf[which]) - modbus->len[which]);

    while (modbus->len[which] > 9) {
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

        if (modbusLen < 2 || modbusLen > MAX_MODBUS_BUFFER) {
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

        if (modbus->packets[which] == 1) {
            arkime_field_int_add(unitIdField, session, unitId);
            arkime_field_int_add(protocolIdField, session, protocolId);
        }

        modbus->len[which] -= 6 + modbusLen;
        memmove(modbus->buf[which], modbus->buf[which] + modbusLen + 6, modbus->len[which]);
    }

    return 0;
}
/******************************************************************************/
LOCAL void modbus_tcp_free(ArkimeSession_t UNUSED(*session), void *uw)
{
    ModbusInfo_t *info = uw;

    ARKIME_TYPE_FREE(ModbusInfo_t, info);
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

    ModbusInfo_t *info = ARKIME_TYPE_ALLOC0(ModbusInfo_t);
    arkime_parsers_register(session, modbus_tcp_parser, info, modbus_tcp_free);
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
