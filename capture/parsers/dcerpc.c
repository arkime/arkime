/* Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

/*
 * DCE/RPC - Distributed Computing Environment / Remote Procedure Call
 *
 * Used by Windows for many services including:
 *   - DRSUAPI (Active Directory replication)
 *   - SAMR (Security Account Manager)
 *   - NETLOGON (Domain authentication)
 *   - SVCCTL (Service Control Manager)
 *   - SRVSVC (Server Service)
 *
 * Header (16 bytes):
 *   Byte 0:      Version (5)
 *   Byte 1:      Version Minor (0)
 *   Byte 2:      Packet Type
 *   Byte 3:      Flags
 *   Bytes 4-7:   Data Representation
 *   Bytes 8-9:   Frag Length (little-endian)
 *   Bytes 10-11: Auth Length
 *   Bytes 12-15: Call ID
 */

extern ArkimeConfig_t        config;

LOCAL int uuidField;
LOCAL int interfaceField;
LOCAL int opnumField;
LOCAL int versionField;

LOCAL GHashTable *subParsers;

LOCAL int msgTypeField;

// Packet types
#define DCERPC_REQUEST       0
#define DCERPC_RESPONSE      2
#define DCERPC_FAULT         3
#define DCERPC_BIND          11
#define DCERPC_BIND_ACK      12
#define DCERPC_BIND_NAK      13
#define DCERPC_ALTER_CTX     14
#define DCERPC_ALTER_CTX_RSP 15
#define DCERPC_SHUTDOWN      17
#define DCERPC_CO_CANCEL     18
#define DCERPC_ORPHANED      19

LOCAL const char *msgTypes[] = {
    [DCERPC_REQUEST]       = "request",
    [DCERPC_RESPONSE]      = "response",
    [DCERPC_FAULT]         = "fault",
    [DCERPC_BIND]          = "bind",
    [DCERPC_BIND_ACK]      = "bind-ack",
    [DCERPC_BIND_NAK]      = "bind-nak",
    [DCERPC_ALTER_CTX]     = "alter-context",
    [DCERPC_ALTER_CTX_RSP] = "alter-context-resp",
    [DCERPC_SHUTDOWN]      = "shutdown",
    [DCERPC_CO_CANCEL]     = "co-cancel",
    [DCERPC_ORPHANED]      = "orphaned"
};

/******************************************************************************/
// Format UUID from wire format to string
LOCAL void dcerpc_format_uuid(BSB *bsb, char *out, gboolean le)
{
    uint32_t data1 = 0;
    uint16_t data2 = 0, data3 = 0;
    const uint8_t *data4;

    if (le) {
        BSB_LIMPORT_u32(*bsb, data1);
        BSB_LIMPORT_u16(*bsb, data2);
        BSB_LIMPORT_u16(*bsb, data3);
    } else {
        BSB_IMPORT_u32(*bsb, data1);
        BSB_IMPORT_u16(*bsb, data2);
        BSB_IMPORT_u16(*bsb, data3);
    }
    BSB_IMPORT_ptr(*bsb, data4, 8);

    if (BSB_IS_ERROR(*bsb))
        return;

    snprintf(out, 37, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             data1, data2, data3,
             data4[0], data4[1],
             data4[2], data4[3], data4[4], data4[5], data4[6], data4[7]);
}

