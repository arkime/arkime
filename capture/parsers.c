/* parsers.c  -- Functions for dealing with classification and parsers
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
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
 */

#include "moloch.h"
#include <fcntl.h>
#include "gmodule.h"
#include "magic.h"
#include "bsb.h"

//#define DEBUG_PARSERS 1

/******************************************************************************/
extern MolochConfig_t        config;
static gchar                 classTag[100];

static magic_t               cookie[MOLOCH_MAX_PACKET_THREADS];

extern unsigned char         moloch_char_to_hexstr[256][3];

int    userField;

enum MolochMagicMode { MOLOCH_MAGICMODE_LIBMAGIC, MOLOCH_MAGICMODE_MOLOCHMAGIC, MOLOCH_MAGICMODE_BASIC, MOLOCH_MAGICMODE_NONE};

LOCAL enum MolochMagicMode magicMode;
/*############################## molochmagic ##############################*/

typedef struct {
    uint8_t *match;
    char    *mime;
    short    matchlen;
    short    mimelen;
    uint8_t  ignoreCase;
} MolochMagic_t;

#define MOLOCH_MAGIC_MAX_SEARCH 100
#define MOLOCH_MAGIC_MAX_OFFSET 15

LOCAL MolochMagic_t magicSearch[MOLOCH_MAGIC_MAX_SEARCH];
LOCAL int           magicSearchLen;

typedef struct {
    MolochMagic_t **magic;
    uint8_t         num;
    uint8_t         size;
} MolochMagicList_t;
LOCAL MolochMagicList_t magicMatch[MOLOCH_MAGIC_MAX_OFFSET][256][256];

int magicOffset[MOLOCH_MAGIC_MAX_OFFSET];
int magicOffsetNum;

/******************************************************************************/
void moloch_parsers_molochmagic_add_list(MolochMagicList_t *list, MolochMagic_t *magic)
{
    if (list->size == 0) {
        list->size = 3;
        list->magic = malloc(list->size * sizeof(MolochMagic_t *));
    } else if (list->size == list->num) {
        list->size *= 1.5;
        list->magic = realloc(list->magic, list->size * sizeof(MolochMagic_t *));
    }

    list->magic[list->num] = magic;
    list->num++;
}
/******************************************************************************/
void moloch_parsers_molochmagic_add(int offset, uint8_t *match, int matchlen, char *mime, int ignoreCase)
{
    int offsetPos;
    for (offsetPos = 0; offsetPos < magicOffsetNum; offsetPos++) {
        if (magicOffset[offsetPos] == offset) {
            break;
        }
    }

    if (offsetPos == MOLOCH_MAGIC_MAX_OFFSET) {
        LOG("Too many offsets, can't add %d %s", offset, mime);
        return;
    }

    if (offsetPos == magicOffsetNum) {
        magicOffset[magicOffsetNum] = offset;
        magicOffsetNum++;
    }


    MolochMagic_t *magic = MOLOCH_TYPE_ALLOC(MolochMagic_t);

    magic->match = match;
    magic->matchlen = matchlen;
    magic->mime = mime;
    magic->mimelen = strlen(mime);
    magicSearch[magicSearchLen].ignoreCase = ignoreCase;

    if (ignoreCase) {
        moloch_parsers_molochmagic_add_list(&magicMatch[offsetPos][tolower(match[0])][tolower(match[1])], magic);
        moloch_parsers_molochmagic_add_list(&magicMatch[offsetPos][tolower(match[0])][toupper(match[1])], magic);
        moloch_parsers_molochmagic_add_list(&magicMatch[offsetPos][toupper(match[0])][tolower(match[1])], magic);
        moloch_parsers_molochmagic_add_list(&magicMatch[offsetPos][toupper(match[0])][toupper(match[1])], magic);
    } else {
        moloch_parsers_molochmagic_add_list(&magicMatch[offsetPos][match[0]][match[1]], magic);
    }
}

/******************************************************************************/
void moloch_parsers_molochmagic_add_search(uint8_t *match, int matchlen, char *mime, int ignoreCase)
{
    if (magicSearchLen >= MOLOCH_MAGIC_MAX_SEARCH) {
        LOG("Too many searches, can't add %s", mime);
        return;
    }
    magicSearch[magicSearchLen].match = match;
    magicSearch[magicSearchLen].matchlen = matchlen;
    magicSearch[magicSearchLen].mime = mime;
    magicSearch[magicSearchLen].mimelen = strlen(mime);
    magicSearch[magicSearchLen].ignoreCase = ignoreCase;

    magicSearchLen++;
}

