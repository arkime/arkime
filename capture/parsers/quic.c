/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * https://www.chromium.org/quic
 * https://docs.google.com/document/d/1WJvyZflAO2pq77yOLbp9NsGjC1CHetAXV8I0fQe-B_U
 *
 */
#include "moloch.h"
#include <arpa/inet.h>

extern MolochConfig_t        config;
LOCAL  int hostField;
LOCAL  int uaField;
LOCAL  int versionField;

#define FBZERO_MAX_SIZE 4096
typedef struct {
    unsigned char  data[FBZERO_MAX_SIZE];
    int            pos;
} FBZeroInfo_t;

/******************************************************************************/
LOCAL int quic_chlo_parser(MolochSession_t *session, BSB dbsb) {

    guchar *tag = 0;
    int     tagLen = 0;
    BSB_LIMPORT_ptr(dbsb, tag, 4);
    BSB_LIMPORT_u16(dbsb, tagLen);
    BSB_LIMPORT_skip(dbsb, 2);

    moloch_session_add_protocol(session, "quic");

    if (!tag || memcmp(tag, "CHLO", 4) != 0) {
        return 0;
    }

    guchar *tagDataStart = dbsb.buf + tagLen*8 + 8;
    uint32_t dlen = BSB_REMAINING(dbsb);

    uint32_t start = 0;
    while (!BSB_IS_ERROR(dbsb) && BSB_REMAINING(dbsb) && tagLen > 0) {
        guchar   *subTag = 0;
        uint32_t  endOffset = 0;

        BSB_LIMPORT_ptr(dbsb, subTag, 4);
        BSB_LIMPORT_u32(dbsb, endOffset);

        if (endOffset > dlen || start > dlen || start > endOffset)
            return 1;

        if (!subTag)
            return 1;

        if (memcmp(subTag, "SNI\x00", 4) == 0) {
            moloch_field_string_add(hostField, session, (char *)tagDataStart+start, endOffset-start, TRUE);
        } else if (memcmp(subTag, "UAID", 4) == 0) {
            moloch_field_string_add(uaField, session, (char *)tagDataStart+start, endOffset-start, TRUE);
        } else if (memcmp(subTag, "VER\x00", 4) == 0) {
            moloch_field_string_add(versionField, session, (char *)tagDataStart+start, endOffset-start, TRUE);
        } else {
            //LOG("Subtag: %4.4s len: %d %.*s", subTag, endOffset-start, endOffset-start, tagDataStart+start);
        }
        start = endOffset;
        tagLen--;
    }
    return 1;
}
/******************************************************************************/
LOCAL int quic_udp_parser(MolochSession_t *session, void *UNUSED(uw), const unsigned char *data, int len, int UNUSED(which))
{
    int version = -1;
    int offset = 1;

    // PUBLIC_FLAG_RESET
    if (data[0] & 0x02) {
        return 0;
    }

    // CID
    if (data[0] & 0x08) {
        offset += 8;
    }

    // Get version
    if (data[0] & 0x01 && data[offset] == 'Q') {
        version = (data[offset+1] - '0') * 100 +
                  (data[offset+2] - '0') * 10 +
                  (data[offset+3] - '0');
        offset += 4;
    }

    // Unsupported version
    if (version < 24) {
        moloch_parsers_unregister(session, uw);
        return 0;
    }

    // Diversification only is from server to client, so we can ignore

    // Packet number size
    if ((data[0] & 0x30) == 0) {
        offset++;
    } else {
        offset += ((data[0] & 0x30) >> 4) * 2;
    }

    // Hash
    offset += 12;

    // Private Flags
    if (version < 34)
        offset++;

    if (offset > len)
        return 0;

    BSB bsb;
    BSB_INIT(bsb, data+offset, len-offset);

    while (!BSB_IS_ERROR(bsb) && BSB_REMAINING(bsb)) {
        uint8_t type = 0;
        BSB_LIMPORT_u08(bsb, type);

        //1fdooossB
        if ((type & 0x80) == 0) {
            return 0;
        }

        int offsetLen = 0;
        if (type & 0x1C) {
            offsetLen = ((type & 0x1C) >> 2) + 1;
        }

        int streamLen = (type & 0x03) + 1;

        BSB_LIMPORT_skip(bsb, streamLen + offsetLen);

        int dataLen = BSB_REMAINING(bsb);
        if (type & 0x20) {
            BSB_LIMPORT_u16(bsb, dataLen);
        }

        if (BSB_IS_ERROR(bsb))
            return 0;

        BSB dbsb;
        BSB_INIT(dbsb, BSB_WORK_PTR(bsb), MIN(dataLen, BSB_REMAINING(bsb)));
        BSB_IMPORT_skip(bsb, dataLen);

        quic_chlo_parser(session,dbsb);
        moloch_parsers_unregister(session, uw);
        return 0;
    }

    return 0;
}
/******************************************************************************/
LOCAL void quic_udp_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len > 100 && (data[0] & 0x83) == 0x01) {
        moloch_parsers_register(session, quic_udp_parser, 0, 0);
    }
}
/******************************************************************************/
LOCAL void quic_add(MolochSession_t *UNUSED(session), const unsigned char *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    moloch_session_add_protocol(session, "quic");
}
/******************************************************************************/
LOCAL void quic_fbzero_free(MolochSession_t UNUSED(*session), void *uw)
{
    FBZeroInfo_t            *fbzero          = uw;

    MOLOCH_TYPE_FREE(FBZeroInfo_t, fbzero);
}
/******************************************************************************/
LOCAL int quic_fb_tcp_parser(MolochSession_t *session, void *uw, const unsigned char *data, int remaining, int which)
{
    if (which != 0)
        return 0;

    FBZeroInfo_t *fbzero = uw;

    remaining = MIN(remaining, FBZERO_MAX_SIZE - fbzero->pos);
    memcpy(fbzero->data + fbzero->pos, data, remaining);
    fbzero->pos += remaining;

    if (fbzero->pos < 7)
        return 0;

    int len = (fbzero->data[6] << 8) | fbzero->data[5];
    if (fbzero->pos < len + 9)
        return 0;

    BSB dbsb;
    BSB_INIT(dbsb, fbzero->data+9, len);

    if (quic_chlo_parser(session, dbsb))
        moloch_session_add_protocol(session, "fbzero");

    moloch_parsers_unregister(session, uw);
    return 0;
}

