/* zeekintel.c  -- Plugin that reads Zeek (Bro) Intel framework files and
 *                 marks/tags sessions whose ips, hosts, urls, emails or
 *                 hashes match a loaded indicator.
 *
 * The plugin reads one or more files in the Zeek Intel framework TSV format
 * (the same format consumed by Zeek's `Intel::read_files`), for example:
 *
 *   #fields	indicator	indicator_type	meta.source	meta.desc	meta.url
 *   1.2.3.4	Intel::ADDR	source1	Some description	http://example.com
 *   10.0.0.0/8	Intel::SUBNET	source1	-	-
 *   evil.example.com	Intel::DOMAIN	source2	-	-
 *   d41d8cd98f00b204e9800998ecf8427e	Intel::FILE_HASH	source3	-	-
 *
 * The standard Zeek log directives (#separator, #fields, #unset_field,
 * #empty_field, #set_separator) are honored, and bare files that simply list
 * `indicator<TAB>indicator_type<TAB>meta.source...` (with no #fields header)
 * are also accepted using the default column order.
 *
 * On every session save the plugin matches the session against the loaded
 * indicators and, on a hit, adds the configured tag plus an entry in the
 * zeekintel[] object field.  Each entry mirrors the Zeek intel framework -
 * indicator, indicator_type, where (the Arkime field expression matched in,
 * eg ip.dst), source - plus the optional meta.desc and meta.url metadata.
 * The match objects are built and serialized at save time, so the subfields
 * are not individually searchable.
 *
 * The session fields to match are discovered from Arkime's field registry at
 * startup rather than hardcoded: every IP-typed field is matched against
 * ADDR/SUBNET indicators, and fields are matched against DOMAIN/URL/EMAIL/hash
 * indicators based on their "host"/"url"/"user"/"md5"/"sha*" category.  This
 * automatically covers ip/host/url/hash fields from all parsers and plugins.
 * A few host fields (eg DNS) live on FAKE schema fields whose category is
 * dropped after registration, so their internal getCb data fields are added
 * explicitly by expression.  (DNS answer IPs and cert hashes live inside
 * object fields not exposed as flat session fields, so are not matched.)
 *
 * Config (in the [default] section or a node specific section):
 *   plugins=zeekintel.so
 *   zeekIntelFiles=/path/one.intel;/path/two.intel  # ; separated list
 *   zeekIntelTag=zeek:intel                          # tag added on a match
 *
 * The files are registered with Arkime's standard config file monitor
 * (arkime_config_monitor_files) so they are loaded at startup and reloaded
 * automatically whenever they change.
 *
 * Copyright 2026 Andy Wick. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/socket.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include "patricia.h"
#include "arkime.h"

/******************************************************************************/

extern ArkimeConfig_t        config;

#define ZEEKINTEL_HASH_SIZE  16381

/*
 * Zeek indicator_type is an enum from a small fixed set, so it is stored as an
 * int and mapped to its output name only when written to a session - no
 * per-indicator string is allocated.  The output name is the lowercased Zeek
 * suffix (eg Intel::DOMAIN -> "domain"), which is the Arkime convention.
 */
typedef enum {
    ZEEK_INTEL_INVALID = 0,   // returned by lookup on failure / zero-init
    ZEEK_INTEL_ADDR,
    ZEEK_INTEL_SUBNET,
    ZEEK_INTEL_DOMAIN,
    ZEEK_INTEL_URL,
    ZEEK_INTEL_EMAIL,
    ZEEK_INTEL_FILE_HASH,
    ZEEK_INTEL_CERT_HASH,
    ZEEK_INTEL_PUBKEY_HASH,
    ZEEK_INTEL_TYPE_NUM
} ZeekIntelType_t;

LOCAL const char *zeekIntelTypeNames[ZEEK_INTEL_TYPE_NUM] = {
    NULL,   // ZEEK_INTEL_INVALID
    "addr", "subnet", "domain", "url",
    "email", "file_hash", "cert_hash", "pubkey_hash"
};

/* All Zeek indicator_type names share this prefix. */
#define ZEEK_INTEL_PREFIX     "Intel::"
#define ZEEK_INTEL_PREFIX_LEN (sizeof(ZEEK_INTEL_PREFIX) - 1)

/* One loaded indicator. */
typedef struct {
    char                    *indicator;
    char                    *source;   // meta.source (may be NULL)
    char                    *desc;     // meta.desc   (may be NULL)
    char                    *url;      // meta.url    (may be NULL)
    ZeekIntelType_t          type;     // Zeek indicator_type
} ZeekIntelItem_t;

/*
 * One match on a session, saved as an object in the zeekintel[] array.  String
 * fields are dup'd from the (reloadable) intel db; 'where' points at a process
 * lifetime field expression so it is referenced, not owned.
 */
typedef struct {
    char                    *indicator;
    char                    *source;   // may be NULL
    char                    *desc;     // may be NULL
    char                    *url;      // may be NULL
    const char              *where;    // Arkime expression matched in (not owned)
    ZeekIntelType_t          type;
} ZeekIntelMatch_t;

/* Hash bucket entry, keyed by the (normalized) indicator string. */
typedef struct zeekintel_string {
    struct zeekintel_string *s_next, *s_prev;
    char                    *str;      // hash key
    GPtrArray               *items;    // ZeekIntelItem_t *
    uint32_t                 s_hash;
    uint16_t                 s_bucket;
} ZeekIntelString_t;

typedef struct {
    struct zeekintel_string *s_next, *s_prev;
    int                      s_count;
} ZeekIntelStringHead_t;

