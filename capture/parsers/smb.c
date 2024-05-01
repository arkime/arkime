/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

//#define SMBDEBUG

extern ArkimeConfig_t   config;

LOCAL  int domainField;
LOCAL  int userField;
LOCAL  int hostField;
LOCAL  int osField;
LOCAL  int verField;
LOCAL  int fnField;
LOCAL  int shareField;
LOCAL  int dialectField;

#define MAX_SMB_BUFFER 8192
#define MAX_SMB1_DIALECTS 10
typedef struct {
    char               buf[2][MAX_SMB_BUFFER];
    uint32_t           remlen[2];
    short              buflen[2];
    uint16_t           flags2[2];
    uint8_t            version[2];
    char               state[2];
    char              *dialects[MAX_SMB1_DIALECTS];
    uint8_t            dialectsLen;
} SMBInfo_t;

// States
#define SMB_NETBIOS             0
#define SMB_SMBHEADER           1
#define SMB_SKIP                2
#define SMB1_TREE_CONNECT_ANDX 10
#define SMB1_DELETE            11
#define SMB1_OPEN_ANDX         12
#define SMB1_CREATE_ANDX       13
#define SMB1_SETUP_ANDX        14
#define SMB1_NEGOTIATE_REQ     15
#define SMB1_NEGOTIATE_RSP     16

#define SMB2_TREE_CONNECT      20
#define SMB2_CREATE            21
#define SMB2_NEGOTIATE         22

// SMB1 Flags
#define SMB1_FLAGS_REPLY       0x80
#define SMB1_FLAGS2_UNICODE    0x8000

// SMB2 Flags
#define SMB2_FLAGS_SERVER_TO_REDIR 0x00000001