/******************************************************************************/
const char *moloch_parsers_magic_basic(MolochSession_t *session, int field, const char *data, int len)
{
    switch (data[0]) {
    case 0:
        if (len > 10 && memcmp(data+4, "ftyp", 4) == 0) {
            if (memcmp(data+8, "qt", 2) == 0) {
                return moloch_field_string_add(field, session, "video/quicktime", 15, TRUE);
            }
            if (memcmp(data+8, "3g", 2) == 0) {
                return moloch_field_string_add(field, session, "video/3gpp", 10, TRUE);
            }
        } else if (memcmp(data, "\000\001\000\000\000", 5) == 0) {
            return moloch_field_string_add(field, session, "application/x-font-ttf", 22, TRUE);
        }
        break;
    case '\032':
        if (memcmp(data, "\x1a\x45\xdf\xa3", 4) == 0) {
            if (moloch_memstr(data+4, len-4, "webm", 4)) {
                return moloch_field_string_add(field, session, "video/webm", 10, TRUE);
            }
            if (moloch_memstr(data+4, len-4, "matroska", 8)) {
                return moloch_field_string_add(field, session, "video/x-matroska", 16, TRUE);
            }
        }
        break;
    case '\037':
        if (data[1] == '\213') {
            return moloch_field_string_add(field, session, "application/x-gzip", 18, TRUE);
        }
        if (data[1] == '\235') {
            return moloch_field_string_add(field, session, "application/x-compress", 22, TRUE);
        }
        break;
#ifdef OID_DECODE_SOMEDAY
    case 0x30:
        if (len > 100 && (gchar)data[1] == (gchar)0x82) {
            MolochASNSeq_t seq[5];
            int i;
            int num = moloch_parsers_asn_get_sequence(seq, 5, (unsigned char *)data, len, TRUE);
            for (i = 0; i < num; i++) {
                if (seq[i].pc && seq[i].tag == 16) {
                    BSB tbsb;
                    BSB_INIT(tbsb, seq[i].value, seq[i].len);
                    uint32_t ipc, itag, ilen;
                    unsigned char *ivalue;
                    ivalue = moloch_parsers_asn_get_tlv(&tbsb, &ipc, &itag, &ilen);
                    if (itag != 6)
                        continue;
                    char oid[100];
                    moloch_parsers_asn_decode_oid(oid, sizeof(oid), ivalue, ilen);
                    printf("%s ", oid);
                    moloch_print_hex_string(ivalue, ilen);
                    if (ilen == 9 && memcmp(ivalue, "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x05", 9) == 0) {
                    }
                }
            }
        }
        break;
#endif
    case '#':
        if (data[1] == '!') {
            return moloch_field_string_add(field, session, "text/x-shellscript", 18, TRUE);
        }
        break;
    case '%':
        if (memcmp(data, "%PDF-", 5) == 0) {
            return moloch_field_string_add(field, session, "application/pdf", 15, TRUE);
        }
        break;
    case '<':
        switch(data[1]) {
        case '!':
            if (len > 14 && strncasecmp(data, "<!doctype html", 14) == 0) {
                return moloch_field_string_add(field, session, "text/html", 9, TRUE);
            }
            break;
        case '?':
            if (strncasecmp(data, "<?xml", 5) == 0) {
                if (moloch_memstr(data+5, len-5, "<svg", 4)) {
                    return moloch_field_string_add(field, session, "image/svg+xml", 13, TRUE);
                }
                return moloch_field_string_add(field, session, "text/xml", 8, TRUE);
            }
            break;
        case 'B':
        case 'b':
            if (strncasecmp(data, "<body", 5) == 0) {
                return moloch_field_string_add(field, session, "text/html", 9, TRUE);
            }
            break;
        case 'H':
        case 'h':
            if (strncasecmp(data, "<head", 5) == 0) {
                return moloch_field_string_add(field, session, "text/html", 9, TRUE);
            }
            if (strncasecmp(data, "<html", 5) == 0) {
                return moloch_field_string_add(field, session, "text/html", 9, TRUE);
            }
            break;
        case 's':
        case 'S':
            if (strncasecmp(data, "<svg", 4) == 0) {
                return moloch_field_string_add(field, session, "image/svg", 9, TRUE);
            }
            break;
        }
        break;
    case '{':
        if (data[1] == '"' && isalpha(data[2])) {
            return moloch_field_string_add(field, session, "application/json", 16, TRUE);
        }
        break;
    case '8':
        if (memcmp(data, "8BPS", 4) == 0) {
            return moloch_field_string_add(field, session, "image/vnd.adobe.photoshop", 25, TRUE);
        }
        break;
    case 'B':
        if (data[1] == 'M') {
            return moloch_field_string_add(field, session, "application/x-ms-bmp", 20, TRUE);
        }

        if (memcmp(data, "BZh", 3) == 0) {
            return moloch_field_string_add(field, session, "application/x-bzip2", 19, TRUE);
        }
        break;
    case 'C':
        if (memcmp(data, "CWS", 3) == 0) {
            return moloch_field_string_add(field, session, "application/x-shockwave-flash", 29, TRUE);
        }
        break;
    case 'F':
        if (memcmp(data, "FLV\001", 4) == 0) {
            return moloch_field_string_add(field, session, "video/x-flv", 11, TRUE);
        }
        break;
    case 'G':
        if (memcmp(data, "GIF8", 4) == 0) {
            return moloch_field_string_add(field, session, "image/gif", 9, TRUE);
        }
        break;
    case 'I':
        if (memcmp(data, "ID3", 3) == 0) {
            return moloch_field_string_add(field, session, "audio/mpeg", 10, TRUE);
        }
        break;
    case 'M':
        if (data[1] == 'Z') {
            return moloch_field_string_add(field, session, "application/x-dosexec", 21, TRUE);
        }
        if (memcmp(data, "MSCF\000\000", 6) == 0) {
            return moloch_field_string_add(field, session, "application/vnd.ms-cab-compressed", 33, TRUE);
        }
        break;
    case 'O':
        if (len > 40 && memcmp(data, "OggS", 4) == 0) {
            // https://speex.org/docs/manual/speex-manual/node8.html
            if (memcmp(data+28, "Speex   ", 8) == 0) {
                return moloch_field_string_add(field, session, "audio/ogg", 9, TRUE);
            }

            // https://xiph.org/flac/ogg_mapping.html
            if (memcmp(data+29, "FLAC", 4) == 0) {
                return moloch_field_string_add(field, session, "audio/ogg", 9, TRUE);
            }

            // https://xiph.org/vorbis/doc/Vorbis_I_spec.html
            if (memcmp(data+28, "\001vorbis", 7) == 0) {
                return moloch_field_string_add(field, session, "audio/ogg", 9, TRUE);
            }

            // https://www.theora.org/doc/Theora.pdf
            if (memcmp(data+28, "\x80theora", 7) == 0) {
                return moloch_field_string_add(field, session, "video/ogg", 9, TRUE);
            }
        } else if (memcmp(data, "OTTO", 4) == 0) {
            return moloch_field_string_add(field, session, "application/vnd.ms-opentype", 27, TRUE);
        }
        break;
    case 'P':
        if (memcmp(data, "PK\003\004", 4) == 0) {
            return moloch_field_string_add(field, session, "application/zip", 15, TRUE);
        }
        if (memcmp(data, "PK\005\005", 4) == 0) {
            return moloch_field_string_add(field, session, "application/zip", 15, TRUE);
        }
        break;
    case 'R':
        if (memcmp(data, "RIFF", 4) == 0) {
            return moloch_field_string_add(field, session, "audio/x-wav", 11, TRUE);
        }
        if (memcmp(data, "Rar!\x1a", 5) == 0) {
            return moloch_field_string_add(field, session, "application/x-rar", 17, TRUE);
        }
        break;
    case 'W':
        if (memcmp(data, "WAVE", 4) == 0) {
            return moloch_field_string_add(field, session, "audio/x-wav", 11, TRUE);
        }
        break;
    case 'd':
        if (len > 20 && memcmp(data, "d8:announce", 11) == 0) {
            return moloch_field_string_add(field, session, "application/x-bittorrent", 24, TRUE);
        }
        break;
    case 'w':
        if (memcmp(data, "wOFF", 4) == 0) {
            return moloch_field_string_add(field, session, "application/font-woff", 21, TRUE);
        }
        break;
    case '\x89':
        if (memcmp(data, "\x89PNG", 4) == 0) {
            return moloch_field_string_add(field, session, "image/png", 9, TRUE);
        }
        break;
    case '\375':
        if (memcmp(data, "\3757zXZ", 5) == 0) {
            return moloch_field_string_add(field, session, "application/x-xz", 16, TRUE);
        }
        break;
    case '\377':
        if (len > 10 && memcmp(data, "\377\330\377", 3) == 0) {
            return moloch_field_string_add(field, session, "image/jpeg", 10, TRUE);
        }
        break;
    } /* switch */

    if (len > 257+5 && memcmp(data+257, "ustar", 5) == 0) {
        return moloch_field_string_add(field, session, "application/x-tar", 17, TRUE);
    }
    if (moloch_memstr(data, len, "document.write", 14) ||
        moloch_memstr(data, len, "'use strict'", 12)) {
        return moloch_field_string_add(field, session, "text/javascript", 15, TRUE);
    }
    return NULL;
}
/******************************************************************************/
const char *moloch_parsers_magic_molochmagic(MolochSession_t *session, int field, const char *data, int len)
{
    int i, offset, offsetPos;
    uint8_t *udata = (uint8_t *)data;
    for (offsetPos = 0; offsetPos < magicOffsetNum; offsetPos++) {
        offset = magicOffset[offsetPos];

        if (offset + 1 >= len)
            continue;

        //LOG("offset: %d len: %d udata: %.*s %02x %02x", offset, len, len-offset, udata+offset, udata[offset], udata[offset+1]);

        MolochMagicList_t *list = &magicMatch[offsetPos][(int)(udata[offset])][(int)(udata[offset+1])];

        for (i = 0; i < list->num; i++) {
            //LOG("offset: %d i: %d len: %d matchlen: %d", offset, i, len, list->magic[i]->matchlen);
            if (len <= list->magic[i]->matchlen + offset)
                continue;
            if (list->magic[i]->ignoreCase) {
                if (strncasecmp((const char *)udata+offset, (const char *)list->magic[i]->match, list->magic[i]->matchlen) == 0) {
                    return moloch_field_string_add(field, session, list->magic[i]->mime, list->magic[i]->mimelen, TRUE);
                }
            } else {
                if (memcmp(udata+offset, list->magic[i]->match, list->magic[i]->matchlen) == 0) {
                    return moloch_field_string_add(field, session, list->magic[i]->mime, list->magic[i]->mimelen, TRUE);
                }
            }
        }
    }

    for (i = 0; i < magicSearchLen; i++) {
        if (magicSearch[i].ignoreCase) {
            if (moloch_memcasestr(data, len, (const char*)magicSearch[i].match, magicSearch[i].matchlen)) {
                return moloch_field_string_add(field, session, magicSearch[i].mime, magicSearch[i].mimelen, TRUE);
            }
        } else {
            if (moloch_memstr(data, len, (const char*)magicSearch[i].match, magicSearch[i].matchlen)) {
                return moloch_field_string_add(field, session, magicSearch[i].mime, magicSearch[i].mimelen, TRUE);
            }
        }
    }

    return NULL;
}
/******************************************************************************/
const char *moloch_parsers_magic(MolochSession_t *session, int field, const char *data, int len)
{
    const char *m;
    if (len < 5 || magicMode == MOLOCH_MAGICMODE_NONE)
        return NULL;

    switch (magicMode) {
    case MOLOCH_MAGICMODE_LIBMAGIC:
        m = magic_buffer(cookie[session->thread], data, MIN(len,50));
        if (m) {
            int len;
            char *semi = strchr(m, ';');
            if (semi) {
                len = semi - m;
            } else {
                len = strlen(m);
            }
            return moloch_field_string_add(field, session, m, len, TRUE);
        }
        return NULL;
    case MOLOCH_MAGICMODE_BASIC:
        return moloch_parsers_magic_basic(session, field, data, len);
    case MOLOCH_MAGICMODE_MOLOCHMAGIC:
        return moloch_parsers_magic_molochmagic(session, field, data, len);
    default:
        return NULL;
    }
}
/******************************************************************************/
void moloch_parsers_initial_tag(MolochSession_t *session)
{
    int i;

    if (config.nodeClass)
        moloch_session_add_tag(session, classTag);

    if (config.extraTags) {
        for (i = 0; config.extraTags[i]; i++) {
            moloch_session_add_tag(session, config.extraTags[i]);
        }
    }

    switch(session->protocol) {
    case IPPROTO_TCP:
        moloch_session_add_protocol(session, "tcp");
        break;
    case IPPROTO_UDP:
        moloch_session_add_protocol(session, "udp");
        break;
    case IPPROTO_ICMP:
        moloch_session_add_protocol(session, "icmp");
        break;
    }

    moloch_field_ops_run(session, &config.ops);
}