typedef HASH_VAR(s_, ZeekIntelStringHash_t, ZeekIntelStringHead_t, ZEEKINTEL_HASH_SIZE);

/* The whole intel database, swapped atomically on reload. */
typedef struct {
    ZeekIntelStringHash_t    domains;      // Intel::DOMAIN
    ZeekIntelStringHash_t    urls;         // Intel::URL
    ZeekIntelStringHash_t    emails;       // Intel::EMAIL
    ZeekIntelStringHash_t    fileHashes;   // Intel::FILE_HASH
    ZeekIntelStringHash_t    certHashes;   // Intel::CERT_HASH
    ZeekIntelStringHash_t    pubkeyHashes; // Intel::PUBKEY_HASH
    patricia_tree_t         *ips;          // Intel::ADDR / Intel::SUBNET
} ZeekIntelDB_t;

/******************************************************************************/

LOCAL ZeekIntelDB_t         *currentDB;     // read lock-free by packet threads

LOCAL char                 **zeekIntelFiles;
LOCAL char                  *zeekIntelTag;

LOCAL int                    zeekintelField;     // the zeekintel[] object field

/*
 * Field positions to match against, discovered at startup from the field
 * registry by type (IPs) and category (host/url/hash/user) instead of being
 * hardcoded.  Each list holds the position that actually holds the data
 * (which, for fields like dns.host, is an internal getCb field).
 */
LOCAL GArray                *ipFields;      // ARKIME_FIELD_TYPE_IS_IP fields
LOCAL GArray                *domainFields;  // category "host"
LOCAL GArray                *urlFields;     // category "url"
LOCAL GArray                *fileHashFields;    // category "md5"/"sha1"/"sha256", non-cert fields
LOCAL GArray                *certHashFields;    // category "md5"/"sha1"/"sha256" on a "cert.*" field
LOCAL GArray                *pubkeyHashFields;  // category "md5"/"sha1"/"sha256" on a "cert.*pubkey*" field
LOCAL GArray                *emailFields;   // category "user"

/******************************************************************************/
LOCAL void zeekintel_item_free(gpointer data)
{
    ZeekIntelItem_t *item = data;
    g_free(item->indicator);
    if (item->source)
        g_free(item->source);
    if (item->desc)
        g_free(item->desc);
    if (item->url)
        g_free(item->url);
    ARKIME_TYPE_FREE(ZeekIntelItem_t, item);
}
/******************************************************************************/
LOCAL void zeekintel_ip_free(GPtrArray *items)
{
    if (items)
        g_ptr_array_free(items, TRUE);
}
/******************************************************************************/
LOCAL void zeekintel_hash_free(ZeekIntelStringHash_t *hash)
{
    ZeekIntelString_t *s;
    HASH_FORALL_POP_HEAD2(s_, *hash, s) {
        g_free(s->str);
        g_ptr_array_free(s->items, TRUE);
        ARKIME_TYPE_FREE(ZeekIntelString_t, s);
    }
}
/******************************************************************************/
LOCAL void zeekintel_db_free(ZeekIntelDB_t *db)
{
    if (!db)
        return;

    zeekintel_hash_free(&db->domains);
    zeekintel_hash_free(&db->urls);
    zeekintel_hash_free(&db->emails);
    zeekintel_hash_free(&db->fileHashes);
    zeekintel_hash_free(&db->certHashes);
    zeekintel_hash_free(&db->pubkeyHashes);

    if (db->ips)
        Destroy_Patricia(db->ips, (patricia_fn_data_t)zeekintel_ip_free);

    ARKIME_TYPE_FREE(ZeekIntelDB_t, db);
}
/******************************************************************************/
LOCAL ZeekIntelDB_t *zeekintel_db_new()
{
    ZeekIntelDB_t *db = ARKIME_TYPE_ALLOC0(ZeekIntelDB_t);

    HASH_INIT(s_, db->domains,      arkime_string_hash, arkime_string_cmp);
    HASH_INIT(s_, db->urls,         arkime_string_hash, arkime_string_cmp);
    HASH_INIT(s_, db->emails,       arkime_string_hash, arkime_string_cmp);
    HASH_INIT(s_, db->fileHashes,   arkime_string_hash, arkime_string_cmp);
    HASH_INIT(s_, db->certHashes,   arkime_string_hash, arkime_string_cmp);
    HASH_INIT(s_, db->pubkeyHashes, arkime_string_hash, arkime_string_cmp);
    db->ips = New_Patricia(128);

    return db;
}
/******************************************************************************/
/*
 * Add an item under a (caller owned) key in one of the string hashes.
 */
LOCAL void zeekintel_hash_add(ZeekIntelStringHash_t *hash, const char *key, ZeekIntelItem_t *item)
{
    ZeekIntelString_t *s;
    uint32_t           h = arkime_string_hash(key);

    HASH_FIND_HASH(s_, *hash, h, key, s);
    if (!s) {
        s = ARKIME_TYPE_ALLOC0(ZeekIntelString_t);
        s->str = g_strdup(key);
        s->items = g_ptr_array_new_with_free_func(zeekintel_item_free);
        HASH_ADD_HASH(s_, *hash, h, s->str, s);
    }
    g_ptr_array_add(s->items, item);
}
/******************************************************************************/
/*
 * Insert an ADDR/SUBNET indicator.  IPv4 indicators are stored v4-mapped
 * (::ffff:a.b.c.d, a /n subnet becomes /(96+n)) so both families live in the
 * one 128 bit tree without cross-family bit collisions - patricia compares
 * raw bits and ignores prefix->family, and session addresses are already
 * v4-mapped in6_addrs.
 */
