#include <string.h>
#include <ctype.h>
#include "arkimeconfig.h"
#include "moloch.h"

extern MolochConfig_t        config;

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
LOCAL int modbus_tcp_parser(MolochSession_t *session, void *uw, const unsigned char *data, int len, int which) {

    if (len < MODBUS_TCP_HEADER_LEN || data[3] != 0) {
        return MOLOCH_PARSER_UNREGISTER;
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
            return MOLOCH_PARSER_UNREGISTER;
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
            moloch_field_int_add(exceptionCodeField, session, exceptionCode);
        }

        if (which == 0) {
            moloch_field_int_add(transactionIdField, session, transactionId);
            moloch_field_int_add(functionCodeField, session, functionCode);
        }

        if (modbus->packets[which] == 1) {
            moloch_field_int_add(unitIdField, session, unitId);
            moloch_field_int_add(protocolIdField, session, protocolId);
        }

        modbus->len[which] -= 6 + modbusLen;
        memmove(modbus->buf[which], modbus->buf[which] + modbusLen + 6, modbus->len[which]);
    }

    return 0;
}
/******************************************************************************/
LOCAL void modbus_tcp_free(MolochSession_t UNUSED(*session), void *uw)
{
    ModbusInfo_t *info = uw;

    MOLOCH_TYPE_FREE(ModbusInfo_t, info);
}
/******************************************************************************/
LOCAL void modbus_tcp_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void UNUSED(*uw))
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

    if (moloch_session_has_protocol(session, "modbus")) {
        return;
    }
    moloch_session_add_protocol(session, "modbus");

    ModbusInfo_t *info = MOLOCH_TYPE_ALLOC0(ModbusInfo_t);
    moloch_parsers_register(session, modbus_tcp_parser, info, modbus_tcp_free);
}
/******************************************************************************/
void moloch_parser_init()
{
    // All packets to/from port 502 likely to be Modbus TCP, can cause false positives
    moloch_parsers_classifier_register_port("modbus", NULL, 502, MOLOCH_PARSERS_PORT_TCP, modbus_tcp_classify);

    unitIdField = moloch_field_define("modbus", "integer",
        "modbus.unitid", "Modbus Unit ID", "modbus.unitid",
        "Modbus Unit ID",
        MOLOCH_FIELD_TYPE_INT, 0,
        (char *) NULL);

    transactionIdField = moloch_field_define("modbus", "integer",
        "modbus.transactionid", "Modbus Transaction IDs", "modbus.transactionid",
        "Modbus Transaction IDs",
        MOLOCH_FIELD_TYPE_INT_GHASH, MOLOCH_FIELD_FLAG_CNT,
        (char *) NULL);

    protocolIdField = moloch_field_define("modbus", "integer",
        "modbus.protocolid", "Modbus Protocol ID", "modbus.protocolid",
        "Modbus Protocol ID (should always be 0)",
        MOLOCH_FIELD_TYPE_INT, 0,
        (char *) NULL);

    functionCodeField = moloch_field_define("modbus", "integer",
        "modbus.funccode", "Modbus Function Code", "modbus.funccode",
        "Modbus Function Codes",
        MOLOCH_FIELD_TYPE_INT_GHASH, MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    exceptionCodeField = moloch_field_define("modbus", "integer",
        "modbus.exccode", "Modbus Exception Code", "modbus.exccode",
        "Modbus Exception Codes",
        MOLOCH_FIELD_TYPE_INT_GHASH, MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);
}
