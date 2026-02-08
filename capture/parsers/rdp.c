/* Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

/*
 * RDP (Remote Desktop Protocol)
 *
 * Layered protocol:
 *   TPKT (ISO Transport): 0x03 0x00 <length 2 bytes>
 *   X.224 (COTP): Connection Request (0xE0) / Confirm (0xD0)
 *   RDP Negotiation Request/Response
 *   MCS Connect Initial (contains Client Core Data)
 *
 * Security-relevant fields:
 *   - Username from cookie
 *   - Requested/selected security protocols
 *   - Client name, keyboard layout, build version
 */

extern ArkimeConfig_t        config;

LOCAL int userField;
LOCAL int clientNameField;
LOCAL int keyboardLayoutField;
LOCAL int clientBuildField;
LOCAL int requestedProtocolsField;
LOCAL int selectedProtocolField;

// RDP parser state (stored in ArkimeParserBuf_t.state[0])
#define RDP_STATE_INIT          0
#define RDP_STATE_GOT_REQUEST   1
#define RDP_STATE_GOT_CONFIRM   2
#define RDP_STATE_GOT_MCS       3
#define RDP_STATE_DONE          4

// RDP Negotiation types
#define TYPE_RDP_NEG_REQ  0x01
#define TYPE_RDP_NEG_RSP  0x02
#define TYPE_RDP_NEG_FAILURE 0x03

// Protocol flags
#define PROTOCOL_RDP       0x00
#define PROTOCOL_SSL       0x01
#define PROTOCOL_HYBRID    0x02  // CredSSP + TLS (NLA)
#define PROTOCOL_RDSTLS    0x04
#define PROTOCOL_HYBRID_EX 0x08

// Common keyboard layouts
LOCAL const char *keyboardLayouts[] = {
    [0x0401] = "ar-SA",
    [0x0404] = "zh-TW",
    [0x0405] = "cs-CZ",
    [0x0407] = "de-DE",
    [0x0408] = "el-GR",
    [0x0409] = "en-US",
    [0x040c] = "fr-FR",
    [0x040d] = "he-IL",
    [0x040e] = "hu-HU",
    [0x0410] = "it-IT",
    [0x0411] = "ja-JP",
    [0x0412] = "ko-KR",
    [0x0413] = "nl-NL",
    [0x0415] = "pl-PL",
    [0x0416] = "pt-BR",
    [0x0418] = "ro-RO",
    [0x0419] = "ru-RU",
    [0x041b] = "sk-SK",
    [0x041e] = "th-TH",
    [0x041f] = "tr-TR",
    [0x0422] = "uk-UA",
    [0x0429] = "fa-IR",
    [0x042a] = "vi-VN",
    [0x0804] = "zh-CN",
    [0x0809] = "en-GB",
    [0x0816] = "pt-PT",
    [0x0c0a] = "es-ES"
};

/******************************************************************************/
LOCAL void rdp_add_protocols(ArkimeSession_t *session, int field, uint32_t protocols)
{
    if (protocols == 0) {
        arkime_field_string_add(field, session, "rdp", -1, TRUE);
    }
    if (protocols & PROTOCOL_SSL) {
        arkime_field_string_add(field, session, "ssl", -1, TRUE);
    }
    if (protocols & PROTOCOL_HYBRID) {
        arkime_field_string_add(field, session, "nla", -1, TRUE);
    }
    if (protocols & PROTOCOL_RDSTLS) {
        arkime_field_string_add(field, session, "rdstls", -1, TRUE);
    }
    if (protocols & PROTOCOL_HYBRID_EX) {
        arkime_field_string_add(field, session, "nla-ext", -1, TRUE);
    }
}