LOCAL void zeekintel_ip_add(patricia_tree_t *tree, const char *indicator, ZeekIntelItem_t *item)
{
    char        mapped[80];
    const char *slash = strchr(indicator, '/');
    size_t      addrLen = slash ? (size_t)(slash - indicator) : strlen(indicator);
    gboolean    isV4 = !memchr(indicator, ':', addrLen);

    if (slash) {
        char *end;
        long  bits = strtol(slash + 1, &end, 10);
        if (end == slash + 1 || *end != 0 || bits < 0 || bits > (isV4 ? 32 : 128)) {
            if (config.debug)
                LOG("Invalid indicator mask %s", indicator);
            zeekintel_item_free(item);
            return;
        }
    }

    if (isV4) {
        if (slash) {
            if (addrLen >= 40) {
                if (config.debug)
                    LOG("Invalid IPv4 indicator %s", indicator);
                zeekintel_item_free(item);
                return;
            }
            snprintf(mapped, sizeof(mapped), "::ffff:%.*s/%ld", (int)addrLen, indicator, 96 + strtol(slash + 1, NULL, 10));
        } else {
            snprintf(mapped, sizeof(mapped), "::ffff:%s", indicator);
        }
        indicator = mapped;
    }

    patricia_node_t *node = make_and_lookup(tree, (char *)indicator);
    if (!node) {
        if (config.debug)
            LOG("Couldn't create node for %s", indicator);
        zeekintel_item_free(item);
        return;
    }
    if (!node->data)
        node->data = g_ptr_array_new_with_free_func(zeekintel_item_free);
    g_ptr_array_add((GPtrArray *)node->data, item);
}
/******************************************************************************/
/*
 * Map a Zeek indicator_type string to its enum.  All supported names share
 * the "Intel::" prefix and have a unique first character after it, so we
 * verify the prefix, switch on that character, and confirm with one strcmp.
 * Returns ZEEK_INTEL_INVALID for types Arkime has no session field to match
 * against (SOFTWARE, USER_NAME, FILE_NAME, ...).
 */
LOCAL ZeekIntelType_t zeekintel_type_lookup(const char *type)
{
    if (strncmp(type, ZEEK_INTEL_PREFIX, ZEEK_INTEL_PREFIX_LEN) != 0)
        return ZEEK_INTEL_INVALID;

    const char *rest = type + ZEEK_INTEL_PREFIX_LEN;

    switch (rest[0]) {
    case 'A':
        if (strcmp(rest, "ADDR")        == 0) return ZEEK_INTEL_ADDR;
        break;
    case 'S':
        if (strcmp(rest, "SUBNET")      == 0) return ZEEK_INTEL_SUBNET;
        break;
    case 'D':
        if (strcmp(rest, "DOMAIN")      == 0) return ZEEK_INTEL_DOMAIN;
        break;
    case 'U':
        if (strcmp(rest, "URL")         == 0) return ZEEK_INTEL_URL;
        break;
    case 'E':
        if (strcmp(rest, "EMAIL")       == 0) return ZEEK_INTEL_EMAIL;
        break;
    case 'F':
        if (strcmp(rest, "FILE_HASH")   == 0) return ZEEK_INTEL_FILE_HASH;
        break;
    case 'C':
        if (strcmp(rest, "CERT_HASH")   == 0) return ZEEK_INTEL_CERT_HASH;
        break;
    case 'P':
        if (strcmp(rest, "PUBKEY_HASH") == 0) return ZEEK_INTEL_PUBKEY_HASH;
        break;
    }
    return ZEEK_INTEL_INVALID;
}
/******************************************************************************/
/*
 * Store a single parsed indicator into the right structure based on its
 * Zeek indicator_type.  Returns TRUE if it was stored.
 */
LOCAL gboolean zeekintel_db_add(ZeekIntelDB_t *db, const char *indicator, const char *type, const char *source, const char *desc, const char *url)
{
    ZeekIntelType_t t = zeekintel_type_lookup(type);
    if (t == ZEEK_INTEL_INVALID) {
        if (config.debug > 1)
            LOG("Skipping unsupported indicator_type %s for %s", type, indicator);
        return FALSE;
    }

    ZeekIntelItem_t *item = ARKIME_TYPE_ALLOC0(ZeekIntelItem_t);
    item->indicator = g_strdup(indicator);
    item->source    = source ? g_strdup(source) : NULL;
    item->desc      = desc   ? g_strdup(desc)   : NULL;
    item->url       = url    ? g_strdup(url)    : NULL;
    item->type      = t;

    switch (t) {
    case ZEEK_INTEL_ADDR:
    case ZEEK_INTEL_SUBNET:
        zeekintel_ip_add(db->ips, indicator, item);
        break;
    case ZEEK_INTEL_URL:
        // URLs are case sensitive, store as-is (matched against http.uri)
        zeekintel_hash_add(&db->urls, indicator, item);
        break;
    case ZEEK_INTEL_DOMAIN: {
        char *key = g_ascii_strdown(indicator, -1);
        zeekintel_hash_add(&db->domains, key, item);
        g_free(key);
        break;
    }
    case ZEEK_INTEL_EMAIL: {
        char *key = g_ascii_strdown(indicator, -1);
        zeekintel_hash_add(&db->emails, key, item);
        g_free(key);
        break;
    }
    case ZEEK_INTEL_FILE_HASH: {
        char *key = g_ascii_strdown(indicator, -1);
        zeekintel_hash_add(&db->fileHashes, key, item);
        g_free(key);
        break;
    }
    case ZEEK_INTEL_CERT_HASH: {
        char *key = g_ascii_strdown(indicator, -1);
        zeekintel_hash_add(&db->certHashes, key, item);
        g_free(key);
        break;
    }
    case ZEEK_INTEL_PUBKEY_HASH: {
        char *key = g_ascii_strdown(indicator, -1);
        zeekintel_hash_add(&db->pubkeyHashes, key, item);
        g_free(key);
        break;
    }
    default:
        zeekintel_item_free(item);
        return FALSE;
    }
    return TRUE;
}
/******************************************************************************/
/*
 * Decode a Zeek #separator escape such as \x09 into a single character.
 */
