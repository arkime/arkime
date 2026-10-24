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


#define YAML_NODE_SEQUENCE_VALUE GINT_TO_POINTER(1)
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
    char                *bpf;                        // String version of bpf
    struct bpf_program  *bpfp[ARKIME_DLT_MAX];       // Per-DLT compiled bpf; atomic pointers, rebuilt on recompile, old defer-freed
    ArkimeFieldOps_t     ops;                        // Ops to run on match
    ArkimeRulesV2Prog_t *prog;                       // Compiled expression or fields:, NULL for bpf rules
    uint64_t             matched;                    // How many times was matched
    uint8_t              saveFlags;                  // When to save for beforeSave
    uint8_t              log;                        // should we log or not
    uint8_t              setRule;                    // This is a set rule type
} ArkimeRule_t;

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
    GPtrArray             *fieldsAny[ARKIME_FIELDS_MAX];   // expression rules to check on any new value

    GPtrArray             *rules[ARKIME_RULE_TYPE_NUM];
} ArkimeRulesInfo_t;

LOCAL ArkimeRulesInfo_t    current;
LOCAL ArkimeRulesInfo_t    loading;
LOCAL char               **rulesFiles;

extern ArkimeFileInfo_t    fileInfo[256];

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

LOCAL int srcPortField;
LOCAL int srcIpField;
LOCAL int dstPortField;
LOCAL int dstIpField;


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
    if (node->value != YAML_NODE_SEQUENCE_VALUE)
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

    if (!value || value == YAML_NODE_SEQUENCE_VALUE)
        node->value = value;
    else if (value[0] == '$' && value[1] == '{' && value[strlen(value) - 1] == '}') {
        value[strlen(value) - 1] = 0;
        char *config_value = arkime_config_str(NULL, value + 2, NULL);
        if (!config_value)
            CONFIGEXIT("Couldn't find config value %s", value + 2);
        g_free(value);
        node->value = config_value;
    } else {
        node->value = value;
    }

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
LOCAL YamlNode_t *arkime_rules_parser_parse_yaml(char *filename, YamlNode_t *parent, yaml_parser_t *parser, gboolean sequence)
{

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

        switch (event.type) {
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
        default:
            ;
        }
        yaml_event_delete(&event);
    }

    return parent;
}
/******************************************************************************/
LOCAL void arkime_rules_parser_print(YamlNode_t *node, int level)
{
    static const char indent[] = "                                                             ";
    if (node->value == YAML_NODE_SEQUENCE_VALUE) {
        printf("%.*s - %s\n", level, indent, node->key);
    } else if (node->value) {
        printf("%.*s %s: %s\n", level, indent, node->key, node->value);
    } else {
        printf("%.*s %s:\n", level, indent, node->key);
        for (int i = 0; i < (int)node->values->len; i++)
            arkime_rules_parser_print(g_ptr_array_index(node->values, i), level + 1);
    }
}
/******************************************************************************/
LOCAL YamlNode_t *arkime_rules_parser_get(YamlNode_t *node, const char *path)
{

    while (1) {
        const char *colon = strchr(path, ':');
        int   len;

        if (colon)
            len = colon - path;
        else
            len = strlen(path);

        if (!node->values)
            return NULL;

        int i;
        for (i = 0; i < (int)node->values->len; i++) {
            const char *key = ((YamlNode_t *)g_ptr_array_index(node->values, i))->key;
            if (strncmp(path, key, len) == 0 && key[len] == 0)
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
LOCAL char *arkime_rules_parser_get_value(YamlNode_t *parent, const char *path)
{
    const YamlNode_t *node = arkime_rules_parser_get(parent, path);
    if (!node)
        return NULL;
    return node->value;
}
/******************************************************************************/
LOCAL GPtrArray *arkime_rules_parser_get_values(YamlNode_t *parent, const char *path)
{
    const YamlNode_t *node = arkime_rules_parser_get(parent, path);
    if (!node)
        return NULL;
    return node->values;
}
/******************************************************************************/
/* fieldSet indexes, from a value to the rules to check when it is set */
LOCAL void arkime_rules_index_add(GHashTable **index, gboolean strKey, gpointer key, ArkimeRule_t *rule)
{
    if (!*index) {
        if (strKey)
            *index = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, arkime_rules_free_array);
        else
            *index = g_hash_table_new_full(NULL, NULL, NULL, arkime_rules_free_array);
    }

    GPtrArray *rules = g_hash_table_lookup(*index, key);
    if (!rules) {
        rules = g_ptr_array_new();
        g_hash_table_insert(*index, strKey ? g_strdup(key) : key, rules);
    }
    g_ptr_array_add(rules, rule);
}
/******************************************************************************/
LOCAL void arkime_rules_index_ip(int pos, gboolean v4, const char *key, ArkimeRule_t *rule)
{
    if (!loading.fieldsTree4[pos]) {
        loading.fieldsTree4[pos] = New_Patricia(32);
        loading.fieldsTree6[pos] = New_Patricia(128);
    }

    patricia_node_t *node = make_and_lookup(v4 ? loading.fieldsTree4[pos] : loading.fieldsTree6[pos], (char *)key);
    if (!node)
        return;
    if (!node->data)
        node->data = g_ptr_array_new();
    g_ptr_array_add(node->data, rule);
}
/******************************************************************************/
LOCAL uint8_t *arkime_rules_str_match_key(int type, const char *key)
{
    int len = strlen(key);
    if (len > 255)
        CONFIGEXIT("Match %s is too large", key);

    uint8_t *nkey = g_malloc(len + 3);
    nkey[0] = type;
    nkey[1] = len;
    memcpy(nkey + 2, key, len + 1);
    return nkey;
}
/******************************************************************************/
/* Add a value of a fields: rule. The rule is compiled to an expression, and
 * for fieldSet rules the value is indexed so setting it checks the rule. */
LOCAL void arkime_rules_load_add_field(ArkimeRule_t *rule, ArkimeRulesV2Fields_t *fields, int pos, int mtype, const char *key)
{
    config.fields[pos]->ruleEnabled = 1;
    arkime_rulesv2_fields_add(fields, pos, FALSE, mtype, key);

    if (mtype != 0) {
        if (rule->setRule) {
            uint8_t *nkey = arkime_rules_str_match_key(mtype, key);
            arkime_rules_index_add(&loading.fieldsMatch[pos], TRUE, nkey, rule);
            g_free(nkey);
        }
        return;
    }

    switch (config.fields[pos]->type) {
    case ARKIME_FIELD_TYPE_INT:
    case ARKIME_FIELD_TYPE_INT_ARRAY:
    case ARKIME_FIELD_TYPE_INT_ARRAY_UNIQUE:
    case ARKIME_FIELD_TYPE_INT_HASH:
    case ARKIME_FIELD_TYPE_INT_GHASH: {
        const char *dash = key[0] != '-' ? strchr(key, '-') : NULL;
        uint32_t min, max;
        if (dash) {
            min = arkime_atoin(key, dash - key);
            max = atoi(dash + 1);
            if (min > max)
                CONFIGEXIT("Min %u > Max %u not allowed", min, max);
        } else {
            min = max = atoi(key);
        }

        if (!rule->setRule)
            break;

        // Small ranges are indexed as values
        if (max - min < 20) {
            for (uint32_t n = min; n <= max && n >= min; n++)
                arkime_rules_index_add(&loading.fieldsHash[pos], FALSE, (void *)(long)n, rule);
        } else {
            ArkimeRuleIntMatch_t match;
            match.min = min;
            match.max = max;
            arkime_rules_index_add(&loading.fieldsMatch[pos], FALSE, (gpointer)match.num, rule);
        }
        break;
    }

    case ARKIME_FIELD_TYPE_FLOAT:
    case ARKIME_FIELD_TYPE_FLOAT_ARRAY:
    case ARKIME_FIELD_TYPE_FLOAT_GHASH:
        if (key[0] != '-' && strchr(key, '-') != 0)
            LOGEXIT("Range match not supported for float fields");

        if (rule->setRule) {
            float f = atof(key);
            uint32_t fint;
            memcpy(&fint, &f, 4);
            arkime_rules_index_add(&loading.fieldsHash[pos], FALSE, (gpointer)(long)fint, rule);
        }
        break;

    case ARKIME_FIELD_TYPE_IP:
    case ARKIME_FIELD_TYPE_IP_GHASH:
        if (strchr(key, '-') != 0)
            LOGEXIT("Range match not supported for ip fields");

        if (!rule->setRule)
            break;
        if (strcmp(key, "ipv4") == 0)
            arkime_rules_index_ip(pos, TRUE, "0.0.0.0/0", rule);
        else if (strcmp(key, "ipv6") == 0)
            arkime_rules_index_ip(pos, FALSE, "::/0", rule);
        else
            arkime_rules_index_ip(pos, strchr(key, '.') != 0, key, rule);
        break;

    case ARKIME_FIELD_TYPE_STR:
    case ARKIME_FIELD_TYPE_STR_ARRAY:
    case ARKIME_FIELD_TYPE_STR_HASH:
    case ARKIME_FIELD_TYPE_STR_GHASH:
        if (rule->setRule)
            arkime_rules_index_add(&loading.fieldsHash[pos], TRUE, (gpointer)key, rule);
        break;

    case ARKIME_FIELD_TYPE_OBJECT:
        // Unsupported
        break;
    }
}
/******************************************************************************/
// kinds is field pos to 1 for normal or 2 for ! fields, they can't be mixed
LOCAL void arkime_rules_parser_load_add_field_not(const char *filename, ArkimeRule_t *rule, ArkimeRulesV2Fields_t *fields, GHashTable *kinds, YamlNode_t *node)
{
    if (rule->setRule) {
        CONFIGEXIT("%s:'%s' NOT rules are not supported for fieldSet rules", filename, rule->name);
    }

    int pos = arkime_field_by_exp(node->key + 1);
    if (pos == -1)
        CONFIGEXIT("%s:'%s' Couldn't find field '%s'", filename, rule->name, node->key + 1);

    if (GPOINTER_TO_INT(g_hash_table_lookup(kinds, GINT_TO_POINTER(pos))) == 1)
        CONFIGEXIT("%s:'%s' Can't have both ! and normal fields: %s", filename, rule->name, node->key + 1);
    g_hash_table_insert(kinds, GINT_TO_POINTER(pos), GINT_TO_POINTER(2));

    switch (config.fields[pos]->type) {
    case ARKIME_FIELD_TYPE_STR:
    case ARKIME_FIELD_TYPE_STR_ARRAY:
    case ARKIME_FIELD_TYPE_STR_HASH:
    case ARKIME_FIELD_TYPE_STR_GHASH:
    case ARKIME_FIELD_TYPE_INT:
    case ARKIME_FIELD_TYPE_INT_ARRAY:
    case ARKIME_FIELD_TYPE_INT_HASH:
    case ARKIME_FIELD_TYPE_INT_GHASH:
        break;
    default:
        CONFIGEXIT("%s:'%s' Currently only string and integer fields support NOT rules %s", filename, rule->name, node->key + 1);
    }

    if (node->value) {
        arkime_rulesv2_fields_add(fields, pos, TRUE, ARKIME_RULESV2_FIELDS_EXACT, node->value);
    } else {
        for (int j = 0; j < (int)node->values->len; j++) {
            const YamlNode_t *fnode = g_ptr_array_index(node->values, j);
            arkime_rulesv2_fields_add(fields, pos, TRUE, ARKIME_RULESV2_FIELDS_EXACT, fnode->key);
        }
    }
}
/******************************************************************************/
/* Index a fieldSet expression rule under the values that can make it true,
 * in the same indexes as fields: rules plus fieldsAny for any new value. */
LOCAL void arkime_rules_load_triggers(ArkimeRule_t *rule)
{
    const ArkimeRulesV2Trigger_t *triggers;
    const int num = arkime_rulesv2_triggers(rule->prog, &triggers);

    for (int t = 0; t < num; t++) {
        const ArkimeRulesV2Trigger_t *trigger = &triggers[t];
        const int pos = trigger->field;
        config.fields[pos]->ruleEnabled = 1;

        switch (trigger->kind) {
        case ARKIME_RULESV2_TRIGGER_ANY:
            if (!loading.fieldsAny[pos])
                loading.fieldsAny[pos] = g_ptr_array_new();
            g_ptr_array_add(loading.fieldsAny[pos], rule);
            break;
        case ARKIME_RULESV2_TRIGGER_INT:
            arkime_rules_index_add(&loading.fieldsHash[pos], FALSE, (gpointer)(long)trigger->imin, rule);
            break;
        case ARKIME_RULESV2_TRIGGER_INT_RANGE: {
            ArkimeRuleIntMatch_t match;
            match.min = trigger->imin;
            match.max = trigger->imax;
            arkime_rules_index_add(&loading.fieldsMatch[pos], FALSE, (gpointer)match.num, rule);
            break;
        }
        case ARKIME_RULESV2_TRIGGER_FLOAT: {
            uint32_t fint;
            memcpy(&fint, &trigger->f, 4);
            arkime_rules_index_add(&loading.fieldsHash[pos], FALSE, (gpointer)(long)fint, rule);
            break;
        }
        case ARKIME_RULESV2_TRIGGER_STR:
            arkime_rules_index_add(&loading.fieldsHash[pos], TRUE, trigger->str, rule);
            break;
        case ARKIME_RULESV2_TRIGGER_HEAD:
        case ARKIME_RULESV2_TRIGGER_TAIL:
        case ARKIME_RULESV2_TRIGGER_CONTAINS: {
            const int type = trigger->kind == ARKIME_RULESV2_TRIGGER_HEAD ? ARKIME_RULES_STR_MATCH_HEAD :
                             trigger->kind == ARKIME_RULESV2_TRIGGER_TAIL ? ARKIME_RULES_STR_MATCH_TAIL : ARKIME_RULES_STR_MATCH_CONTAINS;
            uint8_t *nkey = arkime_rules_str_match_key(type, trigger->str);
            arkime_rules_index_add(&loading.fieldsMatch[pos], TRUE, nkey, rule);
            g_free(nkey);
            break;
        }
        case ARKIME_RULESV2_TRIGGER_CIDR:
            arkime_rules_index_ip(pos, trigger->family == 4, trigger->str, rule);
            break;
        }
    }
}
/******************************************************************************/
LOCAL void arkime_rules_parser_load_rule(char *filename, YamlNode_t *parent)
{
    const char *name = arkime_rules_parser_get_value(parent, "name");
    if (!name)
        CONFIGEXIT("%s: name required for rule", filename);

    const char *when = arkime_rules_parser_get_value(parent, "when");
    if (!when)
        CONFIGEXIT("%s: when required for rule '%s'", filename, name);

    const char *bpf = arkime_rules_parser_get_value(parent, "bpf");
    GPtrArray *fields = arkime_rules_parser_get_values(parent, "fields");
    const char *expression = arkime_rules_parser_get_value(parent, "expression");

    if (!bpf && !fields && !expression)
        CONFIGEXIT("%s: bpf, fields, or expressions required for rule '%s'", filename, name);

    if ((bpf && fields) || (bpf && expression) || (fields && expression))
        CONFIGEXIT("%s: Only one of bpf, fields, or expressions can be set for rule '%s'", filename, name);

    GPtrArray  *ops = arkime_rules_parser_get_values(parent, "ops");
    if (!ops)
        CONFIGEXIT("%s: ops required for rule '%s'", filename, name);

    char *log = arkime_rules_parser_get_value(parent, "log");

    int type = 0;
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


    ArkimeRule_t *rule = ARKIME_TYPE_ALLOC0(ArkimeRule_t);
    g_ptr_array_add(loading.rules[type], rule);
    rule->name = g_strdup(name);
    rule->filename = filename;
    rule->saveFlags = saveFlags;
    rule->log = log && strcasecmp(log, "true") == 0;
    rule->setRule = type == ARKIME_RULE_TYPE_FIELD_SET;
    if (bpf)
        rule->bpf = g_strdup(bpf);
    if (expression) {
        rule->prog = arkime_rulesv2_compile(filename, rule->name, type, expression);
        if (rule->setRule)
            arkime_rules_load_triggers(rule);
    }

    if (fields) {
        ArkimeRulesV2Fields_t *builder = arkime_rulesv2_fields_new();
        GHashTable *kinds = g_hash_table_new(NULL, NULL);

        for (int i = 0; i < (int)fields->len; i++) {
            YamlNode_t *node = g_ptr_array_index(fields, i);
            int mtype = 0;

            if (node->key[0] == '!') {
                arkime_rules_parser_load_add_field_not(filename, rule, builder, kinds, node);
                continue;
            }

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

            if (GPOINTER_TO_INT(g_hash_table_lookup(kinds, GINT_TO_POINTER(pos))) == 2)
                CONFIGEXIT("%s: Can't have both ! and normal fields: %s", filename, node->key);
            g_hash_table_insert(kinds, GINT_TO_POINTER(pos), GINT_TO_POINTER(1));

            const ArkimeFieldType ftype = config.fields[pos]->type;
            if (ftype == ARKIME_FIELD_TYPE_OBJECT)
                CONFIGEXIT("%s: Currently don't support any generic object fields", filename);
            if (mtype != 0 && !ARKIME_FIELD_TYPE_IS_STR(ftype))
                CONFIGEXIT("Rule field %s doesn't support modifier %s", node->key, comma);

            if (node->value) {
                arkime_rules_load_add_field(rule, builder, pos, mtype, node->value);
            } else {
                for (int j = 0; j < (int)node->values->len; j++) {
                    const YamlNode_t *fnode = g_ptr_array_index(node->values, j);
                    arkime_rules_load_add_field(rule, builder, pos, mtype, fnode->key);
                }
            }
        }

        g_hash_table_destroy(kinds);
        rule->prog = arkime_rulesv2_fields_compile(builder, filename, rule->name, type);
    }

    arkime_field_ops_init(&rule->ops, ops->len, ARKIME_FIELD_OPS_FLAGS_COPY);
    for (int i = 0; i < (int)ops->len; i++) {
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
    const char *str;
    GPtrArray  *rules;

    str = arkime_rules_parser_get_value(parent, "version");
    if (!str || strcmp(str, "1") != 0) {
        CONFIGEXIT("%s: Missing version: 1", filename);
    }

    rules = arkime_rules_parser_get_values(parent, "rules");
    if (!rules) {
        CONFIGEXIT("%s: Missing rules", filename);
    }

    for (int i = 0; i < (int)rules->len; i++) {
        arkime_rules_parser_load_rule(filename, g_ptr_array_index(rules, i));
    }
}
/******************************************************************************/
LOCAL void arkime_rules_load_complete()
{
    char      **bpfs;
    GRegex     *regex = g_regex_new(":\\s*(\\d+)\\s*$", 0, 0, 0);

    for (int t = 0; t < ARKIME_RULE_TYPE_NUM; t++) {
        if (!loading.rules[t])
            loading.rules[t] = g_ptr_array_new();
    }

    bpfs = arkime_config_str_list(NULL, "dontSaveBPFs", NULL);
    int pos = arkime_field_by_exp("_maxPacketsToSave");
    gint start_pos;
    if (bpfs) {
        for (int i = 0; bpfs[i]; i++) {
            ArkimeRule_t *rule = ARKIME_TYPE_ALLOC0(ArkimeRule_t);
            g_ptr_array_add(loading.rules[ARKIME_RULE_TYPE_SESSION_SETUP], rule);
            rule->filename = "dontSaveBPFs";
            arkime_field_ops_init(&rule->ops, 1, ARKIME_FIELD_OPS_FLAGS_COPY);

            GMatchInfo *match_info = 0;
            g_regex_match(regex, bpfs[i], 0, &match_info);
            if (g_match_info_matches(match_info)) {
                g_match_info_fetch_pos(match_info, 0, &start_pos, NULL);
                rule->bpf = g_strndup(bpfs[i], start_pos);
                char *fetched = g_match_info_fetch(match_info, 1);
                arkime_field_ops_add(&rule->ops, pos, fetched, -1);
                g_free(fetched);
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
        for (int i = 0; bpfs[i]; i++) {
            ArkimeRule_t *rule = ARKIME_TYPE_ALLOC0(ArkimeRule_t);
            g_ptr_array_add(loading.rules[ARKIME_RULE_TYPE_SESSION_SETUP], rule);
            rule->filename = "minPacketsSaveBPFs";
            arkime_field_ops_init(&rule->ops, 1, ARKIME_FIELD_OPS_FLAGS_COPY);

            GMatchInfo *match_info = 0;
            g_regex_match(regex, bpfs[i], 0, &match_info);
            if (g_match_info_matches(match_info)) {
                g_match_info_fetch_pos(match_info, 0, &start_pos, NULL);
                rule->bpf = g_strndup(bpfs[i], start_pos);
                char *fetched = g_match_info_fetch(match_info, 1);
                arkime_field_ops_add(&rule->ops, pos, fetched, -1);
                g_free(fetched);
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

    // Precompile each rule's bpf for every supported DLT now that current is set.
    arkime_rules_recompile();
}
/******************************************************************************/
void arkime_rules_bpfp_free(gpointer data)
{
    struct bpf_program *bpfp = data;
    if (bpfp->bf_len)
        pcap_freecode(bpfp);
    g_free(bpfp);
}
/******************************************************************************/
LOCAL void arkime_rules_free(ArkimeRulesInfo_t *freeing)
{
    for (int i = 0; i < ARKIME_FIELDS_MAX; i++) {
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
        if (freeing->fieldsAny[i]) {
            g_ptr_array_free(freeing->fieldsAny[i], TRUE);
        }
    }

    for (int t = 0; t < ARKIME_RULE_TYPE_NUM; t++) {
        if (!freeing->rules[t])
            continue;
        for (guint r = 0; r < freeing->rules[t]->len; r++) {
            ArkimeRule_t *rule = g_ptr_array_index(freeing->rules[t], r);

            g_free(rule->name);
            if (rule->bpf)
                g_free(rule->bpf);
            for (int d = 0; d < ARKIME_DLT_MAX; d++) {
                if (rule->bpfp[d])
                    arkime_rules_bpfp_free(rule->bpfp[d]);
            }

            arkime_field_ops_free(&rule->ops);
            arkime_rulesv2_prog_free(rule->prog);
            ARKIME_TYPE_FREE(ArkimeRule_t, rule);
        }
        g_ptr_array_free(freeing->rules[t], TRUE);
    }

    ARKIME_TYPE_FREE(ArkimeRulesInfo_t, freeing);
}
/******************************************************************************/
LOCAL void arkime_rules_load(char **names)
{
    // Make a copy of current items to free later

    ArkimeRulesInfo_t *freeing = ARKIME_TYPE_ALLOC0(ArkimeRulesInfo_t);
    memcpy(freeing, &current, sizeof(current));

    for (int t = 0; t < ARKIME_RULE_TYPE_NUM; t++) {
        if (!loading.rules[t])
            loading.rules[t] = g_ptr_array_new();
    }

    // Load all the rule files
    for (int i = 0; names[i]; i++) {
        yaml_parser_t parser;
        yaml_parser_initialize(&parser);
        FILE *input = fopen(names[i], "rb");
        if (!input)
            CONFIGEXIT("cannot open rules file %s", names[i]);

        yaml_parser_set_input_file(&parser, input);
        YamlNode_t *parent = arkime_rules_parser_parse_yaml(names[i], NULL, &parser, FALSE);
        yaml_parser_delete(&parser);
        fclose(input);
        if (!parent) {
            LOG("WARNING - %s has no rules", names[i]);
            continue;
        }
        if (config.debug > 1) {
            arkime_rules_parser_print(parent, 0);
        }
        arkime_rules_parser_load_file(names[i], parent);
        arkime_rules_parser_free_node(parent);
    }

    // Part 2, which will also copy loading to current
    arkime_rules_load_complete();

    // Now schedule free of current items
    arkime_free_later(freeing, (GDestroyNotify) arkime_rules_free);
}
/******************************************************************************/
/* Precompile a filter for every DLT Arkime supports, indexed by the supported-DLT
 * index, so packets pick theirs at match time and no per-file recompile is
 * needed. Packet threads may be reading bpfp[] so never mutate a live program,
 * build fresh ones, atomically swap each pointer, and let arkime_free_later
 * release the old ones after readers have quiesced. */
void arkime_rules_bpf_compile(const char *filename, const char *bpf, struct bpf_program **bpfp)
{
    const int snaplen = config.snapLen > 0 ? config.snapLen : ARKIME_PACKET_MAX_LEN;
    const int dltCount = arkime_packet_dlt_index_count();

    int compiled = 0;
    int attempted = 0;
    for (int d = 0; d < dltCount; d++) {
        const int dlt = arkime_packet_index_to_dlt(d);

        // BPF doesn't apply to NFLOG, leave an empty program so the
        // rule simply never matches NFLOG packets.
        struct bpf_program *newbpfp = g_new0(struct bpf_program, 1);
        if (dlt != DLT_NFLOG) {
            attempted++;
            pcap_t *deadPcap = pcap_open_dead(dlt, snaplen);
            if (pcap_compile(deadPcap, newbpfp, bpf, 1, PCAP_NETMASK_UNKNOWN) == -1) {
                // The filter may be valid for some DLTs but not others
                // (e.g. "ether ..." on DLT_RAW); only treat it as fatal
                // below if it fails to compile for every DLT.
                newbpfp->bf_len = 0;
                newbpfp->bf_insns = NULL;
            } else {
                compiled++;
            }
            pcap_close(deadPcap);
        }

        struct bpf_program *oldbpfp = bpfp[d];
        ARKIME_THREAD_ATOMIC_STORE(bpfp[d], newbpfp);
        if (oldbpfp)
            arkime_free_later(oldbpfp, arkime_rules_bpfp_free);
    }

    if (attempted > 0 && compiled == 0 && !config.ignoreErrors) {
        pcap_t *deadPcap = pcap_open_dead(DLT_EN10MB, snaplen);
        CONFIGEXIT("Couldn't compile bpf filter %s: '%s' with %s", filename, bpf, pcap_geterr(deadPcap));
    }
}
/******************************************************************************/
/* Called on the main thread once rules are (re)loaded. */
void arkime_rules_recompile()
{
    for (int t = 0; t < ARKIME_RULE_TYPE_NUM; t++) {
        if (!current.rules[t])
            continue;
        for (guint r = 0; r < current.rules[t]->len; r++) {
            ArkimeRule_t *rule = g_ptr_array_index(current.rules[t], r);
            if (rule->bpf)
                arkime_rules_bpf_compile(rule->filename, rule->bpf, rule->bpfp);
            if (rule->prog)
                arkime_rulesv2_recompile(rule->prog, rule->filename);
        }
    }
}
/******************************************************************************/
// For fieldSet, triggerField was just set to triggerValue, else -1
LOCAL void arkime_rules_match(ArkimeSession_t *const session, ArkimeRule_t *const rule, ArkimePacket_t *packet, int triggerField, const void *triggerValue)
{
    if (rule->log) {
        char ipStr[200];

        arkime_session_pretty_string(session, ipStr, sizeof(ipStr));

        if (rule->prog) {
            GString *explain = g_string_new(NULL);
            arkime_rulesv2_explain(rule->prog, session, packet, triggerField, triggerValue, explain);
            if (explain->len > 0)
                LOG("%s - %s - %s", rule->name, ipStr, explain->str);
            g_string_free(explain, TRUE);
        }
    }
    ARKIME_THREAD_INCR(rule->matched);
    arkime_field_ops_run(session, &rule->ops);
}
/******************************************************************************/
// One field being set, and the rules already checked for it
typedef struct {
    const void   *value;
    ArkimeRule_t *rules[32];
    int           num;
    GHashTable   *more;     // once rules is full
} ArkimeRulesSetEvent_t;

// A rule can be indexed under several keys that match the same value, returns if it was already checked
LOCAL gboolean arkime_rules_set_event_seen(ArkimeRulesSetEvent_t *event, ArkimeRule_t *rule)
{
    for (int s = 0; s < event->num; s++) {
        if (event->rules[s] == rule)
            return TRUE;
    }
    if (event->num < (int)G_N_ELEMENTS(event->rules)) {
        event->rules[event->num++] = rule;
        return FALSE;
    }
    if (!event->more)
        event->more = g_hash_table_new(NULL, NULL);
    return !g_hash_table_add(event->more, rule);
}
/******************************************************************************/
LOCAL void arkime_rules_run_field_set_rules(ArkimeSession_t *session, int pos, GPtrArray *rules, ArkimeRulesSetEvent_t *event)
{
    for (int r = 0; r < (int)rules->len; r++) {
        ArkimeRule_t *rule = g_ptr_array_index(rules, r);
        if (arkime_rules_set_event_seen(event, rule))
            continue;

        if (arkime_rulesv2_eval_set(rule->prog, session, pos, event->value))
            arkime_rules_match(session, rule, NULL, pos, event->value);
    }
}
/******************************************************************************/
void arkime_rules_run_field_set(ArkimeSession_t *session, int pos, const gpointer value)
{
    ArkimeRulesSetEvent_t event;
    event.value = value;
    event.num = 0;
    event.more = NULL;

    if (ARKIME_FIELD_TYPE_IS_IP(config.fields[pos]->type)) {

        patricia_node_t *nodes[PATRICIA_MAXBITS + 1];

        int cnt = 0;
        if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)value)) {
            if (current.fieldsTree4[pos])
                cnt = patricia_search_all2(current.fieldsTree4[pos], ((u_char *)value) + 12, 32, nodes, PATRICIA_MAXBITS + 1);
        } else {
            if (current.fieldsTree6[pos])
                cnt = patricia_search_all2(current.fieldsTree6[pos], (u_char *)value, 128, nodes, PATRICIA_MAXBITS + 1);
        }

        // These are all the possible rules that match
        for (int i = 0; i < cnt; i++) {
            arkime_rules_run_field_set_rules(session, pos, nodes[i]->data, &event);
        }
    } else {
        GPtrArray *rules;

        // See if this value matches anything in our matching list
        if (current.fieldsMatch[pos]) {
            GHashTableIter         iter;

            g_hash_table_iter_init(&iter, current.fieldsMatch[pos]);
            if (ARKIME_FIELD_TYPE_IS_INT(config.fields[pos]->type)) {
                uint64_t               num;

                while (g_hash_table_iter_next(&iter, (gpointer *)&num, (gpointer *)&rules)) {
                    ArkimeRuleIntMatch_t match;
                    match.num = num;
                    uint32_t test = (uint32_t)(long)value;
                    if (test >= match.min && test <= match.max) {
                        arkime_rules_run_field_set_rules(session, pos, rules, &event);
                    }
                }
            } else {
                uint8_t               *akey;
                int                    len = strlen(value);

                while (g_hash_table_iter_next(&iter, (gpointer *)&akey, (gpointer *)&rules)) {
                    if (len < akey[1])
                        continue;

                    switch (akey[0]) {
                    case ARKIME_RULES_STR_MATCH_TAIL:
                        if (memcmp(akey + 2, (char *)value + len - akey[1], akey[1]) == 0)
                            arkime_rules_run_field_set_rules(session, pos, rules, &event);
                        break;
                    case ARKIME_RULES_STR_MATCH_HEAD:
                        if (memcmp(akey + 2, (char *)value, akey[1]) == 0)
                            arkime_rules_run_field_set_rules(session, pos, rules, &event);
                        break;
                    case ARKIME_RULES_STR_MATCH_CONTAINS:
                        if (arkime_memstr((char *)value, len, (char *)akey + 2, akey[1]) != 0)
                            arkime_rules_run_field_set_rules(session, pos, rules, &event);
                    }
                }
            }
        }

        // See if this value is in the hash table of values we are watching for
        if (current.fieldsHash[pos]) {
            rules = g_hash_table_lookup(current.fieldsHash[pos], value);
            if (rules)
                arkime_rules_run_field_set_rules(session, pos, rules, &event);
        }
    }

    if (current.fieldsAny[pos])
        arkime_rules_run_field_set_rules(session, pos, current.fieldsAny[pos], &event);

    if (event.more)
        g_hash_table_destroy(event.more);
}
/******************************************************************************/
void arkime_rules_run_session_setup(ArkimeSession_t *session, ArkimePacket_t *packet)
{
    for (guint r = 0; r < current.rules[ARKIME_RULE_TYPE_SESSION_SETUP]->len; r++) {
        ArkimeRule_t *rule = g_ptr_array_index(current.rules[ARKIME_RULE_TYPE_SESSION_SETUP], r);
        if (rule->prog) {
            if (arkime_rulesv2_eval(rule->prog, session, packet))
                arkime_rules_match(session, rule, packet, -1, NULL);
        } else if (rule->bpf) {
            const int dltIndex = arkime_packet_dlt_index(packet);
            if (dltIndex < 0)
                continue;
            struct bpf_program *bpfp = ARKIME_THREAD_ATOMIC_LOAD(rule->bpfp[dltIndex]);
            if (bpfp && bpfp->bf_len && bpf_filter(bpfp->bf_insns, packet->pkt, packet->pktlen, packet->pktlen)) {
                arkime_rules_match(session, rule, packet, -1, NULL);
            }
        }
    }
}
/******************************************************************************/
void arkime_rules_run_after_classify(ArkimeSession_t *session)
{
    for (guint r = 0; r < current.rules[ARKIME_RULE_TYPE_AFTER_CLASSIFY]->len; r++) {
        ArkimeRule_t *rule = g_ptr_array_index(current.rules[ARKIME_RULE_TYPE_AFTER_CLASSIFY], r);
        if (rule->prog && arkime_rulesv2_eval(rule->prog, session, NULL))
            arkime_rules_match(session, rule, NULL, -1, NULL);
    }
}
/******************************************************************************/
void arkime_rules_run_before_save(ArkimeSession_t *session, int final)
{
    final = 1 << final;
    for (guint r = 0; r < current.rules[ARKIME_RULE_TYPE_BEFORE_SAVE]->len; r++) {
        ArkimeRule_t *rule = g_ptr_array_index(current.rules[ARKIME_RULE_TYPE_BEFORE_SAVE], r);
        if ((rule->saveFlags & final) == 0) {
            continue;
        }

        if (rule->prog && arkime_rulesv2_eval(rule->prog, session, NULL))
            arkime_rules_match(session, rule, NULL, -1, NULL);
    }
}
/******************************************************************************/
void arkime_rules_session_create(ArkimeSession_t *session)
{
    switch (session->ipProtocol) {
    case IPPROTO_SCTP:
    case IPPROTO_TCP:
    case IPPROTO_UDP:
        if (config.fields[srcPortField]->ruleEnabled)
            arkime_rules_run_field_set(session, srcPortField, (gpointer)(long)session->port1);
        if (config.fields[dstPortField]->ruleEnabled)
            arkime_rules_run_field_set(session, dstPortField, (gpointer)(long)session->port2);
    // NO BREAK because TCP/UDP/SCTP have ip also
    // fall through
    case IPPROTO_ESP:
    case IPPROTO_ICMP:
    case IPPROTO_ICMPV6:
        if (config.fields[srcIpField]->ruleEnabled)
            arkime_rules_run_field_set(session, srcIpField, &session->addr1);
        if (config.fields[dstIpField]->ruleEnabled)
            arkime_rules_run_field_set(session, dstIpField, &session->addr2);
        break;
    }
}
/******************************************************************************/
void arkime_rules_stats()
{
    int header = 0;

    for (int t = 0; t < ARKIME_RULE_TYPE_NUM; t++) {
        for (guint r = 0; r < current.rules[t]->len; r++) {
            const ArkimeRule_t *rule = g_ptr_array_index(current.rules[t], r);
            if (rule->matched) {
                if (!header) {
                    printf("%-35s %-30s %s\n", "File", "Rule", "Matched");
                    header = 1;
                }
                printf("%-35s %-30s %" PRIu64 "\n",
                       rule->filename,
                       rule->name,
                       rule->matched);
            }
        }
    }
}
/******************************************************************************/
void arkime_rules_init()
{
    rulesFiles = arkime_config_str_list(NULL, "rulesFiles", NULL);

    srcPortField = arkime_field_by_exp("port.src");
    srcIpField = arkime_field_by_exp("ip.src");
    dstPortField = arkime_field_by_exp("port.dst");
    dstIpField = arkime_field_by_exp("ip.dst");

    // arkime_rules_session_create runs fieldSet for these
    arkime_field_internal_sets_rules(srcPortField);
    arkime_field_internal_sets_rules(srcIpField);
    arkime_field_internal_sets_rules(dstPortField);
    arkime_field_internal_sets_rules(dstIpField);

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
