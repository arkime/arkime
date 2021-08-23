/******************************************************************************/
/* rules.c  -- Functions dealing with rules files
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
#include <stdarg.h>
#include <arpa/inet.h>
#include "yaml.h"
#include "patricia.h"
#include "pcap.h"

/******************************************************************************/
extern MolochConfig_t        config;

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

#define MOLOCH_SAVE_FLAG_MIDDLE 0x01
#define MOLOCH_SAVE_FLAG_FINAL  0x02
#define MOLOCH_SAVE_FLAG_BOTH   0x03

typedef struct {
    char                *filename;
    char                *name;
    char                *bpf;                      // String version of bpf
    struct bpf_program   bpfp;
    GHashTable          *hash[MOLOCH_FIELDS_MAX];  // For each non ip field in rule
    GPtrArray           *match[MOLOCH_FIELDS_MAX]; // For any string fields with , modifier
    patricia_tree_t     *tree4[MOLOCH_FIELDS_MAX];
    patricia_tree_t     *tree6[MOLOCH_FIELDS_MAX];
    MolochFieldOps_t     ops;                      // Ops to run on match
    uint64_t             matched;                  // How many times was matched
    uint16_t            *fields;                   // fieldsLen length array of field pos
    uint16_t             fieldsLen;
    uint8_t              saveFlags;                // When to save for beforeSave
    uint8_t              log;                      // should we log or not
} MolochRule_t;

#define MOLOCH_RULES_MAX     100

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
    GHashTable            *fieldsHash[MOLOCH_FIELDS_MAX];
    patricia_tree_t       *fieldsTree4[MOLOCH_FIELDS_MAX];
    patricia_tree_t       *fieldsTree6[MOLOCH_FIELDS_MAX];
    GHashTable            *fieldsMatch[MOLOCH_FIELDS_MAX];

    int                    rulesLen[MOLOCH_RULE_TYPE_NUM];
    MolochRule_t          *rules[MOLOCH_RULE_TYPE_NUM][MOLOCH_RULES_MAX+1];
} MolochRulesInfo_t;

LOCAL MolochRulesInfo_t    current;
LOCAL MolochRulesInfo_t    loading;
LOCAL char               **rulesFiles;

LOCAL pcap_t              *deadPcap;
extern MolochPcapFileHdr_t pcapFileHeader;

#define MOLOCH_RULES_STR_MATCH_HEAD      1
#define MOLOCH_RULES_STR_MATCH_TAIL      2
#define MOLOCH_RULES_STR_MATCH_CONTAINS  3

