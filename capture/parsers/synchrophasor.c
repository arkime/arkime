/* synchrophasor.c
 *
 * Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * IEEE C37.118.2 Synchrophasor parser for power systems
 */
#include <string.h>
#include "arkimeconfig.h"
#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL int frameTypeField;
LOCAL int idcodeField;
LOCAL int cmdField;
LOCAL int stationField;
LOCAL int dataRateField;
LOCAL int numPMUField;

// Sync byte must be 0xAA
#define SYNCHROPHASOR_SYNC_BYTE  0xAA
#define SYNCHROPHASOR_MIN_LEN    16   // SYNC(2) + FRAMESIZE(2) + IDCODE(2) + SOC(4) + FRACSEC(4) + CHK(2)
#define SYNCHROPHASOR_MAX_FRAME  65535

LOCAL const char *frameTypeNames[] = {
    "data",
    "header",
    "cfg1",
    "cfg2",
    "command",
    "cfg3"
};

/******************************************************************************/
LOCAL void synchrophasor_parse_frame(ArkimeSession_t *session, const uint8_t *data, int len)
{
    if (len < SYNCHROPHASOR_MIN_LEN)
        return;

    BSB bsb;
    BSB_INIT(bsb, data, len);

    // SYNC field: byte 0 = 0xAA, byte 1 = version(bits 0-3) + frameType(bits 4-6) + reserved(bit 7)
    uint8_t syncByte = 0;
    BSB_IMPORT_u08(bsb, syncByte);
    if (syncByte != SYNCHROPHASOR_SYNC_BYTE)
        return;

    uint8_t syncByte2 = 0;
    BSB_IMPORT_u08(bsb, syncByte2);
    uint8_t frameType = (syncByte2 >> 4) & 0x07;

    if (frameType < ARRAY_LEN(frameTypeNames)) {
        arkime_field_string_add(frameTypeField, session, frameTypeNames[frameType], -1, TRUE);
    }

    // FRAMESIZE
    uint16_t frameSize = 0;
    BSB_IMPORT_u16(bsb, frameSize);

    // IDCODE (data stream ID)
    uint16_t idcode = 0;
    BSB_IMPORT_u16(bsb, idcode);
    arkime_field_int_add(idcodeField, session, idcode);

    // Skip SOC(4) + FRACSEC(4)
    BSB_IMPORT_skip(bsb, 8);

    // Parse frame-specific payload
    int payloadLen = frameSize - SYNCHROPHASOR_MIN_LEN;
    if (payloadLen <= 0 || BSB_REMAINING(bsb) < (uint32_t)payloadLen + 2)
        return;

    switch (frameType) {
    case 4: { // Command frame
        if (payloadLen >= 2) {
            uint16_t cmd = 0;
            BSB_IMPORT_u16(bsb, cmd);
            arkime_field_int_add(cmdField, session, cmd);
        }
        break;
    }
    case 2:   // Config 1
    case 3:   // Config 2
    case 5: { // Config 3
        // Skip TIME_BASE(4)
        if (frameType == 5 && payloadLen >= 6) {
            BSB_IMPORT_skip(bsb, 2); // CONT_IDX for CFG-3
        }
        if (BSB_REMAINING(bsb) < 6)
            break;
        BSB_IMPORT_skip(bsb, 4); // TIME_BASE

        uint16_t numPMU = 0;
        BSB_IMPORT_u16(bsb, numPMU);
        arkime_field_int_add(numPMUField, session, numPMU);

        // Parse each PMU config
        for (uint16_t i = 0; i < numPMU && !BSB_IS_ERROR(bsb); i++) {
            if (frameType == 5) {
                // CFG-3: variable length station name
                if (BSB_REMAINING(bsb) < 1)
                    break;
                uint8_t nameLen = 0;
                BSB_IMPORT_u08(bsb, nameLen);
                if (BSB_REMAINING(bsb) < nameLen)
                    break;
                const uint8_t *namePtr = BSB_WORK_PTR(bsb);
                arkime_field_string_add(stationField, session, (const char *)namePtr, nameLen, TRUE);
                BSB_IMPORT_skip(bsb, nameLen);
            } else {
                // CFG-1/CFG-2: 16-byte fixed station name
                if (BSB_REMAINING(bsb) < 16)
                    break;
                const uint8_t *namePtr = BSB_WORK_PTR(bsb);
                int nameLen = 16;
                while (nameLen > 0 && namePtr[nameLen - 1] == ' ')
                    nameLen--;
                if (nameLen > 0)
                    arkime_field_string_add(stationField, session, (const char *)namePtr, nameLen, TRUE);
                BSB_IMPORT_skip(bsb, 16);
            }

            // IDCODE(2) + GlobalPMUID(16 for CFG-3) + FORMAT(2)
            BSB_IMPORT_skip(bsb, 2); // IDCODE
            if (frameType == 5)
                BSB_IMPORT_skip(bsb, 16); // Global PMU ID
            BSB_IMPORT_skip(bsb, 2); // FORMAT

            if (BSB_REMAINING(bsb) < 6)
                break;

            uint16_t phnmr = 0, annmr = 0, dgnmr = 0;
            BSB_IMPORT_u16(bsb, phnmr);
            BSB_IMPORT_u16(bsb, annmr);
            BSB_IMPORT_u16(bsb, dgnmr);

            if (frameType == 5) {
                // CFG-3: variable length channel names
                for (uint16_t j = 0; j < phnmr + annmr + dgnmr && !BSB_IS_ERROR(bsb); j++) {
                    uint8_t chNameLen = 0;
                    BSB_IMPORT_u08(bsb, chNameLen);
                    BSB_IMPORT_skip(bsb, chNameLen);
                }
                // PHUNIT(8*phnmr) + ANUNIT(8*annmr) + DIGUNIT(4*dgnmr)
                BSB_IMPORT_skip(bsb, 8 * phnmr + 8 * annmr + 4 * dgnmr);
                // lat(4) + lon(4) + elev(4) + svcClass(1) + window(4) + groupDelay(4)
                BSB_IMPORT_skip(bsb, 21);
            } else {
                // CFG-1/CFG-2: 16-byte fixed channel names
                // PHNAM(16*phnmr) + ANNAM(16*annmr) + DGNAM(16*16*dgnmr)
                BSB_IMPORT_skip(bsb, 16 * phnmr + 16 * annmr + 256 * dgnmr);
                // PHUNIT(4*phnmr) + ANUNIT(4*annmr) + DIGUNIT(4*dgnmr)
                BSB_IMPORT_skip(bsb, 4 * phnmr + 4 * annmr + 4 * dgnmr);
            }

            // FNOM(2) + CFGCNT(2)
            BSB_IMPORT_skip(bsb, 4);
        }

        // DATA_RATE follows all PMU configs
        if (!BSB_IS_ERROR(bsb) && BSB_REMAINING(bsb) >= 2) {
            uint16_t dataRate = 0;
            BSB_IMPORT_u16(bsb, dataRate);
            arkime_field_int_add(dataRateField, session, dataRate);
        }
        break;
    }
    default:
        break;
    }
}
/******************************************************************************/
LOCAL int synchrophasor_tcp_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int which)
{
    ArkimeParserBuf_t *buf = uw;

    arkime_parser_buf_add(buf, which, data, len);

    while (buf->len[which] >= SYNCHROPHASOR_MIN_LEN) {
        // Check for sync byte
        if (buf->buf[which][0] != SYNCHROPHASOR_SYNC_BYTE) {
            // Lost sync, scan for next 0xAA
            const uint8_t *b = buf->buf[which];
            int bufLen = buf->len[which];
            int i;
            for (i = 1; i < bufLen; i++) {
                if (b[i] == SYNCHROPHASOR_SYNC_BYTE)
                    break;
            }
            if (i >= bufLen) {
                arkime_parser_buf_del(buf, which, bufLen);
                return ARKIME_PARSER_UNREGISTER;
            }
            arkime_parser_buf_del(buf, which, i);
            continue;
        }

        // Read frame size from bytes 2-3
        uint16_t frameSize = (buf->buf[which][2] << 8) | buf->buf[which][3];

        if (frameSize < SYNCHROPHASOR_MIN_LEN || frameSize > SYNCHROPHASOR_MAX_FRAME) {
            // Invalid frame size, skip sync byte
            arkime_parser_buf_del(buf, which, 1);
            continue;
        }

        if ((int)frameSize > (int)buf->len[which]) {
            return 0; // Need more data
        }

        synchrophasor_parse_frame(session, buf->buf[which], frameSize);
        arkime_parser_buf_del(buf, which, frameSize);
    }

    return 0;
}
/******************************************************************************/
LOCAL int synchrophasor_udp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    if (len < SYNCHROPHASOR_MIN_LEN)
        return ARKIME_PARSER_UNREGISTER;

    if (data[0] != SYNCHROPHASOR_SYNC_BYTE)
        return ARKIME_PARSER_UNREGISTER;

    synchrophasor_parse_frame(session, data, len);

    return 0;
}
/******************************************************************************/
LOCAL void synchrophasor_tcp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void UNUSED(*uw))
{
    if (len < SYNCHROPHASOR_MIN_LEN)
        return;

    if (arkime_session_has_protocol(session, "synchrophasor"))
        return;

    // Validate version (bits 0-3 of byte 1) should be 1 or 2
    uint8_t version = data[1] & 0x0F;
    if (version < 1 || version > 2)
        return;

    // Validate frame type (bits 4-6 of byte 1) should be 0-5
    uint8_t frameType = (data[1] >> 4) & 0x07;
    if (frameType > 5)
        return;

    // Validate frame size
    uint16_t frameSize = (data[2] << 8) | data[3];
    if (frameSize < SYNCHROPHASOR_MIN_LEN)
        return;

    arkime_session_add_protocol(session, "synchrophasor");

    ArkimeParserBuf_t *info = arkime_parser_buf_create();
    arkime_parsers_register(session, synchrophasor_tcp_parser, info, arkime_parser_buf_session_free);
}
/******************************************************************************/
LOCAL void synchrophasor_udp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void UNUSED(*uw))
{
    ARKIME_RETURN_IF_DNS_PORT;

    if (len < SYNCHROPHASOR_MIN_LEN)
        return;

    if (arkime_session_has_protocol(session, "synchrophasor"))
        return;

    // Validate version
    uint8_t version = data[1] & 0x0F;
    if (version < 1 || version > 2)
        return;

    // Validate frame type
    uint8_t frameType = (data[1] >> 4) & 0x07;
    if (frameType > 5)
        return;

    // Validate frame size
    uint16_t frameSize = (data[2] << 8) | data[3];
    if (frameSize < SYNCHROPHASOR_MIN_LEN || frameSize > len)
        return;

    arkime_session_add_protocol(session, "synchrophasor");

    arkime_parsers_register(session, synchrophasor_udp_parser, NULL, NULL);
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_parsers_classifier_register_tcp("synchrophasor", NULL, 0, (const uint8_t *)"\xaa", 1, synchrophasor_tcp_classify);
    arkime_parsers_classifier_register_udp("synchrophasor", NULL, 0, (const uint8_t *)"\xaa", 1, synchrophasor_udp_classify);

    frameTypeField = arkime_field_define("synchrophasor", "lotermfield",
                                          "synchrophasor.frameType", "Frame Type", "synchrophasor.frameType",
                                          "Synchrophasor frame types",
                                          ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                          (char *)NULL);

    idcodeField = arkime_field_define("synchrophasor", "integer",
                                      "synchrophasor.idcode", "ID Code", "synchrophasor.idcode",
                                      "Synchrophasor data stream ID codes",
                                      ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                      (char *)NULL);

    cmdField = arkime_field_define("synchrophasor", "integer",
                                   "synchrophasor.cmd", "Command", "synchrophasor.cmd",
                                   "Synchrophasor command codes",
                                   ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                   (char *)NULL);

    stationField = arkime_field_define("synchrophasor", "termfield",
                                       "synchrophasor.station", "Station Name", "synchrophasor.station",
                                       "Synchrophasor PMU station names",
                                       ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    dataRateField = arkime_field_define("synchrophasor", "integer",
                                        "synchrophasor.dataRate", "Data Rate", "synchrophasor.dataRate",
                                        "Synchrophasor data rate (frames/sec)",
                                        ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);

    numPMUField = arkime_field_define("synchrophasor", "integer",
                                      "synchrophasor.numPMU", "Number of PMUs", "synchrophasor.numPMU",
                                      "Number of PMUs in synchrophasor config",
                                      ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                      (char *)NULL);
}
