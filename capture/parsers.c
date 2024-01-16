/* parsers.c  -- Functions for dealing with classification and parsers
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"
#include <fcntl.h>
#include "gmodule.h"
#include "magic.h"
#include "bsb.h"

//#define DEBUG_PARSERS 1

/******************************************************************************/
extern ArkimeConfig_t        config;
LOCAL  gchar                 classTag[100];

LOCAL  magic_t               cookie[ARKIME_MAX_PACKET_THREADS];

extern uint8_t               arkime_char_to_hexstr[256][3];

int    userField;

enum ArkimeMagicMode { ARKIME_MAGICMODE_LIBMAGIC, ARKIME_MAGICMODE_BOTH, ARKIME_MAGICMODE_BASIC, ARKIME_MAGICMODE_NONE};

LOCAL enum ArkimeMagicMode magicMode;

/******************************************************************************/
typedef struct {
    GPtrArray *funcs;
    uint16_t   id;
} ArkimeNamedInfo_t;

#define MAX_NAMED_FUNCS  100
LOCAL uint16_t           namedFuncsMax = 0;
LOCAL ArkimeNamedInfo_t *namedFuncsArr[MAX_NAMED_FUNCS];
LOCAL GHashTable        *namedFuncsHash;
/******************************************************************************/
#define MAGIC_MATCH(offset, needle) memcmp(data+offset, needle, sizeof(needle)-1) == 0
#define MAGIC_MATCH_LEN(offset, needle) ((len > (int)sizeof(needle)-1+offset) && (memcmp(data+offset, needle, sizeof(needle)-1) == 0))

#define MAGIC_MEMSTR(offset, needle) arkime_memstr(data+offset, len-offset, needle, sizeof(needle)-1)
#define MAGIC_MEMSTR_LEN(offset, needle) ((len > (int)sizeof(needle)-1+offset) && (arkime_memstr(data+offset, len-offset, needle, sizeof(needle)-1)))

#define MAGIC_STRCASE(offset, needle) strncasecmp(data+offset, needle, sizeof(needle)-1) == 0
#define MAGIC_STRCASE_LEN(offset, needle) ((len > (int)sizeof(needle)-1+offset) && (strncasecmp(data+offset, needle, sizeof(needle)-1) == 0))

