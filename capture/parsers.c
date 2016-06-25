/* parsers.c  -- Functions for dealing with classification and parsers
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
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

/******************************************************************************/
void moloch_parsers_magic(MolochSession_t *session, int field, const char *data, int len)
{
    if (len < 3)
        return;

    const char *m = magic_buffer(cookie[session->thread], data, MIN(len,50));
    if (m) {
        int len;
        char *semi = strchr(m, ';');
        if (semi) {
            len = semi - m;
        } else {
            len = strlen(m);
        }
        moloch_field_string_add(field, session, m, len, TRUE);
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

    int t;
    for (t = 0; t < config.packetThreads; t++) {
        cookie[t] = magic_open(flags);
        if (!cookie[t]) {
            LOG("Error with libmagic %s", magic_error(cookie[t]));
        } else {
            magic_load(cookie[t], NULL);
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
    int tagsField = moloch_field_define("general", "termfield",
        "tags", "Tags", "ta",
        "Tags set for session",
        MOLOCH_FIELD_TYPE_INT_GHASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);

    moloch_field_define("general", "lotermfield",
        "asset", "Asset", "asset-term",
        "Asset name",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_COUNT | MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        NULL);

    if (config.nodeClass) {
        snprintf(classTag, sizeof(classTag), "node:%s", config.nodeClass);
        moloch_db_get_tag(NULL, tagsField, classTag, NULL);
    }

    if (config.extraTags) {
        int i;
        for (i = 0; config.extraTags[i]; i++) {
            moloch_db_get_tag(NULL, tagsField, config.extraTags[i], NULL);
        }
    }
}
/******************************************************************************/
void moloch_parsers_exit() {
    int t;
    for (t = 0; t < config.packetThreads; t++) {
        magic_close(cookie[t]);
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

MolochClassifyHead_t classifersTcp0;
MolochClassifyHead_t classifersTcp1[256];
MolochClassifyHead_t classifersTcp2[256][256];

MolochClassifyHead_t classifersUdp0;
MolochClassifyHead_t classifersUdp1[256];
MolochClassifyHead_t classifersUdp2[256][256];

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
void moloch_parsers_classifier_register_tcp_internal(const char *name, void *uw, int offset, const unsigned char *match, int matchlen, MolochClassifyFunc func, size_t sessionsize, int apiversion)
{
    if (sizeof(MolochSession_t) != sessionsize) {
        LOG("Plugin '%s' built with different version of moloch.h\n %lu != %lu", name, sizeof(MolochSession_t),  sessionsize);
        exit(-1);
    }

    if (MOLOCH_API_VERSION != apiversion) {
        LOG("Plugin '%s' built with different version of moloch.h\n %u %d", name, MOLOCH_API_VERSION, apiversion);
        exit(-1);
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
        LOG("Plugin '%s' built with different version of moloch.h", name);
        exit(-1);
    }

    if (MOLOCH_API_VERSION != apiversion) {
        LOG("Plugin '%s' built with different version of moloch.h", name);
        exit(-1);
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
}