/******************************************************************************/
// Parse BIND request to extract interface UUID
LOCAL void dcerpc_parse_bind(ArkimeSession_t *session, BSB *bsb, gboolean le)
{
    // BIND structure after 16-byte header:
    // Bytes 0-1:   Max Xmit Frag
    // Bytes 2-3:   Max Recv Frag
    // Bytes 4-7:   Assoc Group
    // Byte 8:      Num Context Items
    // Bytes 9-11:  Reserved

    uint8_t numCtx;

    BSB_IMPORT_skip(*bsb, 8);  // Max Xmit/Recv Frag, Assoc Group
    BSB_IMPORT_u08(*bsb, numCtx);
    BSB_IMPORT_skip(*bsb, 3);  // Reserved

    if (BSB_IS_ERROR(*bsb))
        return;

    for (int i = 0; i < numCtx && BSB_REMAINING(*bsb) >= 44; i++) {
        // Context item:
        // Bytes 0-1:   Context ID
        // Byte 2:      Num Trans Items
        // Byte 3:      Reserved
        // Bytes 4-19:  Abstract Syntax UUID (interface)
        // Bytes 20-23: Abstract Syntax Version
        // Then transfer syntaxes (20 bytes each)

        uint8_t numTrans;

        BSB_IMPORT_skip(*bsb, 2);  // Context ID
        BSB_IMPORT_u08(*bsb, numTrans);
        BSB_IMPORT_skip(*bsb, 1);  // Reserved

        if (BSB_IS_ERROR(*bsb))
            return;

        char uuid[37];
        dcerpc_format_uuid(bsb, uuid, le);

        uint16_t ifaceVer;
        if (le) {
            BSB_LIMPORT_u16(*bsb, ifaceVer);
        } else {
            BSB_IMPORT_u16(*bsb, ifaceVer);
        }
        BSB_IMPORT_skip(*bsb, 2);  // Minor version

        if (BSB_IS_ERROR(*bsb))
            return;

        arkime_field_string_add(uuidField, session, uuid, 36, TRUE);

        const ArkimeParserInfo_t *info = g_hash_table_lookup(subParsers, uuid);
        if (info) {
            arkime_field_string_add(interfaceField, session, info->uw, -1, TRUE);
            arkime_session_add_protocol(session, info->uw);
        }

        char verStr[8];
        snprintf(verStr, sizeof(verStr), "%u", ifaceVer);
        arkime_field_string_add(versionField, session, verStr, -1, TRUE);

        // Skip transfer syntaxes (20 bytes each: 16 UUID + 4 version)
        BSB_IMPORT_skip(*bsb, numTrans * 20);
    }
}

/******************************************************************************/
// Parse REQUEST to extract opnum
LOCAL void dcerpc_parse_request(ArkimeSession_t *session, BSB *bsb, gboolean le)
{
    // REQUEST structure after 16-byte header:
    // Bytes 0-3:   Alloc Hint
    // Bytes 4-5:   Context ID
    // Bytes 6-7:   Opnum

    uint16_t opnum;

    BSB_IMPORT_skip(*bsb, 6);  // Alloc Hint, Context ID
    if (le) {
        BSB_LIMPORT_u16(*bsb, opnum);
    } else {
        BSB_IMPORT_u16(*bsb, opnum);
    }

    if (BSB_IS_ERROR(*bsb))
        return;

    arkime_field_int_add(opnumField, session, opnum);
}

/******************************************************************************/
LOCAL void dcerpc_process_pdu(ArkimeSession_t *session, const uint8_t *data, int len)
{
    BSB bsb;
    BSB_INIT(bsb, data, len);

    uint8_t pktType, dataRep;

    BSB_IMPORT_skip(bsb, 2);  // Version
    BSB_IMPORT_u08(bsb, pktType);
    BSB_IMPORT_skip(bsb, 1);  // Flags
    BSB_IMPORT_u08(bsb, dataRep);  // First byte of data representation
    BSB_IMPORT_skip(bsb, 11);  // Rest of data rep, frag len, auth len, call id

    if (BSB_IS_ERROR(bsb))
        return;

    // Add message type
    if (pktType < ARRAY_LEN(msgTypes) && msgTypes[pktType]) {
        arkime_field_string_add(msgTypeField, session, msgTypes[pktType], -1, TRUE);
    }

    // Bit 4 of dataRep: 0 = big-endian, 1 = little-endian
    gboolean le = (dataRep & 0x10) != 0;

    switch (pktType) {
    case DCERPC_BIND:
    case DCERPC_ALTER_CTX:
        dcerpc_parse_bind(session, &bsb, le);
        break;
    case DCERPC_REQUEST:
        dcerpc_parse_request(session, &bsb, le);
        break;
    }
}