/******************************************************************************/
LOCAL void rdp_parse_client_core_data(ArkimeSession_t *session, BSB *bsb)
{
    // Client Core Data (TS_UD_CS_CORE):
    //   version (4), desktopWidth (2), desktopHeight (2), colorDepth (2),
    //   SASSequence (2), keyboardLayout (4), clientBuild (4), clientName (32)...

    if (BSB_REMAINING(*bsb) < 134)
        return;

    BSB_IMPORT_skip(*bsb, 12);  // version, width, height, colorDepth, SASSequence

    uint32_t keyboardLayout = 0;
    BSB_LIMPORT_u32(*bsb, keyboardLayout);

    uint32_t clientBuild = 0;
    BSB_LIMPORT_u32(*bsb, clientBuild);

    // clientName is 32 bytes, UTF-16LE, null-padded
    if (BSB_REMAINING(*bsb) < 32)
        return;

    const uint8_t *clientNamePtr = BSB_WORK_PTR(*bsb);
    BSB_IMPORT_skip(*bsb, 32);

    // Convert UTF-16LE to UTF-8
    GError *error = NULL;
    gsize bread, bwritten;
    char *clientName = g_convert((const char *)clientNamePtr, 32, "UTF-8", "UTF-16LE", &bread, &bwritten, &error);
    if (clientName && bwritten > 0) {
        // Trim nulls
        while (bwritten > 0 && clientName[bwritten - 1] == 0)
            bwritten--;
        if (bwritten > 0)
            arkime_field_string_add(clientNameField, session, clientName, bwritten, TRUE);
        g_free(clientName);
    }
    if (error)
        g_error_free(error);

    // Add keyboard layout
    if (keyboardLayout < ARRAY_LEN(keyboardLayouts) && keyboardLayouts[keyboardLayout]) {
        arkime_field_string_add(keyboardLayoutField, session, keyboardLayouts[keyboardLayout], -1, TRUE);
    } else if (keyboardLayout > 0) {
        char layoutStr[12];
        snprintf(layoutStr, sizeof(layoutStr), "0x%04x", keyboardLayout);
        arkime_field_string_add(keyboardLayoutField, session, layoutStr, -1, TRUE);
    }

    // Add client build
    if (clientBuild > 0) {
        arkime_field_int_add(clientBuildField, session, clientBuild);
    }
}

/******************************************************************************/
LOCAL void rdp_parse_gcc_user_data(ArkimeSession_t *session, BSB *bsb)
{
    // GCC User Data blocks: type (2) + length (2) + data
    while (BSB_REMAINING(*bsb) >= 4 && !BSB_IS_ERROR(*bsb)) {
        uint16_t type = 0, length = 0;
        BSB_LIMPORT_u16(*bsb, type);
        BSB_LIMPORT_u16(*bsb, length);

        if (length < 4 || length > BSB_REMAINING(*bsb) + 4)
            break;

        BSB blockBsb;
        BSB_IMPORT_bsb(*bsb, blockBsb, length - 4);

        // CS_CORE = 0xC001
        if (type == 0xC001) {
            rdp_parse_client_core_data(session, &blockBsb);
        }
    }
}

/******************************************************************************/
LOCAL void rdp_parse_mcs_connect(ArkimeSession_t *session, BSB *bsb)
{
    // MCS Connect Initial is BER encoded
    // We need to find the GCC Conference Create Request inside
    // which contains the Client Core Data

    if (BSB_REMAINING(*bsb) < 30)
        return;

    // Skip MCS header and find GCC data
    // Look for "Duca" magic (0x44756361) which marks GCC user data
    const uint8_t *ptr = BSB_WORK_PTR(*bsb);
    int remaining = BSB_REMAINING(*bsb);

    for (int i = 0; i < remaining - 4; i++) {
        if (ptr[i] == 0x44 && ptr[i + 1] == 0x75 && ptr[i + 2] == 0x63 && ptr[i + 3] == 0x61) {
            // Found "Duca" - user data starts after
            BSB gccBsb;
            BSB_INIT(gccBsb, ptr + i + 4, remaining - i - 4);
            rdp_parse_gcc_user_data(session, &gccBsb);
            break;
        }
    }
}