LOCAL char zeekintel_unescape_sep(const char *s)
{
    if (s[0] == '\\' && (s[1] == 'x' || s[1] == 'X'))
        return (char)strtol(s + 2, NULL, 16);
    if (s[0] == '\\' && s[1] == 't')
        return '\t';
    return s[0];
}
/******************************************************************************/
/*
 * Parse one Zeek intel file into db.  Returns the number of stored indicators.
 */
LOCAL int zeekintel_parse_file(ZeekIntelDB_t *db, const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        LOG("ERROR - Can't open zeekIntelFiles entry '%s' : %s", filename, strerror(errno));
        return 0;
    }

    char     sep      = '\t';
    char    *unset    = g_strdup("-");
    char    *empty    = g_strdup("(empty)");

    int      indicatorCol = 0;
    int      typeCol      = 1;
    int      sourceCol    = 2;
    int      descCol      = 3;
    int      urlCol       = 4;

    char    *line    = NULL;
    size_t   lineCap = 0;
    ssize_t  lineLen;
    int      count   = 0;

    while ((lineLen = getline(&line, &lineCap, fp)) >= 0) {
        while (lineLen > 0 && (line[lineLen - 1] == '\n' || line[lineLen - 1] == '\r'))
            line[--lineLen] = 0;

        if (lineLen == 0)
            continue;

        if (line[0] == '#') {
            if (strncmp(line, "#separator", 10) == 0) {
                const char *v = line + 10;
                while (*v == ' ' || *v == '\t')
                    v++;
                if (*v)
                    sep = zeekintel_unescape_sep(v);
            } else if (strncmp(line, "#fields", 7) == 0) {
                const char sepstr[2] = {sep, 0};
                gchar **cols = g_strsplit(line, sepstr, -1);
                indicatorCol = typeCol = sourceCol = descCol = urlCol = -1;
                for (int c = 1; cols[c]; c++) {
                    if (strcmp(cols[c], "indicator") == 0)
                        indicatorCol = c - 1;
                    else if (strcmp(cols[c], "indicator_type") == 0)
                        typeCol = c - 1;
                    else if (strcmp(cols[c], "meta.source") == 0)
                        sourceCol = c - 1;
                    else if (strcmp(cols[c], "meta.desc") == 0)
                        descCol = c - 1;
                    else if (strcmp(cols[c], "meta.url") == 0)
                        urlCol = c - 1;
                }
                g_strfreev(cols);
            } else if (strncmp(line, "#unset_field", 12) == 0) {
                const char *v = strchr(line, sep);
                if (v) {
                    g_free(unset);
                    unset = g_strdup(v + 1);
                }
            } else if (strncmp(line, "#empty_field", 12) == 0) {
                const char *v = strchr(line, sep);
                if (v) {
                    g_free(empty);
                    empty = g_strdup(v + 1);
                }
            }
            // All other directives (#set_separator, #types, #path, ...) ignored
            continue;
        }

        if (indicatorCol < 0 || typeCol < 0)
            continue;

        const char sepstr[2] = {sep, 0};
        gchar **vals = g_strsplit(line, sepstr, -1);
        guint   nvals = g_strv_length(vals);

        if ((guint)indicatorCol >= nvals || (guint)typeCol >= nvals) {
            g_strfreev(vals);
            continue;
        }

        const char *indicator = vals[indicatorCol];
        const char *type      = vals[typeCol];
        const char *source    = (sourceCol >= 0 && (guint)sourceCol < nvals) ? vals[sourceCol] : NULL;
        const char *desc      = (descCol   >= 0 && (guint)descCol   < nvals) ? vals[descCol]   : NULL;
        const char *url       = (urlCol    >= 0 && (guint)urlCol    < nvals) ? vals[urlCol]    : NULL;

        if (!indicator[0] || !type[0]) {
            g_strfreev(vals);
            continue;
        }

        // A hyphen (or empty) meta field is a null value in the Zeek format.
        if (source && (!source[0] || strcmp(source, unset) == 0 || strcmp(source, empty) == 0))
            source = NULL;
        if (desc && (!desc[0] || strcmp(desc, unset) == 0 || strcmp(desc, empty) == 0))
            desc = NULL;
        if (url && (!url[0] || strcmp(url, unset) == 0 || strcmp(url, empty) == 0))
            url = NULL;

        if (zeekintel_db_add(db, indicator, type, source, desc, url))
            count++;

        g_strfreev(vals);
    }

    free(line);
    g_free(unset);
    g_free(empty);
    fclose(fp);

    if (config.debug)
        LOG("Loaded %d indicators from %s", count, filename);

    return count;
}
/******************************************************************************/
/*
 * Rebuild the intel database from all configured files and atomically swap
 * it in.  The previous database is freed after a grace period so packet
 * threads reading it never see freed memory.
 *
 * Registered as an ArkimeFilesChange_cb so Arkime calls it once at startup
 * and again whenever any of the monitored files change.
 */
