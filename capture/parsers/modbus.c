#include <string.h>
#include <ctype.h>
#include "arkimeconfig.h"
#include "moloch.h"

extern MolochConfig_t        config;

typedef struct {
    int transactionId;
    int protocolId;
    int dataLen;
    int unitId;
    int lastTransactionId;
    int transactionCount;
    int firstPacket; // 0 false, 1 true
} ModbusInfo_t;

LOCAL  int unitIdField;
LOCAL  int transactionIdField;
LOCAL  int protocolIdField;
LOCAL  int transactionCountField;
LOCAL  int lengthDataBytesField;

#define MODBUS_TCP_HEADER_LEN 7

/******************************************************************************/
LOCAL int modbus_tcp_parser(MolochSession_t *session, void *uw, const unsigned char *data, int len, int UNUSED(which)) {

    if (len < MODBUS_TCP_HEADER_LEN || data[3] != 0) {
        moloch_parsers_unregister(session, uw);
        return -1;
    }

    ModbusInfo_t *info = uw;

    if (info->firstPacket) {
        info->transactionId = (data[0] << 8) + data[1];
        info->lastTransactionId = info->transactionId;
        moloch_field_int_add(transactionIdField, session, info->lastTransactionId);
        info->transactionCount = 1;
        info->firstPacket = 0;
        // Next values should not change over the course of the session
        info->protocolId = 0;
        info->unitId = data[6];
        moloch_field_int_add(unitIdField, session, info->unitId);
        moloch_field_int_add(protocolIdField, session, info->protocolId);
        LOG("First transaction: ID %d, Count %d", info->transactionId, info->transactionCount);
    }

    // Extract Modbus TCP PDU header data
    info->transactionId = (data[0] << 8) + data[1];
    info->dataLen += (data[4] << 8) + data[5] - 2; // Minus 2 to account the for the Unit ID and Function Code bytes

    if (info->lastTransactionId != info->transactionId) {
        info->transactionCount++;
        LOG("New transaction: ID %d, Last ID %d Count %d", info->transactionId, info->lastTransactionId, info->transactionCount);
        info->lastTransactionId = info->transactionId;
        moloch_field_int_add(transactionIdField, session, info->lastTransactionId);
    }

    moloch_field_int_add(transactionCountField, session, info->transactionCount);
    moloch_field_int_add(lengthDataBytesField, session, info->dataLen);

    return 0;
}
/******************************************************************************/
LOCAL void modbus_tcp_free(MolochSession_t UNUSED(*session), void *uw)
{
    ModbusInfo_t *info = uw;

    MOLOCH_TYPE_FREE(ModbusInfo_t, info);
}
/******************************************************************************/
LOCAL void modbus_tcp_classify(MolochSession_t *session, const unsigned char UNUSED(*data), int len, int UNUSED(which), void UNUSED(*uw))
{
    // Checks that the Modbus data has at least the length of a full header and the protocol id is 0
    if (len < MODBUS_TCP_HEADER_LEN || data[3] != 0) {
        return;
    }
    if (moloch_session_has_protocol(session, "modbus")) {
        return;
    }
    moloch_session_add_protocol(session, "modbus");

    ModbusInfo_t *info = MOLOCH_TYPE_ALLOC0(ModbusInfo_t);
    info->firstPacket = 1;
    info->dataLen = 0;
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
        MOLOCH_FIELD_TYPE_INT_ARRAY, 0,
        (char *) NULL);

    protocolIdField = moloch_field_define("modbus", "integer",
        "modbus.protocolid", "Modbus Protocol ID", "modbus.protocolid",
        "Modbus Protocol ID (should always be 0)",
        MOLOCH_FIELD_TYPE_INT, 0,
        (char *) NULL);

    transactionCountField = moloch_field_define("modbus", "integer",
        "modbus.transactioncnt","Number of Modbus transactions", "modbus.transactioncnt",
        "Modbus transaction counter (times that the transaction ID changed)",
        MOLOCH_FIELD_TYPE_INT, 0,
        (char *) NULL);

    lengthDataBytesField = moloch_field_define("modbus", "integer",
        "modbus.datalength", "Length of Modbus Data", "modbus.datalength",
        "Modbus Data Bytes Length from Header",
        MOLOCH_FIELD_TYPE_INT, 0,
        (char *) NULL);
}
