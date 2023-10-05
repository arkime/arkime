/******************************************************************************/
/* rules.c  -- Functions dealing with rules files
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <stdarg.h>
#include <arpa/inet.h>
#include "yaml.h"
#include "patricia.h"
#include "pcap.h"

/******************************************************************************/
extern ArkimeConfig_t        config;

//#define RULES_DEBUG 1

#ifdef RULES_DEBUG
LOCAL char *yaml_names[] = {
    "YAML_NO_EVENT",
    "YAML_STREAM_START_EVENT",
    "YAML_STREAM_END_EVENT",
    "YAML_DOCUMENT_START_EVENT",
    "YAML_DOCUMENT_END_EVENT",
    "YAML_ALIAS_EVENT",
    "YAML_SCALAR_EVENT",
    "YAML_SEQUENCE_START_EVENT",
    "YAML_SEQUENCE_END_EVENT",
    "YAML_MAPPING_START_EVENT",
    "YAML_MAPPING_END_EVENT"
};
#endif


#define YAML_NODE_SEQUENCE_VALUE (char *)1
typedef struct {
    char      *key;
    char      *value;
    GPtrArray *values;
} YamlNode_t;

#define ARKIME_SAVE_FLAG_MIDDLE 0x01
#define ARKIME_SAVE_FLAG_FINAL  0x02
#define ARKIME_SAVE_FLAG_BOTH   0x03

typedef struct {
    char                *filename;
    char                *name;
    char                *bpf;                      // String version of bpf
    struct bpf_program   bpfp;
    GHashTable          *hash[ARKIME_FIELDS_MAX];  // For each non ip field in rule
    GPtrArray           *match[ARKIME_FIELDS_MAX]; // For any string fields with , modifier or int fields range
    patricia_tree_t     *tree4[ARKIME_FIELDS_MAX];
    patricia_tree_t     *tree6[ARKIME_FIELDS_MAX];
    ArkimeFieldOps_t     ops;                      // Ops to run on match
    uint64_t             matched;                  // How many times was matched
    uint16_t            *fields;                   // fieldsLen length array of field pos
    uint16_t             fieldsLen;
    uint8_t              saveFlags;                // When to save for beforeSave
    uint8_t              log;                      // should we log or not
} ArkimeRule_t;

#define ARKIME_RULES_MAX     100

/* All the information about the rules.  To support reloading while running
 * there can be multiple info variables.
 * current - has the ones that the packetThreads are using
 * loading - has the ones that the main thread is loading into
 *
 * The fields* elements are a mapping from ALL possible values to rules with those values.
 * Used by the fieldset rule type.  This allows us on field setting to find just the rules
 * that we need to eval.
 */
typedef struct {
    GHashTable            *fieldsHash[ARKIME_FIELDS_MAX];
    patricia_tree_t       *fieldsTree4[ARKIME_FIELDS_MAX];
    patricia_tree_t       *fieldsTree6[ARKIME_FIELDS_MAX];
    GHashTable            *fieldsMatch[ARKIME_FIELDS_MAX];

    int                    rulesLen[ARKIME_RULE_TYPE_NUM];
    ArkimeRule_t          *rules[ARKIME_RULE_TYPE_NUM][ARKIME_RULES_MAX + 1];
} ArkimeRulesInfo_t;

LOCAL ArkimeRulesInfo_t    current;
LOCAL ArkimeRulesInfo_t    loading;
LOCAL char               **rulesFiles;

LOCAL pcap_t              *deadPcap;
extern ArkimePcapFileHdr_t pcapFileHeader;

#define ARKIME_RULES_STR_MATCH_HEAD      1
#define ARKIME_RULES_STR_MATCH_TAIL      2
#define ARKIME_RULES_STR_MATCH_CONTAINS  3

typedef union {
    struct {
      uint32_t      min;
      uint32_t      max;
    };
    uint64_t        num;
} ArkimeRuleIntMatch_t;