LOCAL void zeekintel_load(char **names)
{
    ZeekIntelDB_t *db = zeekintel_db_new();

    int total = 0;
    int numFiles = 0;
    for (int i = 0; names[i]; i++) {
        total += zeekintel_parse_file(db, names[i]);
        numFiles++;
    }

    LOG("Loaded %d Zeek intel indicators from %d file(s)", total, numFiles);

    ZeekIntelDB_t *old = ARKIME_THREAD_ATOMIC_LOAD(currentDB);
    ARKIME_THREAD_ATOMIC_STORE(currentDB, db);
    if (old)
        arkime_free_later(old, (GDestroyNotify)zeekintel_db_free);
}
/******************************************************************************/
/*
 * Serialize one match object into the zeekintel[] array.  The framework writes
 * the array brackets, commas and "zeekintel"/"zeekintelCnt" keys; we just emit
 * one {...}.  Field names mirror the Zeek intel framework (seen.indicator,
 * seen.indicator_type, seen.where, sources) plus meta.desc / meta.url.
 */
LOCAL void zeekintel_object_save(BSB *jbsb, ArkimeFieldObject_t *object, ArkimeSession_t *UNUSED(session))
{
    if (object->object == NULL)
        return;

    const ZeekIntelMatch_t *m = object->object;

    BSB_EXPORT_u08(*jbsb, '{');

    BSB_EXPORT_cstr(*jbsb, "\"indicator\":");
    arkime_db_js0n_str(jbsb, (uint8_t *)m->indicator, TRUE);
    BSB_EXPORT_u08(*jbsb, ',');

    BSB_EXPORT_sprintf(*jbsb, "\"indicator_type\":\"%s\",", zeekIntelTypeNames[m->type]);

    if (m->where)
        BSB_EXPORT_sprintf(*jbsb, "\"where\":\"%s\",", m->where);

    if (m->source) {
        BSB_EXPORT_cstr(*jbsb, "\"source\":");
        arkime_db_js0n_str(jbsb, (uint8_t *)m->source, TRUE);
        BSB_EXPORT_u08(*jbsb, ',');
    }
    if (m->desc) {
        BSB_EXPORT_cstr(*jbsb, "\"desc\":");
        arkime_db_js0n_str(jbsb, (uint8_t *)m->desc, TRUE);
        BSB_EXPORT_u08(*jbsb, ',');
    }
    if (m->url) {
        BSB_EXPORT_cstr(*jbsb, "\"url\":");
        arkime_db_js0n_str(jbsb, (uint8_t *)m->url, TRUE);
        BSB_EXPORT_u08(*jbsb, ',');
    }

    BSB_EXPORT_rewind(*jbsb, 1); // remove trailing comma
    BSB_EXPORT_u08(*jbsb, '}');
}
/******************************************************************************/
LOCAL void zeekintel_object_free(ArkimeFieldObject_t *object)
{
    ZeekIntelMatch_t *m = object->object;
    if (m) {
        g_free(m->indicator);
        g_free(m->source);
        g_free(m->desc);
        g_free(m->url);
        ARKIME_TYPE_FREE(ZeekIntelMatch_t, m);
    }
    ARKIME_TYPE_FREE(ArkimeFieldObject_t, object);
}
/******************************************************************************/
LOCAL uint32_t zeekintel_object_hash(const void *key)
{
    const ZeekIntelMatch_t *m = key;
    uint32_t h = arkime_string_hash(m->indicator) * 31 + (uint32_t)m->type;
    if (m->where)
        h = h * 31 + arkime_string_hash(m->where);
    return h;
}
/******************************************************************************/
/*
 * Two matches are equal (deduped) when the same indicator was seen in the same
 * place from the same source.  Returns non-zero when equal.
 */
LOCAL int zeekintel_object_cmp(const void *keyv, const void *elementv)
{
    const ArkimeFieldObject_t *element = elementv;
    if (element->object == NULL)
        return 0;

    const ZeekIntelMatch_t *a = keyv;
    const ZeekIntelMatch_t *b = element->object;

    if (a->type != b->type || strcmp(a->indicator, b->indicator) != 0)
        return 0;
    if ((a->where == NULL) != (b->where == NULL))
        return 0;
    if (a->where && strcmp(a->where, b->where) != 0)
        return 0;
    if ((a->source == NULL) != (b->source == NULL))
        return 0;
    if (a->source && strcmp(a->source, b->source) != 0)
        return 0;
    return 1;
}
/******************************************************************************/
/*
 * Add the tag and one zeekintel[] match object for every loaded item under a
 * hit.  'where' is the Arkime expression of the session field the indicator
 * matched in (eg ip.dst, host.http, dns.host).
 */