#define MAGIC_RESULT(str) arkime_field_string_add(field, session, str, sizeof(str)-1, TRUE), str
const char *arkime_parsers_magic_basic(ArkimeSession_t *session, int field, const char *data, int len)
{
    switch (data[0]) {
    case 0:
        if (len > 10 && MAGIC_MATCH(4, "ftyp")) {
            if (MAGIC_MATCH(8, "qt")) {
                return MAGIC_RESULT("video/quicktime");
            }
            if (MAGIC_MATCH(8, "3g")) {
                return MAGIC_RESULT("video/3gpp");
            }
        }
        if (MAGIC_MATCH(0, "\000\001\000\000\000")) {
            return MAGIC_RESULT("application/x-font-ttf");
        }
        if (MAGIC_MATCH(0, "\000\000\002\000\001\000")) {
            return MAGIC_RESULT("image/x-win-bitmap");
        }
        break;
    case '\032':
        if (MAGIC_MATCH(0, "\x1a\x45\xdf\xa3")) {
            if (MAGIC_MEMSTR_LEN(4, "webm")) {
                return MAGIC_RESULT("video/webm");
            }
            if (MAGIC_MEMSTR_LEN(4, "matroska")) {
                return MAGIC_RESULT("video/x-matroska");
            }
        }
        break;
    case '\037':
        if (data[1] == '\213') {
            return MAGIC_RESULT("application/x-gzip");
        }
        if (data[1] == '\235') {
            return MAGIC_RESULT("application/x-compress");
        }
        break;
#ifdef OID_DECODE_SOMEDAY
    case 0x30:
        if (len > 100 && (gchar)data[1] == (gchar)0x82) {
            ArkimeASNSeq_t seq[5];
            int i;
            int num = arkime_parsers_asn_get_sequence(seq, 5, (uint8_t *)data, len, TRUE);
            for (i = 0; i < num; i++) {
                if (seq[i].pc && seq[i].tag == 16) {
                    BSB tbsb;
                    BSB_INIT(tbsb, seq[i].value, seq[i].len);
                    uint32_t ipc, itag, ilen;
                    uint8_t *ivalue;
                    ivalue = arkime_parsers_asn_get_tlv(&tbsb, &ipc, &itag, &ilen);
                    if (itag != 6)
                        continue;
                    char oid[100];
                    arkime_parsers_asn_decode_oid(oid, sizeof(oid), ivalue, ilen);
                    printf("%s ", oid);
                    arkime_print_hex_string(ivalue, ilen);
                    if (ilen == 9 && MAGIC_MATCH(ivalue, "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x05")) {
                    }
                }
            }
        }
        break;
#endif
    case '!':
        if (MAGIC_MATCH_LEN(1, "<arch>\ndebian-binary")) {
            return MAGIC_RESULT("application/x-debian-package");
        }
        break;
    case '#':
        if (data[1] == '!') {
            if (MAGIC_MEMSTR_LEN(3, "node")) {
                return MAGIC_RESULT("application/javascript");
            } else if (MAGIC_MEMSTR_LEN(3, "perl")) {
                return MAGIC_RESULT("text/x-perl");
            } else if (MAGIC_MEMSTR_LEN(3, "ruby")) {
                return MAGIC_RESULT("text/x-ruby");
            } else if (MAGIC_MEMSTR_LEN(3, "python")) {
                return MAGIC_RESULT("text/x-python");
            }
            return MAGIC_RESULT("text/x-shellscript");
        }
        break;
    case '%':
        if (MAGIC_MATCH(0, "%PDF-")) {
            return MAGIC_RESULT("application/pdf");
        }
        break;
    case '<':
        switch(data[1]) {
        case '!':
            if (MAGIC_STRCASE_LEN(0, "<!doctype html")) {
                return MAGIC_RESULT("text/html");
            }
            if (MAGIC_STRCASE_LEN(0, "<!doctype svg")) {
                return MAGIC_RESULT("text/svg+xml");
            }
            break;
        case '?':
            if (MAGIC_STRCASE(0, "<?xml")) {
                if (MAGIC_MEMSTR_LEN(5, "<svg")) {
                    return MAGIC_RESULT("image/svg+xml");
                }
                return MAGIC_RESULT("text/xml");
            }
            if (MAGIC_STRCASE_LEN(2, "php") || MAGIC_STRCASE_LEN(2, " php")) {
                return MAGIC_RESULT("text/x-php");
            }
            break;
        case 'B':
        case 'b':
            if (MAGIC_STRCASE(0, "<body")) {
                return MAGIC_RESULT("text/html");
            }
            break;
        case 'H':
        case 'h':
            if (MAGIC_STRCASE(0, "<head")) {
                return MAGIC_RESULT("text/html");
            }
            if (MAGIC_STRCASE(0, "<html")) {
                return MAGIC_RESULT("text/html");
            }
            break;
        case 's':
        case 'S':
            if (MAGIC_STRCASE(0, "<svg")) {
                return MAGIC_RESULT("image/svg");
            }
            break;
        }
        break;
    case '{':
        if (data[1] == '"' && isalpha(data[2])) {
            return MAGIC_RESULT("application/json");
        }
        break;
    case '8':
        if (MAGIC_MATCH(0, "8BPS")) {
            return MAGIC_RESULT("image/vnd.adobe.photoshop");
        }
        break;
    case 'B':
        if (data[1] == 'M') {
            return MAGIC_RESULT("application/x-ms-bmp");
        }

        if (MAGIC_MATCH(0, "BZh")) {
            return MAGIC_RESULT("application/x-bzip2");
        }
        break;
    case 'C':
        if (MAGIC_MATCH(0, "CWS")) {
            return MAGIC_RESULT("application/x-shockwave-flash");
        }
        break;
    case 'F':
        if (MAGIC_MATCH(0, "FLV\001")) {
            return MAGIC_RESULT("video/x-flv");
        }
        break;
    case 'G':
        if (MAGIC_MATCH(0, "GIF8")) {
            return MAGIC_RESULT("image/gif");
        }
        if (len > 188 && data[2] == 0 && data[188] == 'G') {
            return MAGIC_RESULT("video/mp2t");
        }
        break;
    case 'i':
        if (MAGIC_MATCH(0, "icns")) {
            return MAGIC_RESULT("image/x-icns");
        }
        break;
    case 'I':
        if (MAGIC_MATCH(0, "ID3")) {
            return MAGIC_RESULT("audio/mpeg");
        }
        break;
    case 'M':
        if (data[1] == 'Z') {
            return MAGIC_RESULT("application/x-dosexec");
        }
        if (MAGIC_MATCH_LEN(0, "MSCF\000\000")) {
            return MAGIC_RESULT("application/vnd.ms-cab-compressed");
        }
        break;
    case 'O':
        if (len > 40 && MAGIC_MATCH(0, "OggS")) {
            // https://speex.org/docs/manual/speex-manual/node8.html
            if (MAGIC_MATCH(28, "Speex   ")) {
                return MAGIC_RESULT("audio/ogg");
            }

            // https://xiph.org/flac/ogg_mapping.html
            if (MAGIC_MATCH(29, "FLAC")) {
                return MAGIC_RESULT("audio/ogg");
            }

            // https://xiph.org/vorbis/doc/Vorbis_I_spec.html
            if (MAGIC_MATCH(28, "\001vorbis")) {
                return MAGIC_RESULT("audio/ogg");
            }

            // https://www.theora.org/doc/Theora.pdf
            if (MAGIC_MATCH(28, "\x80theora")) {
                return MAGIC_RESULT("video/ogg");
            }
        } else if (MAGIC_MATCH(0, "OTTO")) {
            return MAGIC_RESULT("application/vnd.ms-opentype");
        }
        break;
    case 'P':
        if (MAGIC_MATCH(0, "PK\003\004")) {
            return MAGIC_RESULT("application/zip");
        }
        if (MAGIC_MATCH(0, "PK\005\006")) {
            return MAGIC_RESULT("application/zip");
        }
        if (MAGIC_MATCH_LEN(0, "PK\007\008PK")) {
            return MAGIC_RESULT("application/zip");
        }
        break;
    case 'R':
        if (MAGIC_MATCH(0, "RIFF")) {
            return MAGIC_RESULT("audio/x-wav");
        }
        if (MAGIC_MATCH(0, "Rar!\x1a")) {
            return MAGIC_RESULT("application/x-rar");
        }
        break;
    case 'W':
        if (MAGIC_MATCH(0, "WAVE")) {
            return MAGIC_RESULT("audio/x-wav");
        }
        break;
    case 'd':
        if (MAGIC_MATCH_LEN(0, "d8:announce")) {
            return MAGIC_RESULT("application/x-bittorrent");
        }
        break;
    case 'w':
        if (MAGIC_MATCH(0, "wOFF")) {
            return MAGIC_RESULT("application/font-woff");
        }
        if (MAGIC_MATCH(0, "wOF2")) {
            return MAGIC_RESULT("application/font-woff2");
        }
        break;
    case '\x89':
        if (MAGIC_MATCH(0, "\x89PNG")) {
            return MAGIC_RESULT("image/png");
        }
        break;
    case '\375':
        if (MAGIC_MATCH_LEN(0, "\3757zXZ")) {
            return MAGIC_RESULT("application/x-xz");
        }
        break;
    case '\377':
        if (len > 10 && MAGIC_MATCH(0, "\377\330\377")) {
            return MAGIC_RESULT("image/jpeg");
        }
        break;
    case '\xed':
        if (len > 10 && MAGIC_MATCH(0, "\xed\xab\xee\xdb")) {
            return MAGIC_RESULT("application/x-rpm");
        }
        break;
    } /* switch */

    if (MAGIC_MATCH_LEN(257, "ustar")) {
        return MAGIC_RESULT("application/x-tar");
    }
    if (MAGIC_MEMSTR_LEN(0, "document.write") ||
        MAGIC_MEMSTR_LEN(0, "'use strict'")) {
        return MAGIC_RESULT("text/javascript");
    }
    return NULL;
}
/******************************************************************************/
const char *arkime_parsers_magic(ArkimeSession_t *session, int field, const char *data, int len)
{
    const char *m;
    if (len < 5)
        return NULL;

    switch (magicMode) {
    case ARKIME_MAGICMODE_BASIC:
        return arkime_parsers_magic_basic(session, field, data, len);

    case ARKIME_MAGICMODE_BOTH:
        m = arkime_parsers_magic_basic(session, field, data, len);
        if (m)
            return m;

    // Fall thru
    case ARKIME_MAGICMODE_LIBMAGIC:
        m = magic_buffer(cookie[session->thread], data, MIN(len, 50));
        if (m) {
            int mlen;
            const char *semi = strchr(m, ';');
            if (semi) {
                mlen = semi - m;
            } else {
                mlen = strlen(m);
            }
            return arkime_field_string_add(field, session, m, mlen, TRUE);
        }
        return NULL;
    case ARKIME_MAGICMODE_NONE:
    default:
        return NULL;
    }
}
/******************************************************************************/
void arkime_parsers_initial_tag(ArkimeSession_t *session)
{
    if (config.nodeClass)
        arkime_session_add_tag(session, classTag);

    if (config.extraTags) {
        int i;
        for (i = 0; config.extraTags[i]; i++) {
            arkime_session_add_tag(session, config.extraTags[i]);
        }
    }

    arkime_field_ops_run(session, &config.ops);
}