LOCAL void arkime_rules_load_add_field_range_match(ArkimeRule_t *rule, int pos, char *key);
/******************************************************************************/
LOCAL void arkime_rules_free_array(gpointer data)
{
    g_ptr_array_free(data, TRUE);
}
/******************************************************************************/
LOCAL void arkime_rules_parser_free_node(YamlNode_t *node)
{
    if (node->key)
        g_free(node->key);
    if (node->value > YAML_NODE_SEQUENCE_VALUE)
        g_free(node->value);
    if (node->values)
        g_ptr_array_free(node->values, TRUE);
    ARKIME_TYPE_FREE(YamlNode_t, node);
}
/******************************************************************************/
LOCAL YamlNode_t *arkime_rules_parser_add_node(YamlNode_t *parent, char *key, char *value)
{
    YamlNode_t *node = ARKIME_TYPE_ALLOC(YamlNode_t);
    node->key = key;
    node->value = value;

    if (value) {
        node->values = NULL;
    } else {
        node->values = g_ptr_array_new_with_free_func((GDestroyNotify)arkime_rules_parser_free_node);
    }

    if (parent) {
        if (!key) {
            char str[10];
            snprintf(str, sizeof(str), "%u", parent->values->len);
            node->key = g_strdup(str);
        }
        g_ptr_array_add(parent->values, node);
    }

    return node;
}
/******************************************************************************/
LOCAL YamlNode_t *arkime_rules_parser_parse_yaml(char *filename, YamlNode_t *parent, yaml_parser_t *parser, gboolean sequence) {

    char *key = NULL;
    YamlNode_t *node;

    int done = 0;
    while (!done) {
        yaml_event_t event;

        if (!yaml_parser_parse(parser, &event))
            CONFIGEXIT("%s:%zu - Parse error '%s'", filename, parser->problem_mark.line, parser->problem);

#ifdef RULES_DEBUG
        LOG("%s %d", yaml_names[event.type], event.type);
#endif

        switch(event.type) {
        case YAML_NO_EVENT:
            done = 1;
            break;
        case YAML_SCALAR_EVENT:
#ifdef RULES_DEBUG
            LOG("SCALAR_EVENT: %s => %s", key, event.data.scalar.value);
#endif

            if (sequence) {
                arkime_rules_parser_add_node(parent, g_strdup((gchar *)event.data.scalar.value), YAML_NODE_SEQUENCE_VALUE);
            } else if (key) {
                arkime_rules_parser_add_node(parent, key, g_strdup((gchar *)event.data.scalar.value));
                key = NULL;
            } else {
                key = g_strdup((gchar *)event.data.scalar.value);
            }
            break;
        case YAML_SEQUENCE_START_EVENT:
        case YAML_MAPPING_START_EVENT:
            if (parent == NULL) {
                parent = node = arkime_rules_parser_add_node(NULL, g_strdup("root"), NULL);
            } else {
                node = arkime_rules_parser_add_node(parent, key, NULL);
            }
            key = NULL;
            if (arkime_rules_parser_parse_yaml(filename, node, parser, event.type == YAML_SEQUENCE_START_EVENT) == NULL)
                return NULL;

            break;
        case YAML_MAPPING_END_EVENT:
        case YAML_SEQUENCE_END_EVENT:
            done = 1;
        default: ;
        }
        yaml_event_delete(&event);
    }

    return parent;
}
/******************************************************************************/
LOCAL void arkime_rules_parser_print(YamlNode_t *node, int level)
{
    static char indent[] = "                                                             ";
    if (node->value == YAML_NODE_SEQUENCE_VALUE) {
        printf("%.*s - %s\n", level, indent, node->key);
    } else if (node->value) {
        printf("%.*s %s: %s\n", level, indent, node->key, node->value);
    } else {
        printf("%.*s %s:\n", level, indent, node->key);
        int i;
        for (i = 0; i < (int)node->values->len; i++)
            arkime_rules_parser_print(g_ptr_array_index(node->values, i), level + 1);
    }
}
/******************************************************************************/
LOCAL YamlNode_t *arkime_rules_parser_get(YamlNode_t *node, char *path)
{

    while (1) {
        char *colon = strchr(path, ':');
        int   len;

        if (colon)
            len = colon - path;
        else
            len = strlen(path);

        if (!node->values)
            return NULL;

        int i;
        for (i = 0; i < (int)node->values->len; i++) {
            if (strncmp(path, ((YamlNode_t*)g_ptr_array_index(node->values, i))->key, len) == 0)
                break;
        }
        if (i == (int)node->values->len)
            return NULL;
        node = g_ptr_array_index(node->values, i);

        if (!colon)
            return node;
        path += len + 1;
    }
}
/******************************************************************************/
LOCAL char *arkime_rules_parser_get_value(YamlNode_t *parent, char *path)
{
    YamlNode_t *node = arkime_rules_parser_get(parent, path);
    if (!node)
        return NULL;
    return node->value;
}
/******************************************************************************/
LOCAL GPtrArray *arkime_rules_parser_get_values(YamlNode_t *parent, char *path)
{
    YamlNode_t *node = arkime_rules_parser_get(parent, path);
    if (!node)
        return NULL;
    return node->values;
}
/******************************************************************************/
LOCAL void arkime_rules_load_add_field(ArkimeRule_t *rule, int pos, char *key)
{
    uint32_t         n;
    float            f;
    uint32_t         fint;
    GPtrArray       *rules;
    patricia_node_t *node;

    config.fields[pos]->ruleEnabled = 1;

    switch (config.fields[pos]->type) {
    case ARKIME_FIELD_TYPE_INT:
    case ARKIME_FIELD_TYPE_INT_ARRAY:
    case ARKIME_FIELD_TYPE_INT_HASH:
    case ARKIME_FIELD_TYPE_INT_GHASH:
        if (key[0] != '-' && strchr(key, '-') != 0) {
            arkime_rules_load_add_field_range_match(rule, pos, key);
            return;
        }

        if (!loading.fieldsHash[pos])
            loading.fieldsHash[pos] = g_hash_table_new_full(NULL, NULL, NULL, arkime_rules_free_array);

        n = atoi(key);
        g_hash_table_add(rule->hash[pos], (void *)(long)n);

        rules = g_hash_table_lookup(loading.fieldsHash[pos], (void *)(long)n);
        if (!rules) {
            rules = g_ptr_array_new();
            g_hash_table_insert(loading.fieldsHash[pos], (void *)(long)n, rules);
        }
        g_ptr_array_add(rules, rule);
        break;

    case ARKIME_FIELD_TYPE_FLOAT:
    case ARKIME_FIELD_TYPE_FLOAT_ARRAY:
    case ARKIME_FIELD_TYPE_FLOAT_GHASH:
        if (!loading.fieldsHash[pos])
            loading.fieldsHash[pos] = g_hash_table_new_full(NULL, NULL, NULL, arkime_rules_free_array);

        f = atof(key);
        memcpy(&fint, &f, 4);
        g_hash_table_add(rule->hash[pos], (gpointer)(long)fint);

        rules = g_hash_table_lookup(loading.fieldsHash[pos], (gpointer)(long)fint);
        if (!rules) {
            rules = g_ptr_array_new();
            g_hash_table_insert(loading.fieldsHash[pos], (gpointer)(long)fint, rules);
        }
        g_ptr_array_add(rules, rule);
        break;

    case ARKIME_FIELD_TYPE_IP:
    case ARKIME_FIELD_TYPE_IP_GHASH:
        if (!loading.fieldsTree4[pos]) {
            loading.fieldsTree4[pos] = New_Patricia(32);
            loading.fieldsTree6[pos] = New_Patricia(128);
        }

        if (strcmp(key, "ipv4") == 0) {
            make_and_lookup(rule->tree4[pos], "0.0.0.0/0");
            node = make_and_lookup(loading.fieldsTree4[pos], "0.0.0.0/0");
        } else if (strcmp(key, "ipv6") == 0) {
            make_and_lookup(rule->tree6[pos], "::/0");
            node = make_and_lookup(loading.fieldsTree6[pos], "::/0");
        } else if (strchr(key, '.') != 0) {
            make_and_lookup(rule->tree4[pos], key);
            node = make_and_lookup(loading.fieldsTree4[pos], key);
        } else {
            make_and_lookup(rule->tree6[pos], key);
            node = make_and_lookup(loading.fieldsTree6[pos], key);
        }
        if (node->data) {
            rules = node->data;
        } else {
            node->data = rules = g_ptr_array_new();
        }
        g_ptr_array_add(rules, rule);
        break;


    case ARKIME_FIELD_TYPE_STR:
    case ARKIME_FIELD_TYPE_STR_ARRAY:
    case ARKIME_FIELD_TYPE_STR_HASH:
    case ARKIME_FIELD_TYPE_STR_GHASH:
        if (!loading.fieldsHash[pos])
            loading.fieldsHash[pos] = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, arkime_rules_free_array);

        g_hash_table_add(rule->hash[pos], g_strdup(key));

        rules = g_hash_table_lookup(loading.fieldsHash[pos], key);
        if (!rules) {
            rules = g_ptr_array_new();
            g_hash_table_insert(loading.fieldsHash[pos], g_strdup(key), rules);
        }
        g_ptr_array_add(rules, rule);
        break;
    case ARKIME_FIELD_TYPE_CERTSINFO:
        // Unsupported
        break;
    }
}
/******************************************************************************/
LOCAL void arkime_rules_load_add_field_range_match(ArkimeRule_t *rule, int pos, char *key)
{
    char *dash = strchr(key, '-');
    *dash = 0;

    uint32_t min = atoi(key);
    uint32_t max = atoi(dash + 1);
    if (min > max)
        CONFIGEXIT("Min %u > Max %u not allowed", min, max);

    // If the range is small convert back to non range.
    if (max - min < 20) {
        char str[30];
        for (;min <= max; min++) {
            snprintf(str, sizeof(str), "%u", min);
            arkime_rules_load_add_field(rule, pos, str);
        }
        return;
    }

    ArkimeRuleIntMatch_t match;
    match.min = min;
    match.max = max;

    if (!rule->match[pos])
        rule->match[pos] = g_ptr_array_new();

    g_ptr_array_add(rule->match[pos], (gpointer)match.num);

    if (!loading.fieldsMatch[pos])
        loading.fieldsMatch[pos] = g_hash_table_new_full(g_direct_hash, g_direct_equal, g_free, arkime_rules_free_array);

    GPtrArray *rules = g_hash_table_lookup(loading.fieldsMatch[pos], (gpointer)match.num);
    if (!rules) {
        rules = g_ptr_array_new();
        g_hash_table_insert(loading.fieldsMatch[pos], (gpointer)match.num, rules);
    }
    g_ptr_array_add(rules, rule);
}
/******************************************************************************/
LOCAL void arkime_rules_load_add_field_match(ArkimeRule_t *rule, int pos, int type, char *key)
{
    int len = strlen(key);

    config.fields[pos]->ruleEnabled = 1;

    if (len > 255)
        CONFIGEXIT("Match %s is to too large", key);

    uint8_t *nkey = g_malloc(len + 3);
    nkey[0] = type;
    nkey[1] = len;
    memcpy(nkey + 2, key, len + 1);

    g_ptr_array_add(rule->match[pos], (char *)nkey); // Just made a copy above

    if (!loading.fieldsMatch[pos])
        loading.fieldsMatch[pos] = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, arkime_rules_free_array);

    GPtrArray *rules = g_hash_table_lookup(loading.fieldsMatch[pos], nkey);
    if (!rules) {
        rules = g_ptr_array_new();
        g_hash_table_insert(loading.fieldsMatch[pos], g_strdup((char *)nkey), rules);
    }
    g_ptr_array_add(rules, rule);
}
/******************************************************************************/
LOCAL void arkime_rules_parser_load_rule(char *filename, YamlNode_t *parent)
{
    char *name = arkime_rules_parser_get_value(parent, "name");
    if (!name)
        CONFIGEXIT("%s: name required for rule", filename);

    char *when = arkime_rules_parser_get_value(parent, "when");
    if (!when)
        CONFIGEXIT("%s: when required for rule '%s'", filename, name);

    char *bpf = arkime_rules_parser_get_value(parent, "bpf");
    GPtrArray *fields = arkime_rules_parser_get_values(parent, "fields");
    char *expression = arkime_rules_parser_get_value(parent, "expression");

    if (!bpf && !fields && !expression)
        CONFIGEXIT("%s: bpf, fields, or expressions required for rule '%s'", filename, name);

    if ((bpf && fields) || (bpf && expression) || (fields && expression))
        CONFIGEXIT("%s: Only one of bpf, fields, or expressions can be set for rule '%s'", filename, name);

    GPtrArray  *ops = arkime_rules_parser_get_values(parent, "ops");
    if (!ops)
        CONFIGEXIT("%s: ops required for rule '%s'", filename, name);

    if (expression) {
        CONFIGEXIT("Currently don't support expression, hopefully soon!");
    }

    char *log = arkime_rules_parser_get_value(parent, "log");

    int type;
    int saveFlags = 0;
    if (strcmp(when, "everyPacket") == 0) {
        type = ARKIME_RULE_TYPE_EVERY_PACKET;
        if (!bpf)
            CONFIGEXIT("%s: everyPacket only supports bpf", filename);
    } else if (strcmp(when, "sessionSetup") == 0) {
        type = ARKIME_RULE_TYPE_SESSION_SETUP;
    } else if (strcmp(when, "afterClassify") == 0) {
        type = ARKIME_RULE_TYPE_AFTER_CLASSIFY;
        if (bpf)
            CONFIGEXIT("%s: %s doesn't support bpf", filename, when);
    } else if (strcmp(when, "fieldSet") == 0) {
        type = ARKIME_RULE_TYPE_FIELD_SET;
        if (bpf)
            CONFIGEXIT("%s: %s doesn't support bpf", filename, when);
    } else if (strcmp(when, "beforeMiddleSave") == 0) {
        type = ARKIME_RULE_TYPE_BEFORE_SAVE;
        saveFlags = ARKIME_SAVE_FLAG_MIDDLE;
        if (bpf)
            CONFIGEXIT("%s: %s doesn't support bpf", filename, when);
    } else if (strcmp(when, "beforeFinalSave") == 0) {
        type = ARKIME_RULE_TYPE_BEFORE_SAVE;
        saveFlags = ARKIME_SAVE_FLAG_FINAL;
        if (bpf)
            CONFIGEXIT("%s: %s doesn't support bpf", filename, when);
    } else if (strcmp(when, "beforeBothSave") == 0) {
        type = ARKIME_RULE_TYPE_BEFORE_SAVE;
        saveFlags = ARKIME_SAVE_FLAG_BOTH;
        if (bpf)
            CONFIGEXIT("%s: %s doesn't support bpf", filename, when);
    } else {
        CONFIGEXIT("%s: Unknown when '%s'", filename, when);
    }

    if (loading.rulesLen[type] >= ARKIME_RULES_MAX)
        CONFIGEXIT("Too many %s rules", when);

    int n = loading.rulesLen[type]++;
    ArkimeRule_t *rule = loading.rules[type][n] = ARKIME_TYPE_ALLOC0(ArkimeRule_t);
    rule->name = g_strdup(name);
    rule->filename = filename;
    rule->saveFlags = saveFlags;
    rule->log = log && strcasecmp(log, "true") == 0;
    if (bpf)
        rule->bpf = g_strdup(bpf);

    if (fields) {
        int i;
        int mtype = 0;
        rule->fields = malloc((int)fields->len * 2);
        for (i = 0; i < (int)fields->len; i++) {
            YamlNode_t *node = g_ptr_array_index(fields, i);

            char *comma = strchr(node->key, ',');
            if (comma) {
                *comma = 0;
                comma++;
                if (strcmp(comma, "tail") == 0 || strcmp(comma, "endsWith") == 0) {
                    mtype = ARKIME_RULES_STR_MATCH_TAIL;
                } else if (strcmp(comma, "head") == 0 || strcmp(comma, "startsWith") == 0) {
                    mtype = ARKIME_RULES_STR_MATCH_HEAD;
                } else if (strcmp(comma, "contains") == 0) {
                    mtype = ARKIME_RULES_STR_MATCH_CONTAINS;
                } else {
                    CONFIGEXIT("Rule field %s doesn't support modifier %s", node->key, comma);
                }
            }

            int pos = arkime_field_by_exp(node->key);
            if (pos == -1)
                CONFIGEXIT("%s Couldn't find field '%s'", filename, node->key);

            // Add this fieldPos to the list if not already there
            if (!(rule->hash[pos] || rule->tree4[pos] || rule->match[pos]))
                rule->fields[(int)rule->fieldsLen++] = pos;

            switch (config.fields[pos]->type) {
            case ARKIME_FIELD_TYPE_INT:
            case ARKIME_FIELD_TYPE_INT_ARRAY:
            case ARKIME_FIELD_TYPE_INT_HASH:
            case ARKIME_FIELD_TYPE_INT_GHASH:
                if (mtype != 0)
                    CONFIGEXIT("Rule field %s doesn't support modifier %s", node->key, comma);

                if (!rule->hash[pos])
                    rule->hash[pos] = g_hash_table_new_full(NULL, NULL, NULL, NULL);
                break;

            case ARKIME_FIELD_TYPE_FLOAT:
            case ARKIME_FIELD_TYPE_FLOAT_ARRAY:
            case ARKIME_FIELD_TYPE_FLOAT_GHASH:
                if (mtype != 0)
                    CONFIGEXIT("Rule field %s doesn't support modifier %s", node->key, comma);

                if (!rule->hash[pos])
                    rule->hash[pos] = g_hash_table_new_full(NULL, NULL, NULL, NULL);
                break;

            case ARKIME_FIELD_TYPE_IP:
            case ARKIME_FIELD_TYPE_IP_GHASH:
                if (mtype != 0)
                    CONFIGEXIT("Rule field %s doesn't support modifier %s", node->key, comma);

                if (!rule->tree4[pos]) {
                    rule->tree4[pos] = New_Patricia(32);
                    rule->tree6[pos] = New_Patricia(128);
                }
                break;

            case ARKIME_FIELD_TYPE_STR:
            case ARKIME_FIELD_TYPE_STR_ARRAY:
            case ARKIME_FIELD_TYPE_STR_HASH:
            case ARKIME_FIELD_TYPE_STR_GHASH:
                if (mtype != 0) {
                    if (!rule->match[pos])
                        rule->match[pos] = g_ptr_array_new_with_free_func(g_free);
                } else {
                    if (!rule->hash[pos])
                        rule->hash[pos] = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
                }
                break;

            case ARKIME_FIELD_TYPE_CERTSINFO:
                CONFIGEXIT("%s: Currently don't support any certs fields", filename);
            }

            if (node->value) {
                if (mtype != 0)
                    arkime_rules_load_add_field_match(rule, pos, mtype, node->value);
                else
                    arkime_rules_load_add_field(rule, pos, node->value);
            } else {
                int j;
                for (j = 0; j < (int)node->values->len; j++) {
                    YamlNode_t *fnode = g_ptr_array_index(node->values, j);
                    if (mtype != 0)
                        arkime_rules_load_add_field_match(rule, pos, mtype, fnode->key);
                    else
                        arkime_rules_load_add_field(rule, pos, fnode->key);
                }
            }
        }
    }

    arkime_field_ops_init(&rule->ops, ops->len, ARKIME_FIELD_OPS_FLAGS_COPY);
    int i;
    for (i = 0; i < (int)ops->len; i++) {
        YamlNode_t *node = g_ptr_array_index(ops, i);
        int pos = arkime_field_by_exp(node->key);
        if (pos == -1)
            CONFIGEXIT("%s Couldn't find field '%s'", filename, node->key);
        arkime_field_ops_add(&rule->ops, pos, node->value, strlen(node->value));
    }
}
/******************************************************************************/
LOCAL void arkime_rules_parser_load_file(char *filename, YamlNode_t *parent)
{
    char       *str;
    GPtrArray  *rules;

    str = arkime_rules_parser_get_value(parent, "version");
    if (!str || strcmp(str, "1") != 0) {
        CONFIGEXIT("%s: Missing version: 1", filename);
    }

    rules = arkime_rules_parser_get_values(parent, "rules");
    if (!rules) {
        CONFIGEXIT("%s: Missing rules", filename);
    }

    int i;
    for (i = 0; i < (int)rules->len; i++) {
        arkime_rules_parser_load_rule(filename, g_ptr_array_index(rules, i));
    }
}
/******************************************************************************/
LOCAL void arkime_rules_load_complete()
{
    char      **bpfs;
    GRegex     *regex = g_regex_new(":\\s*(\\d+)\\s*$", 0, 0, 0);
    int         i;

    bpfs = arkime_config_str_list(NULL, "dontSaveBPFs", NULL);
    int pos = arkime_field_by_exp("_maxPacketsToSave");
    gint start_pos;
    if (bpfs) {
        for (i = 0; bpfs[i]; i++) {
            int n = loading.rulesLen[ARKIME_RULE_TYPE_SESSION_SETUP]++;
            ArkimeRule_t *rule = loading.rules[ARKIME_RULE_TYPE_SESSION_SETUP][n] = ARKIME_TYPE_ALLOC0(ArkimeRule_t);
            rule->filename = "dontSaveBPFs";
            arkime_field_ops_init(&rule->ops, 1, ARKIME_FIELD_OPS_FLAGS_COPY);

            GMatchInfo *match_info = 0;
            g_regex_match(regex, bpfs[i], 0, &match_info);
            if (g_match_info_matches(match_info)) {
                g_match_info_fetch_pos (match_info, 1, &start_pos, NULL);
                rule->bpf = g_strndup(bpfs[i], start_pos - 1);
                arkime_field_ops_add(&rule->ops, pos, g_match_info_fetch(match_info, 1), -1);
            } else {
                rule->bpf = g_strdup(bpfs[i]);
                arkime_field_ops_add(&rule->ops, pos, "1", -1);
            }
            g_match_info_free(match_info);
        }
        g_strfreev(bpfs);
    }

    bpfs = arkime_config_str_list(NULL, "minPacketsSaveBPFs", NULL);
    pos = arkime_field_by_exp("_minPacketsBeforeSavingSPI");
    if (bpfs) {
        for (i = 0; bpfs[i]; i++) {
            int n = loading.rulesLen[ARKIME_RULE_TYPE_SESSION_SETUP]++;
            ArkimeRule_t *rule = loading.rules[ARKIME_RULE_TYPE_SESSION_SETUP][n] = ARKIME_TYPE_ALLOC0(ArkimeRule_t);
            rule->filename = "minPacketsSaveBPFs";
            arkime_field_ops_init(&rule->ops, 1, ARKIME_FIELD_OPS_FLAGS_COPY);

            GMatchInfo *match_info = 0;
            g_regex_match(regex, bpfs[i], 0, &match_info);
            if (g_match_info_matches(match_info)) {
                g_match_info_fetch_pos (match_info, 1, &start_pos, NULL);
                rule->bpf = g_strndup(bpfs[i], start_pos - 1);
                arkime_field_ops_add(&rule->ops, pos, g_match_info_fetch(match_info, 1), -1);
            } else {
                rule->bpf = g_strdup(bpfs[i]);
                arkime_field_ops_add(&rule->ops, pos, "1", -1);
            }
            g_match_info_free(match_info);
        }
        g_strfreev(bpfs);
    }
    g_regex_unref(regex);

    memcpy(&current, &loading, sizeof(loading));
    memset(&loading, 0, sizeof(loading));
}
/******************************************************************************/
LOCAL void arkime_rules_free(ArkimeRulesInfo_t *freeing)
{
    int    i, t, r;

    for (i = 0; i < ARKIME_FIELDS_MAX; i++) {
        if (freeing->fieldsHash[i]) {
            g_hash_table_destroy(freeing->fieldsHash[i]);
        }
        if (freeing->fieldsTree4[i]) {
            Destroy_Patricia(freeing->fieldsTree4[i], arkime_rules_free_array);
        }
        if (freeing->fieldsTree6[i]) {
            Destroy_Patricia(freeing->fieldsTree6[i], arkime_rules_free_array);
        }
        if (freeing->fieldsMatch[i]) {
            g_hash_table_destroy(freeing->fieldsMatch[i]);
        }
    }

    for (t = 0; t < ARKIME_RULE_TYPE_NUM; t++) {
        for (r = 0; r < freeing->rulesLen[t]; r++) {
            ArkimeRule_t *rule = freeing->rules[t][r];

            g_free(rule->name);
            if (rule->bpf)
                g_free(rule->bpf);

            for (i = 0; i < ARKIME_FIELDS_MAX; i++) {
                if (rule->hash[i]) {
                    g_hash_table_destroy(rule->hash[i]);
                }
                if (rule->tree4[i]) {
                    Destroy_Patricia(rule->tree4[i], arkime_rules_free_array);
                }
                if (rule->tree6[i]) {
                    Destroy_Patricia(rule->tree6[i], arkime_rules_free_array);
                }
                if (rule->match[i]) {
                    g_ptr_array_free(rule->match[i], TRUE);
                }
            }

            arkime_field_ops_free(&rule->ops);
            ARKIME_TYPE_FREE(ArkimeRule_t, rule);
        }
    }

    ARKIME_TYPE_FREE(ArkimeRulesInfo_t, freeing);
}
/******************************************************************************/
void arkime_rules_load(char **names)
{
    int    i;

    // Make a copy of current items to free later

    ArkimeRulesInfo_t *freeing = ARKIME_TYPE_ALLOC0(ArkimeRulesInfo_t);
    memcpy(freeing, &current, sizeof(current));

    // Load all the rule files
    for (i = 0; names[i]; i++) {
        yaml_parser_t parser;
        yaml_parser_initialize(&parser);
        FILE *input = fopen(names[i], "rb");
        if (!input)
            CONFIGEXIT("can not open rules file %s", names[i]);

        yaml_parser_set_input_file(&parser, input);
        YamlNode_t *parent = arkime_rules_parser_parse_yaml(names[i], NULL, &parser, FALSE);
        yaml_parser_delete(&parser);
        if (!parent) {
            LOG("WARNING %s - has no rules", names[i]);
            continue;
        }
        if (config.debug > 1) {
            arkime_rules_parser_print(parent, 0);
        }
        arkime_rules_parser_load_file(names[i], parent);
        arkime_rules_parser_free_node(parent);
        fclose(input);
    }

    // Part 2, which will also copy loading to current
    arkime_rules_load_complete();

    // Now schedule free of current items
    arkime_free_later(freeing, (GDestroyNotify) arkime_rules_free);
}
/******************************************************************************/
/* Called at the start on main thread or each time a new file is open on single thread */
void arkime_rules_recompile()
{
    int t, r;

    if (deadPcap)
        pcap_close(deadPcap);

    deadPcap = pcap_open_dead(pcapFileHeader.dlt, pcapFileHeader.snaplen);
    ArkimeRule_t *rule;
    for (t = 0; t < ARKIME_RULE_TYPE_NUM; t++) {
        for (r = 0; (rule = current.rules[t][r]); r++) {
            if (!rule->bpf)
                continue;

            pcap_freecode(&rule->bpfp);
            if (pcapFileHeader.dlt != DLT_NFLOG) {
                if (pcap_compile(deadPcap, &rule->bpfp, rule->bpf, 1, PCAP_NETMASK_UNKNOWN) == -1 && !config.ignoreErrors) {
                    CONFIGEXIT("Couldn't compile bpf filter %s: '%s' with %s", rule->filename, rule->bpf, pcap_geterr(deadPcap));
                }
            } else {
                rule->bpfp.bf_len = 0;
            }
        }
    }
}
/******************************************************************************/
LOCAL gboolean arkime_rules_check_ip(const ArkimeRule_t * const rule, const int p, const struct in6_addr *ip, BSB *logStr)
{
    if (IN6_IS_ADDR_V4MAPPED(ip)) {
        patricia_node_t *node = patricia_search_best3 (rule->tree4[p], ((u_char *)ip->s6_addr) + 12, 32);
        if (!node)
            return FALSE;
        if (!logStr)
            return TRUE;
        BSB_EXPORT_sprintf(*logStr, "%s: %u.%u.%u.%u/%u, ",
                config.fields[p]->expression,
                node->prefix->add.sin.s_addr & 0xff,
                (node->prefix->add.sin.s_addr >> 8) & 0xff,
                (node->prefix->add.sin.s_addr >> 16) & 0xff,
                (node->prefix->add.sin.s_addr >> 24) & 0xff,
                node->prefix->bitlen);
    } else {
        patricia_node_t *node = patricia_search_best3 (rule->tree6[p], (u_char *)ip->s6_addr, 128);
        if (!node)
            return FALSE;
        if (!logStr)
            return TRUE;

        BSB_EXPORT_sprintf(*logStr, "%s: ", config.fields[p]->expression);
        BSB_EXPORT_inet_ntop(*logStr, AF_INET6, &node->prefix->add.sin6);
        BSB_EXPORT_sprintf(*logStr, "/%u, ", node->prefix->bitlen);
    }
    return TRUE;
}
/******************************************************************************/
LOCAL gboolean arkime_rules_check_str_match(const ArkimeRule_t * const rule, int p, const char * const key, BSB *logStr)
{

    if (rule->hash[p] && g_hash_table_contains(rule->hash[p], key)) {
        if (logStr) {
            BSB_EXPORT_sprintf(*logStr, "%s: %s, ", config.fields[p]->expression, key);
        }
        return TRUE;
    }

    const GPtrArray * const matches = rule->match[p];

    if (!matches)
        return FALSE;

    guint len = strlen(key);

    for (guint i = 0; i < matches->len; i++) {
        uint8_t *akey = g_ptr_array_index(matches, i);
        if (len < akey[1])
            continue;

        switch (akey[0]) {
        case ARKIME_RULES_STR_MATCH_TAIL:
            if (memcmp(akey + 2, key + len - akey[1], akey[1]) == 0) {
                if (logStr) {
                    BSB_EXPORT_sprintf(*logStr, "%s,tail: %s, ", config.fields[p]->expression, akey + 2);
                }
                return TRUE;
            }
            break;
        case ARKIME_RULES_STR_MATCH_HEAD:
            if (memcmp(akey + 2, key, akey[1]) == 0) {
                if (logStr) {
                    BSB_EXPORT_sprintf(*logStr, "%s,head: %s, ", config.fields[p]->expression, akey + 2);
                }
                return TRUE;
            }
            break;
        case ARKIME_RULES_STR_MATCH_CONTAINS:
            if (arkime_memstr(key, len, (char*)akey + 2, akey[1]) != 0) {
                if (logStr) {
                    BSB_EXPORT_sprintf(*logStr, "%s,contains: %s, ", config.fields[p]->expression, akey + 2);
                }
                return TRUE;
            }
            break;
        }
    }

    return FALSE;
}
LOCAL void arkime_rules_check_rule_fields(ArkimeSession_t * const session, ArkimeRule_t * const rule, int skipPos, BSB *logStr);
/******************************************************************************/
LOCAL void arkime_rules_match(ArkimeSession_t * const session, ArkimeRule_t * const rule)
{
    if (rule->log) {
        char ipStr[200];
        char logStr[5000];
        BSB bsb;

        arkime_session_pretty_string(session, ipStr, sizeof(ipStr));

        BSB_INIT(bsb, logStr, sizeof(logStr));

        arkime_rules_check_rule_fields(session, rule, -1, &bsb);

        if (BSB_LENGTH(bsb) > 2) {
            LOG("%s - %s - %.*s",rule->name, ipStr, (int)BSB_LENGTH(bsb) - 2, logStr);
        }
    }
    ARKIME_THREAD_INCR(rule->matched);
    arkime_field_ops_run(session, &rule->ops);
}
/******************************************************************************/
#define RULE_LOG_INT(_v) \
    if (good && logStr) \
        BSB_EXPORT_sprintf(*logStr, "%s: %u, ", config.fields[p]->expression, (unsigned int)(_v))