LOCAL void zeekintel_apply(ArkimeSession_t *session, const GPtrArray *items, const char *where)
{
    for (guint i = 0; i < items->len; i++) {
        const ZeekIntelItem_t *item = g_ptr_array_index(items, i);

        arkime_session_add_tag(session, zeekIntelTag);

        ZeekIntelMatch_t *m = ARKIME_TYPE_ALLOC0(ZeekIntelMatch_t);
        m->indicator = g_strdup(item->indicator);
        m->type      = item->type;
        m->where     = where;
        m->source    = item->source ? g_strdup(item->source) : NULL;
        m->desc      = item->desc   ? g_strdup(item->desc)   : NULL;
        m->url       = item->url    ? g_strdup(item->url)    : NULL;

        // indicator/source/desc/url are untrusted feed data run through
        // arkime_db_js0n_str's JSON escaper, which can expand a byte up to
        // 6x (eg a control char becomes \u00XX); double them as a safety
        // margin so jsonSize doesn't undercount and truncate the document.
        int len = 100 + strlen(zeekIntelTypeNames[m->type])
                  + (m->where  ? strlen(m->where)          : 0)
                  + 2 * (strlen(m->indicator)
                         + (m->source ? strlen(m->source) : 0)
                         + (m->desc   ? strlen(m->desc)   : 0)
                         + (m->url    ? strlen(m->url)    : 0));

        ArkimeFieldObject_t *fobject = ARKIME_TYPE_ALLOC0(ArkimeFieldObject_t);
        fobject->object = m;

        if (!arkime_field_object_add(zeekintelField, session, fobject, len))
            zeekintel_object_free(fobject);  // duplicate match
    }
}
/******************************************************************************/
LOCAL void zeekintel_match_ip(ArkimeSession_t *session, ZeekIntelDB_t *db, const struct in6_addr *addr, const char *where)
{
    patricia_node_t *nodes[PATRICIA_MAXBITS + 1];
    prefix_t         prefix;

    // Session addresses are v4-mapped in6_addrs, matching how v4 indicators
    // are stored, so everything is searched as a single 128 bit family.
    prefix.family = AF_INET6;
    prefix.bitlen = 128;
    memcpy(&prefix.add.sin6.s6_addr, addr, 16);

    int cnt = patricia_search_all(db->ips, &prefix, 1, nodes);
    for (int i = 0; i < cnt; i++) {
        if (nodes[i]->data)
            zeekintel_apply(session, (GPtrArray *)nodes[i]->data, where);
    }
}
/******************************************************************************/
/*
 * Match a single string value against one of the string hashes.  Values are
 * lowercased first when 'lower' is set (domains/emails/hashes are stored
 * lowercased; URLs are matched case sensitively).
 */
LOCAL void zeekintel_match_str_value(ArkimeSession_t *session, ZeekIntelStringHash_t *hash, const char *str, gboolean lower, const char *where)
{
    if (!str)
        return;

    ZeekIntelString_t *found;

    if (lower) {
        char *key = g_ascii_strdown(str, -1);
        HASH_FIND(s_, *hash, key, found);
        g_free(key);
    } else {
        HASH_FIND(s_, *hash, str, found);
    }

    if (found)
        zeekintel_apply(session, found->items, where);
}
/******************************************************************************/
/*
 * Match every value of one session string field (any storage type, read via
 * getCb when the field has one - eg dns.host) against a string hash.
 */
LOCAL void zeekintel_match_str_field(ArkimeSession_t *session, ZeekIntelStringHash_t *hash, int pos, gboolean lower)
{
    const ArkimeFieldInfo_t *info = config.fields[pos];
    if (!info)
        return;

    const char *where = info->expression;

    void *getval = NULL;
    if (info->getCb) {
        getval = info->getCb(session, pos);
        if (!getval)
            return;
    } else if (pos >= session->maxFields || !session->fields[pos]) {
        return;
    }

    switch (info->type) {
    case ARKIME_FIELD_TYPE_STR:
        zeekintel_match_str_value(session, hash, info->getCb ? (char *)getval : session->fields[pos]->str, lower, where);
        break;
    case ARKIME_FIELD_TYPE_STR_ARRAY: {
        const GPtrArray *arr = info->getCb ? (GPtrArray *)getval : session->fields[pos]->sarray;
        for (guint i = 0; i < arr->len; i++)
            zeekintel_match_str_value(session, hash, g_ptr_array_index(arr, i), lower, where);
        break;
    }
    case ARKIME_FIELD_TYPE_STR_HASH: {
        if (info->getCb)
            break;  // no getCb STR_HASH fields exist
        const ArkimeStringHashStd_t *shash = session->fields[pos]->shash;
        const ArkimeString_t *hstring;
        HASH_FORALL2(s_, *shash, hstring)
        zeekintel_match_str_value(session, hash, hstring->str, lower, where);
        break;
    }
    case ARKIME_FIELD_TYPE_STR_GHASH: {
        GHashTable    *ghash = info->getCb ? (GHashTable *)getval : session->fields[pos]->ghash;
        GHashTableIter iter;
        gpointer       ikey;
        g_hash_table_iter_init(&iter, ghash);
        while (g_hash_table_iter_next(&iter, &ikey, NULL))
            zeekintel_match_str_value(session, hash, (char *)ikey, lower, where);
        break;
    }
    default:
        break;
    }
}
/******************************************************************************/
/*
 * Match every IP of one session IP field (single ip or ghash of ips) against
 * the intel IP/subnet tree.
 */
LOCAL void zeekintel_match_ip_field(ArkimeSession_t *session, ZeekIntelDB_t *db, int pos)
{
    const ArkimeFieldInfo_t *info = config.fields[pos];
    if (!info || pos >= session->maxFields || !session->fields[pos])
        return;

    const char *where = info->expression;

    if (info->type == ARKIME_FIELD_TYPE_IP) {
        zeekintel_match_ip(session, db, session->fields[pos]->ip, where);
    } else { // ARKIME_FIELD_TYPE_IP_GHASH
        GHashTable    *ghash = session->fields[pos]->ghash;
        GHashTableIter iter;
        gpointer       ikey;
        g_hash_table_iter_init(&iter, ghash);
        while (g_hash_table_iter_next(&iter, &ikey, NULL))
            zeekintel_match_ip(session, db, (struct in6_addr *)ikey, where);
    }
}
/******************************************************************************/
/*
 * Called by arkime when a session is about to be saved.
 */