/*############################## ASN ##############################*/

/******************************************************************************/
unsigned char *
moloch_parsers_asn_get_tlv(BSB *bsb, uint32_t *apc, uint32_t *atag, uint32_t *alen)
{

    if (BSB_REMAINING(*bsb) < 2)
        goto get_tlv_error;

    u_char ch = 0;
    BSB_IMPORT_u08(*bsb, ch);

    *apc = (ch >> 5) & 0x1;
    *atag = 0;

    if ((ch & 0x1f) ==  0x1f) {
        while (BSB_REMAINING(*bsb)) {
            BSB_IMPORT_u08(*bsb, ch);
            (*atag) = ((*atag) << 7) | ch;
            if ((ch & 0x80) == 0)
                break;
        }
    } else {
        *atag = ch & 0x1f;
        BSB_IMPORT_u08(*bsb, ch);
    }

    if (BSB_IS_ERROR(*bsb) || ch == 0x80) {
        goto get_tlv_error;
    }

    if (ch & 0x80) {
        int cnt = ch & 0x7f;
        (*alen) = 0;
        while (cnt > 0 && BSB_REMAINING(*bsb)) {
            BSB_IMPORT_u08(*bsb, ch);
            (*alen) = ((*alen) << 8) | ch;
            cnt--;
        }
    } else {
        (*alen) = ch;
    }

    if (*alen > BSB_REMAINING(*bsb))
        *alen = BSB_REMAINING(*bsb);

    unsigned char *value;
    BSB_IMPORT_ptr(*bsb, value, *alen);
    if (BSB_IS_ERROR(*bsb)) {
        goto get_tlv_error;
    }

    return value;

get_tlv_error:
    (*apc)  = 0;
    (*alen) = 0;
    (*atag) = 0;
    return 0;
}
/******************************************************************************/
int moloch_parsers_asn_get_sequence(MolochASNSeq_t *seqs, int maxSeq, const unsigned char *data, int len, gboolean wrapper)
{
    int num = 0;
    BSB bsb;
    BSB_INIT(bsb, data, len);
    if (wrapper) {
        uint32_t ipc, itag, ilen;
        unsigned char *ivalue;
        ivalue = moloch_parsers_asn_get_tlv(&bsb, &ipc, &itag, &ilen);
        if (!ipc || itag != 16)
            return 0;
        BSB_INIT(bsb, ivalue, ilen);
    }
    while (BSB_NOT_ERROR(bsb) && num < maxSeq) {
        seqs[num].value = moloch_parsers_asn_get_tlv(&bsb, &seqs[num].pc, &seqs[num].tag, &seqs[num].len);
        if (seqs[num].value == 0)
            break;
#ifdef DEBUG_PARSERS
        LOG("%d %p %d %d %d %d", num, seqs[num].value, seqs[num].pc, seqs[num].tag, seqs[num].len, BSB_IS_ERROR(bsb));
#endif
        num++;
    }
    return num;
}
/******************************************************************************/
const char *moloch_parsers_asn_sequence_to_string(MolochASNSeq_t *seq, int *len)
{
    if (!seq->pc) {
        *len = seq->len;
        return (const char*)seq->value;
    }

    BSB bsb;
    BSB_INIT(bsb, seq->value, seq->len);
    uint32_t ipc, itag, ilen;
    char *ivalue;
    ivalue = (char *)moloch_parsers_asn_get_tlv(&bsb, &ipc, &itag, &ilen);
    *len = ilen;
    return ivalue;
}
/******************************************************************************/
void moloch_parsers_asn_decode_oid(char *buf, int bufsz, unsigned char *oid, int len) {
    int buflen = 0;
    int pos = 0;
    int first = TRUE;
    int value = 0;

    buf[0] = 0;

    for (pos = 0; pos < len; pos++) {
        value = (value << 7) | (oid[pos] & 0x7f);
        if (oid[pos] & 0x80) {
            continue;
        }

        if (first) {
            first = FALSE;
            if (value > 40) /* two values in first byte */
                buflen = snprintf(buf, bufsz, "%d.%d", value/40, value % 40);
            else /* one value in first byte */
                buflen = snprintf(buf, bufsz, "%d", value);
        } else if (buflen < bufsz) {
            buflen += snprintf(buf+buflen, bufsz-buflen, ".%d", value);
        }

        value = 0;
    }
}