/******************************************************************************/
LOCAL int dcerpc_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int remaining, int which)
{
    ArkimeParserBuf_t *pb = uw;

    if (arkime_parser_buf_add(pb, which, data, remaining) < 0)
        return 0;

    while (pb->len[which] >= 16) {
        // Check byte order from data representation (byte 4, bit 4: 0=BE, 1=LE)
        gboolean le = (pb->buf[which][4] & 0x10) != 0;

        // Get frag length from header (bytes 8-9)
        uint16_t fragLen;
        if (le) {
            fragLen = pb->buf[which][8] | (pb->buf[which][9] << 8);
        } else {
            fragLen = (pb->buf[which][8] << 8) | pb->buf[which][9];
        }

        if (fragLen < 16 || fragLen > 8192) {
            arkime_parser_buf_skip(pb, which, pb->len[which]);
            return 0;
        }

        if (pb->len[which] < fragLen)
            return 0;

        dcerpc_process_pdu(session, pb->buf[which], fragLen);
        arkime_parser_buf_del(pb, which, fragLen);
    }

    return 0;
}

/******************************************************************************/
LOCAL void dcerpc_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 16)
        return;

    // DCE/RPC header check:
    // Byte 0: Version = 5
    // Byte 1: Version Minor = 0
    // Byte 2: Packet Type (0-19)
    if (data[0] != 5 || data[1] != 0)
        return;

    if (data[2] > 19)
        return;

    if (arkime_session_has_protocol(session, "dcerpc"))
        return;

    arkime_session_add_protocol(session, "dcerpc");

    ArkimeParserBuf_t *pb = arkime_parser_buf_create();
    arkime_parsers_register(session, dcerpc_parser, pb, arkime_parser_buf_session_free);
}