LOCAL void zeekintel_plugin_save(ArkimeSession_t *session, int UNUSED(final))
{
    ZeekIntelDB_t *db = ARKIME_THREAD_ATOMIC_LOAD(currentDB);
    if (!db)
        return;

    // Primary src/dst IPs (ie ip.src / ip.dst)
    zeekintel_match_ip(session, db, &session->addr1, "ip.src");
    zeekintel_match_ip(session, db, &session->addr2, "ip.dst");

    for (guint i = 0; i < ipFields->len; i++)
        zeekintel_match_ip_field(session, db, g_array_index(ipFields, int, i));

    for (guint i = 0; i < domainFields->len; i++)
        zeekintel_match_str_field(session, &db->domains, g_array_index(domainFields, int, i), TRUE);

    for (guint i = 0; i < urlFields->len; i++)
        zeekintel_match_str_field(session, &db->urls, g_array_index(urlFields, int, i), FALSE);

    for (guint i = 0; i < fileHashFields->len; i++)
        zeekintel_match_str_field(session, &db->fileHashes, g_array_index(fileHashFields, int, i), TRUE);

    for (guint i = 0; i < certHashFields->len; i++)
        zeekintel_match_str_field(session, &db->certHashes, g_array_index(certHashFields, int, i), TRUE);

    for (guint i = 0; i < pubkeyHashFields->len; i++)
        zeekintel_match_str_field(session, &db->pubkeyHashes, g_array_index(pubkeyHashFields, int, i), TRUE);

    for (guint i = 0; i < emailFields->len; i++)
        zeekintel_match_str_field(session, &db->emails, g_array_index(emailFields, int, i), TRUE);
}
/******************************************************************************/
/*
 * Does a field's category string contain 'token' as a whole word?  The
 * category may be a bare string ("host") or a JSON array ("[\"url\",\"host\"]").
 */
LOCAL gboolean zeekintel_category_has(const char *category, const char *token)
{
    if (!category)
        return FALSE;

    const int   tlen = strlen(token);
    const char *p    = category;

    while ((p = strstr(p, token))) {
        const char before = (p == category) ? 0 : p[-1];
        const char after  = p[tlen];
        if (!isalnum((unsigned char)before) && before != '_' &&
            !isalnum((unsigned char)after)  && after  != '_')
            return TRUE;
        p += tlen;
    }
    return FALSE;
}
/******************************************************************************/
LOCAL void zeekintel_field_list_add(GArray *list, int pos)
{
    if (pos < 0 || pos >= ARKIME_FIELDS_MAX)
        return;
    for (guint i = 0; i < list->len; i++)    // dedupe
        if (g_array_index(list, int, i) == pos)
            return;
    g_array_append_val(list, pos);
}
/******************************************************************************/
/*
 * Classify one registered field into the match lists.  IPs are found by type;
 * host/url/hash/email by category.  Real (non-FAKE) fields hold their own data
 * at info->pos; FAKE schema fields (which carry the category) are removed and
 * freed by field.c after registration, so the getCb fields that actually hold
 * their data have no category and are added explicitly below.
 */
LOCAL void zeekintel_classify_field(const ArkimeFieldInfo_t *info)
{
    if (!info)
        return;

    if (ARKIME_FIELD_TYPE_IS_IP(info->type)) {
        // ip.src/ip.dst are getCb fields equal to addr1/addr2 (matched
        // directly); object-backed ip fields have no flat storage.
        if (!info->getCb && !(info->flags & ARKIME_FIELD_FLAG_FAKE))
            zeekintel_field_list_add(ipFields, info->pos);
        return;
    }

    if (!ARKIME_FIELD_TYPE_IS_STR(info->type) || !info->category)
        return;

    if (zeekintel_category_has(info->category, "host"))
        zeekintel_field_list_add(domainFields, info->pos);
    if (zeekintel_category_has(info->category, "url"))
        zeekintel_field_list_add(urlFields, info->pos);
    if (zeekintel_category_has(info->category, "md5") ||
        zeekintel_category_has(info->category, "sha1") ||
        zeekintel_category_has(info->category, "sha256")) {
        // Zeek treats file/content hashes, cert hashes, and cert public-key
        // hashes as distinct indicator types (FILE_HASH/CERT_HASH/PUBKEY_HASH)
        // that must not cross-match; route cert-related fields to their own
        // lists instead of lumping every md5/sha1/sha256 field together.
        if (strncmp(info->expression, "cert.", 5) == 0) {
            if (strstr(info->expression, "pubkey"))
                zeekintel_field_list_add(pubkeyHashFields, info->pos);
            else
                zeekintel_field_list_add(certHashFields, info->pos);
        } else {
            zeekintel_field_list_add(fileHashFields, info->pos);
        }
    }
    if (zeekintel_category_has(info->category, "user"))
        zeekintel_field_list_add(emailFields, info->pos);
}
/******************************************************************************/
/*
 * Add a field to a match list by expression.  Used for fields whose category
 * lived on a FAKE schema field (now freed): the expression resolves to the
 * internal getCb field that actually produces the data.
 */