/******************************************************************************/
void moloch_parsers_init()
{
    moloch_field_define("general", "lotermfield",
        "user", "User", "user",
        "External user set for session",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        "category", "user",
        NULL);

    moloch_field_define("general", "integer",
        "session.segments", "Session Segments", "ss",
        "Number of segments in session so far",
        0,  MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    moloch_field_define("general", "integer",
        "session.length", "Session Length", "sl",
        "Session Length in milliseconds so far",
        0,  MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    userField = moloch_field_define("general", "lotermfield",
        "user", "User", "user",
        "External user set for session",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        "category", "user",
        NULL);

    int flags = MAGIC_MIME;

    char *strMagicMode = moloch_config_str(NULL, "magicMode", "libmagic");

    if (strcmp(strMagicMode, "libmagic") == 0) {
        magicMode = MOLOCH_MAGICMODE_LIBMAGIC;
    } else if (strcmp(strMagicMode, "libmagicnotext") == 0) {
        magicMode = MOLOCH_MAGICMODE_LIBMAGIC;
        flags |= MAGIC_NO_CHECK_TEXT;
    } else if (strcmp(strMagicMode, "molochmagic") == 0) {
        magicMode = MOLOCH_MAGICMODE_MOLOCHMAGIC;
        void molochmagic_load();
        molochmagic_load();
    } else if (strcmp(strMagicMode, "basic") == 0) {
        magicMode = MOLOCH_MAGICMODE_BASIC;
    } else if (strcmp(strMagicMode, "none") == 0) {
        magicMode = MOLOCH_MAGICMODE_NONE;
    } else {
        LOGEXIT("Unknown magicMode '%s'", strMagicMode);
    }

#ifdef MAGIC_NO_CHECK_COMPRESS
    flags |= MAGIC_NO_CHECK_COMPRESS |
             MAGIC_NO_CHECK_TAR      |
             MAGIC_NO_CHECK_APPTYPE  |
             MAGIC_NO_CHECK_ELF      |
             MAGIC_NO_CHECK_TOKENS;
#endif
#ifdef MAGIC_NO_CHECK_CDF
    flags |= MAGIC_NO_CHECK_CDF;
#endif

    if (magicMode == MOLOCH_MAGICMODE_LIBMAGIC) {
        int t;
        for (t = 0; t < config.packetThreads; t++) {
            cookie[t] = magic_open(flags);
            if (!cookie[t]) {
                LOG("Error with libmagic %s", magic_error(cookie[t]));
            } else {
                magic_load(cookie[t], NULL);
            }
        }
    }

    MolochStringHashStd_t loaded;
    HASH_INIT(s_, loaded, moloch_string_hash, moloch_string_cmp);

    MolochString_t *hstring;
    int d;

    for (d = 0; config.parsersDir[d]; d++) {
        GError      *error = 0;
        GDir *dir = g_dir_open(config.parsersDir[d], 0, &error);

        if (error) {
            LOG("Error with %s: %s", config.parsersDir[d], error->message);
            g_error_free(error);
            if (dir)
                g_dir_close(dir);
            continue;
        }

        if (!dir)
            continue;

        const gchar *filename;
        while (1) {
            filename = g_dir_read_name(dir);

            // No more files, stop processing this directory
            if (!filename)
                break;

            // Skip hidden files/directories
            if (filename[0] == '.')
                continue;

            // If it doesn't end with .so we ignore it
            if (strlen(filename) < 3 || strcasecmp(".so", filename + strlen(filename)-3) != 0) {
                continue;
            }

            HASH_FIND(s_, loaded, filename, hstring);
            if (hstring) {
                if (config.debug) {
                    LOG("Skipping %s in %s since already loaded", filename, config.parsersDir[d]);
                }
                continue; /* Already loaded */
            }

            gchar   *path = g_build_filename (config.parsersDir[d], filename, NULL);
            GModule *parser = g_module_open (path, 0); /*G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);*/

            if (!parser) {
                LOG("ERROR - Couldn't load parser %s from '%s'\n%s", filename, path, g_module_error());
                g_free (path);
                continue;
            }
            g_free (path);

            MolochPluginInitFunc parser_init;

            if (!g_module_symbol(parser, "moloch_parser_init", (gpointer *)(char*)&parser_init) || parser_init == NULL) {
                LOG("ERROR - Module %s doesn't have a moloch_parser_init", filename);
                continue;
            }

            parser_init();

            hstring = MOLOCH_TYPE_ALLOC0(MolochString_t);
            hstring->str = g_strdup(filename);
            hstring->len = strlen(filename);
            HASH_ADD(s_, loaded, hstring->str, hstring);
        }

        g_dir_close(dir);
    }

    HASH_FORALL_POP_HEAD(s_, loaded, hstring,
        g_free(hstring->str);
        MOLOCH_TYPE_FREE(MolochString_t, hstring);
    );

    // Set tags field up AFTER loading plugins
    config.tagsField = moloch_field_define("general", "termfield",
        "tags", "Tags", "ta",
        "Tags set for session",
        MOLOCH_FIELD_TYPE_INT_GHASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);

    config.tagsStringField = moloch_field_define("general", "notreal",
        "tags", "Tags", "tags-term",
        "Tags set for session",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_LINKED_SESSIONS | MOLOCH_FIELD_FLAG_NODB,
        NULL);

    moloch_field_define("general", "lotermfield",
        "asset", "Asset", "asset-term",
        "Asset name",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_COUNT | MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);

    if (config.nodeClass) {
        snprintf(classTag, sizeof(classTag), "node:%s", config.nodeClass);
        moloch_db_get_tag(NULL, config.tagsField, classTag, NULL);
    }

    if (config.extraTags) {
        int i;
        for (i = 0; config.extraTags[i]; i++) {
            moloch_db_get_tag(NULL, config.tagsField, config.extraTags[i], NULL);
        }
    }

    if (config.extraOps) {
        int i;
        for (i = 0; config.extraOps[i]; i++) { }
        moloch_field_ops_init(&config.ops, i, 0);
        for (i = 0; config.extraOps[i]; i++) {
            char *equal = strchr(config.extraOps[i], '=');
            if (!equal) {
                LOGEXIT("Must be FieldExpr=value, missing equal '%s'", config.extraOps[i]);
            }
            int len = strlen(equal+1);
            if (!len) {
                LOGEXIT("Must be FieldExpr=value, empty value for '%s'", config.extraOps[i]);
            }
            *equal = 0;
            int fieldPos = moloch_field_by_exp(config.extraOps[i]);
            if (fieldPos == -1) {
                LOGEXIT("Must be FieldExpr=value, Unknown field expression '%s'", config.extraOps[i]);
            }
            moloch_field_ops_add(&config.ops, fieldPos, equal+1, len);
        }
    } else {
        moloch_field_ops_init(&config.ops, 0, 0);
    }
}
/******************************************************************************/
void moloch_parsers_exit() {
    if (magicMode == MOLOCH_MAGICMODE_LIBMAGIC) {
        int t;
        for (t = 0; t < config.packetThreads; t++) {
            magic_close(cookie[t]);
        }
    }
}
/******************************************************************************/
void moloch_print_hex_string(const unsigned char* data, unsigned int length)
{
    unsigned int i;

    for (i = 0; i < length; i++)
    {
        printf("%02x", data[i]);
    }

    printf("\n");
}
/******************************************************************************/
char *moloch_sprint_hex_string(char *buf, const unsigned char* data, unsigned int length)
{
    unsigned int i;

    for (i = 0; i < length; i++)
    {
        memcpy(buf+i*2, moloch_char_to_hexstr[data[i]], 2);
    }
    buf[i*2] = 0;
    return buf;
}
/******************************************************************************/
void  moloch_parsers_register2(MolochSession_t *session, MolochParserFunc func, void *uw, MolochParserFreeFunc ffunc, MolochParserSaveFunc sfunc)
{
    if (session->parserNum >= session->parserLen) {
        if (session->parserLen == 0) {
            session->parserLen = 2;
        } else {
            session->parserLen *= 1.67;
        }
    }
    session->parserInfo = realloc(session->parserInfo, sizeof(MolochParserInfo_t) * session->parserLen);

    session->parserInfo[session->parserNum].parserFunc     = func;
    session->parserInfo[session->parserNum].uw             = uw;
    session->parserInfo[session->parserNum].parserFreeFunc = ffunc;
    session->parserInfo[session->parserNum].parserSaveFunc = sfunc;

    session->parserNum++;
}
/******************************************************************************/
void  moloch_parsers_unregister(MolochSession_t *session, void *uw)
{
    int i;
    for (i = 0; i < session->parserNum; i++) {
        if (session->parserInfo[i].uw == uw) {
            if (session->parserInfo[i].parserFreeFunc) {
                session->parserInfo[i].parserFreeFunc(session, uw);
                session->parserInfo[i].parserFreeFunc = 0;
            }

            session->parserInfo[i].parserSaveFunc = 0;
            session->parserInfo[i].parserFunc = 0;
            session->parserInfo[i].uw = 0;
            break;
        }
    }
}
/******************************************************************************/
typedef struct moloch_classify_t
{
    const char          *name;
    void                *uw;
    int                  offset;
    const unsigned char *match;
    int                  matchlen;
    int                  minlen;
    MolochClassifyFunc   func;
} MolochClassify_t;

typedef struct
{
    MolochClassify_t   **arr;
    short               size;
    short               cnt;
} MolochClassifyHead_t;

LOCAL MolochClassifyHead_t classifersTcp0;
LOCAL MolochClassifyHead_t classifersTcp1[256];
LOCAL MolochClassifyHead_t classifersTcp2[256][256];
LOCAL MolochClassifyHead_t classifersTcpPortSrc[0xffff];
LOCAL MolochClassifyHead_t classifersTcpPortDst[0xffff];

LOCAL MolochClassifyHead_t classifersUdp0;
LOCAL MolochClassifyHead_t classifersUdp1[256];
LOCAL MolochClassifyHead_t classifersUdp2[256][256];
LOCAL MolochClassifyHead_t classifersUdpPortSrc[0xffff];
LOCAL MolochClassifyHead_t classifersUdpPortDst[0xffff];

/******************************************************************************/
void moloch_parsers_classifier_add(MolochClassifyHead_t *ch, MolochClassify_t *c)
{
    int i;
    for (i = 0; i < ch->cnt; i++) {
        if (ch->arr[i]->offset == c->offset &&
            ch->arr[i]->func == c->func &&
            strcmp(ch->arr[i]->name, c->name) == 0 &&
            memcmp(ch->arr[i]->match, c->match, c->matchlen) == 0) {

            if (config.debug > 1) {
                LOG("Info, duplicate (could be normal) %s %s", c->name, c->match);
            }
            MOLOCH_TYPE_FREE(MolochClassify_t, c);
            return;
        }
    }
    if (ch->cnt >= ch->size) {
        if (ch->size == 0) {
            ch->size = 2;
        } else {
            ch->size *= 1.67;
        }
        ch->arr = realloc(ch->arr, sizeof(MolochClassify_t *) * ch->size);
    }

    ch->arr[ch->cnt] = c;
    ch->cnt++;
}
/******************************************************************************/
void moloch_parsers_classifier_register_port_internal(const char *name, void *uw, uint16_t port, uint32_t type, MolochClassifyFunc func, size_t sessionsize, int apiversion)
{
    if (sizeof(MolochSession_t) != sessionsize) {
        LOGEXIT("Plugin '%s' built with different version of moloch.h\n %lu != %lu", name, sizeof(MolochSession_t),  sessionsize);
    }

    if (MOLOCH_API_VERSION != apiversion) {
        LOGEXIT("Plugin '%s' built with different version of moloch.h\n %u %d", name, MOLOCH_API_VERSION, apiversion);
    }

    MolochClassify_t *c = MOLOCH_TYPE_ALLOC(MolochClassify_t);
    c->name     = name;
    c->uw       = uw;
    c->func     = func;

    if (config.debug)
        LOG("adding %s port:%u type:%02x uw:%p", name, port, type, uw);

    if (type & MOLOCH_PARSERS_PORT_TCP_SRC)
        moloch_parsers_classifier_add(&classifersTcpPortSrc[port], c);
    if (type & MOLOCH_PARSERS_PORT_TCP_DST)
        moloch_parsers_classifier_add(&classifersTcpPortDst[port], c);

    if (type & MOLOCH_PARSERS_PORT_UDP_SRC)
        moloch_parsers_classifier_add(&classifersUdpPortSrc[port], c);
    if (type & MOLOCH_PARSERS_PORT_UDP_DST)
        moloch_parsers_classifier_add(&classifersUdpPortDst[port], c);
}
/******************************************************************************/
void moloch_parsers_classifier_register_tcp_internal(const char *name, void *uw, int offset, const unsigned char *match, int matchlen, MolochClassifyFunc func, size_t sessionsize, int apiversion)
{
    if (sizeof(MolochSession_t) != sessionsize) {
        LOGEXIT("Plugin '%s' built with different version of moloch.h\n %lu != %lu", name, sizeof(MolochSession_t),  sessionsize);
    }

    if (MOLOCH_API_VERSION != apiversion) {
        LOGEXIT("Plugin '%s' built with different version of moloch.h\n %u %d", name, MOLOCH_API_VERSION, apiversion);
    }

    MolochClassify_t *c = MOLOCH_TYPE_ALLOC(MolochClassify_t);
    c->name     = name;
    c->uw       = uw;
    c->offset   = offset;
    c->match    = match;
    c->matchlen = matchlen;
    c->minlen   = matchlen + offset;
    c->func     = func;

    if (config.debug)
        LOG("adding %s matchlen:%d offset:%d match %s ", name, matchlen, offset, match);
    if (matchlen == 0 || offset != 0) {
        moloch_parsers_classifier_add(&classifersTcp0, c);
    } else if (matchlen == 1) {
        moloch_parsers_classifier_add(&classifersTcp1[(uint8_t)match[0]], c);
    } else  {
        c->match += 2;
        c->matchlen -= 2;
        moloch_parsers_classifier_add(&classifersTcp2[(uint8_t)match[0]][(uint8_t)match[1]], c);
    }
}
/******************************************************************************/
void moloch_parsers_classifier_register_udp_internal(const char *name, void *uw, int offset, const unsigned char *match, int matchlen, MolochClassifyFunc func, size_t sessionsize, int apiversion)
{
    if (sizeof(MolochSession_t) != sessionsize) {
        LOGEXIT("Plugin '%s' built with different version of moloch.h", name);
    }

    if (MOLOCH_API_VERSION != apiversion) {
        LOGEXIT("Plugin '%s' built with different version of moloch.h", name);
    }

    MolochClassify_t *c = MOLOCH_TYPE_ALLOC(MolochClassify_t);
    c->name     = name;
    c->uw       = uw;
    c->offset   = offset;
    c->match    = match;
    c->matchlen = matchlen;
    c->minlen   = matchlen + offset;
    c->func     = func;

    if (config.debug)
        LOG("adding %s matchlen:%d offset:%d match %s ", name, matchlen, offset, match);
    if (matchlen == 0 || offset != 0) {
        moloch_parsers_classifier_add(&classifersUdp0, c);
    } else if (matchlen == 1) {
        moloch_parsers_classifier_add(&classifersUdp1[(uint8_t)match[0]], c);
    } else  {
        c->match += 2;
        c->matchlen -= 2;
        moloch_parsers_classifier_add(&classifersUdp2[(uint8_t)match[0]][(uint8_t)match[1]], c);
    }
}
/******************************************************************************/
void moloch_parsers_classify_udp(MolochSession_t *session, const unsigned char *data, int remaining, int which)
{
    int i;

    if (remaining < 2)
        return;

#ifdef DEBUG_PARSERS
    char buf[101];
    LOG("len: %d direction: %d hex: %s data: %.*s", remaining, which, moloch_sprint_hex_string(buf, data, MIN(remaining, 50)), MIN(remaining, 50), data);
#endif

    for (i = 0; i < classifersUdpPortSrc[session->port1].cnt; i++) {
        classifersUdpPortSrc[session->port1].arr[i]->func(session, data, remaining, which, classifersUdpPortSrc[session->port1].arr[i]->uw);
    }

    for (i = 0; i < classifersUdpPortDst[session->port2].cnt; i++) {
        classifersUdpPortDst[session->port2].arr[i]->func(session, data, remaining, which, classifersUdpPortDst[session->port2].arr[i]->uw);
    }

    for (i = 0; i < classifersUdp0.cnt; i++) {
        MolochClassify_t *c = classifersUdp0.arr[i];
        if (remaining >= c->minlen && memcmp(data + c->offset, c->match, c->matchlen) == 0) {
            c->func(session, data, remaining, which, c->uw);
        }
    }

    for (i = 0; i < classifersUdp1[data[0]].cnt; i++)
        classifersUdp1[data[0]].arr[i]->func(session, data, remaining, which, classifersUdp1[data[0]].arr[i]->uw);

    for (i = 0; i < classifersUdp2[data[0]][data[1]].cnt; i++) {
        MolochClassify_t *c = classifersUdp2[data[0]][data[1]].arr[i];
        if (remaining >= c->minlen && memcmp(data+2, c->match, c->matchlen) == 0) {
            c->func(session, data, remaining, which, c->uw);
        }
    }

    moloch_rules_run_after_classify(session);
}
/******************************************************************************/
void moloch_parsers_classify_tcp(MolochSession_t *session, const unsigned char *data, int remaining, int which)
{
    int i;

#ifdef DEBUG_PARSERS
    char buf[101];
    LOG("len: %d direction: %d hex: %s data: %.*s", remaining, which, moloch_sprint_hex_string(buf, data, MIN(remaining, 50)), MIN(remaining, 50), data);
#endif

    if (remaining < 2)
        return;

    for (i = 0; i < classifersTcpPortSrc[session->port1].cnt; i++) {
        classifersTcpPortSrc[session->port1].arr[i]->func(session, data, remaining, which, classifersTcpPortSrc[session->port1].arr[i]);
    }

    for (i = 0; i < classifersTcpPortDst[session->port2].cnt; i++) {
        classifersTcpPortDst[session->port2].arr[i]->func(session, data, remaining, which, classifersTcpPortDst[session->port2].arr[i]);
    }

    for (i = 0; i < classifersTcp0.cnt; i++) {
        MolochClassify_t *c = classifersTcp0.arr[i];
        if (remaining >= c->minlen && memcmp(data + c->offset, c->match, c->matchlen) == 0) {
            c->func(session, data, remaining, which, c->uw);
        }
    }

    for (i = 0; i < classifersTcp1[data[0]].cnt; i++) {
        classifersTcp1[data[0]].arr[i]->func(session, data, remaining, which, classifersTcp1[data[0]].arr[i]);
    }

    for (i = 0; i < classifersTcp2[data[0]][data[1]].cnt; i++) {
        MolochClassify_t *c = classifersTcp2[data[0]][data[1]].arr[i];
        if (remaining >= c->minlen && memcmp(data+2, c->match, c->matchlen) == 0) {
            c->func(session, data, remaining, which, c->uw);
        }
    }

    moloch_rules_run_after_classify(session);
}