/******************************************************************************/
LOCAL void quic_fb_tcp_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int len, int which, void *UNUSED(uw))
{
    if (which == 0 && len > 13) {
        FBZeroInfo_t *fbzero = MOLOCH_TYPE_ALLOC(FBZeroInfo_t);
        fbzero->pos = 0;
        moloch_parsers_register(session, quic_fb_tcp_parser, fbzero, quic_fbzero_free);
    }
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_parsers_classifier_register_udp("quic", NULL, 9, (const unsigned char *)"Q03", 3, quic_udp_classify);
    moloch_parsers_classifier_register_udp("quic", NULL, 9, (const unsigned char *)"Q02", 3, quic_udp_classify);
    moloch_parsers_classifier_register_tcp("fbzero", NULL, 0, (const unsigned char *)"\x31QTV", 4, quic_fb_tcp_classify);
    moloch_parsers_classifier_register_udp("quic", NULL, 9, (const unsigned char *)"PRST", 4, quic_add);

    hostField = moloch_field_define("quic", "lotermfield",
        "host.quic", "Hostname", "quic.host", 
        "QUIC host header field", 
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT, 
        "aliases", "[\"quic.host\"]", NULL);

    uaField = moloch_field_define("quic", "termfield",
        "quic.user-agent", "User-Agent", "quic.useragent",
        "User-Agent",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL);

    versionField = moloch_field_define("quic", "termfield",
        "quic.version", "Version", "quic.version",
        "QUIC Version",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL);
}