/******************************************************************************/
LOCAL int rdp_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int which)
{
    ArkimeParserBuf_t *rdp = uw;

    arkime_parser_buf_add(rdp, which, data, len);

    BSB bsb;
    BSB_INIT(bsb, rdp->buf[which], rdp->len[which]);

    while (BSB_REMAINING(bsb) >= 4) {
        const uint8_t *frameStart = BSB_WORK_PTR(bsb);

        // TPKT header
        uint8_t version = 0;
        BSB_IMPORT_u08(bsb, version);

        if (version != 3) {
            rdp->state[which] = RDP_STATE_DONE;
            break;
        }

        BSB_IMPORT_skip(bsb, 1);  // reserved

        uint16_t tpktLen = 0;
        BSB_IMPORT_u16(bsb, tpktLen);

        if (tpktLen < 7 || tpktLen > 8192) {
            rdp->state[which] = RDP_STATE_DONE;
            break;
        }

        // Need full TPKT frame
        int frameOffset = frameStart - rdp->buf[which];
        if (frameOffset + tpktLen > rdp->len[which]) {
            break;  // Wait for more data
        }

        // X.224 header
        uint8_t x224Code = 0;
        BSB_IMPORT_skip(bsb, 1);  // x224 length
        BSB_IMPORT_u08(bsb, x224Code);

        // Connection Request (0xE0)
        if (x224Code == 0xE0) {
            BSB_IMPORT_skip(bsb, 5);  // dst-ref, src-ref, class

            // Look for cookie and RDP negotiation request
            if (BSB_REMAINING(bsb) >= 17) {
                const uint8_t *cookiePtr = BSB_WORK_PTR(bsb);
                if (memcmp(cookiePtr, "Cookie: mstshash=", 17) == 0) {
                    BSB_IMPORT_skip(bsb, 17);
                    const uint8_t *start = BSB_WORK_PTR(bsb);
                    int maxLen = BSB_REMAINING(bsb);
                    int userLen = 0;

                    while (userLen < maxLen && start[userLen] != '\r' && start[userLen] != '\n')
                        userLen++;

                    if (userLen > 0)
                        arkime_field_string_add_lower(userField, session, (char *)start, userLen);

                    BSB_IMPORT_skip(bsb, userLen);
                    // Skip CRLF
                    if (BSB_REMAINING(bsb) >= 2)
                        BSB_IMPORT_skip(bsb, 2);
                }
            }

            // RDP Negotiation Request at end
            if (BSB_REMAINING(bsb) >= 8) {
                uint8_t negType = 0;
                BSB_IMPORT_u08(bsb, negType);

                if (negType == TYPE_RDP_NEG_REQ) {
                    BSB_IMPORT_skip(bsb, 3);  // flags, length
                    uint32_t requestedProtocols = 0;
                    BSB_LIMPORT_u32(bsb, requestedProtocols);
                    rdp_add_protocols(session, requestedProtocolsField, requestedProtocols);
                }
            }
            rdp->state[which] = RDP_STATE_GOT_REQUEST;
        }
        // Connection Confirm (0xD0)
        else if (x224Code == 0xD0) {
            BSB_IMPORT_skip(bsb, 5);  // dst-ref, src-ref, class

            // RDP Negotiation Response at end
            if (BSB_REMAINING(bsb) >= 8) {
                uint8_t negType = 0;
                BSB_IMPORT_u08(bsb, negType);

                if (negType == TYPE_RDP_NEG_RSP) {
                    BSB_IMPORT_skip(bsb, 3);  // flags, length
                    uint32_t selectedProtocol = 0;
                    BSB_LIMPORT_u32(bsb, selectedProtocol);
                    rdp_add_protocols(session, selectedProtocolField, selectedProtocol);
                }
            }
            rdp->state[which] = RDP_STATE_GOT_CONFIRM;
        }
        // Data (0xF0) - could be MCS Connect Initial
        else if (x224Code == 0xF0) {
            BSB_IMPORT_skip(bsb, 1);  // EOT

            // Check for MCS Connect Initial (BER tag 0x7f65)
            if (BSB_REMAINING(bsb) >= 2) {
                uint8_t tag1 = 0, tag2 = 0;
                BSB_IMPORT_u08(bsb, tag1);
                BSB_IMPORT_u08(bsb, tag2);

                if (tag1 == 0x7f && tag2 == 0x65) {
                    // Skip BER length encoding
                    uint8_t lenByte = 0;
                    BSB_IMPORT_u08(bsb, lenByte);
                    if (lenByte & 0x80) {
                        int lenBytes = lenByte & 0x7f;
                        BSB_IMPORT_skip(bsb, lenBytes);
                    }
                    rdp_parse_mcs_connect(session, &bsb);
                    rdp->state[which] = RDP_STATE_GOT_MCS;
                }
            }
        }

        // Skip to end of this TPKT frame
        arkime_parser_buf_del(rdp, which, frameOffset + tpktLen);
        BSB_INIT(bsb, rdp->buf[which], rdp->len[which]);

        // Unregister after we have enough info or both sides done/error
        if (rdp->state[0] >= RDP_STATE_GOT_CONFIRM && rdp->state[1] >= RDP_STATE_GOT_CONFIRM) {
            return ARKIME_PARSER_UNREGISTER;
        }
        if (rdp->state[0] == RDP_STATE_DONE && rdp->state[1] == RDP_STATE_DONE) {
            return ARKIME_PARSER_UNREGISTER;
        }
        if (rdp->state[which] == RDP_STATE_GOT_MCS) {
            if (rdp->state[(which + 1) % 2] >= RDP_STATE_GOT_CONFIRM) {
                return ARKIME_PARSER_UNREGISTER;
            }
        }
    }

    // If this side hit DONE (invalid data), unregister if other side also done or confirmed
    if (rdp->state[which] == RDP_STATE_DONE) {
        if (rdp->state[(which + 1) % 2] >= RDP_STATE_GOT_CONFIRM ||
            rdp->state[(which + 1) % 2] == RDP_STATE_DONE) {
            return ARKIME_PARSER_UNREGISTER;
        }
    }

    return 0;
}