/******************************************************************************/
void moloch_rules_free_array(gpointer data)
{
    g_ptr_array_free(data, TRUE);
}
/******************************************************************************/
void moloch_rules_parser_free_node(YamlNode_t *node)
{
    if (node->key)
        g_free(node->key);
    if (node->value > YAML_NODE_SEQUENCE_VALUE)
        g_free(node->value);
    if (node->values)
        g_ptr_array_free(node->values, TRUE);
    MOLOCH_TYPE_FREE(YamlNode_t, node);
}
/******************************************************************************/
YamlNode_t *moloch_rules_parser_add_node(YamlNode_t *parent, char *key, char *value)
{
    YamlNode_t *node = MOLOCH_TYPE_ALLOC(YamlNode_t);
    node->key = key;
    node->value = value;

    if (value) {
        node->values = NULL;
    } else {
        node->values = g_ptr_array_new_with_free_func((GDestroyNotify)moloch_rules_parser_free_node);
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
YamlNode_t *moloch_rules_parser_parse_yaml(char *filename, YamlNode_t *parent, yaml_parser_t *parser, gboolean sequence) {

    char *key = NULL;
    YamlNode_t *node;

    int done = 0;
    while (!done) {
        yaml_event_t event;

        if (!yaml_parser_parse(parser, &event))
            LOGEXIT("%s:%zu - Parse error '%s'", filename, parser->problem_mark.line, parser->problem);

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
                moloch_rules_parser_add_node(parent, g_strdup((gchar *)event.data.scalar.value), YAML_NODE_SEQUENCE_VALUE);
            } else if (key) {
                moloch_rules_parser_add_node(parent, key, g_strdup((gchar *)event.data.scalar.value));
                key = NULL;
            } else {
                key = g_strdup((gchar *)event.data.scalar.value);
            }
            break;
        case YAML_SEQUENCE_START_EVENT:
        case YAML_MAPPING_START_EVENT:
            if (parent == NULL) {
                parent = node = moloch_rules_parser_add_node(NULL, g_strdup("root"), NULL);
            } else {
                node = moloch_rules_parser_add_node(parent, key, NULL);
            }
            key = NULL;
            if (moloch_rules_parser_parse_yaml(filename, node, parser, event.type == YAML_SEQUENCE_START_EVENT) == NULL)
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
void moloch_rules_parser_print(YamlNode_t *node, int level)
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
            moloch_rules_parser_print(g_ptr_array_index(node->values, i), level+1);
    }
}
/******************************************************************************/
YamlNode_t *moloch_rules_parser_get(YamlNode_t *node, char *path)
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
char *moloch_rules_parser_get_value(YamlNode_t *parent, char *path)
{
    YamlNode_t *node = moloch_rules_parser_get(parent, path);
    if (!node)
        return NULL;
    return node->value;
}
/******************************************************************************/
GPtrArray *moloch_rules_parser_get_values(YamlNode_t *parent, char *path)
{
    YamlNode_t *node = moloch_rules_parser_get(parent, path);
    if (!node)
        return NULL;
    return node->values;
}
/******************************************************************************/
void moloch_rules_load_add_field(MolochRule_t *rule, int pos, char *key)
{
    uint32_t         n;
    float            f;
    uint32_t         fint;
    GPtrArray       *rules;
    patricia_node_t *node;

    config.fields[pos]->ruleEnabled = 1;

    switch (config.fields[pos]->type) {
    case MOLOCH_FIELD_TYPE_INT:
    case MOLOCH_FIELD_TYPE_INT_ARRAY:
    case MOLOCH_FIELD_TYPE_INT_HASH:
    case MOLOCH_FIELD_TYPE_INT_GHASH:
        if (!loading.fieldsHash[pos])
            loading.fieldsHash[pos] = g_hash_table_new_full(NULL, NULL, NULL, moloch_rules_free_array);

        n = atoi(key);
        g_hash_table_add(rule->hash[pos], (void *)(long)n);

        rules = g_hash_table_lookup(loading.fieldsHash[pos], (void *)(long)n);
        if (!rules) {
            rules = g_ptr_array_new();
            g_hash_table_insert(loading.fieldsHash[pos], (void *)(long)n, rules);
        }
        g_ptr_array_add(rules, rule);
        break;

    case MOLOCH_FIELD_TYPE_FLOAT:
    case MOLOCH_FIELD_TYPE_FLOAT_ARRAY:
    case MOLOCH_FIELD_TYPE_FLOAT_GHASH:
        if (!loading.fieldsHash[pos])
            loading.fieldsHash[pos] = g_hash_table_new_full(NULL, NULL, NULL, moloch_rules_free_array);

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

    case MOLOCH_FIELD_TYPE_IP:
    case MOLOCH_FIELD_TYPE_IP_GHASH:
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


    case MOLOCH_FIELD_TYPE_STR:
    case MOLOCH_FIELD_TYPE_STR_ARRAY:
    case MOLOCH_FIELD_TYPE_STR_HASH:
    case MOLOCH_FIELD_TYPE_STR_GHASH:
        if (!loading.fieldsHash[pos])
            loading.fieldsHash[pos] = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, moloch_rules_free_array);

        g_hash_table_add(rule->hash[pos], g_strdup(key));

        rules = g_hash_table_lookup(loading.fieldsHash[pos], key);
        if (!rules) {
            rules = g_ptr_array_new();
            g_hash_table_insert(loading.fieldsHash[pos], g_strdup(key), rules);
        }
        g_ptr_array_add(rules, rule);
        break;
    case MOLOCH_FIELD_TYPE_CERTSINFO:
        // Unsupported
        break;
    }
}
/******************************************************************************/
void moloch_rules_load_add_field_match(MolochRule_t *rule, int pos, int type, char *key)
{
    GPtrArray       *rules;
    int              len = strlen(key);

    if (len > 255)
        LOGEXIT("Match %s is to too large", key);

    uint8_t *nkey = g_malloc(len+3);
    nkey[0] = type;
    nkey[1] = len;
    memcpy(nkey+2, key, len+1);

    if (!loading.fieldsMatch[pos])
        loading.fieldsMatch[pos] = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, moloch_rules_free_array);

    g_ptr_array_add(rule->match[pos], (char *)nkey); // Just made a copy above

    rules = g_hash_table_lookup(loading.fieldsMatch[pos], nkey);
    if (!rules) {
        rules = g_ptr_array_new();
        g_hash_table_insert(loading.fieldsMatch[pos], g_strdup((char *)nkey), rules);
    }
    g_ptr_array_add(rules, rule);
}
/******************************************************************************/
void moloch_rules_parser_load_rule(char *filename, YamlNode_t *parent)
{
    char *name = moloch_rules_parser_get_value(parent, "name");
    if (!name)
        LOGEXIT("%s: name required for rule", filename);

    char *when = moloch_rules_parser_get_value(parent, "when");
    if (!when)
        LOGEXIT("%s: when required for rule '%s'", filename, name);

    char *bpf = moloch_rules_parser_get_value(parent, "bpf");
    GPtrArray *fields = moloch_rules_parser_get_values(parent, "fields");
    char *expression = moloch_rules_parser_get_value(parent, "expression");

    if (!bpf && !fields && !expression)
        LOGEXIT("%s: bpf, fields, or expressions required for rule '%s'", filename, name);

    if ((bpf && fields) || (bpf && expression) || (fields && expression))
        LOGEXIT("%s: Only one of bpf, fields, or expressions can be set for rule '%s'", filename, name);

    GPtrArray  *ops = moloch_rules_parser_get_values(parent, "ops");
    if (!ops)
        LOGEXIT("%s: ops required for rule '%s'", filename, name);

    if (expression) {
        LOGEXIT("Currently don't support expression, hopefully soon!");
    }

    char *log = moloch_rules_parser_get_value(parent, "log");

    int type;
    int saveFlags = 0;
    if (strcmp(when, "everyPacket") == 0) {
        type = MOLOCH_RULE_TYPE_EVERY_PACKET;
        if (!bpf)
            LOGEXIT("%s: everyPacket only supports bpf", filename);
    } else if (strcmp(when, "sessionSetup") == 0) {
        type = MOLOCH_RULE_TYPE_SESSION_SETUP;
    } else if (strcmp(when, "afterClassify") == 0) {
        type = MOLOCH_RULE_TYPE_AFTER_CLASSIFY;
        if (bpf)
            LOGEXIT("%s: %s doesn't support bpf", filename, when);
    } else if (strcmp(when, "fieldSet") == 0) {
        type = MOLOCH_RULE_TYPE_FIELD_SET;
        if (bpf)
            LOGEXIT("%s: %s doesn't support bpf", filename, when);
    } else if (strcmp(when, "beforeMiddleSave") == 0) {
        type = MOLOCH_RULE_TYPE_BEFORE_SAVE;
        saveFlags = MOLOCH_SAVE_FLAG_MIDDLE;
        if (bpf)
            LOGEXIT("%s: %s doesn't support bpf", filename, when);
    } else if (strcmp(when, "beforeFinalSave") == 0) {
        type = MOLOCH_RULE_TYPE_BEFORE_SAVE;
        saveFlags = MOLOCH_SAVE_FLAG_FINAL;
        if (bpf)
            LOGEXIT("%s: %s doesn't support bpf", filename, when);
    } else if (strcmp(when, "beforeBothSave") == 0) {
        type = MOLOCH_RULE_TYPE_BEFORE_SAVE;
        saveFlags = MOLOCH_SAVE_FLAG_BOTH;
        if (bpf)
            LOGEXIT("%s: %s doesn't support bpf", filename, when);
    } else {
        LOGEXIT("%s: Unknown when '%s'", filename, when);
    }

    if (loading.rulesLen[type] >= MOLOCH_RULES_MAX)
        LOGEXIT("Too many %s rules", when);

    int n = loading.rulesLen[type]++;
    MolochRule_t *rule = loading.rules[type][n] = MOLOCH_TYPE_ALLOC0(MolochRule_t);
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
                    mtype = MOLOCH_RULES_STR_MATCH_TAIL;
                } else if (strcmp(comma, "head") == 0 || strcmp(comma, "startsWith") == 0) {
                    mtype = MOLOCH_RULES_STR_MATCH_HEAD;
                } else if (strcmp(comma, "contains") == 0) {
                    mtype = MOLOCH_RULES_STR_MATCH_CONTAINS;
                } else {
                    LOGEXIT("Rule field %s doesn't support modifier %s", node->key, comma);
                }
            }

            int pos = moloch_field_by_exp(node->key);
            if (pos == -1)
                LOGEXIT("%s Couldn't find field '%s'", filename, node->key);

            // Add this fieldPos to the list if not already there
            if (!(rule->hash[pos] || rule->tree4[pos] || rule->match[pos]))
                rule->fields[(int)rule->fieldsLen++] = pos;

            switch (config.fields[pos]->type) {
            case MOLOCH_FIELD_TYPE_INT:
            case MOLOCH_FIELD_TYPE_INT_ARRAY:
            case MOLOCH_FIELD_TYPE_INT_HASH:
            case MOLOCH_FIELD_TYPE_INT_GHASH:
                if (mtype != 0)
                    LOGEXIT("Rule field %s doesn't support modifier %s", node->key, comma);

                if (!rule->hash[pos])
                    rule->hash[pos] = g_hash_table_new_full(NULL, NULL, NULL, NULL);
                break;

            case MOLOCH_FIELD_TYPE_FLOAT:
            case MOLOCH_FIELD_TYPE_FLOAT_ARRAY:
            case MOLOCH_FIELD_TYPE_FLOAT_GHASH:
                if (mtype != 0)
                    LOGEXIT("Rule field %s doesn't support modifier %s", node->key, comma);

                if (!rule->hash[pos])
                    rule->hash[pos] = g_hash_table_new_full(NULL, NULL, NULL, NULL);
                break;

            case MOLOCH_FIELD_TYPE_IP:
            case MOLOCH_FIELD_TYPE_IP_GHASH:
                if (mtype != 0)
                    LOGEXIT("Rule field %s doesn't support modifier %s", node->key, comma);

                if (!rule->tree4[pos]) {
                    rule->tree4[pos] = New_Patricia(32);
                    rule->tree6[pos] = New_Patricia(128);
                }
                break;

            case MOLOCH_FIELD_TYPE_STR:
            case MOLOCH_FIELD_TYPE_STR_ARRAY:
            case MOLOCH_FIELD_TYPE_STR_HASH:
            case MOLOCH_FIELD_TYPE_STR_GHASH:
                if (mtype != 0) {
                    if (!rule->match[pos])
                        rule->match[pos] = g_ptr_array_new_with_free_func(g_free);
                } else {
                    if (!rule->hash[pos])
                        rule->hash[pos] = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
                }
                break;

            case MOLOCH_FIELD_TYPE_CERTSINFO:
                LOGEXIT("%s: Currently don't support any certs fields", filename);
            }

            if (node->value) {
                if (mtype != 0)
                    moloch_rules_load_add_field_match(rule, pos, mtype, node->value);
                else
                    moloch_rules_load_add_field(rule, pos, node->value);
            } else {
                int j;
                for (j = 0; j < (int)node->values->len; j++) {
                    YamlNode_t *fnode = g_ptr_array_index(node->values, j);
                    if (mtype != 0)
                        moloch_rules_load_add_field_match(rule, pos, mtype, fnode->key);
                    else
                        moloch_rules_load_add_field(rule, pos, fnode->key);
                }
            }
        }
    }

    moloch_field_ops_init(&rule->ops, ops->len, MOLOCH_FIELD_OPS_FLAGS_COPY);
    int i;
    for (i = 0; i < (int)ops->len; i++) {
        YamlNode_t *node = g_ptr_array_index(ops, i);
        int pos = moloch_field_by_exp(node->key);
        if (pos == -1)
            LOGEXIT("%s Couldn't find field '%s'", filename, node->key);
        moloch_field_ops_add(&rule->ops, pos, node->value, strlen(node->value));
    }
}
/******************************************************************************/
void moloch_rules_parser_load_file(char *filename, YamlNode_t *parent)
{
    char       *str;
    GPtrArray  *rules;

    str = moloch_rules_parser_get_value(parent, "version");
    if (!str || strcmp(str, "1") != 0) {
        LOGEXIT("%s: Missing version: 1", filename);
    }

    rules = moloch_rules_parser_get_values(parent, "rules");
    if (!rules) {
        LOGEXIT("%s: Missing rules", filename);
    }

    int i;
    for (i = 0; i < (int)rules->len; i++) {
        moloch_rules_parser_load_rule(filename, g_ptr_array_index(rules, i));
    }
}
/******************************************************************************/
void moloch_rules_load_complete()
{
    char      **bpfs;
    GRegex     *regex = g_regex_new(":\\s*(\\d+)\\s*$", 0, 0, 0);
    int         i;

    bpfs = moloch_config_str_list(NULL, "dontSaveBPFs", NULL);
    int pos = moloch_field_by_exp("_maxPacketsToSave");
    gint start_pos;
    if (bpfs) {
        for (i = 0; bpfs[i]; i++) {
            int n = loading.rulesLen[MOLOCH_RULE_TYPE_SESSION_SETUP]++;
            MolochRule_t *rule = loading.rules[MOLOCH_RULE_TYPE_SESSION_SETUP][n] = MOLOCH_TYPE_ALLOC0(MolochRule_t);
            rule->filename = "dontSaveBPFs";
            moloch_field_ops_init(&rule->ops, 1, MOLOCH_FIELD_OPS_FLAGS_COPY);

            GMatchInfo *match_info = 0;
            g_regex_match(regex, bpfs[i], 0, &match_info);
            if (g_match_info_matches(match_info)) {
                g_match_info_fetch_pos (match_info, 1, &start_pos, NULL);
                rule->bpf = g_strndup(bpfs[i], start_pos-1);
                moloch_field_ops_add(&rule->ops, pos, g_match_info_fetch(match_info, 1), -1);
            } else {
                rule->bpf = g_strdup(bpfs[i]);
                moloch_field_ops_add(&rule->ops, pos, "1", -1);
            }
            g_match_info_free(match_info);
        }
        g_strfreev(bpfs);
    }

    bpfs = moloch_config_str_list(NULL, "minPacketsSaveBPFs", NULL);
    pos = moloch_field_by_exp("_minPacketsBeforeSavingSPI");
    if (bpfs) {
        for (i = 0; bpfs[i]; i++) {
            int n = loading.rulesLen[MOLOCH_RULE_TYPE_SESSION_SETUP]++;
            MolochRule_t *rule = loading.rules[MOLOCH_RULE_TYPE_SESSION_SETUP][n] = MOLOCH_TYPE_ALLOC0(MolochRule_t);
            rule->filename = "minPacketsSaveBPFs";
            moloch_field_ops_init(&rule->ops, 1, MOLOCH_FIELD_OPS_FLAGS_COPY);

            GMatchInfo *match_info = 0;
            g_regex_match(regex, bpfs[i], 0, &match_info);
            if (g_match_info_matches(match_info)) {
                g_match_info_fetch_pos (match_info, 1, &start_pos, NULL);
                rule->bpf = g_strndup(bpfs[i], start_pos-1);
                moloch_field_ops_add(&rule->ops, pos, g_match_info_fetch(match_info, 1), -1);
            } else {
                rule->bpf = g_strdup(bpfs[i]);
                moloch_field_ops_add(&rule->ops, pos, "1", -1);
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
void moloch_rules_free(MolochRulesInfo_t *freeing)
{
    int    i, t, r;

    for (i = 0; i < MOLOCH_FIELDS_MAX; i++) {
        if (freeing->fieldsHash[i]) {
            g_hash_table_destroy(freeing->fieldsHash[i]);
        }
        if (freeing->fieldsTree4[i]) {
            Destroy_Patricia(freeing->fieldsTree4[i], moloch_rules_free_array);
        }
        if (freeing->fieldsTree6[i]) {
            Destroy_Patricia(freeing->fieldsTree6[i], moloch_rules_free_array);
        }
        if (freeing->fieldsMatch[i]) {
            g_hash_table_destroy(freeing->fieldsMatch[i]);
        }
    }

    for (t = 0; t < MOLOCH_RULE_TYPE_NUM; t++) {
        for (r = 0; r < freeing->rulesLen[t]; r++) {
            MolochRule_t *rule = freeing->rules[t][r];

            g_free(rule->name);
            if (rule->bpf)
                g_free(rule->bpf);

            for (i = 0; i < MOLOCH_FIELDS_MAX; i++) {
                if (rule->hash[i]) {
                    g_hash_table_destroy(rule->hash[i]);
                }
                if (rule->tree4[i]) {
                    Destroy_Patricia(rule->tree4[i], moloch_rules_free_array);
                }
                if (rule->tree6[i]) {
                    Destroy_Patricia(rule->tree6[i], moloch_rules_free_array);
                }
                if (rule->match[i]) {
                    g_ptr_array_free(rule->match[i], TRUE);
                }
            }

            moloch_field_ops_free(&rule->ops);
            MOLOCH_TYPE_FREE(MolochRule_t, rule);
        }
    }

    MOLOCH_TYPE_FREE(MolochRulesInfo_t, freeing);
}
/******************************************************************************/
void moloch_rules_load(char **names)
{
    int    i;

    // Make a copy of current items to free later

    MolochRulesInfo_t *freeing = MOLOCH_TYPE_ALLOC0(MolochRulesInfo_t);
    memcpy(freeing, &current, sizeof(current));

    // Load all the rule files
    for (i = 0; names[i]; i++) {
        yaml_parser_t parser;
        yaml_parser_initialize(&parser);
        FILE *input = fopen(names[i], "rb");
        if (!input)
            LOGEXIT("ERROR - can not open rules file %s", names[i]);

        yaml_parser_set_input_file(&parser, input);
        YamlNode_t *parent = moloch_rules_parser_parse_yaml(names[i], NULL, &parser, FALSE);
        yaml_parser_delete(&parser);
        if (!parent) {
            LOG("WARNING %s - has no rules", names[i]);
            continue;
        }
        if (config.debug > 1) {
            moloch_rules_parser_print(parent, 0);
        }
        moloch_rules_parser_load_file(names[i], parent);
        moloch_rules_parser_free_node(parent);
        fclose(input);
    }

    // Part 2, which will also copy loading to current
    moloch_rules_load_complete();

    // Now schedule free of current items
    moloch_free_later(freeing, (GDestroyNotify) moloch_rules_free);
}
/******************************************************************************/
/* Called at the start on main thread or each time a new file is open on single thread */
void moloch_rules_recompile()
{
    int t, r;

    if (deadPcap)
        pcap_close(deadPcap);

    deadPcap = pcap_open_dead(pcapFileHeader.dlt, pcapFileHeader.snaplen);
    MolochRule_t *rule;
    for (t = 0; t < MOLOCH_RULE_TYPE_NUM; t++) {
        for (r = 0; (rule = current.rules[t][r]); r++) {
            if (!rule->bpf)
                continue;

            pcap_freecode(&rule->bpfp);
            if (pcapFileHeader.dlt != DLT_NFLOG) {
                if (pcap_compile(deadPcap, &rule->bpfp, rule->bpf, 1, PCAP_NETMASK_UNKNOWN) == -1) {
                    LOGEXIT("ERROR - Couldn't compile bpf filter %s: '%s' with %s", rule->filename, rule->bpf, pcap_geterr(deadPcap));
                }
            } else {
                rule->bpfp.bf_len = 0;
            }
        }
    }
}
/******************************************************************************/
LOCAL gboolean moloch_rules_check_ip(const MolochRule_t * const rule, const int p, const struct in6_addr *ip, BSB *logStr)
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
LOCAL gboolean moloch_rules_check_str_match(const MolochRule_t * const rule, int p, const char * const key, BSB *logStr)
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
        case MOLOCH_RULES_STR_MATCH_TAIL:
            if (memcmp(akey+2, key + len - akey[1], akey[1]) == 0) {
                if (logStr) {
                    BSB_EXPORT_sprintf(*logStr, "%s,tail: %s, ", config.fields[p]->expression, akey+2);
                }
                return TRUE;
            }
        case MOLOCH_RULES_STR_MATCH_HEAD:
            if (memcmp(akey+2, key, akey[1]) == 0) {
                if (logStr) {
                    BSB_EXPORT_sprintf(*logStr, "%s,head: %s, ", config.fields[p]->expression, akey+2);
                }
                return TRUE;
            }
        case MOLOCH_RULES_STR_MATCH_CONTAINS:
            if (moloch_memstr(key, len, (char*)akey+2, akey[1]) != 0) {
                if (logStr) {
                    BSB_EXPORT_sprintf(*logStr, "%s,contains: %s, ", config.fields[p]->expression, akey+2);
                }
                return TRUE;
            }
        }
    }

    return FALSE;
}
LOCAL void moloch_rules_check_rule_fields(MolochSession_t * const session, MolochRule_t * const rule, int skipPos, BSB *logStr);
/******************************************************************************/
LOCAL void moloch_rules_match(MolochSession_t * const session, MolochRule_t * const rule)
{
    if (rule->log) {
        char ipStr[200];
        char logStr[5000];
        BSB bsb;


        BSB_INIT(bsb, ipStr, sizeof(ipStr));

        if (IN6_IS_ADDR_V4MAPPED(&session->addr1)) {
            uint32_t ip1 = MOLOCH_V6_TO_V4(session->addr1);
            uint32_t ip2 = MOLOCH_V6_TO_V4(session->addr2);
            BSB_EXPORT_sprintf(bsb, "%u.%u.%u.%u => %u.%u.%u.%u:%u", ip1 & 0xff, (ip1 >> 8) & 0xff, (ip1 >> 16) & 0xff, (ip1 >> 24) & 0xff,
                ip2 & 0xff, (ip2 >> 8) & 0xff, (ip2 >> 16) & 0xff, (ip2 >> 24) & 0xff, session->port2);
        } else {
            BSB_EXPORT_inet_ntop(bsb, AF_INET6, &session->addr1);
            BSB_EXPORT_cstr(bsb, " => ");
            BSB_EXPORT_inet_ntop(bsb, AF_INET6, &session->addr2);
            BSB_EXPORT_sprintf(bsb, ".%u", session->port2);
        }

        BSB_INIT(bsb, logStr, sizeof(logStr));

        moloch_rules_check_rule_fields(session, rule, -1, &bsb);

        if (BSB_LENGTH(bsb) > 2) {
            LOG("%s - %s - %.*s",rule->name, ipStr, (int)BSB_LENGTH(bsb) - 2, logStr);
        }
    }
    MOLOCH_THREAD_INCR(rule->matched);
    moloch_field_ops_run(session, &rule->ops);
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

LOCAL void moloch_rules_check_rule_fields(MolochSession_t * const session, MolochRule_t * const rule, int skipPos, BSB *logStr)
{
    MolochString_t        *hstring;
    MolochInt_t           *hint;
    MolochStringHashStd_t *shash;
    MolochIntHashStd_t    *ihash;
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
        if (p >= MOLOCH_FIELD_EXSPECIAL_START) {
            switch (p) {
            case MOLOCH_FIELD_EXSPECIAL_SRC_IP:
                good = moloch_rules_check_ip(rule, p, &session->addr1, logStr);
                break;
            case MOLOCH_FIELD_EXSPECIAL_SRC_PORT:
                G_HASH_TABLE_CONTAINS_CHECK(session->port1);
                break;
            case MOLOCH_FIELD_EXSPECIAL_DST_IP:
                good = moloch_rules_check_ip(rule, p, &session->addr2, logStr);
                break;
            case MOLOCH_FIELD_EXSPECIAL_DST_PORT:
                G_HASH_TABLE_CONTAINS_CHECK(session->port2);
                break;
            case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_SYN:
                G_HASH_TABLE_CONTAINS_CHECK(session->tcpFlagCnt[MOLOCH_TCPFLAG_SYN]);
                break;
            case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_SYN_ACK:
                G_HASH_TABLE_CONTAINS_CHECK(session->tcpFlagCnt[MOLOCH_TCPFLAG_SYN_ACK]);
                break;
            case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_ACK:
                G_HASH_TABLE_CONTAINS_CHECK(session->tcpFlagCnt[MOLOCH_TCPFLAG_ACK]);
                break;
            case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_PSH:
                G_HASH_TABLE_CONTAINS_CHECK(session->tcpFlagCnt[MOLOCH_TCPFLAG_PSH]);
                break;
            case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_RST:
                G_HASH_TABLE_CONTAINS_CHECK(session->tcpFlagCnt[MOLOCH_TCPFLAG_RST]);
                break;
            case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_FIN:
                G_HASH_TABLE_CONTAINS_CHECK(session->tcpFlagCnt[MOLOCH_TCPFLAG_FIN]);
                break;
            case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_URG:
                G_HASH_TABLE_CONTAINS_CHECK(session->tcpFlagCnt[MOLOCH_TCPFLAG_URG]);
                break;
            case MOLOCH_FIELD_EXSPECIAL_PACKETS_SRC:
                G_HASH_TABLE_CONTAINS_CHECK(session->packets[0]);
                break;
            case MOLOCH_FIELD_EXSPECIAL_PACKETS_DST:
                G_HASH_TABLE_CONTAINS_CHECK(session->packets[1]);
                break;
            case MOLOCH_FIELD_EXSPECIAL_DATABYTES_SRC:
                G_HASH_TABLE_CONTAINS_CHECK_U64(session->databytes[0]);
                break;
            case MOLOCH_FIELD_EXSPECIAL_DATABYTES_DST:
                G_HASH_TABLE_CONTAINS_CHECK_U64(session->databytes[1]);
                break;
            case MOLOCH_FIELD_EXSPECIAL_COMMUNITYID:
                if (session->ses == SESSION_ICMP) {
                    good = 0;
                    break;
                }
                // Only caculate once since several rules for session could use it
                if (!communityId)
                    communityId = moloch_db_community_id(session);

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
        if (p >= MOLOCH_FIELDS_CNT_MIN) {
            int cp = p - MOLOCH_FIELDS_CNT_MIN; // cp is the field we are getting the count of
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
            case MOLOCH_FIELD_TYPE_IP:
            case MOLOCH_FIELD_TYPE_INT:
            case MOLOCH_FIELD_TYPE_FLOAT:
            case MOLOCH_FIELD_TYPE_STR:
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)1);
                RULE_LOG_INT(1);
                break;

            case MOLOCH_FIELD_TYPE_INT_ARRAY:
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)session->fields[cp]->iarray->len);
                RULE_LOG_INT(session->fields[cp]->iarray->len);
                break;
            case MOLOCH_FIELD_TYPE_FLOAT_ARRAY:
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)session->fields[cp]->farray->len);
                RULE_LOG_INT(session->fields[cp]->farray->len);
                break;
            case MOLOCH_FIELD_TYPE_INT_HASH:
                ihash = session->fields[cp]->ihash;
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)HASH_COUNT(s_, *ihash));
                RULE_LOG_INT(HASH_COUNT(s_, *ihash));
                break;
            case MOLOCH_FIELD_TYPE_IP_GHASH:
            case MOLOCH_FIELD_TYPE_FLOAT_GHASH:
            case MOLOCH_FIELD_TYPE_STR_GHASH:
            case MOLOCH_FIELD_TYPE_INT_GHASH:
                ghash = session->fields[cp]->ghash;
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)g_hash_table_size(ghash));
                RULE_LOG_INT(g_hash_table_size(ghash));
                break;
            case MOLOCH_FIELD_TYPE_STR_ARRAY:
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)session->fields[cp]->sarray->len);
                RULE_LOG_INT(session->fields[cp]->sarray->len);
                break;
            case MOLOCH_FIELD_TYPE_STR_HASH:
                shash = session->fields[cp]->shash;
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)HASH_COUNT(s_, *shash));
                RULE_LOG_INT(HASH_COUNT(s_, *shash));
                break;
            case MOLOCH_FIELD_TYPE_CERTSINFO:
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
        case MOLOCH_FIELD_TYPE_IP:
            good = moloch_rules_check_ip(rule, p, session->fields[p]->ip, logStr);
            break;

        case MOLOCH_FIELD_TYPE_INT:
            good = g_hash_table_contains(rule->hash[p], (gpointer)(long)session->fields[p]->i);
            RULE_LOG_INT(session->fields[p]->i);
            break;
        case MOLOCH_FIELD_TYPE_INT_ARRAY:
            good = 0;
            for(i = 0; i < (int)session->fields[p]->iarray->len; i++) {
                if (g_hash_table_contains(rule->hash[p], (gpointer)(long)g_array_index(session->fields[p]->iarray, uint32_t, i))) {
                    good = 1;
                    RULE_LOG_INT(g_array_index(session->fields[p]->iarray, uint32_t, i));
                    break;
                }
            }
            break;
        case MOLOCH_FIELD_TYPE_INT_HASH:
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
        case MOLOCH_FIELD_TYPE_INT_GHASH:
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

        case MOLOCH_FIELD_TYPE_FLOAT:
            good = g_hash_table_contains(rule->hash[p], (gpointer)(long)session->fields[p]->f);
            RULE_LOG_FLOAT(session->fields[p]->f);
            break;
        case MOLOCH_FIELD_TYPE_FLOAT_ARRAY:
            good = 0;
            for(i = 0; i < (int)session->fields[p]->farray->len; i++) {
                if (g_hash_table_contains(rule->hash[p], (gpointer)(long)g_array_index(session->fields[p]->farray, float, i))) {
                    good = 1;
                    RULE_LOG_FLOAT(g_array_index(session->fields[p]->farray, float, i));
                    break;
                }
            }
            break;
        case MOLOCH_FIELD_TYPE_FLOAT_GHASH:
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

        case MOLOCH_FIELD_TYPE_IP_GHASH:
            ghash = session->fields[p]->ghash;
            g_hash_table_iter_init (&iter, ghash);
            good = 0;
            while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
                if (moloch_rules_check_ip(rule, p, ikey, logStr)) {
                    good = 1;
                    break;
                }
            }
            break;
        case MOLOCH_FIELD_TYPE_STR_GHASH:
            ghash = session->fields[p]->ghash;
            g_hash_table_iter_init (&iter, ghash);
            good = 0;
            while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
                if (moloch_rules_check_str_match(rule, p, ikey, logStr)) {
                    good = 1;
                    break;
                }
            }
            break;
        case MOLOCH_FIELD_TYPE_STR:
            good = moloch_rules_check_str_match(rule, p, session->fields[p]->str, logStr);
            break;
        case MOLOCH_FIELD_TYPE_STR_ARRAY:
            good = 0;
            for(i = 0; i < (int)session->fields[p]->sarray->len; i++) {
                if (moloch_rules_check_str_match(rule, p, g_ptr_array_index(session->fields[p]->sarray, i), logStr)) {
                    good = 1;
                    break;
                }
            }
            break;
        case MOLOCH_FIELD_TYPE_STR_HASH:
            shash = session->fields[p]->shash;
            good = 0;
            HASH_FORALL(s_, *shash, hstring,
                if (moloch_rules_check_str_match(rule, p, (gpointer)hstring->str, logStr)) {
                    good = 1;
                    break;
                }
            );
            break;
        case MOLOCH_FIELD_TYPE_CERTSINFO:
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
        moloch_rules_match(session, rule);
    } else {
#ifdef RULES_DEBUG
        LOG("%s %s didn't matched", rule->filename, rule->name);
#endif
    }

    g_free(communityId);
}
/******************************************************************************/
void moloch_rules_run_field_set_rules(MolochSession_t *session, int pos, GPtrArray *rules)
{
    for (int r = 0; r < (int)rules->len; r++) {
        MolochRule_t *rule = g_ptr_array_index(rules, r);

        // If there is only 1 field we are checking for then the ops can be run since it matched above
        if (rule->fieldsLen == 1) {
            moloch_rules_match(session, rule);
            continue;
        }

        // Need to check other fields in rule
        moloch_rules_check_rule_fields(session, rule, pos, NULL);
    }
}
/******************************************************************************/
void moloch_rules_run_field_set(MolochSession_t *session, int pos, const gpointer value)
{
    GPtrArray             *rules;

    if (config.fields[pos]->type == MOLOCH_FIELD_TYPE_IP ||
        config.fields[pos]->type == MOLOCH_FIELD_TYPE_IP_GHASH) {

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
            moloch_rules_run_field_set_rules(session, pos, nodes[i]->data);
        }
    } else {
        // See if this value is in the hash table of matches we are watching for
        if (current.fieldsMatch[pos]) {
            GHashTableIter         iter;
            uint8_t               *akey;
            int                    len = strlen(value);

            g_hash_table_iter_init (&iter, current.fieldsMatch[pos]);
            while (g_hash_table_iter_next (&iter, (gpointer *)&akey, (gpointer *)&rules)) {

                if (len < akey[1])
                    continue;

                switch (akey[0]) {
                case MOLOCH_RULES_STR_MATCH_TAIL:
                    if (memcmp(akey+2, value + len - akey[1], akey[1]) == 0)
                        moloch_rules_run_field_set_rules(session, pos, rules);
                    break;
                case MOLOCH_RULES_STR_MATCH_HEAD:
                    if (memcmp(akey+2, value, akey[1]) == 0)
                        moloch_rules_run_field_set_rules(session, pos, rules);
                    break;
                case MOLOCH_RULES_STR_MATCH_CONTAINS:
                    if (moloch_memstr(value, len, (char*)akey+2, akey[1]) != 0)
                        moloch_rules_run_field_set_rules(session, pos, rules);
                }
            }
        }

        // See if this value is in the hash table of values we are watching for
        rules = g_hash_table_lookup(current.fieldsHash[pos], value);
        if (rules)
            moloch_rules_run_field_set_rules(session, pos, rules);
    }
}
/******************************************************************************/
void moloch_rules_run_session_setup(MolochSession_t *session, MolochPacket_t *packet)
{
    int r;
    MolochRule_t *rule;
    for (r = 0; (rule = current.rules[MOLOCH_RULE_TYPE_SESSION_SETUP][r]); r++) {
        if (rule->fieldsLen) {
            moloch_rules_check_rule_fields(session, rule, -1, NULL);
        } else if (rule->bpfp.bf_len && bpf_filter(rule->bpfp.bf_insns, packet->pkt, packet->pktlen, packet->pktlen)) {
            moloch_rules_match(session, rule);
        }
    }
}
/******************************************************************************/
void moloch_rules_run_after_classify(MolochSession_t *session)
{
    int r;
    MolochRule_t *rule;
    for (r = 0; (rule = current.rules[MOLOCH_RULE_TYPE_AFTER_CLASSIFY][r]); r++) {
        if (rule->fieldsLen) {
            moloch_rules_check_rule_fields(session, rule, -1, NULL);
        }
    }
}
/******************************************************************************/
void moloch_rules_run_before_save(MolochSession_t *session, int final)
{
    int r;
    final = 1 << final;
    MolochRule_t *rule;
    for (r = 0; (rule = current.rules[MOLOCH_RULE_TYPE_BEFORE_SAVE][r]); r++) {
        if ((rule->saveFlags & final) == 0) {
            continue;
        }

        if (rule->fieldsLen) {
            moloch_rules_check_rule_fields(session, rule, -1, NULL);
        }
    }
}
/******************************************************************************/
void moloch_rules_session_create(MolochSession_t *session)
{
    switch (session->ipProtocol) {
    case IPPROTO_SCTP:
    case IPPROTO_TCP:
    case IPPROTO_UDP:
        if (config.fields[MOLOCH_FIELD_EXSPECIAL_SRC_PORT]->ruleEnabled)
            moloch_rules_run_field_set(session, MOLOCH_FIELD_EXSPECIAL_SRC_PORT, (gpointer)(long)session->port1);
        if (config.fields[MOLOCH_FIELD_EXSPECIAL_DST_PORT]->ruleEnabled)
            moloch_rules_run_field_set(session, MOLOCH_FIELD_EXSPECIAL_DST_PORT, (gpointer)(long)session->port2);
        // NO BREAK because TCP/UDP/SCTP have ip also
        // fall through
    case IPPROTO_ESP:
    case IPPROTO_ICMP:
    case IPPROTO_ICMPV6:
        if (config.fields[MOLOCH_FIELD_EXSPECIAL_SRC_IP]->ruleEnabled)
            moloch_rules_run_field_set(session, MOLOCH_FIELD_EXSPECIAL_SRC_IP, &session->addr1);
        if (config.fields[MOLOCH_FIELD_EXSPECIAL_DST_IP]->ruleEnabled)
            moloch_rules_run_field_set(session, MOLOCH_FIELD_EXSPECIAL_DST_IP, &session->addr2);
        break;
    }
}
/******************************************************************************/
void moloch_rules_stats()
{
    int t, r;
    int header = 0;

    for (t = 0; t < MOLOCH_RULE_TYPE_NUM; t++) {
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
void moloch_rules_init()
{
    rulesFiles = moloch_config_str_list(NULL, "rulesFiles", NULL);

    if (rulesFiles) {
        moloch_config_monitor_files("rules files", rulesFiles, moloch_rules_load);
    } else
        moloch_rules_load_complete();
}
/******************************************************************************/
void moloch_rules_exit()
{
    g_strfreev(rulesFiles);
}