/******************************************************************************/
void arkime_parser_init()
{
    // Get sub-parser table and register known interfaces
    subParsers = arkime_parsers_get_sub("dcerpc");
    arkime_parsers_register_sub("dcerpc", "e3514235-4b06-11d1-ab04-00c04fc2dcd2", NULL, "drsuapi");      // AD replication, DCSync
    arkime_parsers_register_sub("dcerpc", "12345778-1234-abcd-ef00-0123456789ac", NULL, "samr");         // SAM enumeration
    arkime_parsers_register_sub("dcerpc", "12345678-1234-abcd-ef00-01234567cffb", NULL, "netlogon");     // Domain auth, ZeroLogon
    arkime_parsers_register_sub("dcerpc", "367abb81-9844-35f1-ad32-98f038001003", NULL, "svcctl");       // Service control, PSExec
    arkime_parsers_register_sub("dcerpc", "4b324fc8-1670-01d3-1278-5a47bf6ee188", NULL, "srvsvc");       // Server service, shares
    arkime_parsers_register_sub("dcerpc", "1ff70682-0a51-30e8-076d-740be8cee98b", NULL, "atsvc");        // Task scheduler
    arkime_parsers_register_sub("dcerpc", "338cd001-2244-31f1-aaaa-900038001003", NULL, "winreg");       // Remote registry
    arkime_parsers_register_sub("dcerpc", "c681d488-d850-11d0-8c52-00c04fd90f7e", NULL, "efsrpc");       // EFS, PetitPotam
    arkime_parsers_register_sub("dcerpc", "df1941c5-fe89-4e79-bf10-463657acf44d", NULL, "efsr");         // EFS alt
    arkime_parsers_register_sub("dcerpc", "12345778-1234-abcd-ef00-0123456789ab", NULL, "lsarpc");       // LSA
    arkime_parsers_register_sub("dcerpc", "6bffd098-a112-3610-9833-46c3f87e345a", NULL, "wkssvc");       // Workstation service
    arkime_parsers_register_sub("dcerpc", "3919286a-b10c-11d0-9ba8-00c04fd92ef5", NULL, "dssetup");      // Domain setup
    arkime_parsers_register_sub("dcerpc", "e1af8308-5d1f-11c9-91a4-08002b14a0fa", NULL, "epmapper");     // Endpoint mapper
    arkime_parsers_register_sub("dcerpc", "86d35949-83c9-4044-b424-db363231fd0c", NULL, "itaskscheduler"); // Task scheduler v2
    arkime_parsers_register_sub("dcerpc", "00000131-0000-0000-c000-000000000046", NULL, "ioxidresolver"); // DCOM resolver
    arkime_parsers_register_sub("dcerpc", "00000134-0000-0000-c000-000000000046", NULL, "iremunknown");   // DCOM remote unknown
    arkime_parsers_register_sub("dcerpc", "000001a0-0000-0000-c000-000000000046", NULL, "iremunknown2");  // DCOM remote unknown v2
    arkime_parsers_register_sub("dcerpc", "ccd8c074-d0e5-4a40-92b4-d074faa6ba28", NULL, "witness");       // SMB Witness Service
    arkime_parsers_register_sub("dcerpc", "afa8bd80-7d8a-11c9-bef4-08002b102989", NULL, "mgmt");          // RPC management
    arkime_parsers_register_sub("dcerpc", "f5cc5a18-4264-101a-8c59-08002b2f8426", NULL, "nspi");          // Name Service Provider Interface

    uuidField = arkime_field_define("dcerpc", "termfield",
                                    "dcerpc.uuid", "UUID", "dcerpc.uuid",
                                    "DCE/RPC interface UUID",
                                    ARKIME_FIELD_TYPE_STR_HASH, ARKIME_FIELD_FLAG_CNT,
                                    (char *)NULL);

    interfaceField = arkime_field_define("dcerpc", "termfield",
                                         "dcerpc.interface", "Interface", "dcerpc.interface",
                                         "DCE/RPC interface name",
                                         ARKIME_FIELD_TYPE_STR_HASH, ARKIME_FIELD_FLAG_CNT,
                                         (char *)NULL);

    msgTypeField = arkime_field_define("dcerpc", "termfield",
                                       "dcerpc.msgType", "Message Type", "dcerpc.msgType",
                                       "DCE/RPC message type",
                                       ARKIME_FIELD_TYPE_STR_HASH, ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    opnumField = arkime_field_define("dcerpc", "integer",
                                     "dcerpc.opnum", "Opnum", "dcerpc.opnum",
                                     "DCE/RPC operation number",
                                     ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                     (char *)NULL);

    versionField = arkime_field_define("dcerpc", "termfield",
                                       "dcerpc.version", "Version", "dcerpc.version",
                                       "DCE/RPC interface version",
                                       ARKIME_FIELD_TYPE_STR_HASH, ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    // Register for DCE/RPC signature: version 5.0, bind request
    arkime_parsers_classifier_register_tcp("dcerpc", NULL, 0, (uint8_t *)"\x05\x00\x0b", 3, dcerpc_classify);

    // Also match request packets (type 0)
    arkime_parsers_classifier_register_tcp("dcerpc", NULL, 0, (uint8_t *)"\x05\x00\x00", 3, dcerpc_classify);

    // And response packets (type 2)
    arkime_parsers_classifier_register_tcp("dcerpc", NULL, 0, (uint8_t *)"\x05\x00\x02", 3, dcerpc_classify);

    // Bind Ack (type 12)
    arkime_parsers_classifier_register_tcp("dcerpc", NULL, 0, (uint8_t *)"\x05\x00\x0c", 3, dcerpc_classify);
}