/******************************************************************************/
LOCAL void rdp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "rdp"))
        return;

    if (len < 11)
        return;

    // TPKT: version=3, reserved=0
    if (data[0] != 0x03 || data[1] != 0x00)
        return;

    uint16_t tpktLen = (data[2] << 8) | data[3];
    if (tpktLen > len + 100)  // Allow some slack
        return;

    // X.224 CR (0xE0) or CC (0xD0) - RDP identification requires connection setup
    uint8_t x224Code = data[5];
    if (x224Code != 0xE0 && x224Code != 0xD0)
        return;

    // Positively identify RDP by looking for "Cookie:" or RDP Negotiation Request
    if (len < 12)
        return;
    // Variable data starts at offset 11 (after TPKT(4) + COTP len(1) + type(1) + dst_ref(2) + src_ref(2) + class(1))
    if (len >= 28 && memcmp(data + 11, "Cookie: mstshash=", 17) == 0) {
        // RDP cookie found
    } else if (data[11] == TYPE_RDP_NEG_REQ || data[11] == TYPE_RDP_NEG_RSP || data[11] == TYPE_RDP_NEG_FAILURE) {
        // RDP negotiation request/response
    } else {
        return;
    }

    ArkimeParserBuf_t *rdp = arkime_parser_buf_create();
    arkime_session_add_protocol(session, "rdp");
    arkime_parsers_register(session, rdp_parser, rdp, arkime_parser_buf_session_free);
}

/******************************************************************************/
void arkime_parser_init()
{
    userField = arkime_field_by_db("user");

    clientNameField = arkime_field_define("rdp", "termfield",
                                          "rdp.client-name", "Client Name", "rdp.clientName",
                                          "RDP client computer name",
                                          ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT, (char *)NULL);

    keyboardLayoutField = arkime_field_define("rdp", "termfield",
                                              "rdp.keyboard-layout", "Keyboard Layout", "rdp.keyboardLayout",
                                              "RDP client keyboard layout",
                                              ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT, (char *)NULL);

    clientBuildField = arkime_field_define("rdp", "integer",
                                           "rdp.client-build", "Client Build", "rdp.clientBuild",
                                           "RDP client build number",
                                           ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT, (char *)NULL);

    requestedProtocolsField = arkime_field_define("rdp", "termfield",
                                                  "rdp.requested-protocols", "Requested Protocols", "rdp.requestedProtocols",
                                                  "RDP requested security protocols",
                                                  ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT, (char *)NULL);

    selectedProtocolField = arkime_field_define("rdp", "termfield",
                                                "rdp.selected-protocol", "Selected Protocol", "rdp.selectedProtocol",
                                                "RDP selected security protocol",
                                                ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT, (char *)NULL);

    arkime_parsers_classifier_register_tcp("rdp", NULL, 0, (const uint8_t *)"\x03\x00", 2, rdp_classify);
}
