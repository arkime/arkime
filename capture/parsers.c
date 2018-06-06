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
LOCAL  gchar                 classTag[100];

LOCAL  magic_t               cookie[MOLOCH_MAX_PACKET_THREADS];

extern unsigned char         moloch_char_to_hexstr[256][3];

int    userField;

enum MolochMagicMode { MOLOCH_MAGICMODE_LIBMAGIC, MOLOCH_MAGICMODE_BOTH, MOLOCH_MAGICMODE_BASIC, MOLOCH_MAGICMODE_NONE};

LOCAL enum MolochMagicMode magicMode;

/******************************************************************************/
#define MOLOCH_MAGIC_RESULT(str) moloch_field_string_add(field, session, str, sizeof(str)-1, TRUE)
const char *moloch_parsers_magic_basic(MolochSession_t *session, int field, const char *data, int len)
{
    switch (data[0]) {
    case 0:
        if (len > 10 && memcmp(data+4, "ftyp", 4) == 0) {
            if (memcmp(data+8, "qt", 2) == 0) {
                return MOLOCH_MAGIC_RESULT("video/quicktime");
            }
            if (memcmp(data+8, "3g", 2) == 0) {
                return MOLOCH_MAGIC_RESULT("video/3gpp");
            }
        } else if (memcmp(data, "\000\001\000\000\000", 5) == 0) {
            return MOLOCH_MAGIC_RESULT("application/x-font-ttf");
        }
        break;
    case '\032':
        if (memcmp(data, "\x1a\x45\xdf\xa3", 4) == 0) {
            if (moloch_memstr(data+4, len-4, "webm", 4)) {
                return MOLOCH_MAGIC_RESULT("video/webm");
            }
            if (moloch_memstr(data+4, len-4, "matroska", 8)) {
                return MOLOCH_MAGIC_RESULT("video/x-matroska");
            }
        }
        break;
    case '\037':
        if (data[1] == '\213') {
            return MOLOCH_MAGIC_RESULT("application/x-gzip");
        }
        if (data[1] == '\235') {
            return MOLOCH_MAGIC_RESULT("application/x-compress");
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
            if (moloch_memstr(data+3, len-3, "node", 4)) {
                return MOLOCH_MAGIC_RESULT("application/javascript");
            } else if (moloch_memstr(data+3, len-3, "perl", 4)) {
                return MOLOCH_MAGIC_RESULT("text/x-perl");
            } else if (moloch_memstr(data+3, len-3, "ruby", 4)) {
                return MOLOCH_MAGIC_RESULT("text/x-ruby");
            } else if (moloch_memstr(data+3, len-3, "python", 6)) {
                return MOLOCH_MAGIC_RESULT("text/x-python");
            }
            return MOLOCH_MAGIC_RESULT("text/x-shellscript");
        }
        break;
    case '%':
        if (memcmp(data, "%PDF-", 5) == 0) {
            return MOLOCH_MAGIC_RESULT("application/pdf");
        }
        break;
    case '<':
        switch(data[1]) {
        case '!':
            if (len > 14 && strncasecmp(data, "<!doctype html", 14) == 0) {
                return MOLOCH_MAGIC_RESULT("text/html");
            }
            if (len > 13 && strncasecmp(data, "<!doctype svg", 13) == 0) {
                return MOLOCH_MAGIC_RESULT("text/svg+xml");
            }
            break;
        case '?':
            if (strncasecmp(data, "<?xml", 5) == 0) {
                if (moloch_memstr(data+5, len-5, "<svg", 4)) {
                    return MOLOCH_MAGIC_RESULT("image/svg+xml");
                }
                return MOLOCH_MAGIC_RESULT("text/xml");
            }
            if (strncasecmp(data+2, "php", 3) == 0 ||
                strncasecmp(data+2, " php", 4) == 0) {
                return MOLOCH_MAGIC_RESULT("text/x-php");
            }
            break;
        case 'B':
        case 'b':
            if (strncasecmp(data, "<body", 5) == 0) {
                return MOLOCH_MAGIC_RESULT("text/html");
            }
            break;
        case 'H':
        case 'h':
            if (strncasecmp(data, "<head", 5) == 0) {
                return MOLOCH_MAGIC_RESULT("text/html");
            }
            if (strncasecmp(data, "<html", 5) == 0) {
                return MOLOCH_MAGIC_RESULT("text/html");
            }
            break;
        case 's':
        case 'S':
            if (strncasecmp(data, "<svg", 4) == 0) {
                return MOLOCH_MAGIC_RESULT("image/svg");
            }
            break;
        }
        break;
    case '{':
        if (data[1] == '"' && isalpha(data[2])) {
            return MOLOCH_MAGIC_RESULT("application/json");
        }
        break;
    case '8':
        if (memcmp(data, "8BPS", 4) == 0) {
            return MOLOCH_MAGIC_RESULT("image/vnd.adobe.photoshop");
        }
        break;
    case 'B':
        if (data[1] == 'M') {
            return MOLOCH_MAGIC_RESULT("application/x-ms-bmp");
        }

        if (memcmp(data, "BZh", 3) == 0) {
            return MOLOCH_MAGIC_RESULT("application/x-bzip2");
        }
        break;
    case 'C':
        if (memcmp(data, "CWS", 3) == 0) {
            return MOLOCH_MAGIC_RESULT("application/x-shockwave-flash");
        }
        break;
    case 'F':
        if (memcmp(data, "FLV\001", 4) == 0) {
            return MOLOCH_MAGIC_RESULT("video/x-flv");
        }
        break;
    case 'G':
        if (memcmp(data, "GIF8", 4) == 0) {
            return MOLOCH_MAGIC_RESULT("image/gif");
        }
        break;
    case 'I':
        if (memcmp(data, "ID3", 3) == 0) {
            return MOLOCH_MAGIC_RESULT("audio/mpeg");
        }
        break;
    case 'M':
        if (data[1] == 'Z') {
            return MOLOCH_MAGIC_RESULT("application/x-dosexec");
        }
        if (memcmp(data, "MSCF\000\000", 6) == 0) {
            return MOLOCH_MAGIC_RESULT("application/vnd.ms-cab-compressed");
        }
        break;
    case 'O':
        if (len > 40 && memcmp(data, "OggS", 4) == 0) {
            // https://speex.org/docs/manual/speex-manual/node8.html
            if (memcmp(data+28, "Speex   ", 8) == 0) {
                return MOLOCH_MAGIC_RESULT("audio/ogg");
            }

            // https://xiph.org/flac/ogg_mapping.html
            if (memcmp(data+29, "FLAC", 4) == 0) {
                return MOLOCH_MAGIC_RESULT("audio/ogg");
            }

            // https://xiph.org/vorbis/doc/Vorbis_I_spec.html
            if (memcmp(data+28, "\001vorbis", 7) == 0) {
                return MOLOCH_MAGIC_RESULT("audio/ogg");
            }

            // https://www.theora.org/doc/Theora.pdf
            if (memcmp(data+28, "\x80theora", 7) == 0) {
                return MOLOCH_MAGIC_RESULT("video/ogg");
            }
        } else if (memcmp(data, "OTTO", 4) == 0) {
            return MOLOCH_MAGIC_RESULT("application/vnd.ms-opentype");
        }
        break;
    case 'P':
        if (memcmp(data, "PK\003\004", 4) == 0) {
            return MOLOCH_MAGIC_RESULT("application/zip");
        }
        if (memcmp(data, "PK\005\006", 4) == 0) {
            return MOLOCH_MAGIC_RESULT("application/zip");
        }
        if (memcmp(data, "PK\007\008PK", 6) == 0) {
            return MOLOCH_MAGIC_RESULT("application/zip");
        }
        break;
    case 'R':
        if (memcmp(data, "RIFF", 4) == 0) {
            return MOLOCH_MAGIC_RESULT("audio/x-wav");
        }
        if (memcmp(data, "Rar!\x1a", 5) == 0) {
            return MOLOCH_MAGIC_RESULT("application/x-rar");
        }
        break;
    case 'W':
        if (memcmp(data, "WAVE", 4) == 0) {
            return MOLOCH_MAGIC_RESULT("audio/x-wav");
        }
        break;
    case 'd':
        if (len > 20 && memcmp(data, "d8:announce", 11) == 0) {
            return MOLOCH_MAGIC_RESULT("application/x-bittorrent");
        }
        break;
    case 'w':
        if (memcmp(data, "wOFF", 4) == 0) {
            return MOLOCH_MAGIC_RESULT("application/font-woff");
        }
        if (memcmp(data, "wOF2", 4) == 0) {
            return MOLOCH_MAGIC_RESULT("application/font-woff2");
        }
        break;
    case '\x89':
        if (memcmp(data, "\x89PNG", 4) == 0) {
            return MOLOCH_MAGIC_RESULT("image/png");
        }
        break;
    case '\375':
        if (memcmp(data, "\3757zXZ", 5) == 0) {
            return MOLOCH_MAGIC_RESULT("application/x-xz");
        }
        break;
    case '\377':
        if (len > 10 && memcmp(data, "\377\330\377", 3) == 0) {
            return MOLOCH_MAGIC_RESULT("image/jpeg");
        }
        break;
    case '\xed':
        if (len > 10 && memcmp(data, "\xed\xab\xee\xdb", 4) == 0) {
            return MOLOCH_MAGIC_RESULT("application/x-rpm");
        }
        break;
    } /* switch */

    if (len > 257+5 && memcmp(data+257, "ustar", 5) == 0) {
        return MOLOCH_MAGIC_RESULT("application/x-tar");
    }
    if (moloch_memstr(data, len, "document.write", 14) ||
        moloch_memstr(data, len, "'use strict'", 12)) {
        return MOLOCH_MAGIC_RESULT("text/javascript");
    }
    return NULL;
}
/******************************************************************************/
const char *moloch_parsers_magic(MolochSession_t *session, int field, const char *data, int len)
{
    const char *m;
    if (len < 5)
        return NULL;

    switch (magicMode) {
    case MOLOCH_MAGICMODE_BASIC:
        return moloch_parsers_magic_basic(session, field, data, len);

    case MOLOCH_MAGICMODE_BOTH:
        m = moloch_parsers_magic_basic(session, field, data, len);
        if (m)
            return m;

        // Fall thru
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
    case MOLOCH_MAGICMODE_NONE:
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
    case IPPROTO_ICMPV6:
        moloch_session_add_protocol(session, "icmp");
        break;
    case IPPROTO_SCTP:
        moloch_session_add_protocol(session, "sctp");
        break;
    case IPPROTO_ESP:
        moloch_session_add_protocol(session, "esp");
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
LOCAL int cstring_cmp(const void *a, const void *b)
{
   return strcmp(*(char **)a, *(char **)b);
}
/******************************************************************************/
void moloch_parsers_init()
{
    moloch_field_define("general", "integer",
        "session.segments", "Session Segments", "segmentCnt",
        "Number of segments in session so far",
        0,  MOLOCH_FIELD_FLAG_FAKE,
        NULL);

    moloch_field_define("general", "integer",
        "session.length", "Session Length", "length",
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

    char *strMagicMode = moloch_config_str(NULL, "magicMode", "both");

    if (strcmp(strMagicMode, "libmagic") == 0) {
        magicMode = MOLOCH_MAGICMODE_LIBMAGIC;
    } else if (strcmp(strMagicMode, "libmagicnotext") == 0) {
        magicMode = MOLOCH_MAGICMODE_LIBMAGIC;
        flags |= MAGIC_NO_CHECK_TEXT;
    } else if (strcmp(strMagicMode, "molochmagic") == 0) {
        LOG("WARNING - magicMode of `molochmagic` no longer supported, switching to `both`");
        magicMode = MOLOCH_MAGICMODE_BOTH;
    } else if (strcmp(strMagicMode, "basic") == 0) {
        magicMode = MOLOCH_MAGICMODE_BASIC;
    } else if (strcmp(strMagicMode, "both") == 0) {
        magicMode = MOLOCH_MAGICMODE_BOTH;
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

    if (magicMode == MOLOCH_MAGICMODE_LIBMAGIC || MOLOCH_MAGICMODE_BOTH) {
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
        gchar *filenames[100];
        int    flen = 0;

        while ((filename = g_dir_read_name(dir))) {
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
                g_free (path);
                continue;
            }
            g_free (path);

            MolochPluginInitFunc parser_init;

            if (!g_module_symbol(parser, "moloch_parser_init", (gpointer *)(char*)&parser_init) || parser_init == NULL) {
                LOG("ERROR - Module %s doesn't have a moloch_parser_init", filenames[i]);
                g_free(filenames[i]);
                continue;
            }

            if (config.debug > 1) {
                LOG("Loaded %s", path);
            }


            parser_init();

            hstring = MOLOCH_TYPE_ALLOC0(MolochString_t);
            hstring->str = filenames[i];
            hstring->len = strlen(filenames[i]);
            HASH_ADD(s_, loaded, hstring->str, hstring);
        }
        g_dir_close(dir);
    }

    HASH_FORALL_POP_HEAD(s_, loaded, hstring,
        g_free(hstring->str);
        MOLOCH_TYPE_FREE(MolochString_t, hstring);
    );

    // Set tags field up AFTER loading plugins
    config.tagsStringField = moloch_field_define("general", "termfield",
        "tags", "Tags", "tags",
        "Tags set for session",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);

    moloch_field_define("general", "lotermfield",
        "asset", "Asset", "asset",
        "Asset name",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);


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
    if (magicMode == MOLOCH_MAGICMODE_LIBMAGIC || magicMode == MOLOCH_MAGICMODE_BOTH) {
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
        LOGEXIT("Parser '%s' built with different version of moloch.h\n %lu != %lu", name, sizeof(MolochSession_t),  sessionsize);
    }

    if (MOLOCH_API_VERSION != apiversion) {
        LOGEXIT("Parser '%s' built with different version of moloch.h\n %u %d", name, MOLOCH_API_VERSION, apiversion);
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
        LOGEXIT("Parser '%s' built with different version of moloch.h\n %lu != %lu", name, sizeof(MolochSession_t),  sessionsize);
    }

    if (MOLOCH_API_VERSION != apiversion) {
        LOGEXIT("Parser '%s' built with different version of moloch.h\n %u %d", name, MOLOCH_API_VERSION, apiversion);
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
        LOGEXIT("Parser '%s' built with different version of moloch.h", name);
    }

    if (MOLOCH_API_VERSION != apiversion) {
        LOGEXIT("Parser '%s' built with different version of moloch.h", name);
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