LOCAL void zeekintel_field_list_add_exp(GArray *list, const char *expression)
{
    int pos = arkime_field_by_exp_ignore_error(expression);
    if (pos >= 0)
        zeekintel_field_list_add(list, pos);
}
/******************************************************************************/
/*
 * Scan the whole field registry once to discover all fields worth matching.
 * Runs from plugin init, after all parsers have registered their fields.
 */
LOCAL void zeekintel_build_fields()
{
    ipFields     = g_array_new(FALSE, FALSE, sizeof(int));
    domainFields = g_array_new(FALSE, FALSE, sizeof(int));
    urlFields    = g_array_new(FALSE, FALSE, sizeof(int));
    fileHashFields   = g_array_new(FALSE, FALSE, sizeof(int));
    certHashFields   = g_array_new(FALSE, FALSE, sizeof(int));
    pubkeyHashFields = g_array_new(FALSE, FALSE, sizeof(int));
    emailFields  = g_array_new(FALSE, FALSE, sizeof(int));

    for (int pos = 0; pos < config.maxDbField; pos++)
        zeekintel_classify_field(config.fields[pos]);
    for (int pos = config.minInternalField; pos < ARKIME_FIELDS_MAX; pos++)
        zeekintel_classify_field(config.fields[pos]);

    /*
     * FAKE schema fields carry the category but are removed/freed by field.c
     * after registration, so their host data (served by internal getCb fields)
     * can't be found by category.  Add the known getCb host fields explicitly;
     * the expression resolves to the getCb field and the add dedupes.
     */
    static const char *getcbHostExprs[] = {
        "dns.host", "host.dns.nameserver", "host.dns.mailserver",
        "dns.query.host", "dns.puny", "cert.alt", NULL
    };
    for (int i = 0; getcbHostExprs[i]; i++)
        zeekintel_field_list_add_exp(domainFields, getcbHostExprs[i]);

    if (config.debug)
        LOG("Matching %u ip, %u host, %u url, %u file hash, %u cert hash, %u pubkey hash, %u email fields",
            ipFields->len, domainFields->len, urlFields->len,
            fileHashFields->len, certHashFields->len, pubkeyHashFields->len, emailFields->len);
}
/******************************************************************************/
/*
 * Called by arkime when arkime is quitting.
 */
LOCAL void zeekintel_plugin_exit()
{
    ZeekIntelDB_t *db = ARKIME_THREAD_ATOMIC_LOAD(currentDB);
    ARKIME_THREAD_ATOMIC_STORE(currentDB, NULL);
    zeekintel_db_free(db);
}
/******************************************************************************/
/*
 * Called by arkime when the plugin is loaded.
 */
void arkime_plugin_init()
{
    zeekIntelFiles = arkime_config_str_list(NULL, "zeekIntelFiles", NULL);
    if (!zeekIntelFiles || !zeekIntelFiles[0])
        CONFIGEXIT("zeekintel plugin enabled but no zeekIntelFiles set");

    zeekIntelTag = arkime_config_str(NULL, "zeekIntelTag", "zeek:intel");

    arkime_plugins_register("zeekintel", FALSE);

    arkime_plugins_set_cb("zeekintel",
                          NULL,
                          NULL,
                          NULL,
                          NULL,
                          zeekintel_plugin_save,
                          NULL,
                          zeekintel_plugin_exit,
                          NULL
                         );

    /*
     * One object field holding an array of match objects, each with its own
     * indicator / indicator_type / where / source / desc / url.  The match is
     * built and serialized at save time by zeekintel_object_save(), so these
     * per-subfield FAKE definitions (mirroring dns.answer.* / cert.*) exist
     * only to give the ES mapping/UI a searchable+clickable field per key;
     * field.c removes/frees FAKE fields after registration, so they carry no
     * storage and are not picked up by zeekintel_classify_field() below.
     */
    zeekintelField = arkime_field_object_register("zeekintel", "Zeek intel matches",
                                                  zeekintel_object_save, zeekintel_object_free,
                                                  zeekintel_object_hash, zeekintel_object_cmp);

    arkime_field_define("zeekintel", "lotermfield",
                        "zeekintel.indicator", "Indicator", "zeekintel.indicator",
                        "Zeek intel indicator that matched",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("zeekintel", "lotermfield",
                        "zeekintel.type", "Indicator Type", "zeekintel.indicator_type",
                        "Zeek intel indicator_type of the match",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("zeekintel", "lotermfield",
                        "zeekintel.where", "Where", "zeekintel.where",
                        "Arkime field expression the indicator matched in",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("zeekintel", "lotermfield",
                        "zeekintel.source", "Source", "zeekintel.source",
                        "Zeek intel meta.source of the match",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("zeekintel", "lotermfield",
                        "zeekintel.desc", "Description", "zeekintel.desc",
                        "Zeek intel meta.desc of the match",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    arkime_field_define("zeekintel", "lotermfield",
                        "zeekintel.url", "URL", "zeekintel.url",
                        "Zeek intel meta.url of the match",
                        0, ARKIME_FIELD_FLAG_FAKE,
                        (char *)NULL);

    /*
     * Discover all the session fields worth matching (ip fields by type;
     * host/url/hash/email fields by category) from the field registry, now
     * that every parser has registered its fields.
     */
    zeekintel_build_fields();

    /*
     * Register the files with Arkime's standard config file monitor.  This
     * loads them right away and reloads them whenever they change.
     */
    arkime_config_monitor_files("zeek intel files", zeekIntelFiles, zeekintel_load);
}