/*############################## ASN ##############################*/

/******************************************************************************/
uint8_t *arkime_parsers_asn_get_tlv(BSB *bsb, uint32_t *apc, uint32_t *atag, uint32_t *alen)
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
        if (cnt > 4) {
            goto get_tlv_error;
        }
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

    uint8_t *value;
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
int arkime_parsers_asn_get_sequence(ArkimeASNSeq_t *seqs, int maxSeq, const uint8_t *data, int len, gboolean wrapper)
{
    int num = 0;
    BSB bsb;
    BSB_INIT(bsb, data, len);
    if (wrapper) {
        uint32_t ipc, itag, ilen;
        uint8_t *ivalue;
        ivalue = arkime_parsers_asn_get_tlv(&bsb, &ipc, &itag, &ilen);
        if (!ipc || itag != 16)
            return 0;
        BSB_INIT(bsb, ivalue, ilen);
    }
    while (BSB_NOT_ERROR(bsb) && num < maxSeq) {
        seqs[num].value = arkime_parsers_asn_get_tlv(&bsb, &seqs[num].pc, &seqs[num].tag, &seqs[num].len);
        if (seqs[num].value == 0)
            break;
#ifdef DEBUG_PARSERS
        LOG("%d %p %u %u %u %u", num, seqs[num].value, seqs[num].pc, seqs[num].tag, seqs[num].len, BSB_IS_ERROR(bsb));
#endif
        num++;
    }
    return num;
}
/******************************************************************************/
const char *arkime_parsers_asn_sequence_to_string(ArkimeASNSeq_t *seq, int *len)
{
    if (!seq->pc) {
        *len = seq->len;
        return (const char *)seq->value;
    }

    BSB bsb;
    BSB_INIT(bsb, seq->value, seq->len);
    uint32_t ipc, itag, ilen;
    char *ivalue;
    ivalue = (char *)arkime_parsers_asn_get_tlv(&bsb, &ipc, &itag, &ilen);
    *len = ilen;
    return ivalue;
}
/******************************************************************************/
void arkime_parsers_asn_decode_oid(char *buf, int bufsz, const uint8_t *oid, int len) {
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
                buflen = snprintf(buf, bufsz, "%d.%d", value / 40, value % 40);
            else /* one value in first byte */
                buflen = snprintf(buf, bufsz, "%d", value);
        } else if (buflen < bufsz) {
            buflen += snprintf(buf + buflen, bufsz - buflen, ".%d", value);
        }

        value = 0;
    }
}
/******************************************************************************/
#define char2num(ch) (isdigit(ch)?((ch) - '0'):0)
#define str2num(str) (char2num((str)[0]) * 10 + char2num((str)[1]))
#define str4num(str) (char2num((str)[0]) * 1000 + char2num((str)[1]) * 100 + char2num((str)[2]) * 10 + char2num((str)[3]))
uint64_t arkime_parsers_asn_parse_time(ArkimeSession_t *session, int tag, uint8_t *value, int len)
{
    int        offset = 0;
    struct tm  tm;
    time_t     val;

    //UTCTime
    if (tag == 23 && len > 12) {
        if (len > 17 && value[12] != 'Z')
            offset = str2num(value + 13) * 60 + str2num(value + 15);

        if (value[12] == '-')
            offset = -offset;

        tm.tm_year = str2num(value + 0);
        tm.tm_mon  = str2num(value + 2) - 1;
        tm.tm_mday = str2num(value + 4);
        tm.tm_hour = str2num(value + 6);
        tm.tm_min  = str2num(value + 8);
        tm.tm_sec  = str2num(value + 10);

        if (tm.tm_year < 50)
            tm.tm_year += 100;

        val = timegm(&tm) + offset;
        if (val < 0) {
            val = 0;
            arkime_session_add_tag(session, "cert:pre-epoch-time");
        }
        return val;
    }
    //GeneralizedTime
    else if (tag == 24 && len >= 10) {
        int pos;
        memset(&tm, 0, sizeof(tm));
        tm.tm_year = str4num(value + 0) - 1900;
        tm.tm_mon  = str2num(value + 4) - 1;
        tm.tm_mday = str2num(value + 6);
        tm.tm_hour = str2num(value + 8);
        if (len < 12 || value[10] == 'Z' || value[10] == '+' || value[10] == '-') {
            pos = 10;
            goto gtdone;
        }

        tm.tm_min  = str2num(value + 10);
        if (len < 14 || value[12] == 'Z' || value[12] == '+' || value[12] == '-') {
            pos = 12;
            goto gtdone;
        }
        tm.tm_sec  = str2num(value + 12);
        if (len < 15 || value[14] == 'Z' || value[14] == '+' || value[14] == '-') {
            pos = 14;
            goto gtdone;
        }
        if (value[14] == '.') {
            pos = 18;
        } else {
            pos = 14;
        }
gtdone:
        if (pos == len) {
            val = timegm(&tm);
        } else {
            if (pos + 5 < len && (value[pos] == '+' || value[pos] == '-')) {
                offset = str2num(value + pos + 1) * 60 +  str2num(value + pos + 3);

                if (value[pos] == '-')
                    offset = -offset;
            }
            val = timegm(&tm) + offset;
        }

        if (val < 0) {
            val = 0;
            arkime_session_add_tag(session, "cert:pre-epoch-time");
        }
        return val;
    }
    return 0;
}
/******************************************************************************/
LOCAL int cstring_cmp(const void *a, const void *b)
{
    return strcmp(*(char **)a, *(char **)b);
}
/******************************************************************************/
void arkime_parsers_init()
{
    namedFuncsHash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    if (config.nodeClass)
        snprintf(classTag, sizeof(classTag), "class:%s", config.nodeClass);

    arkime_field_define("general", "integer",
                        "session.segments", "Session Segments", "segmentCnt",
                        "Number of segments in session so far",
                        0,  ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("general", "integer",
                        "session.length", "Session Length", "length",
                        "Session Length in milliseconds so far",
                        0,  ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    userField = arkime_field_define("general", "lotermfield",
                                    "user", "User", "user",
                                    "External user set for session",
                                    ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_LINKED_SESSIONS,
                                    "category", "user",
                                    (char *)NULL);

    int flags = MAGIC_MIME;

    char *strMagicMode = arkime_config_str(NULL, "magicMode", "both");

    if (strcmp(strMagicMode, "libmagic") == 0) {
        magicMode = ARKIME_MAGICMODE_LIBMAGIC;
    } else if (strcmp(strMagicMode, "libmagicnotext") == 0) {
        magicMode = ARKIME_MAGICMODE_LIBMAGIC;
        flags |= MAGIC_NO_CHECK_TEXT;
    } else if (strcmp(strMagicMode, "molochmagic") == 0) {
        LOG("WARNING - magicMode of `molochmagic` no longer supported, switching to `both`");
        magicMode = ARKIME_MAGICMODE_BOTH;
    } else if (strcmp(strMagicMode, "basic") == 0) {
        magicMode = ARKIME_MAGICMODE_BASIC;
    } else if (strcmp(strMagicMode, "both") == 0) {
        magicMode = ARKIME_MAGICMODE_BOTH;
    } else if (strcmp(strMagicMode, "none") == 0) {
        magicMode = ARKIME_MAGICMODE_NONE;
    } else {
        CONFIGEXIT("Unknown magicMode '%s'", strMagicMode);
    }

    g_free(strMagicMode);

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

    if (magicMode == ARKIME_MAGICMODE_LIBMAGIC || magicMode == ARKIME_MAGICMODE_BOTH) {
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

    ArkimeStringHashStd_t loaded;
    HASH_INIT(s_, loaded, arkime_string_hash, arkime_string_cmp);

    ArkimeString_t *hstring;
    int d;

    char **disableParsers = arkime_config_str_list(NULL, "disableParsers", "arp.so");
    for (d = 0; disableParsers[d]; d++) {
        hstring = ARKIME_TYPE_ALLOC0(ArkimeString_t);
        hstring->str = disableParsers[d];
        hstring->len = strlen(disableParsers[d]);
        HASH_ADD(s_, loaded, hstring->str, hstring);
    }

    if (!config.parseSMTP) {
        hstring = ARKIME_TYPE_ALLOC0(ArkimeString_t);
        hstring->str = g_strdup("smtp.so");
        hstring->len = strlen(hstring->str);
        HASH_ADD(s_, loaded, hstring->str, hstring);
    }

    if (!config.parseSMB) {
        hstring = ARKIME_TYPE_ALLOC0(ArkimeString_t);
        hstring->str = g_strdup("smb.so");
        hstring->len = strlen(hstring->str);
        HASH_ADD(s_, loaded, hstring->str, hstring);
    }

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
        gchar *filenames[100];
        int    flen = 0;

        while ((filename = g_dir_read_name(dir)) && flen < 100) {
            // Skip hidden files/directories
            if (filename[0] == '.')
                continue;

            // If it doesn't end with .so we ignore it
            if (strlen(filename) < 3 || strcasecmp(".so", filename + strlen(filename) - 3) != 0) {
                continue;
            }

            HASH_FIND(s_, loaded, filename, hstring);
            if (hstring) {
                if (config.debug) {
                    LOG("Skipping %s in %s since already loaded", filename, config.parsersDir[d]);
                }
                continue; /* Already loaded */
            }

            filenames[flen] = g_strdup(filename);
            flen++;
        }

        qsort((void *)filenames, (size_t)flen, sizeof(char *), cstring_cmp);

        int i;
        for (i = 0; i < flen; i++) {
            gchar *path = g_build_filename (config.parsersDir[d], filenames[i], NULL);
            GModule *parser = g_module_open (path, 0); /*G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);*/

            if (!parser) {
                LOG("ERROR - Couldn't load parser %s from '%s'\n%s", filenames[i], path, g_module_error());
                g_free(filenames[i]);
                g_free (path);
                continue;
            }

            ArkimePluginInitFunc parser_init;

            if (!g_module_symbol(parser, "arkime_parser_init", (gpointer *)(char * )&parser_init) || parser_init == NULL) {
                LOG("ERROR - Module %s doesn't have a arkime_parser_init", filenames[i]);
                g_free(filenames[i]);
                g_free (path);
                continue;
            }

            if (config.debug > 1) {
                LOG("Loaded %s", path);
            }

            parser_init();

            hstring = ARKIME_TYPE_ALLOC0(ArkimeString_t);
            hstring->str = filenames[i];
            hstring->len = strlen(filenames[i]);
            HASH_ADD(s_, loaded, hstring->str, hstring);

            if (config.debug)
                LOG("Loaded %s", path);

            g_free (path);
        }
        g_dir_close(dir);
    }

    if (loaded.count == 0) {
        LOG("WARNING - No parsers loaded, is parsersDir set correctly");
    }

    HASH_FORALL_POP_HEAD2(s_, loaded, hstring) {
        g_free(hstring->str);
        ARKIME_TYPE_FREE(ArkimeString_t, hstring);
    }
    g_free(disableParsers); // NOT, g_strfreev because using the pointers

    // Set tags field up AFTER loading plugins
    config.tagsStringField = arkime_field_define("general", "termfield",
                                                 "tags", "Tags", "tags",
                                                 "Tags set for session",
                                                 ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_LINKED_SESSIONS,
                                                 (char *)NULL);

    arkime_field_define("general", "lotermfield",
                        "asset", "Asset", "asset",
                        "Asset name",
                        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_LINKED_SESSIONS,
                        (char *)NULL);

    gsize keys_len;
    gchar **keys = arkime_config_section_keys(NULL, "custom-fields", &keys_len);

    int i;
    for (i = 0; i < (int)keys_len; i++) {
        char *value = arkime_config_section_str(NULL, "custom-fields", keys[i], NULL);
        arkime_field_define_text_full(keys[i], value, NULL);
        g_free(value);
    }
    g_strfreev(keys);


    if (config.extraOps) {
        for (i = 0; config.extraOps[i]; i++) { }
        arkime_field_ops_init(&config.ops, i, 0);
        for (i = 0; config.extraOps[i]; i++) {
            char *equal = strchr(config.extraOps[i], '=');
            if (!equal) {
                CONFIGEXIT("Must be FieldExpr=value, missing equal '%s'", config.extraOps[i]);
            }
            int len = strlen(equal + 1);
            if (!len) {
                CONFIGEXIT("Must be FieldExpr=value, empty value for '%s'", config.extraOps[i]);
            }
            *equal = 0;
            int fieldPos = arkime_field_by_exp(config.extraOps[i]);
            if (fieldPos == -1) {
                CONFIGEXIT("Must be FieldExpr=value, Unknown field expression '%s'", config.extraOps[i]);
            }
            arkime_field_ops_add(&config.ops, fieldPos, equal + 1, len);
        }
    } else {
        arkime_field_ops_init(&config.ops, 0, 0);
    }
}
/******************************************************************************/
void arkime_parsers_exit() {
    if (magicMode == ARKIME_MAGICMODE_LIBMAGIC || magicMode == ARKIME_MAGICMODE_BOTH) {
        int t;
        for (t = 0; t < config.packetThreads; t++) {
            magic_close(cookie[t]);
        }
    }
}
/******************************************************************************/
void arkime_print_hex_string(const uint8_t *data, unsigned int length)
{
    unsigned int i;

    for (i = 0; i < length; i++)
    {
        printf("%02x", data[i]);
    }

    printf("\n");
}
/******************************************************************************/
char *arkime_sprint_hex_string(char *buf, const uint8_t *data, unsigned int length)
{
    unsigned int i;

    for (i = 0; i < length; i++)
    {
        memcpy(buf + i * 2, arkime_char_to_hexstr[data[i]], 2);
    }
    buf[i * 2] = 0;
    return buf;
}
/******************************************************************************/
void  arkime_parsers_register2(ArkimeSession_t *session, ArkimeParserFunc func, void *uw, ArkimeParserFreeFunc ffunc, ArkimeParserSaveFunc sfunc)
{
#ifdef DEBUG_PARSERS
    LOG("session: %p func: %p uw: %p", session, func, uw);
#endif

    if (session->parserNum > 30) {
        char ipStr[200];
        arkime_session_pretty_string(session, ipStr, sizeof(ipStr));
        LOG("WARNING - Too many parsers registered: %d %s", session->parserNum, ipStr);
        return;
    }

    // Check if this is a duplicate
    for (int i = 0; i < session->parserNum; i++) {
        if (session->parserInfo[i].parserFunc == func && session->parserInfo[i].uw == uw) {
            return;
        }
    }

    if (session->parserNum >= session->parserLen) {
        if (session->parserLen == 0) {
            session->parserLen = 2;
        } else {
            session->parserLen *= 1.67;
        }
        session->parserInfo = realloc(session->parserInfo, sizeof(ArkimeParserInfo_t) * session->parserLen);
    }

    session->parserInfo[session->parserNum].parserFunc     = func;
    session->parserInfo[session->parserNum].uw             = uw;
    session->parserInfo[session->parserNum].parserFreeFunc = ffunc;
    session->parserInfo[session->parserNum].parserSaveFunc = sfunc;

    session->parserNum++;
}
/******************************************************************************/
void  arkime_parsers_unregister(ArkimeSession_t *session, void *uw)
{
    int i;
    for (i = 0; i < session->parserNum; i++) {
        if (session->parserInfo[i].uw == uw && session->parserInfo[i].parserFunc != 0) {
            if (session->parserInfo[i].parserFreeFunc) {
                session->parserInfo[i].parserFreeFunc(session, uw);
            }

            memset(&session->parserInfo[i], 0, sizeof(session->parserInfo[i]));
            break;
        }
    }
}
/******************************************************************************/
typedef struct arkime_classify_t
{
    const char          *name;
    void                *uw;
    int                  offset;
    const uint8_t       *match;
    int                  matchlen;
    int                  minlen;
    ArkimeClassifyFunc   func;
} ArkimeClassify_t;

typedef struct
{
    ArkimeClassify_t   **arr;
    short               size;
    short               cnt;
} ArkimeClassifyHead_t;

LOCAL ArkimeClassifyHead_t classifersTcp0;
LOCAL ArkimeClassifyHead_t classifersTcp1[256];
LOCAL ArkimeClassifyHead_t classifersTcp2[256][256];
LOCAL ArkimeClassifyHead_t classifersTcpPortSrc[0x10000];
LOCAL ArkimeClassifyHead_t classifersTcpPortDst[0x10000];

LOCAL ArkimeClassifyHead_t classifersUdp0;
LOCAL ArkimeClassifyHead_t classifersUdp1[256];
LOCAL ArkimeClassifyHead_t classifersUdp2[256][256];
LOCAL ArkimeClassifyHead_t classifersUdpPortSrc[0x10000];
LOCAL ArkimeClassifyHead_t classifersUdpPortDst[0x10000];

/******************************************************************************/
void arkime_parsers_classifier_add(ArkimeClassifyHead_t *ch, ArkimeClassify_t *c)
{
    int i;
    for (i = 0; i < ch->cnt; i++) {
        if (ch->arr[i]->offset == c->offset &&
            ch->arr[i]->func == c->func &&
            c->matchlen == ch->arr[i]->matchlen &&
            strcmp(ch->arr[i]->name, c->name) == 0 &&
            memcmp(ch->arr[i]->match, c->match, c->matchlen) == 0) {

            if (config.debug > 1) {
                LOG("Info, duplicate (could be normal) %s %s", c->name, c->match);
            }
            ARKIME_TYPE_FREE(ArkimeClassify_t, c);
            return;
        }
    }
    if (ch->cnt >= ch->size) {
        if (ch->size == 0) {
            ch->size = 2;
        } else {
            ch->size *= 1.67;
        }
        ch->arr = realloc(ch->arr, sizeof(ArkimeClassify_t *) * ch->size);
    }

    ch->arr[ch->cnt] = c;
    ch->cnt++;
}
/******************************************************************************/
void arkime_parsers_classifier_register_port_internal(const char *name, void *uw, uint16_t port, uint32_t type, ArkimeClassifyFunc func, size_t sessionsize, int apiversion)
{
    if (sizeof(ArkimeSession_t) != sessionsize) {
        CONFIGEXIT("Parser '%s' built with different version of arkime.h\n %u != %u", name, (unsigned int)sizeof(ArkimeSession_t),  (unsigned int)sessionsize);
    }

    if (ARKIME_API_VERSION != apiversion) {
        CONFIGEXIT("Parser '%s' built with different version of arkime.h\n %d %d", name, ARKIME_API_VERSION, apiversion);
    }

    if ((type & (ARKIME_PARSERS_PORT_UDP | ARKIME_PARSERS_PORT_TCP)) == 0) {
        CONFIGEXIT("Parser '%s' has empty type", name);
    }

    ArkimeClassify_t *c = ARKIME_TYPE_ALLOC0(ArkimeClassify_t);
    c->name     = name;
    c->uw       = uw;
    c->func     = func;

    if (config.debug)
        LOG("adding %s port:%u type:%02x uw:%p", name, port, type, uw);

    if (type & ARKIME_PARSERS_PORT_TCP_SRC)
        arkime_parsers_classifier_add(&classifersTcpPortSrc[port], c);
    if (type & ARKIME_PARSERS_PORT_TCP_DST)
        arkime_parsers_classifier_add(&classifersTcpPortDst[port], c);

    if (type & ARKIME_PARSERS_PORT_UDP_SRC)
        arkime_parsers_classifier_add(&classifersUdpPortSrc[port], c);
    if (type & ARKIME_PARSERS_PORT_UDP_DST)
        arkime_parsers_classifier_add(&classifersUdpPortDst[port], c);
}
/******************************************************************************/
void arkime_parsers_classifier_register_tcp_internal(const char *name, void *uw, int offset, const uint8_t *match, int matchlen, ArkimeClassifyFunc func, size_t sessionsize, int apiversion)
{
    if (sizeof(ArkimeSession_t) != sessionsize) {
        CONFIGEXIT("Parser '%s' built with different version of arkime.h\n %u != %u", name, (unsigned int)sizeof(ArkimeSession_t),  (unsigned int)sessionsize);
    }

    if (ARKIME_API_VERSION != apiversion) {
        CONFIGEXIT("Parser '%s' built with different version of arkime.h\n %d %d", name, ARKIME_API_VERSION, apiversion);
    }

    if (!match && matchlen != 0)
        CONFIGEXIT("Can't have a null match for %s", name);

    ArkimeClassify_t *c = ARKIME_TYPE_ALLOC0(ArkimeClassify_t);
    c->name     = name;
    c->uw       = uw;
    c->offset   = offset;
    c->match    = match;
    c->matchlen = matchlen;
    c->minlen   = matchlen + offset;
    c->func     = func;

    if (config.debug) {
        char hex[1000];
        arkime_sprint_hex_string(hex, match, matchlen);
        LOG("adding %s matchlen:%d offset:%d match %s (0x%s)", name, matchlen, offset, match, hex);
    }
    if (matchlen == 0 || offset != 0) {
        arkime_parsers_classifier_add(&classifersTcp0, c);
    } else if (matchlen == 1) {
        arkime_parsers_classifier_add(&classifersTcp1[(uint8_t)match[0]], c);
    } else  {
        c->match += 2;
        c->matchlen -= 2;
        arkime_parsers_classifier_add(&classifersTcp2[(uint8_t)match[0]][(uint8_t)match[1]], c);
    }
}
/******************************************************************************/
void arkime_parsers_classifier_register_udp_internal(const char *name, void *uw, int offset, const uint8_t *match, int matchlen, ArkimeClassifyFunc func, size_t sessionsize, int apiversion)
{
    if (sizeof(ArkimeSession_t) != sessionsize) {
        CONFIGEXIT("Parser '%s' built with different version of arkime.h", name);
    }

    if (ARKIME_API_VERSION != apiversion) {
        CONFIGEXIT("Parser '%s' built with different version of arkime.h", name);
    }

    ArkimeClassify_t *c = ARKIME_TYPE_ALLOC0(ArkimeClassify_t);
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
        arkime_parsers_classifier_add(&classifersUdp0, c);
    } else if (matchlen == 1) {
        arkime_parsers_classifier_add(&classifersUdp1[(uint8_t)match[0]], c);
    } else  {
        c->match += 2;
        c->matchlen -= 2;
        arkime_parsers_classifier_add(&classifersUdp2[(uint8_t)match[0]][(uint8_t)match[1]], c);
    }
}
/******************************************************************************/
void arkime_parsers_classify_udp(ArkimeSession_t *session, const uint8_t *data, int remaining, int which)
{
    int i;

    if (remaining < 2)
        return;

#ifdef DEBUG_PARSERS
    char buf[101];
    LOG("len: %d direction: %d hex: %s data: %.*s", remaining, which, arkime_sprint_hex_string(buf, data, MIN(remaining, 50)), MIN(remaining, 50), data);
#endif

    for (i = 0; i < classifersUdpPortSrc[session->port1].cnt; i++) {
        classifersUdpPortSrc[session->port1].arr[i]->func(session, data, remaining, which, classifersUdpPortSrc[session->port1].arr[i]->uw);
    }

    for (i = 0; i < classifersUdpPortDst[session->port2].cnt; i++) {
        classifersUdpPortDst[session->port2].arr[i]->func(session, data, remaining, which, classifersUdpPortDst[session->port2].arr[i]->uw);
    }

    for (i = 0; i < classifersUdp0.cnt; i++) {
        ArkimeClassify_t *c = classifersUdp0.arr[i];
        if (remaining >= c->minlen && memcmp(data + c->offset, c->match, c->matchlen) == 0) {
            c->func(session, data, remaining, which, c->uw);
        }
    }

    for (i = 0; i < classifersUdp1[data[0]].cnt; i++)
        classifersUdp1[data[0]].arr[i]->func(session, data, remaining, which, classifersUdp1[data[0]].arr[i]->uw);

    for (i = 0; i < classifersUdp2[data[0]][data[1]].cnt; i++) {
        ArkimeClassify_t *c = classifersUdp2[data[0]][data[1]].arr[i];
        if (remaining >= c->minlen && memcmp(data + 2, c->match, c->matchlen) == 0) {
            c->func(session, data, remaining, which, c->uw);
        }
    }

    arkime_rules_run_after_classify(session);
    if (config.yara && !config.yaraEveryPacket && !session->stopYara)
        arkime_yara_execute(session, data, remaining, 0);
}
/******************************************************************************/
void arkime_parsers_classify_tcp(ArkimeSession_t *session, const uint8_t *data, int remaining, int which)
{
    int i;

#ifdef DEBUG_PARSERS
    char buf[101];
    LOG("len: %d direction: %d hex: %s data: %.*s", remaining, which, arkime_sprint_hex_string(buf, data, MIN(remaining, 50)), MIN(remaining, 50), data);
#endif

    if (remaining < 2)
        return;

    for (i = 0; i < classifersTcpPortSrc[session->port1].cnt; i++) {
        classifersTcpPortSrc[session->port1].arr[i]->func(session, data, remaining, which, classifersTcpPortSrc[session->port1].arr[i]->uw);
    }

    for (i = 0; i < classifersTcpPortDst[session->port2].cnt; i++) {
        classifersTcpPortDst[session->port2].arr[i]->func(session, data, remaining, which, classifersTcpPortDst[session->port2].arr[i]->uw);
    }

    for (i = 0; i < classifersTcp0.cnt; i++) {
        ArkimeClassify_t *c = classifersTcp0.arr[i];
        if (remaining >= c->minlen && memcmp(data + c->offset, c->match, c->matchlen) == 0) {
            c->func(session, data, remaining, which, c->uw);
        }
    }

    for (i = 0; i < classifersTcp1[data[0]].cnt; i++) {
        classifersTcp1[data[0]].arr[i]->func(session, data, remaining, which, classifersTcp1[data[0]].arr[i]);
    }

    for (i = 0; i < classifersTcp2[data[0]][data[1]].cnt; i++) {
        ArkimeClassify_t *c = classifersTcp2[data[0]][data[1]].arr[i];
        if (remaining >= c->minlen && memcmp(data + 2, c->match, c->matchlen) == 0) {
            c->func(session, data, remaining, which, c->uw);
        }
    }

    arkime_rules_run_after_classify(session);
    if (config.yara && !config.yaraEveryPacket && !session->stopYara)
        arkime_yara_execute(session, data, remaining, 0);
}

/******************************************************************************/
uint32_t arkime_parser_add_named_func(const char *name, ArkimeParserNamedFunc func)
{
    ArkimeNamedInfo_t *info = g_hash_table_lookup(namedFuncsHash, name);
    if (!info) {
        info = ARKIME_TYPE_ALLOC0(ArkimeNamedInfo_t);
        info->funcs = g_ptr_array_new();
        namedFuncsMax++; // Don't use 0
        if (namedFuncsMax >= MAX_NAMED_FUNCS) {
            LOGEXIT("ERROR - Too many named functions %s", name);
            return 0;
        }
        info->id = namedFuncsMax;
        namedFuncsArr[namedFuncsMax] = info;
        g_hash_table_insert(namedFuncsHash, g_strdup(name), info);
    }
    g_ptr_array_add(info->funcs, func);
    return info->id;
}
/******************************************************************************/
uint32_t arkime_parser_get_named_func(const char *name)
{
    ArkimeNamedInfo_t *info = g_hash_table_lookup(namedFuncsHash, name);
    if (!info) {
        info = ARKIME_TYPE_ALLOC0(ArkimeNamedInfo_t);
        info->funcs = g_ptr_array_new();
        namedFuncsMax++; // Don't use 0
        if (namedFuncsMax >= MAX_NAMED_FUNCS) {
            LOGEXIT("ERROR - Too many named functions %s", name);
            return 0;
        }
        info->id = namedFuncsMax;
        namedFuncsArr[namedFuncsMax] = info;
        g_hash_table_insert(namedFuncsHash, g_strdup(name), info);
    }
    return info->id;
}
/******************************************************************************/
void arkime_parser_call_named_func(uint32_t id, ArkimeSession_t *session, const uint8_t *data, int len, void *uw)
{
    if (id == 0 || id > namedFuncsMax)
        return;
    ArkimeNamedInfo_t *info = namedFuncsArr[id];
    for (int i = 0; i < (int)info->funcs->len; i++) {
        ArkimeParserNamedFunc func = g_ptr_array_index(info->funcs, i);
        func(session, data, len, uw);
    }
}