#define RULE_LOG_FLOAT(_v) \
    if (good && logStr) \
        BSB_EXPORT_sprintf(*logStr, "%s: %f, ", config.fields[p]->expression, (float)(_v))

#define G_HASH_TABLE_CONTAINS_CHECK(_v) \
    good = g_hash_table_contains(rule->hash[p], (gpointer)(long)_v); \
    RULE_LOG_INT(_v);

#define G_HASH_TABLE_CONTAINS_CHECK_U64(_v) \
    good = g_hash_table_contains(rule->hash[p], (gpointer)(long)_v); \
    if (good && logStr) \
        BSB_EXPORT_sprintf(*logStr, "%s: %" PRIu64 ", ", config.fields[p]->expression, _v);

LOCAL void arkime_rules_check_rule_fields(ArkimeSession_t * const session, ArkimeRule_t * const rule, int skipPos, BSB *logStr)
{
    ArkimeString_t        *hstring;
    ArkimeInt_t           *hint;
    ArkimeStringHashStd_t *shash;
    ArkimeIntHashStd_t    *ihash;
    GHashTable            *ghash;
    GHashTableIter         iter;
    gpointer               ikey;
    gpointer               fkey;
    char                  *communityId = NULL;
    int                    i;
    int                    f;
    int                    good = 1;

    for (f = 0; good && f < rule->fieldsLen; f++) {
        int p = rule->fields[f];
        if (p == skipPos)
            continue;

        // Check fields that are directly in session
        if (p >= ARKIME_FIELD_EXSPECIAL_START) {
            switch (p) {
            case ARKIME_FIELD_EXSPECIAL_SRC_IP:
                good = arkime_rules_check_ip(rule, p, &session->addr1, logStr);
                break;
            case ARKIME_FIELD_EXSPECIAL_SRC_PORT:
                G_HASH_TABLE_CONTAINS_CHECK(session->port1);
                break;
            case ARKIME_FIELD_EXSPECIAL_DST_IP:
                good = arkime_rules_check_ip(rule, p, &session->addr2, logStr);
                break;
            case ARKIME_FIELD_EXSPECIAL_DST_PORT:
                G_HASH_TABLE_CONTAINS_CHECK(session->port2);
                break;
            case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_SYN:
                G_HASH_TABLE_CONTAINS_CHECK(session->tcpFlagCnt[ARKIME_TCPFLAG_SYN]);
                break;
            case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_SYN_ACK:
                G_HASH_TABLE_CONTAINS_CHECK(session->tcpFlagCnt[ARKIME_TCPFLAG_SYN_ACK]);
                break;
            case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_ACK:
                G_HASH_TABLE_CONTAINS_CHECK(session->tcpFlagCnt[ARKIME_TCPFLAG_ACK]);
                break;
            case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_PSH:
                G_HASH_TABLE_CONTAINS_CHECK(session->tcpFlagCnt[ARKIME_TCPFLAG_PSH]);
                break;
            case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_RST:
                G_HASH_TABLE_CONTAINS_CHECK(session->tcpFlagCnt[ARKIME_TCPFLAG_RST]);
                break;
            case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_FIN:
                G_HASH_TABLE_CONTAINS_CHECK(session->tcpFlagCnt[ARKIME_TCPFLAG_FIN]);
                break;
            case ARKIME_FIELD_EXSPECIAL_TCPFLAGS_URG:
                G_HASH_TABLE_CONTAINS_CHECK(session->tcpFlagCnt[ARKIME_TCPFLAG_URG]);
                break;
            case ARKIME_FIELD_EXSPECIAL_PACKETS_SRC:
                G_HASH_TABLE_CONTAINS_CHECK(session->packets[0]);
                break;
            case ARKIME_FIELD_EXSPECIAL_PACKETS_DST:
                G_HASH_TABLE_CONTAINS_CHECK(session->packets[1]);
                break;
            case ARKIME_FIELD_EXSPECIAL_DATABYTES_SRC:
                G_HASH_TABLE_CONTAINS_CHECK_U64(session->databytes[0]);
                break;
            case ARKIME_FIELD_EXSPECIAL_DATABYTES_DST:
                G_HASH_TABLE_CONTAINS_CHECK_U64(session->databytes[1]);
                break;
            case ARKIME_FIELD_EXSPECIAL_COMMUNITYID:
                if (session->ses == SESSION_ICMP) {
                    good = 0;
                    break;
                }
                // Only caculate once since several rules for session could use it
                if (!communityId)
                    communityId = arkime_db_community_id(session);

                good = g_hash_table_contains(rule->hash[p], communityId);
                if (good && logStr) \
                    BSB_EXPORT_sprintf(*logStr, "%s: %s, ", config.fields[p]->expression, communityId);
                break;
            default:
                good = 0;
            }
            continue;
        }

        // Check count fields
        if (p >= ARKIME_FIELDS_CNT_MIN) {
            int cp = p - ARKIME_FIELDS_CNT_MIN; // cp is the field we are getting the count of
            if (cp >= session->maxFields) {
                good = 0;
                continue;
            }

            if (!session->fields[cp]) {
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)0);
                RULE_LOG_INT(0);
                continue;
            }

            switch (config.fields[cp]->type) {
            case ARKIME_FIELD_TYPE_IP:
            case ARKIME_FIELD_TYPE_INT:
            case ARKIME_FIELD_TYPE_FLOAT:
            case ARKIME_FIELD_TYPE_STR:
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)1);
                RULE_LOG_INT(1);
                break;

            case ARKIME_FIELD_TYPE_INT_ARRAY:
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)session->fields[cp]->iarray->len);
                RULE_LOG_INT(session->fields[cp]->iarray->len);
                break;
            case ARKIME_FIELD_TYPE_FLOAT_ARRAY:
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)session->fields[cp]->farray->len);
                RULE_LOG_INT(session->fields[cp]->farray->len);
                break;
            case ARKIME_FIELD_TYPE_INT_HASH:
                ihash = session->fields[cp]->ihash;
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)HASH_COUNT(s_, *ihash));
                RULE_LOG_INT(HASH_COUNT(s_, *ihash));
                break;
            case ARKIME_FIELD_TYPE_IP_GHASH:
            case ARKIME_FIELD_TYPE_FLOAT_GHASH:
            case ARKIME_FIELD_TYPE_STR_GHASH:
            case ARKIME_FIELD_TYPE_INT_GHASH:
                ghash = session->fields[cp]->ghash;
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)g_hash_table_size(ghash));
                RULE_LOG_INT(g_hash_table_size(ghash));
                break;
            case ARKIME_FIELD_TYPE_STR_ARRAY:
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)session->fields[cp]->sarray->len);
                RULE_LOG_INT(session->fields[cp]->sarray->len);
                break;
            case ARKIME_FIELD_TYPE_STR_HASH:
                shash = session->fields[cp]->shash;
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)HASH_COUNT(s_, *shash));
                RULE_LOG_INT(HASH_COUNT(s_, *shash));
                break;
            case ARKIME_FIELD_TYPE_CERTSINFO:
                // Unsupported
                break;
            } /* switch */
            continue;
        }

        // This session doesn't have the field or it isn't set
        if (p >= session->maxFields || !session->fields[p]) {
            good = 0;
            continue;
        }

        // Check a real field
        switch (config.fields[p]->type) {
        case ARKIME_FIELD_TYPE_IP:
            good = arkime_rules_check_ip(rule, p, session->fields[p]->ip, logStr);
            break;

        case ARKIME_FIELD_TYPE_INT:
            good = g_hash_table_contains(rule->hash[p], (gpointer)(long)session->fields[p]->i);
            RULE_LOG_INT(session->fields[p]->i);
            break;
        case ARKIME_FIELD_TYPE_INT_ARRAY:
            good = 0;
            for(i = 0; i < (int)session->fields[p]->iarray->len; i++) {
                if (g_hash_table_contains(rule->hash[p], (gpointer)(long)g_array_index(session->fields[p]->iarray, uint32_t, i))) {
                    good = 1;
                    RULE_LOG_INT(g_array_index(session->fields[p]->iarray, uint32_t, i));
                    break;
                }
            }
            break;
        case ARKIME_FIELD_TYPE_INT_HASH:
            ihash = session->fields[p]->ihash;
            good = 0;
            HASH_FORALL(i_, *ihash, hint,
                if (g_hash_table_contains(rule->hash[p], (gpointer)(long)hint->i_hash)) {
                    good = 1;
                    RULE_LOG_INT(hint->i_hash);
                    break;
                }
            );
            break;
        case ARKIME_FIELD_TYPE_INT_GHASH:
            ghash = session->fields[p]->ghash;
            g_hash_table_iter_init (&iter, ghash);
            good = 0;
            while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
                if (g_hash_table_contains(rule->hash[p], ikey)) {
                    good = 1;
                    RULE_LOG_INT((long)ikey);
                    break;
                }
            }
            break;

        case ARKIME_FIELD_TYPE_FLOAT:
            good = g_hash_table_contains(rule->hash[p], (gpointer)(long)session->fields[p]->f);
            RULE_LOG_FLOAT(session->fields[p]->f);
            break;
        case ARKIME_FIELD_TYPE_FLOAT_ARRAY:
            good = 0;
            for(i = 0; i < (int)session->fields[p]->farray->len; i++) {
                if (g_hash_table_contains(rule->hash[p], (gpointer)(long)g_array_index(session->fields[p]->farray, float, i))) {
                    good = 1;
                    RULE_LOG_FLOAT(g_array_index(session->fields[p]->farray, float, i));
                    break;
                }
            }
            break;
        case ARKIME_FIELD_TYPE_FLOAT_GHASH:
            ghash = session->fields[p]->ghash;
            g_hash_table_iter_init (&iter, ghash);
            good = 0;
            while (g_hash_table_iter_next (&iter, &fkey, NULL)) {
                if (g_hash_table_contains(rule->hash[p], fkey)) {
                    good = 1;
                    RULE_LOG_FLOAT(POINTER_TO_FLOAT(fkey));
                    break;
                }
            }
            break;

        case ARKIME_FIELD_TYPE_IP_GHASH:
            ghash = session->fields[p]->ghash;
            g_hash_table_iter_init (&iter, ghash);
            good = 0;
            while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
                if (arkime_rules_check_ip(rule, p, ikey, logStr)) {
                    good = 1;
                    break;
                }
            }
            break;
        case ARKIME_FIELD_TYPE_STR_GHASH:
            ghash = session->fields[p]->ghash;
            g_hash_table_iter_init (&iter, ghash);
            good = 0;
            while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
                if (arkime_rules_check_str_match(rule, p, ikey, logStr)) {
                    good = 1;
                    break;
                }
            }
            break;
        case ARKIME_FIELD_TYPE_STR:
            good = arkime_rules_check_str_match(rule, p, session->fields[p]->str, logStr);
            break;
        case ARKIME_FIELD_TYPE_STR_ARRAY:
            good = 0;
            for(i = 0; i < (int)session->fields[p]->sarray->len; i++) {
                if (arkime_rules_check_str_match(rule, p, g_ptr_array_index(session->fields[p]->sarray, i), logStr)) {
                    good = 1;
                    break;
                }
            }
            break;
        case ARKIME_FIELD_TYPE_STR_HASH:
            shash = session->fields[p]->shash;
            good = 0;
            HASH_FORALL(s_, *shash, hstring,
                if (arkime_rules_check_str_match(rule, p, (gpointer)hstring->str, logStr)) {
                    good = 1;
                    break;
                }
            );
            break;
        case ARKIME_FIELD_TYPE_CERTSINFO:
            // Unsupported
            break;
        } /* switch */
