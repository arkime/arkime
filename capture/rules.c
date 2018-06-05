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

typedef struct {
    uint8_t             *fields;
    char                *filename;
    char                *bpf;
    struct bpf_program   bpfp;
    GHashTable          *hash[MOLOCH_FIELDS_MAX];
    patricia_tree_t     *tree4[MOLOCH_FIELDS_MAX];
    patricia_tree_t     *tree6[MOLOCH_FIELDS_MAX];
    MolochFieldOps_t     ops;
    int                  fieldsLen;
    int                  saveFlags;
} MolochRule_t;

#define MOLOCH_RULES_MAX     100

// Has all possible values to array of rules

typedef struct {
    GHashTable            *fieldsHash[MOLOCH_FIELDS_MAX];
    patricia_tree_t       *fieldsTree4[MOLOCH_FIELDS_MAX];
    patricia_tree_t       *fieldsTree6[MOLOCH_FIELDS_MAX];

    int                    rulesLen[MOLOCH_RULE_TYPE_NUM];
    MolochRule_t          *rules[MOLOCH_RULE_TYPE_NUM][MOLOCH_RULES_MAX+1];
} MolochRulesInfo_t;

LOCAL MolochRulesInfo_t current;
LOCAL MolochRulesInfo_t loading;
LOCAL MolochRulesInfo_t freeing;