/******************************************************************************/
LOCAL void smb_add_string(ArkimeSession_t *session, int field, const char *buf, int len, int useunicode)
{
    if (len == 0)
        return;

    GError *error = 0;
    gsize bread, bwritten;

    if (useunicode) {
        char *out = g_convert(buf, len, "utf-8", "ucs-2le", &bread, &bwritten, &error);
        if (error) {
            if (config.debug)
                LOG("ERROR %s", error->message);
            g_error_free(error);
        } else {
            if (!arkime_field_string_add(field, session, out, -1, FALSE)) {
                g_free(out);
            }
        }
    } else {
        arkime_field_string_add(field, session, buf, len, TRUE);
    }
}
/******************************************************************************/
// 2.2.13 AUTHENTICATE_MESSAGE from  http://download.microsoft.com/download/9/5/E/95EF66AF-9026-4BB0-A41D-A4F81802D92C/[MS-NLMP].pdf
LOCAL void smb_security_blob(ArkimeSession_t *session, uint8_t *data, int len)
{
    BSB bsb;

    BSB_INIT(bsb, data, len);

    uint32_t apc, atag, alen;
    uint8_t *value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (atag != 1)
        return;

    BSB_INIT(bsb, value, alen);
    value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (atag != 16)
        return;

    BSB_INIT(bsb, value, alen);
    value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);
    if (atag != 2)
        return;

    BSB_INIT(bsb, value, alen);
    value = arkime_parsers_asn_get_tlv(&bsb, &apc, &atag, &alen);

    if (atag != 4 || alen < 7 || memcmp("NTLMSSP", value, 7) != 0)
        return;

    /* Woot, have the part we need to decode */
    BSB_INIT(bsb, value, alen);
    BSB_IMPORT_skip(bsb, 8);

    int type = 0;
    BSB_LIMPORT_u32(bsb, type);

    if (type != 3) // auth type
        return;

    uint32_t lens[6], offsets[6];
    int i;
    for (i = 0; i < 6; i++) {
        BSB_LIMPORT_u16(bsb, lens[i]);
        BSB_IMPORT_skip(bsb, 2);
        BSB_LIMPORT_u32(bsb, offsets[i]);

        if (BSB_IS_ERROR(bsb) || offsets[i] > BSB_SIZE(bsb) || lens[i] > BSB_SIZE(bsb) || offsets[i] + lens[i] > BSB_SIZE(bsb)) {
            arkime_session_add_tag(session, "smb:bad-security-blob");
            return;
        }
    }

    if (BSB_IS_ERROR(bsb))
        return;

    if (lens[2]) {
        smb_add_string(session, domainField, (char *)value + offsets[2], lens[2], TRUE);
    }
    if (lens[3]) {
        smb_add_string(session, userField, (char *)value + offsets[3], lens[3], TRUE);
    }
    if (lens[4]) {
        smb_add_string(session, hostField, (char *)value + offsets[4], lens[4], TRUE);
    }
}
/******************************************************************************/
LOCAL void smb1_str_null_split(char *buf, int len, char **out, int max)
{
    memset(out, 0, max * sizeof(char *));
    int i, p;
    int start = 0;
    for (i = 0, p = 0; i < len && p < max; i++) {
        if (buf[i] == 0) {
            out[p] = buf + start;
            start = i + 1;
            p++;
        }
    }
}
/******************************************************************************/
LOCAL void smb1_parse_osverdomain(ArkimeSession_t *session, char *buf, int len, int useunicode)
{
    char        *out;
    gsize        bread, bwritten;
    GError      *error = 0;

    if (useunicode)
        out = g_convert(buf, len, "utf-8", "ucs-2le", &bread, &bwritten, &error);
    else {
        out = buf;
        bwritten = len;
    }

    if (error) {
        if (config.debug)
            LOG("ERROR %s", error->message);
        g_error_free(error);
        return;
    }

    char *outs[3];
    smb1_str_null_split(out, bwritten, outs, 3);

    if (outs[0] && *outs[0])
        arkime_field_string_add(osField, session, outs[0], -1, TRUE);
    if (outs[1] && *outs[1])
        arkime_field_string_add(verField, session, outs[1], -1, TRUE);
    if (outs[2] && *outs[2])
        arkime_field_string_add(domainField, session, outs[2], -1, TRUE);

    if (useunicode) {
        g_free(out);
    }
}
/******************************************************************************/
LOCAL void smb1_parse_userdomainosver(ArkimeSession_t *session, char *buf, int len, int useunicode)
{
    char        *out;
    gsize        bread, bwritten;
    GError      *error = 0;

    if (useunicode)
        out = g_convert(buf, len, "utf-8", "ucs-2le", &bread, &bwritten, &error);
    else {
        out = buf;
        bwritten = len;
    }

    if (error) {
        if (config.debug)
            LOG("ERROR %s", error->message);
        g_error_free(error);
        return;
    }

    char *outs[4];
    smb1_str_null_split(out, bwritten, outs, 4);

    if (outs[0] && *outs[0])
        arkime_field_string_add(userField, session, outs[0], -1, TRUE);
    if (outs[1] && *outs[1])
        arkime_field_string_add(domainField, session, outs[1], -1, TRUE);
    if (outs[2] && *outs[2])
        arkime_field_string_add(osField, session, outs[2], -1, TRUE);
    if (outs[3] && *outs[3])
        arkime_field_string_add(verField, session, outs[3], -1, TRUE);

    if (useunicode) {
        g_free(out);
    }
}
/******************************************************************************/
LOCAL void smb1_parse_negotiate_request(SMBInfo_t *smb, char *buf, int len)
{
    BSB bsb;
    BSB_INIT(bsb, buf, len);

    if (smb->dialectsLen >= MAX_SMB1_DIALECTS)
        return;

    while (BSB_REMAINING(bsb) > 0) {
        BSB_IMPORT_skip(bsb, 1);
        char *start = (char *)BSB_WORK_PTR(bsb);
        while (BSB_REMAINING(bsb) > 0 && *(BSB_WORK_PTR(bsb)) != 0)
            BSB_IMPORT_skip(bsb, 1);
        if (BSB_REMAINING(bsb) == 0)
            break;
        smb->dialects[smb->dialectsLen] = g_strdup(start);
        smb->dialectsLen++;
        if (smb->dialectsLen >= MAX_SMB1_DIALECTS)
            break;
        BSB_IMPORT_skip(bsb, 1);
    }
}
/******************************************************************************/
LOCAL int smb1_parse(ArkimeSession_t *session, SMBInfo_t *smb, BSB *bsb, char *state, uint32_t *remlen, int which)
{
    const uint8_t *start = BSB_WORK_PTR(*bsb);

    switch (*state) {
    case SMB_SMBHEADER: {
        uint8_t  cmd    = 0;
        uint8_t  flags = 0;
        if (BSB_REMAINING(*bsb) < 32) {
            return 1;
        }
        BSB_IMPORT_skip(*bsb, 4);
        BSB_IMPORT_u08(*bsb, cmd);
        BSB_IMPORT_skip(*bsb, 4);
        BSB_IMPORT_u08(*bsb, flags);
        BSB_LIMPORT_u16(*bsb, smb->flags2[which]);
        BSB_IMPORT_skip(*bsb, 20);
        if ((flags & SMB1_FLAGS_REPLY) == 0) {
            switch (cmd) {
            case 0x06:
                *state = SMB1_DELETE;
                break;
            case 0x2d:
                *state = SMB1_OPEN_ANDX;
                break;
            case 0x72:
                *state = SMB1_NEGOTIATE_REQ;
                break;
            case 0x73:
                *state = SMB1_SETUP_ANDX;
                break;
            case 0x75:
                *state = SMB1_TREE_CONNECT_ANDX;
                break;
            case 0xa2:
                *state = SMB1_CREATE_ANDX;
                break;
            default:
                *state = SMB_SKIP;
            }
        } else {
            switch (cmd) {
            case 0x72:
                *state = SMB1_NEGOTIATE_RSP;
                break;
            default:
                *state = SMB_SKIP;
            }
        }
#ifdef SMBDEBUG
        LOG("%d cmd: %x flags2: %x newstate: %d remlen: %u", which, cmd, smb->flags2[which], *state, *remlen);
#endif
        break;
    }
    case SMB1_CREATE_ANDX:
    case SMB1_OPEN_ANDX: {
        if (BSB_REMAINING(*bsb) < *remlen) {
            return 1;
        }
        int wordcount = 0;
        BSB_IMPORT_u08(*bsb, wordcount);
        BSB_IMPORT_skip(*bsb, wordcount * 2 + 3);
        smb_add_string(session, fnField, (char *)BSB_WORK_PTR(*bsb), BSB_REMAINING(*bsb), smb->flags2[which] & SMB1_FLAGS2_UNICODE);
        *state = SMB_SKIP;
        break;
    }
    case SMB1_DELETE: {
        if (BSB_REMAINING(*bsb) < *remlen) {
            return 1;
        }
        int wordcount = 0;
        BSB_IMPORT_u08(*bsb, wordcount);
        BSB_IMPORT_skip(*bsb, wordcount * 2 + 3);
        if (BSB_IS_ERROR(*bsb))
            return 1;
        smb_add_string(session, fnField, (char *)BSB_WORK_PTR(*bsb), BSB_REMAINING(*bsb), smb->flags2[which] & SMB1_FLAGS2_UNICODE);
        *state = SMB_SKIP;
        break;
    }
    case SMB1_TREE_CONNECT_ANDX: {
        if (BSB_REMAINING(*bsb) < *remlen) {
            return 1;
        }
        int passlength = 0;
        BSB_IMPORT_skip(*bsb, 6);
        BSB_IMPORT_u16(*bsb, passlength);
        BSB_IMPORT_skip(*bsb, 2 + passlength);

        uint32_t offset = ((BSB_WORK_PTR(*bsb) - start) % 2 == 0) ? 2 : 1;

        if (BSB_IS_ERROR(*bsb) || offset > BSB_REMAINING(*bsb)) {
            return 1;
        }
        smb_add_string(session, shareField, (char *)BSB_WORK_PTR(*bsb) + offset, BSB_REMAINING(*bsb) - offset, smb->flags2[which] & SMB1_FLAGS2_UNICODE);
        *state = SMB_SKIP;
        break;
    }

    case SMB1_SETUP_ANDX: { // http://msdn.microsoft.com/en-us/library/ee441849.aspx
        if (BSB_REMAINING(*bsb) < *remlen) {
            BSB_SET_ERROR(*bsb);
            return 1;
        }
        int wordcount = 0;
        BSB_IMPORT_u08(*bsb, wordcount);

        if (wordcount == 12) {
            BSB_IMPORT_skip(*bsb, 14);

            int securitylen = 0;
            BSB_LIMPORT_u16(*bsb, securitylen);

            BSB_IMPORT_skip(*bsb, 10);

            if (securitylen > BSB_REMAINING(*bsb)) {
                BSB_SET_ERROR(*bsb);
                return 1;
            }
            smb_security_blob(session, BSB_WORK_PTR(*bsb), securitylen);
            BSB_IMPORT_skip(*bsb, securitylen);

            uint32_t offset = ((BSB_WORK_PTR(*bsb) - start) % 2 == 0) ? 0 : 1;
            BSB_IMPORT_skip(*bsb, offset);

            if (!BSB_IS_ERROR(*bsb)) {
                smb1_parse_osverdomain(session, (char *)BSB_WORK_PTR(*bsb), BSB_REMAINING(*bsb), smb->flags2[which] & SMB1_FLAGS2_UNICODE);
            }
        } else if (wordcount == 13) {
            BSB_IMPORT_skip(*bsb, 14);

            int ansipw = 0;
            BSB_LIMPORT_u16(*bsb, ansipw);
            int upw = 0;
            BSB_LIMPORT_u16(*bsb, upw);

            BSB_IMPORT_skip(*bsb, 10 + ansipw + upw);

            uint32_t offset = ((BSB_WORK_PTR(*bsb) - start) % 2 == 0) ? 0 : 1;
            BSB_IMPORT_skip(*bsb, offset);

            if (!BSB_IS_ERROR(*bsb)) {
                smb1_parse_userdomainosver(session, (char *)BSB_WORK_PTR(*bsb), BSB_REMAINING(*bsb), smb->flags2[which] & SMB1_FLAGS2_UNICODE);
            }
        }

        *state = SMB_SKIP;
        break;
    }
    case SMB1_NEGOTIATE_REQ: {
        if (BSB_REMAINING(*bsb) < *remlen) {
            BSB_SET_ERROR(*bsb);
            return 1;
        }
        BSB_LIMPORT_skip(*bsb, 1); // wordcount

        int bytecount = 0;
        BSB_LIMPORT_u08(*bsb, bytecount);

        if (bytecount > 0)
            smb1_parse_negotiate_request(smb, (char *)BSB_WORK_PTR(*bsb), BSB_REMAINING(*bsb));

        *state = SMB_SKIP;
        break;
    }
    case SMB1_NEGOTIATE_RSP: {
        if (BSB_REMAINING(*bsb) < *remlen) {
            BSB_SET_ERROR(*bsb);
            return 1;
        }
        int wordcount = 0;
        BSB_IMPORT_u08(*bsb, wordcount);

        if (wordcount < 13) {
            *state = SMB_SKIP;
            break;
        }

        uint16_t dialect = 0;
        BSB_IMPORT_u08(*bsb, dialect);
        if (dialect < smb->dialectsLen) {
            arkime_field_string_add(dialectField, session, smb->dialects[dialect], -1, TRUE);
        }

        *state = SMB_SKIP;
        break;
    }
    } /* switch */

    *remlen -= (BSB_WORK_PTR(*bsb) - start);
    return 0;
}
/******************************************************************************/
LOCAL int smb2_parse(ArkimeSession_t *session, SMBInfo_t *smb, BSB *bsb, char *state, uint32_t *remlen, int UNUSED(which))
{
    const uint8_t *start = BSB_WORK_PTR(*bsb);

    switch (*state) {
    case SMB_SMBHEADER: {
        uint16_t  flags = 0;
        uint16_t  cmd = 0;

        if (BSB_REMAINING(*bsb) < 64) {
            return 1;
        }
        BSB_IMPORT_skip(*bsb, 12);
        BSB_LIMPORT_u16(*bsb, cmd);
        BSB_IMPORT_skip(*bsb, 2);
        BSB_LIMPORT_u32(*bsb, flags);
        BSB_IMPORT_skip(*bsb, 44);

        if ((flags & SMB2_FLAGS_SERVER_TO_REDIR) == 0) {
            switch (cmd) {
            case 0x03:
                *state = SMB2_TREE_CONNECT;
                break;
            case 0x05:
                *state = SMB2_CREATE;
                break;
            default:
                *state = SMB_SKIP;
            }
        } else {
            switch (cmd) {
            case 0x00:
                *state = SMB2_NEGOTIATE;
                break;
            default:
                *state = SMB_SKIP;
            }
        }
#ifdef SMBDEBUG
        LOG("%d cmd: %x flags: %x newstate: %d remlen: %u", which, cmd, flags, *state, *remlen);
#endif
        *remlen -= (BSB_WORK_PTR(*bsb) - start);
        break;
    }
    case SMB2_NEGOTIATE: {
        if (BSB_REMAINING(*bsb) < *remlen) {
            return 1;
        }

        BSB_IMPORT_skip(*bsb, 4);

        uint16_t  dialect = 0;
        BSB_LIMPORT_u16(*bsb, dialect);
        if (dialect != 0 && dialect != 0x02FF) {
            char str[11];
            snprintf(str, sizeof(str), "SMB %d.%d.%d", (dialect >> 8) & 0xf, (dialect >> 4) & 0xf, dialect & 0xf);
            arkime_field_string_add(dialectField, session, str, -1, TRUE);
        }

        *remlen -= (BSB_WORK_PTR(*bsb) - start);
        *state = SMB_SKIP;
        break;
    }
    case SMB2_TREE_CONNECT: {
        uint16_t  pathoffset = 0;
        uint16_t  pathlen = 0;

        if (BSB_REMAINING(*bsb) < *remlen) {
            return 1;
        }
        BSB_IMPORT_skip(*bsb, 4);
        BSB_LIMPORT_u16(*bsb, pathoffset);
        BSB_LIMPORT_u16(*bsb, pathlen);
        pathoffset -= (64 + 8);
        BSB_IMPORT_skip(*bsb, pathoffset);

        if (!BSB_IS_ERROR(*bsb) && pathlen < BSB_REMAINING(*bsb)) {
            smb_add_string(session, shareField, (char *)BSB_WORK_PTR(*bsb), pathlen, smb->flags2[which] & SMB1_FLAGS2_UNICODE);
        }

        *remlen -= (BSB_WORK_PTR(*bsb) - start);
        *state = SMB_SKIP;
        break;
    }
    case SMB2_CREATE: {
        uint16_t  nameoffset = 0;
        uint16_t  namelen = 0;

        if (BSB_REMAINING(*bsb) < *remlen) {
            return 1;
        }
        BSB_IMPORT_skip(*bsb, 44);
        BSB_LIMPORT_u16(*bsb, nameoffset);
        BSB_LIMPORT_u16(*bsb, namelen);
        nameoffset -= (64 + 48);
        BSB_IMPORT_skip(*bsb, nameoffset);

        if (!BSB_IS_ERROR(*bsb) && namelen < BSB_REMAINING(*bsb)) {
            gsize bread, bwritten;
            GError      *error = 0;
            char *out = g_convert((char *)BSB_WORK_PTR(*bsb), namelen, "utf-8", "ucs-2le", &bread, &bwritten, &error);
            if (error) {
                LOG_RATE(5, "ERROR %s", error->message);
                g_error_free(error);
            } else {
                if (!arkime_field_string_add(fnField, session, out, -1, FALSE)) {
                    g_free(out);
                }
            }
        }

        *remlen -= (BSB_WORK_PTR(*bsb) - start);
        *state = SMB_SKIP;
        break;
    }
    }

    return 0;
}
/******************************************************************************/
LOCAL int smb_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int remaining, int which)
{
    SMBInfo_t            *smb          = uw;
    char                 *state        = &smb->state[which];
    char                 *buf          = smb->buf[which];
    short                *buflen       = &smb->buflen[which];
    uint32_t             *remlen       = &smb->remlen[which];

#ifdef SMBDEBUG
    LOG("ENTER: remaining: %d state: %d buflen: %d remlen: %u", remaining, *state, *buflen, *remlen);
#endif

    while (remaining > 0) {
        BSB bsb;
        int done = 0;

        if (*buflen == 0) {
            BSB_INIT(bsb, data, remaining);
            data += remaining;
            remaining = 0;
        } else {
            int len = MIN(remaining, MAX_SMB_BUFFER - (*buflen));
            memcpy(buf + (*buflen), data, len);
            (*buflen) += len;
            remaining -= len;
            data += len;
            BSB_INIT(bsb, buf, *buflen);
        }

        if (*state != SMB_SKIP && *remlen > MAX_SMB_BUFFER) {
#ifndef FUZZLOCH
            LOG("WARNING - Not enough room to parse SMB packet of size %u", *remlen);
#endif
            arkime_parsers_unregister(session, smb);
            return 0;
        }

        while (!done && BSB_REMAINING(bsb) > 0) {
#ifdef SMBDEBUG
            LOG(" S: bsbremaining: %u remaining: %d state: %d buflen: %d remlen: %u done: %d", (uint32_t)BSB_REMAINING(bsb), remaining, *state, *buflen, *remlen, done);
#endif
            switch (*state) {
            case SMB_NETBIOS:
                if (BSB_REMAINING(bsb) < 5) {
                    done = 1;
                    break;
                }

                BSB_IMPORT_skip(bsb, 1);
                BSB_IMPORT_u24(bsb, *remlen);
                // Peak at SMBHEADER for version
                smb->version[which] = *(BSB_WORK_PTR(bsb));
                *state = SMB_SMBHEADER;
                break;
            case SMB_SKIP:
                if (BSB_REMAINING(bsb) < *remlen) {
                    *remlen -= BSB_REMAINING(bsb);
                    BSB_IMPORT_skip(bsb, BSB_REMAINING(bsb));
                } else {
                    BSB_IMPORT_skip(bsb, *remlen);
                    *remlen = 0;
                    *state = SMB_NETBIOS;
                }
                break;
            default:
                if (smb->version[which] == 0xff) {
                    done = smb1_parse(session, smb, &bsb, state, remlen, which);
                } else {
                    done = smb2_parse(session, smb, &bsb, state, remlen, which);
                }
            }

#ifdef SMBDEBUG
            LOG(" E: bsbremaining: %u remaining: %d state: %d buflen: %d remlen: %u done: %d", (uint32_t)BSB_REMAINING(bsb), remaining, *state, *buflen, *remlen, done);
#endif
        }

        if (BSB_IS_ERROR(bsb)) {
            arkime_parsers_unregister(session, smb);
            return 0;
        }

        if (BSB_REMAINING(bsb) > 0 && BSB_WORK_PTR(bsb) != (uint8_t *)buf) {
#ifdef SMBDEBUG
            //LOG("  Moving data %ld %s", BSB_REMAINING(bsb), arkime_session_id_string(session->protocol, session->addr1, session->port1, session->addr2, session->port2));
#endif
            if (BSB_REMAINING(bsb) > MAX_SMB_BUFFER) {
                LOG("WARNING - Not enough room to parse SMB packet of size %u", (uint32_t)BSB_REMAINING(bsb));
                arkime_parsers_unregister(session, smb);
                return 0;
            }
            memmove(buf, BSB_WORK_PTR(bsb), BSB_REMAINING(bsb));
        }
        *buflen = BSB_REMAINING(bsb);
    }
    return 0;
}
/******************************************************************************/
LOCAL void smb_free(ArkimeSession_t UNUSED(*session), void *uw)
{
    SMBInfo_t            *smb          = uw;

    for (int i = 0; i < smb->dialectsLen; i++) {
        g_free(smb->dialects[i]);
    }

    ARKIME_TYPE_FREE(SMBInfo_t, smb);
}
/******************************************************************************/
LOCAL void smb_classify(ArkimeSession_t *session, const uint8_t *data, int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    if (data[4] != 0xff && data[4] != 0xfe)
        return;

    if (arkime_session_has_protocol(session, "smb"))
        return;

    arkime_session_add_protocol(session, "smb");

    SMBInfo_t            *smb          = ARKIME_TYPE_ALLOC0(SMBInfo_t);

    arkime_parsers_register(session, smb_parser, smb, smb_free);
}
/******************************************************************************/
void arkime_parser_init()
{
    shareField = arkime_field_define("smb", "termfield",
                                     "smb.share", "Share", "smb.share",
                                     "SMB shares connected to",
                                     ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                     (char *)NULL);

    fnField = arkime_field_define("smb", "termfield",
                                  "smb.fn", "Filename", "smb.filename",
                                  "SMB files opened, created, deleted",
                                  ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                  (char *)NULL);

    osField = arkime_field_define("smb", "termfield",
                                  "smb.os", "OS", "smb.os",
                                  "SMB OS information",
                                  ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                  (char *)NULL);

    domainField = arkime_field_define("smb", "termfield",
                                      "smb.domain", "Domain", "smb.domain",
                                      "SMB domain",
                                      ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                      (char *)NULL);

    verField = arkime_field_define("smb", "termfield",
                                   "smb.ver", "Version", "smb.version",
                                   "SMB Version information",
                                   ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                   (char *)NULL);

    dialectField = arkime_field_define("smb", "termfield",
                                       "smb.dialect", "Dialect", "smb.dialect",
                                       "SMB Dialect information",
                                       ARKIME_FIELD_TYPE_STR,  0,
                                       (char *)NULL);

    userField = arkime_field_define("smb", "termfield",
                                    "smb.user", "User", "smb.user",
                                    "SMB User",
                                    ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                    "category", "user",
                                    (char *)NULL);

    hostField = arkime_field_define("smb", "termfield",
                                    "host.smb", "Hostname", "smb.host",
                                    "SMB Host name",
                                    ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                    "category", "host",
                                    "aliases", "[\"smb.host\"]",
                                    (char *)NULL);

    arkime_field_define("smb", "lotextfield",
                        "host.smb.tokens", "Hostname Tokens", "smb.hostTokens",
                        "SMB Host Tokens",
                        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_FAKE,
                        "aliases", "[\"smb.host.tokens\"]",
                        (char *)NULL);

    arkime_parsers_classifier_register_tcp("smb", NULL, 5, (uint8_t *)"SMB", 3, smb_classify);
}