#ifdef RULES_DEBUG
        LOG("Field pos:%d %s type:%d good:%d", p, config.fields[p]->expression, config.fields[p]->type, good);
#endif
    }
    if (logStr) {
        // Don't do anything
    } else if (good) {
#ifdef RULES_DEBUG
        LOG("%s %s matched", rule->filename, rule->name);
#endif
        arkime_rules_match(session, rule);
    } else {
#ifdef RULES_DEBUG
        LOG("%s %s didn't matched", rule->filename, rule->name);
#endif
    }

    g_free(communityId);
}
/******************************************************************************/
void arkime_rules_run_field_set_rules(ArkimeSession_t *session, int pos, GPtrArray *rules)
{
    for (int r = 0; r < (int)rules->len; r++) {
        ArkimeRule_t *rule = g_ptr_array_index(rules, r);

        // If there is only 1 field we are checking for then the ops can be run since it matched above
        if (rule->fieldsLen == 1) {
            arkime_rules_match(session, rule);
            continue;
        }

        // Need to check other fields in rule
        arkime_rules_check_rule_fields(session, rule, pos, NULL);
    }
}
/******************************************************************************/
void arkime_rules_run_field_set(ArkimeSession_t *session, int pos, const gpointer value)
{
    if (ARKIME_FIELD_TYPE_IS_IP(config.fields[pos]->type)) {

        patricia_node_t *nodes[PATRICIA_MAXBITS];

        int cnt;
        if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)value)) {
            cnt = patricia_search_all2(current.fieldsTree4[pos], ((u_char *)value) + 12, 32, nodes, PATRICIA_MAXBITS);
        } else {
            cnt = patricia_search_all2(current.fieldsTree6[pos], (u_char *)value, 128, nodes, PATRICIA_MAXBITS);
        }
        if (cnt == 0)
            return;

        // These are all the possible rules that match
        int i;
        for (i = 0; i < cnt; i++) {
            arkime_rules_run_field_set_rules(session, pos, nodes[i]->data);
        }
    } else {
        GPtrArray *rules;

        // See if this value matches anything in our matching list
        if (current.fieldsMatch[pos]) {
            GHashTableIter         iter;

            g_hash_table_iter_init (&iter, current.fieldsMatch[pos]);
            if (ARKIME_FIELD_TYPE_IS_INT(config.fields[pos]->type)) {
                uint64_t               num;

                while (g_hash_table_iter_next (&iter, (gpointer *)&num, (gpointer *)&rules)) {
                    ArkimeRuleIntMatch_t match;
                    match.num = num;
                    uint32_t test = (uint32_t)(long)value;
                    if (test >= match.min && test <= match.max) {
                        arkime_rules_run_field_set_rules(session, pos, rules);
                    }
                }
            } else {
                uint8_t               *akey;
                int                    len = strlen(value);

                while (g_hash_table_iter_next (&iter, (gpointer *)&akey, (gpointer *)&rules)) {
                    if (len < akey[1])
                        continue;

                    switch (akey[0]) {
                    case ARKIME_RULES_STR_MATCH_TAIL:
                        if (memcmp(akey + 2, value + len - akey[1], akey[1]) == 0)
                            arkime_rules_run_field_set_rules(session, pos, rules);
                        break;
                    case ARKIME_RULES_STR_MATCH_HEAD:
                        if (memcmp(akey + 2, value, akey[1]) == 0)
                            arkime_rules_run_field_set_rules(session, pos, rules);
                        break;
                    case ARKIME_RULES_STR_MATCH_CONTAINS:
                        if (arkime_memstr(value, len, (char*)akey + 2, akey[1]) != 0)
                            arkime_rules_run_field_set_rules(session, pos, rules);
                    }
                }
            }
        }

        // See if this value is in the hash table of values we are watching for
        if (current.fieldsHash[pos]) {
            rules = g_hash_table_lookup(current.fieldsHash[pos], value);
            if (rules)
                arkime_rules_run_field_set_rules(session, pos, rules);
        }
    }
}
/******************************************************************************/
void arkime_rules_run_session_setup(ArkimeSession_t *session, ArkimePacket_t *packet)
{
    int r;
    ArkimeRule_t *rule;
    for (r = 0; (rule = current.rules[ARKIME_RULE_TYPE_SESSION_SETUP][r]); r++) {
        if (rule->fieldsLen) {
            arkime_rules_check_rule_fields(session, rule, -1, NULL);
        } else if (rule->bpfp.bf_len && bpf_filter(rule->bpfp.bf_insns, packet->pkt, packet->pktlen, packet->pktlen)) {
            arkime_rules_match(session, rule);
        }
    }
}
/******************************************************************************/
void arkime_rules_run_after_classify(ArkimeSession_t *session)
{
    int r;
    ArkimeRule_t *rule;
    for (r = 0; (rule = current.rules[ARKIME_RULE_TYPE_AFTER_CLASSIFY][r]); r++) {
        if (rule->fieldsLen) {
            arkime_rules_check_rule_fields(session, rule, -1, NULL);
        }
    }
}
/******************************************************************************/
void arkime_rules_run_before_save(ArkimeSession_t *session, int final)
{
    int r;
    final = 1 << final;
    ArkimeRule_t *rule;
    for (r = 0; (rule = current.rules[ARKIME_RULE_TYPE_BEFORE_SAVE][r]); r++) {
        if ((rule->saveFlags & final) == 0) {
            continue;
        }

        if (rule->fieldsLen) {
            arkime_rules_check_rule_fields(session, rule, -1, NULL);
        }
    }
}
/******************************************************************************/
void arkime_rules_session_create(ArkimeSession_t *session)
{
    switch (session->ipProtocol) {
    case IPPROTO_SCTP:
    case IPPROTO_TCP:
    case IPPROTO_UDP:
        if (config.fields[ARKIME_FIELD_EXSPECIAL_SRC_PORT]->ruleEnabled)
            arkime_rules_run_field_set(session, ARKIME_FIELD_EXSPECIAL_SRC_PORT, (gpointer)(long)session->port1);
        if (config.fields[ARKIME_FIELD_EXSPECIAL_DST_PORT]->ruleEnabled)
            arkime_rules_run_field_set(session, ARKIME_FIELD_EXSPECIAL_DST_PORT, (gpointer)(long)session->port2);
        // NO BREAK because TCP/UDP/SCTP have ip also
        // fall through
    case IPPROTO_ESP:
    case IPPROTO_ICMP:
    case IPPROTO_ICMPV6:
        if (config.fields[ARKIME_FIELD_EXSPECIAL_SRC_IP]->ruleEnabled)
            arkime_rules_run_field_set(session, ARKIME_FIELD_EXSPECIAL_SRC_IP, &session->addr1);
        if (config.fields[ARKIME_FIELD_EXSPECIAL_DST_IP]->ruleEnabled)
            arkime_rules_run_field_set(session, ARKIME_FIELD_EXSPECIAL_DST_IP, &session->addr2);
        break;
    }
}
/******************************************************************************/
void arkime_rules_stats()
{
    int t, r;
    int header = 0;

    for (t = 0; t < ARKIME_RULE_TYPE_NUM; t++) {
        if (!current.rulesLen[t])
            continue;
        for (r = 0; r < current.rulesLen[t]; r++) {
            if (current.rules[t][r]->matched) {
                if (!header) {
                    printf("%-35s %-30s %s\n", "File", "Rule", "Matched");
                    header = 1;
                }
                printf("%-35s %-30s %" PRIu64 "\n",
                        current.rules[t][r]->filename,
                        current.rules[t][r]->name,
                        current.rules[t][r]->matched);
            }
        }
    }
}
/******************************************************************************/
void arkime_rules_init()
{
    rulesFiles = arkime_config_str_list(NULL, "rulesFiles", NULL);

    if (rulesFiles) {
        arkime_config_monitor_files("rules files", rulesFiles, arkime_rules_load);
    } else
        arkime_rules_load_complete();
}
/******************************************************************************/
void arkime_rules_exit()
{
    g_strfreev(rulesFiles);
}