LOCAL pcap_t                *deadPcap;
extern MolochPcapFileHdr_t   pcapFileHeader;
/******************************************************************************/
void moloch_rules_free_node(YamlNode_t *node)
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
YamlNode_t *moloch_rules_add_node(YamlNode_t *parent, char *key, char *value)
{
    YamlNode_t *node = MOLOCH_TYPE_ALLOC(YamlNode_t);
    node->key = key;
    node->value = value;

    if (value) {
        node->values = NULL;
    } else {
        node->values = g_ptr_array_new_with_free_func((GDestroyNotify)moloch_rules_free_node);
    }

    if (parent) {
        if (!key) {
            char str[10];
            sprintf(str, "%d", parent->values->len);
            node->key = g_strdup(str);
        }
        g_ptr_array_add(parent->values, node);
    }

    return node;
}
/******************************************************************************/
YamlNode_t *moloch_rules_parse_yaml(char *filename, YamlNode_t *parent, yaml_parser_t *parser, gboolean sequence) {

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
                moloch_rules_add_node(parent, g_strdup((gchar *)event.data.scalar.value), YAML_NODE_SEQUENCE_VALUE);
            } else if (key) {
                moloch_rules_add_node(parent, key, g_strdup((gchar *)event.data.scalar.value));
                key = NULL;
            } else {
                key = g_strdup((gchar *)event.data.scalar.value);
            }
            break;
        case YAML_SEQUENCE_START_EVENT:
        case YAML_MAPPING_START_EVENT:
            if (parent == NULL) {
                parent = node = moloch_rules_add_node(NULL, g_strdup("root"), NULL);
            } else {
                node = moloch_rules_add_node(parent, key, NULL);
            }
            key = NULL;
            if (moloch_rules_parse_yaml(filename, node, parser, event.type == YAML_SEQUENCE_START_EVENT) == NULL)
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
void moloch_rules_parse_print(YamlNode_t *node, int level)
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
            moloch_rules_parse_print(g_ptr_array_index(node->values, i), level+1);
    }
}
/******************************************************************************/
YamlNode_t *moloch_rules_get(YamlNode_t *node, char *path)
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
char *moloch_rules_get_value(YamlNode_t *parent, char *path)
{
    YamlNode_t *node = moloch_rules_get(parent, path);
    if (!node)
        return NULL;
    return node->value;
}
/******************************************************************************/
GPtrArray *moloch_rules_get_values(YamlNode_t *parent, char *path)
{
    YamlNode_t *node = moloch_rules_get(parent, path);
    if (!node)
        return NULL;
    return node->values;
}
/******************************************************************************/
void moloch_rules_free_array(gpointer data)
{
    g_ptr_array_free(data, TRUE);
}
/******************************************************************************/
void moloch_rules_load_add_field(MolochRule_t *rule, int pos, char *key)
{
    uint32_t         n;
    char            *key2;
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

    case MOLOCH_FIELD_TYPE_IP:
    case MOLOCH_FIELD_TYPE_IP_GHASH:
        if (!loading.fieldsTree4[pos]) {
            loading.fieldsTree4[pos] = New_Patricia(32);
            loading.fieldsTree6[pos] = New_Patricia(128);
        }

        if (strchr(key, '.') != 0) {
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
            loading.fieldsHash[pos] = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, moloch_rules_free_array);

        key2 = g_strdup(key);
        if (!g_hash_table_add(rule->hash[pos], key2))
            g_free(key2);

        rules = g_hash_table_lookup(loading.fieldsHash[pos], key);
        if (!rules) {
            rules = g_ptr_array_new();
            g_hash_table_insert(loading.fieldsHash[pos], g_strdup(key), rules);
        }
        g_ptr_array_add(rules, rule);
        break;
    }
}
/******************************************************************************/
void moloch_rules_load_rule(char *filename, YamlNode_t *parent)
{
    char *name = moloch_rules_get_value(parent, "name");
    if (!name)
        LOGEXIT("%s: name required for rule", filename);

    char *when = moloch_rules_get_value(parent, "when");
    if (!when)
        LOGEXIT("%s: when required for rule '%s'", filename, name);

    char *bpf = moloch_rules_get_value(parent, "bpf");
    GPtrArray *fields = moloch_rules_get_values(parent, "fields");
    char *expression = moloch_rules_get_value(parent, "expression");

    if (!bpf && !fields && !expression)
        LOGEXIT("%s: bpf, fields, or expressions required for rule '%s'", filename, name);

    if ((bpf && fields) || (bpf && expression) || (fields && expression))
        LOGEXIT("%s: Only one of bpf, fields, or expressions can be set for rule '%s'", filename, name);

    GPtrArray  *ops = moloch_rules_get_values(parent, "ops");
    if (!ops)
        LOGEXIT("%s: ops required for rule '%s'", filename, name);

    if (expression) {
        LOGEXIT("Currently don't support expression, hopefully soon!");
    }


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
        saveFlags = 1;
        if (bpf)
            LOGEXIT("%s: %s doesn't support bpf", filename, when);
    } else if (strcmp(when, "beforeFinalSave") == 0) {
        type = MOLOCH_RULE_TYPE_BEFORE_SAVE;
        saveFlags = 2;
        if (bpf)
            LOGEXIT("%s: %s doesn't support bpf", filename, when);
    } else if (strcmp(when, "beforeBothSave") == 0) {
        type = MOLOCH_RULE_TYPE_BEFORE_SAVE;
        saveFlags = 3;
        if (bpf)
            LOGEXIT("%s: %s doesn't support bpf", filename, when);
    } else {
        LOGEXIT("%s: Unknown when '%s'", filename, when);
    }

    if (loading.rulesLen[type] >= MOLOCH_RULES_MAX)
        LOGEXIT("Too many %s rules", when);

    int n = loading.rulesLen[type]++;
    MolochRule_t *rule = loading.rules[type][n] = MOLOCH_TYPE_ALLOC0(MolochRule_t);
    rule->filename = filename;
    rule->saveFlags = saveFlags;
    if (bpf)
        rule->bpf = g_strdup(bpf);

    if (fields) {
        int i;
        rule->fields = malloc((int)fields->len);
        for (i = 0; i < (int)fields->len; i++) {
            YamlNode_t *node = g_ptr_array_index(fields, i);
            int pos = moloch_field_by_exp(node->key);
            if (pos == -1)
                LOGEXIT("%s Couldn't find field '%s'", filename, node->key);
            rule->fields[(int)rule->fieldsLen++] = pos;

            switch (config.fields[pos]->type) {
            case MOLOCH_FIELD_TYPE_INT:
            case MOLOCH_FIELD_TYPE_INT_ARRAY:
            case MOLOCH_FIELD_TYPE_INT_HASH:
            case MOLOCH_FIELD_TYPE_INT_GHASH:
                rule->hash[pos] = g_hash_table_new_full(NULL, NULL, NULL, NULL);
                break;

            case MOLOCH_FIELD_TYPE_IP:
            case MOLOCH_FIELD_TYPE_IP_GHASH:
                rule->tree4[pos] = New_Patricia(32);
                rule->tree6[pos] = New_Patricia(128);
                break;

            case MOLOCH_FIELD_TYPE_STR:
            case MOLOCH_FIELD_TYPE_STR_ARRAY:
            case MOLOCH_FIELD_TYPE_STR_HASH:
            case MOLOCH_FIELD_TYPE_STR_GHASH:
                rule->hash[pos] = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
                break;

            case MOLOCH_FIELD_TYPE_CERTSINFO:
                LOGEXIT("%s: Currently don't support any certs fields", filename);
            }

            if (node->value)
                moloch_rules_load_add_field(rule, pos, node->value);
            else {
                int j;
                for (j = 0; j < (int)node->values->len; j++) {
                    YamlNode_t *fnode = g_ptr_array_index(node->values, j);
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
void moloch_rules_load_file(char *filename, YamlNode_t *parent)
{
    char       *str;
    GPtrArray  *rules;

    str = moloch_rules_get_value(parent, "version");
    if (!str || strcmp(str, "1") != 0) {
        LOGEXIT("%s: Missing version: 1", filename);
    }

    rules = moloch_rules_get_values(parent, "rules");
    if (!rules) {
        LOGEXIT("%s: Missing rules", filename);
    }

    int i;
    for (i = 0; i < (int)rules->len; i++) {
        moloch_rules_load_rule(filename, g_ptr_array_index(rules, i));
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
    }
    g_regex_unref(regex);

    memcpy(&current, &loading, sizeof(loading));
    memset(&loading, 0, sizeof(loading));
}
/******************************************************************************/
void moloch_rules_load(char **names)
{
    int    i, t, r;

    if (!names) {
        for (i = 0; i < MOLOCH_FIELDS_MAX; i++) {
            if (freeing.fieldsHash[i]) {
                g_hash_table_destroy(freeing.fieldsHash[i]);
            }
            if (freeing.fieldsTree4[i]) {
                Destroy_Patricia(freeing.fieldsTree4[i], moloch_rules_free_array);
            }
            if (freeing.fieldsTree6[i]) {
                Destroy_Patricia(freeing.fieldsTree6[i], moloch_rules_free_array);
            }
        }

        for (t = 0; t < MOLOCH_RULE_TYPE_NUM; t++) {
            for (r = 0; r < freeing.rulesLen[t]; r++) {
                MolochRule_t *rule = freeing.rules[t][r];

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
                }

                moloch_field_ops_free(&rule->ops);
                MOLOCH_TYPE_FREE(MolochRule_t, rule);
            }
        }

        memset(&freeing, 0, sizeof(loading));
        return;
    }

    // Make a copy of current items to free later
    memcpy(&freeing, &current, sizeof(loading));

    // Load all the rule files
    for (i = 0; names[i]; i++) {
        yaml_parser_t parser;
        yaml_parser_initialize(&parser);
        FILE *input = fopen(names[i], "rb");
        if (!input)
            LOGEXIT("ERROR - can not open rules file %s", names[i]);

        yaml_parser_set_input_file(&parser, input);
        YamlNode_t *parent = moloch_rules_parse_yaml(names[i], NULL, &parser, FALSE);
        yaml_parser_delete(&parser);
        if (!parent) {
            LOG("WARNING %s - has no rules", names[i]);
            continue;
        }
#ifdef RULES_DEBUG
        moloch_rules_parse_print(parent, 0);
#endif
        moloch_rules_load_file(names[i], parent);
        moloch_rules_free_node(parent);
        fclose(input);
    }

    // Part 2, which will also copy loading to current
    moloch_rules_load_complete();
}
/******************************************************************************/
/* Called at the start on main thread or each time a new file is open on single thread */
void moloch_rules_recompile()
{
    int t, r;

    if (deadPcap)
        pcap_close(deadPcap);

    deadPcap = pcap_open_dead(pcapFileHeader.linktype, pcapFileHeader.snaplen);
    MolochRule_t *rule;
    for (t = 0; t < MOLOCH_RULE_TYPE_NUM; t++) {
        for (r = 0; (rule = current.rules[t][r]); r++) {
            if (!rule->bpf)
                continue;

            pcap_freecode(&rule->bpfp);
            if (pcapFileHeader.linktype != 239) {
                if (pcap_compile(deadPcap, &rule->bpfp, rule->bpf, 1, PCAP_NETMASK_UNKNOWN) == -1) {
                    LOGEXIT("ERROR - Couldn't compile filter %s: '%s' with %s", rule->filename, rule->bpf, pcap_geterr(deadPcap));
                }
            } else {
                rule->bpfp.bf_len = 0;
            }
        }
    }
}
/******************************************************************************/
LOCAL gboolean moloch_rules_check_ip(const MolochRule_t *rule, const int p, const struct in6_addr *ip)
{
    if (IN6_IS_ADDR_V4MAPPED(ip)) {
        return patricia_search_best3 (rule->tree4[p], ((u_char *)ip->s6_addr) + 12, 32) != NULL;
    } else {
        return patricia_search_best3 (rule->tree6[p], (u_char *)ip->s6_addr, 128) != NULL;
    }
}
/******************************************************************************/
LOCAL void moloch_rules_check_rule_fields(MolochSession_t *session, MolochRule_t *rule, int skipPos)
{
    MolochString_t        *hstring;
    MolochInt_t           *hint;
    MolochStringHashStd_t *shash;
    MolochIntHashStd_t    *ihash;
    GHashTable            *ghash;
    GHashTableIter         iter;
    gpointer               ikey;
    int                    i;
    int                    f;
    int                    good = 1;

    for (f = 0; good && f < rule->fieldsLen; f++) {
        int p = rule->fields[f];

        if (p == skipPos)
            continue;

        if (p >= session->maxFields) {
            switch (p) {
            case MOLOCH_FIELD_EXSPECIAL_SRC_IP:
                good = moloch_rules_check_ip(rule, p, &session->addr1);
                break;
            case MOLOCH_FIELD_EXSPECIAL_SRC_PORT:
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)session->port1);
                break;
            case MOLOCH_FIELD_EXSPECIAL_DST_IP:
                good = moloch_rules_check_ip(rule, p, &session->addr2);
                break;
            case MOLOCH_FIELD_EXSPECIAL_DST_PORT:
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)session->port2);
                break;
            case MOLOCH_FIELD_EXSPECIAL_TCPFLAGS_SYN:
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)session->tcpFlagCnt[MOLOCH_TCPFLAG_SYN]);
                break;
            case MOLOCH_FIELD_EXSPECIAL_PACKETS_SRC:
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)session->packets[0]);
                break;
            case MOLOCH_FIELD_EXSPECIAL_PACKETS_DST:
                good = g_hash_table_contains(rule->hash[p], (gpointer)(long)session->packets[1]);
                break;
            }
            continue;
        }

        if (!session->fields[p]) {
            good = 0;
            break;
        }

        switch (config.fields[p]->type) {
        case MOLOCH_FIELD_TYPE_IP:
            good = moloch_rules_check_ip(rule, p, session->fields[p]->ip);
            break;

        case MOLOCH_FIELD_TYPE_INT:
            good = g_hash_table_contains(rule->hash[p], (gpointer)(long)session->fields[p]->i);
            break;

        case MOLOCH_FIELD_TYPE_INT_ARRAY:
            good = 0;
            for(i = 0; i < (int)session->fields[p]->iarray->len; i++) {
                if (g_hash_table_contains(rule->hash[p], (gpointer)(long)g_array_index(session->fields[p]->iarray, uint32_t, i))) {
                    good = 1;
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
                    break;
                }
            );
            break;
        case MOLOCH_FIELD_TYPE_IP_GHASH:
            ghash = session->fields[p]->ghash;
            g_hash_table_iter_init (&iter, ghash);
            good = 0;
            while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
                if (moloch_rules_check_ip(rule, p, ikey)) {
                    good = 1;
                    break;
                }
            }
            break;
        case MOLOCH_FIELD_TYPE_STR_GHASH:
        case MOLOCH_FIELD_TYPE_INT_GHASH:
            ghash = session->fields[p]->ghash;
            g_hash_table_iter_init (&iter, ghash);
            good = 0;
            while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
                if (g_hash_table_contains(rule->hash[p], ikey)) {
                    good = 1;
                    break;
                }
            }
            break;
        case MOLOCH_FIELD_TYPE_STR:
            good = g_hash_table_contains(rule->hash[p], session->fields[p]->str);
            break;
        case MOLOCH_FIELD_TYPE_STR_ARRAY:
            good = 0;
            for(i = 0; i < (int)session->fields[p]->sarray->len; i++) {
                if (g_hash_table_contains(rule->hash[p], g_ptr_array_index(session->fields[p]->sarray, i))) {
                    good = 1;
                    break;
                }
            }
            break;
        case MOLOCH_FIELD_TYPE_STR_HASH:
            shash = session->fields[p]->shash;
            good = 0;
            HASH_FORALL(s_, *shash, hstring,
                if (g_hash_table_contains(rule->hash[p], (gpointer)hstring->str)) {
                    good = 1;
                    break;
                }
            );
            break;
        } /* switch */
    }
    if (good) {
        moloch_field_ops_run(session, &rule->ops);
    }
}
/******************************************************************************/
void moloch_rules_run_field_set(MolochSession_t *session, int pos, const gpointer value)
{
    int                    r;

    if (config.fields[pos]->type == MOLOCH_FIELD_TYPE_IP ||
        config.fields[pos]->type == MOLOCH_FIELD_TYPE_IP_GHASH) {

        patricia_node_t *nodes[MOLOCH_RULES_MAX];

        int cnt;
        if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)value)) {
            cnt = patricia_search_all2(current.fieldsTree4[pos], ((u_char *)value) + 12, 32, nodes, MOLOCH_RULES_MAX);
        } else {
            cnt = patricia_search_all2(current.fieldsTree6[pos], (u_char *)value, 128, nodes, MOLOCH_RULES_MAX);
        }
        if (cnt == 0)
            return;

        // These are all the possible rules that match
        int i;
        for (i = 0; i < cnt; i++) {
            GPtrArray *rules = nodes[i]->data;

            for (r = 0; r < (int)rules->len; r++) {
                MolochRule_t *rule = g_ptr_array_index(rules, r);

                // If there is only 1 field we are checking for then the ops can be run since it matched above
                if (rule->fieldsLen == 1) {
                    moloch_field_ops_run(session, &rule->ops);
                    continue;
                }

                // Need to check other fields in rule
                moloch_rules_check_rule_fields(session, rule, pos);
            }
        }
    } else {
        // See if this value is in the hash table of values we are watching for
        GPtrArray *rules = g_hash_table_lookup(current.fieldsHash[pos], value);
        if (!rules)
            return;

        for (r = 0; r < (int)rules->len; r++) {
            MolochRule_t *rule = g_ptr_array_index(rules, r);

            // If there is only 1 field we are checking for then the ops can be run since it matched above
            if (rule->fieldsLen == 1) {
                moloch_field_ops_run(session, &rule->ops);
                return;
            }

            // Need to check other fields in rule
            moloch_rules_check_rule_fields(session, rule, pos);
        }
    }
}
/******************************************************************************/
void moloch_rules_run_session_setup(MolochSession_t *session, MolochPacket_t *packet)
{
    int r;
    MolochRule_t *rule;
    for (r = 0; (rule = current.rules[MOLOCH_RULE_TYPE_SESSION_SETUP][r]); r++) {
        if (rule->fieldsLen) {
            moloch_rules_check_rule_fields(session, rule, -1);
        } else if (rule->bpfp.bf_len && bpf_filter(rule->bpfp.bf_insns, packet->pkt, packet->pktlen, packet->pktlen)) {
            moloch_field_ops_run(session, &rule->ops);
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
            moloch_rules_check_rule_fields(session, rule, -1);
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
            moloch_rules_check_rule_fields(session, rule, -1);
        }
    }
}
/******************************************************************************/
void moloch_rules_session_create(MolochSession_t *session)
{
    switch (session->protocol) {
    case IPPROTO_TCP:
    case IPPROTO_UDP:
        if (config.fields[MOLOCH_FIELD_EXSPECIAL_SRC_IP]->ruleEnabled)
            moloch_rules_run_field_set(session, MOLOCH_FIELD_EXSPECIAL_SRC_IP, &session->addr1);
        if (config.fields[MOLOCH_FIELD_EXSPECIAL_DST_IP]->ruleEnabled)
            moloch_rules_run_field_set(session, MOLOCH_FIELD_EXSPECIAL_DST_IP, &session->addr2);
    case IPPROTO_ICMP:
        if (config.fields[MOLOCH_FIELD_EXSPECIAL_SRC_PORT]->ruleEnabled)
            moloch_rules_run_field_set(session, MOLOCH_FIELD_EXSPECIAL_SRC_PORT, (gpointer)(long)session->port1);
        if (config.fields[MOLOCH_FIELD_EXSPECIAL_DST_PORT]->ruleEnabled)
            moloch_rules_run_field_set(session, MOLOCH_FIELD_EXSPECIAL_DST_PORT, (gpointer)(long)session->port2);
        break;
    }
}
/******************************************************************************/
void moloch_rules_init()
{
    char **rulesFiles = moloch_config_str_list(NULL, "rulesFiles", NULL);

    if (rulesFiles)
        moloch_config_monitor_files("rules files", rulesFiles, moloch_rules_load);
    else
        moloch_rules_load_complete();
}
/******************************************************************************/
void moloch_rules_exit()
{
}
