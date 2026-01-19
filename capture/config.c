/******************************************************************************/
/* config.c  -- Functions dealing with the config file
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkimeconfig.h"
#include "arkime.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include "yaml.h"

extern ArkimeConfig_t        config;

LOCAL GKeyFile             *arkimeKeyFile;
LOCAL char                **overrideIpsFiles;
LOCAL char                **packetDropIpsFiles;

//#define CONFIG_DEBUG 1

#ifdef CONFIG_DEBUG
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

/******************************************************************************/
gchar **arkime_config_section_raw_str_list(GKeyFile *keyfile, const char *section, const char *key, const char *d)
{
    gchar **result;

    if (!keyfile)
        keyfile = arkimeKeyFile;

    if (g_key_file_has_key(keyfile, section, key, NULL)) {
        result = g_key_file_get_string_list(keyfile, section, key, NULL, NULL);
    } else if (d) {
        result = g_strsplit(d, ";", 0);
    } else {
        result = NULL;
    }

    return result;
}

/******************************************************************************/
gchar **arkime_config_section_str_list(GKeyFile *keyfile, const char *section, const char *key, const char *d)
{
    gchar **strs = arkime_config_section_raw_str_list(keyfile, section, key, d);
    if (!strs) {
        if (config.debug) {
            LOG("%s=(null)", key);
        }
        return strs;
    }

    int i, j;
    for (i = j = 0; strs[i]; i++) {
        char *str = strs[i];

        /* Remove leading and trailing spaces */
        while (isspace(*str))
            str++;
        g_strchomp(str);

        /* Empty string */
        if (*str == 0) {
            g_free(strs[i]);
            continue;
        }

        /* Moved front of string, need to realloc so g_strfreev doesn't blow */
        if (str != strs[i]) {
            str = g_strdup(str);
            g_free(strs[i]);
        }

        /* Save string back */
        strs[j] = str;
        j++;
    }

    /* NULL anything at the end that was moved forward */
    for (; j < i; j++)
        strs[j] = NULL;

    if (config.debug) {
        gchar *str = g_strjoinv(";", strs);
        LOG("%s=%s", key, str);
        g_free(str);
    }
    return strs;
}

/******************************************************************************/
gchar *arkime_config_section_str(GKeyFile *keyfile, const char *section, const char *key, const char *d)
{
    char *result;
    if (!keyfile)
        keyfile = arkimeKeyFile;

    if (g_key_file_has_key(keyfile, section, key, NULL)) {
        result = g_key_file_get_string(keyfile, section, key, NULL);
    } else if (d) {
        result = g_strdup(d);
    } else {
        result = NULL;
    }

    if (config.debug) {
        LOG("%s.%s=%s", section, key, result ? result : "(null)");
    }

    return result;
}
/******************************************************************************/
gchar **arkime_config_section_keys(GKeyFile *keyfile, const char *section, gsize *keys_len)
{
    if (!keyfile)
        keyfile = arkimeKeyFile;

    if (!g_key_file_has_group(keyfile, section)) {
        *keys_len = 0;
        return NULL;
    }

    GError *error = 0;
    gchar **keys = g_key_file_get_keys (keyfile, section, keys_len, &error);
    if (error) {
        g_error_free(error);
        *keys_len = 0;
        return NULL;
    }
    return keys;
}

/******************************************************************************/
gchar *arkime_config_str(GKeyFile *keyfile, const char *key, const char *d)
{
    char *result;

    if (!keyfile)
        keyfile = arkimeKeyFile;

    if (config.override && keyfile == arkimeKeyFile && (result = g_hash_table_lookup(config.override, key))) {
        if (result[0] == 0)
            result = NULL;
        else
            result = g_strdup(result);
    } else if (g_key_file_has_key(keyfile, config.nodeName, key, NULL)) {
        result = g_key_file_get_string(keyfile, config.nodeName, key, NULL);
    } else if (config.nodeClass && g_key_file_has_key(keyfile, config.nodeClass, key, NULL)) {
        result = g_key_file_get_string(keyfile, config.nodeClass, key, NULL);
    } else if (g_key_file_has_key(keyfile, "default", key, NULL)) {
        result = g_key_file_get_string(keyfile, "default", key, NULL);
    } else if (d) {
        result = g_strdup(d);
    } else {
        result = NULL;
    }

    if (result)
        g_strstrip(result);

    if (config.debug) {
        LOG("%s=%s", key, result ? result : "(null)");
    }

    return result;
}

/******************************************************************************/
gchar **arkime_config_raw_str_list(GKeyFile *keyfile, const char *key, const char *d)
{
    const char   *hvalue;
    gchar **result;

    if (!keyfile)
        keyfile = arkimeKeyFile;

    if (config.override && keyfile == arkimeKeyFile && (hvalue = g_hash_table_lookup(config.override, key))) {
        result = g_strsplit(hvalue, ";", 0);
    } else if (g_key_file_has_key(keyfile, config.nodeName, key, NULL)) {
        result = g_key_file_get_string_list(keyfile, config.nodeName, key, NULL, NULL);
    } else if (config.nodeClass && g_key_file_has_key(keyfile, config.nodeClass, key, NULL)) {
        result = g_key_file_get_string_list(keyfile, config.nodeClass, key, NULL, NULL);
    } else if (g_key_file_has_key(keyfile, "default", key, NULL)) {
        result = g_key_file_get_string_list(keyfile, "default", key, NULL, NULL);
    } else if (d) {
        result = g_strsplit(d, ";", 0);
    } else {
        result = NULL;
    }

    return result;
}

/******************************************************************************/
gchar **arkime_config_str_list(GKeyFile *keyfile, const char *key, const char *d)
{
    gchar **strs = arkime_config_raw_str_list(keyfile, key, d);
    if (!strs) {
        if (config.debug) {
            LOG("%s=(null)", key);
        }
        return strs;
    }

    int i, j;
    for (i = j = 0; strs[i]; i++) {
        char *str = strs[i];

        /* Remove leading and trailing spaces */
        while (isspace(*str))
            str++;
        g_strchomp(str);

        /* Empty string */
        if (*str == 0) {
            g_free(strs[i]);
            continue;
        }

        /* Moved front of string, need to realloc so g_strfreev doesn't blow */
        if (str != strs[i]) {
            str = g_strdup(str);
            g_free(strs[i]);
        }

        /* Save string back */
        strs[j] = str;
        j++;
    }

    /* NULL anything at the end that was moved forward */
    for (; j < i; j++)
        strs[j] = NULL;

    if (config.debug) {
        gchar *str = g_strjoinv(";", strs);
        LOG("%s=%s", key, str);
        g_free(str);
    }
    return strs;
}

/******************************************************************************/
uint32_t arkime_config_int(GKeyFile *keyfile, const char *key, uint32_t d, uint32_t min, uint32_t max)
{
    const char *result;
    uint32_t    value = d;

    if (!keyfile)
        keyfile = arkimeKeyFile;

    if (config.override && keyfile == arkimeKeyFile && (result = g_hash_table_lookup(config.override, key))) {
        value = atol(result);
    } else if (g_key_file_has_key(keyfile, config.nodeName, key, NULL)) {
        value = g_key_file_get_integer(keyfile, config.nodeName, key, NULL);
    } else if (config.nodeClass && g_key_file_has_key(keyfile, config.nodeClass, key, NULL)) {
        value = g_key_file_get_integer(keyfile, config.nodeClass, key, NULL);
    } else if (g_key_file_has_key(keyfile, "default", key, NULL)) {
        value = g_key_file_get_integer(keyfile, "default", key, NULL);
    }

    if (value < min) {
        LOG("INFO: Resetting %s since %u is less than the min %u", key, value, min);
        value = min;
    }
    if (value > max) {
        LOG("INFO: Resetting %s since %u is greater than the max %u", key, value, max);
        value = max;
    }

    if (config.debug) {
        LOG("%s=%u", key, value);
    }

    return value;
}

/******************************************************************************/
double arkime_config_double(GKeyFile *keyfile, const char *key, double d, double min, double max)
{
    const char *result;
    double      value = d;

    if (!keyfile)
        keyfile = arkimeKeyFile;

    if (config.override && keyfile == arkimeKeyFile && (result = g_hash_table_lookup(config.override, key))) {
        value = atof(result);
    } else if (g_key_file_has_key(keyfile, config.nodeName, key, NULL)) {
        value = g_key_file_get_double(keyfile, config.nodeName, key, NULL);
    } else if (config.nodeClass && g_key_file_has_key(keyfile, config.nodeClass, key, NULL)) {
        value = g_key_file_get_double(keyfile, config.nodeClass, key, NULL);
    } else if (g_key_file_has_key(keyfile, "default", key, NULL)) {
        value = g_key_file_get_double(keyfile, "default", key, NULL);
    }

    if (value < min)
        value = min;
    if (value > max)
        value = max;

    if (config.debug) {
        LOG("%s=%lf", key, value);
    }

    return value;
}

/******************************************************************************/
char arkime_config_boolean(GKeyFile *keyfile, const char *key, char d)
{
    const char *result;
    gboolean    value = d;

    if (!keyfile)
        keyfile = arkimeKeyFile;

    if (config.override && keyfile == arkimeKeyFile && (result = g_hash_table_lookup(config.override, key))) {
        value = strcmp(result, "true") == 0 || strcmp(result, "1") == 0;
    } else if (g_key_file_has_key(keyfile, config.nodeName, key, NULL)) {
        value = g_key_file_get_boolean(keyfile, config.nodeName, key, NULL);
    } else if (config.nodeClass && g_key_file_has_key(keyfile, config.nodeClass, key, NULL)) {
        value = g_key_file_get_boolean(keyfile, config.nodeClass, key, NULL);
    } else if (g_key_file_has_key(keyfile, "default", key, NULL)) {
        value = g_key_file_get_boolean(keyfile, "default", key, NULL);
    }

    if (config.debug) {
        LOG("%s=%s", key, value ? "true" : "false");
    }

    return value;
}
/******************************************************************************/
LOCAL void arkime_config_load_includes(char **includes)
{
    for (int i = 0; includes[i]; i++) {
        GKeyFile *keyFile = g_key_file_new();
        GError *error = 0;
        const char *fn = includes[i];
        if (*fn == '-')
            fn++;

        gboolean status = g_key_file_load_from_file(keyFile, fn, G_KEY_FILE_NONE, &error);
        if (!status || error) {
            if (includes[i][0] == '-') {
                if (error)
                    g_error_free(error);
                continue;
            } else {
                CONFIGEXIT("Couldn't load config includes file (%s) %s", fn, (error ? error->message : ""));
            }
        }

        gchar **groups = g_key_file_get_groups (keyFile, NULL);
        for (int g = 0; groups[g]; g++) {
            gchar **keys = g_key_file_get_keys (keyFile, groups[g], NULL, NULL);
            for (int k = 0; keys[k]; k++) {
                char *value = g_key_file_get_value(keyFile, groups[g], keys[k], NULL);
                if (value) {
                    g_key_file_set_value(arkimeKeyFile, groups[g], keys[k], value);
                    g_free(value);
                }
            }
            g_strfreev(keys);
        }
        g_strfreev(groups);
        g_key_file_free(keyFile);
    }
}
/******************************************************************************/
LOCAL void arkime_config_load_hidden(const char *configFile)
{
    char line[1000];
    FILE *file = fopen(configFile, "r");
    if (!file)
        CONFIGEXIT("Couldn't open %s", configFile);
    if (!fgets(line, sizeof(line), file))
        CONFIGEXIT("Couldn't read %s", configFile);
    fclose(file);

    g_strchomp(line);

    g_free(config.configFile);
    config.configFile = g_strdup(line);
}
/******************************************************************************/
LOCAL char arkime_config_key_sep(const char *key)
{
    if (strcmp(key, "elasticsearch") == 0 ||
        strcmp(key, "usersElasticsearch") == 0)
        return ',';
    return ';';
}
/******************************************************************************/
LOCAL gboolean arkime_config_load_json(GKeyFile *keyfile, char *data, GError **UNUSED(error))
{
    uint32_t sections[4 * 100]; // Can have up to 100 sections
    memset(sections, 0, sizeof(sections));
    js0n((uint8_t *)data, strlen(data), sections, sizeof(sections));

    for (int s = 0; sections[s]; s += 4) {
        char *section = g_strndup(data + sections[s], sections[s + 1]);

        uint32_t keys[4 * 500]; // Can have up to 500 keys
        memset(keys, 0, sizeof(keys));
        js0n((uint8_t *)data + sections[s + 2], sections[s + 3], keys, sizeof(keys));

        for (int k = 0; keys[k]; k += 4) {
            char *key = g_strndup(data + sections[s + 2] + keys[k], keys[k + 1]);
            char *value = g_strndup(data + sections[s + 2] + keys[k + 2], keys[k + 3]);

            // HACK - Convert arrays back into strings
            if (value[0] == '[') {
                uint32_t parts[2 * 100]; // Can have up to 100 keys
                memset(parts, 0, sizeof(parts));
                js0n((uint8_t *)value, keys[k + 3], parts, sizeof(parts));

                char sep = arkime_config_key_sep(key);
                char *buf = malloc(keys[k + 3]);
                BSB bsb;
                BSB_INIT(bsb, buf, keys[k + 3]);
                for (int p = 0; parts[p]; p += 2) {
                    if (p != 0)
                        BSB_EXPORT_u08(bsb, sep);
                    BSB_EXPORT_ptr(bsb, value + parts[p], parts[p + 1]);
                }
                BSB_EXPORT_u08(bsb, 0);
                g_key_file_set_string(keyfile, section, key, buf);
                free(buf);
            } else {
                g_key_file_set_string(keyfile, section, key, value);
            }

            g_free(key);
            g_free(value);
        }
        g_free(section);
    }

    return TRUE;
}
/******************************************************************************/
LOCAL gboolean arkime_config_load_yaml(GKeyFile *keyfile, char *data, GError **UNUSED(error))
{
    yaml_parser_t parser;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (uint8_t *)data, strlen(data));

    int done = 0;
    int level = 0;
    char *section = NULL;
    char *key = NULL;
    char buf[20000];
    char sep = 0;
    BSB bsb = {0, 0, 0};
    while (!done) {
        yaml_event_t event;

        if (!yaml_parser_parse(&parser, &event))
            CONFIGEXIT("line %zu - Parse error '%s'", parser.problem_mark.line, parser.problem);

#ifdef CONFIG_DEBUG
        LOG("event level %d type %d - %s", level, event.type, yaml_names[event.type]);
#endif
        switch (event.type) {
        case YAML_NO_EVENT:
            done = 1;
            break;
        case YAML_SCALAR_EVENT:
            if (level == 1) {
                g_free(section);
                section = g_strdup((char *)event.data.scalar.value);
            } else if (level == 2) {
                if (!key) {
                    key = g_strdup((char *)event.data.scalar.value);
                } else {
#ifdef CONFIG_DEBUG
                    LOG("%s:%s => %s", section, key, event.data.scalar.value);
#endif
                    g_key_file_set_string(keyfile, section, key, (char *)event.data.scalar.value);
                    g_free(key);
                    key = NULL;
                }
            } else if (level == 3) {
                if (BSB_LENGTH(bsb) != 0)
                    BSB_EXPORT_u08(bsb, sep);
                int len = strlen((char *)event.data.scalar.value);
                BSB_EXPORT_ptr(bsb, event.data.scalar.value, len);
            }
            break;
        case YAML_SEQUENCE_START_EVENT:
            BSB_INIT(bsb, buf, sizeof(buf));
            sep = arkime_config_key_sep(key);
            level++;
            break;
        case YAML_MAPPING_START_EVENT:
            level++;
            break;
        case YAML_SEQUENCE_END_EVENT:
            if (level == 3) {
                BSB_EXPORT_u08(bsb, 0);
#ifdef CONFIG_DEBUG
                LOG("%s:%s => %s", section, key, buf);
#endif
                g_key_file_set_string(keyfile, section, key, buf);
                g_free(key);
                key = NULL;
            }
            level--;
            break;
        case YAML_MAPPING_END_EVENT:
            if (level == 1) {
                g_free(section);
                section = NULL;
            } else if (level == 2) {
                g_free(key);
                key = NULL;
            }
            level--;
            break;
        default:
            ;
        }
        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    return TRUE;
}
/******************************************************************************/
LOCAL void arkime_config_override_print(gpointer key, gpointer value, gpointer UNUSED(user_data))
{
    fprintf(stderr, "%s=%s\n", (char *)key, (char *)value);
}
/******************************************************************************/
LOCAL int cstring_cmp(const void *a, const void *b)
{
    return strcmp(*(char **)a, *(char **)b);
}
/******************************************************************************/
LOCAL void arkime_config_load()
{

    gboolean  status;
    GError   *error = 0;
    GKeyFile *keyfile;

    keyfile = arkimeKeyFile = g_key_file_new();

    if (g_str_has_prefix(config.configFile, "urlinfile://")) {
        arkime_config_load_hidden(config.configFile + 12);

    } else if (g_str_has_suffix(config.configFile, ".hiddenconfig")) {
        config.configFile[strlen(config.configFile) - 13] = 0;
        arkime_config_load_hidden(config.configFile);
    }

    if (g_str_has_prefix(config.configFile, "elasticsearch://") || g_str_has_prefix(config.configFile, "elasticsearchs://")) {
        GString *string = g_string_new(config.configFile);
        g_string_replace(string, "elasticsearch", "http", 1);
        g_string_replace(string, "_doc", "_source", 1);
        g_free(config.configFile);
        config.configFile = g_string_free(string, FALSE);
    } else if (g_str_has_prefix(config.configFile, "opensearch://") || g_str_has_prefix(config.configFile, "opensearchs://")) {
        GString *string = g_string_new(config.configFile);
        g_string_replace(string, "opensearch", "http", 1);
        g_string_replace(string, "_doc", "_source", 1);
        g_free(config.configFile);
        config.configFile = g_string_free(string, FALSE);
    }

    if (g_str_has_prefix(config.configFile, "http://") || g_str_has_prefix(config.configFile, "https://")) {
        const char *end = config.configFile + 8;
        while (*end != 0 && *end != '/' && *end != '?') end++;

        char *host = g_strndup(config.configFile, end - config.configFile);

        void *server = arkime_http_create_server(host, 5, 5, TRUE);

        int code;
        uint8_t *data = arkime_http_send_sync(server, "GET", end, strlen(end), NULL, 0, NULL, NULL, &code);

        if (!data || code != 200) {
            free(data);
            CONFIGEXIT("Couldn't download from code: %d host: %s url: %s", code, host, end);
        }

        if (g_str_has_suffix(config.configFile, ".ini"))
            status = g_key_file_load_from_data(keyfile, (gchar *)data, (gsize) -1, G_KEY_FILE_NONE, &error);
        else if (g_str_has_suffix(config.configFile, ".yml") || g_str_has_suffix(config.configFile, ".yaml"))
            status = arkime_config_load_yaml(keyfile, (char *)data, &error);
        else
            status = arkime_config_load_json(keyfile, (char *)data, &error);
        g_free(host);
        free(data);
        arkime_http_free_server(server);
    } else {
        if (g_str_has_suffix(config.configFile, ".json")) {
            gchar *data;
            if (!g_file_get_contents(config.configFile, &data, NULL, &error))
                CONFIGEXIT("Couldn't load config file (%s) %s", config.configFile, (error ? error->message : ""));
            status = arkime_config_load_json(keyfile, data, &error);
            g_free(data);
        } else if (g_str_has_suffix(config.configFile, ".yml") || g_str_has_suffix(config.configFile, ".yaml")) {
            gchar *data;
            if (!g_file_get_contents(config.configFile, &data, NULL, &error))
                CONFIGEXIT("Couldn't load config file (%s) %s", config.configFile, (error ? error->message : ""));
            status = arkime_config_load_yaml(keyfile, data, &error);
            g_free(data);
        } else
            status = g_key_file_load_from_file(keyfile, config.configFile, G_KEY_FILE_NONE, &error);
    }

    if (!status || error) {
        if (config.noConfigOption) {
            LOG("Couldn't load config file (%s) %s", config.configFile, (error ? error->message : ""));
            g_key_file_load_from_data(keyfile, (gchar *)"[default]\n", (gsize) -1, G_KEY_FILE_NONE, &error);
        } else
            CONFIGEXIT("Couldn't load config file (%s) %s", config.configFile, (error ? error->message : ""));
    }

    char **includes = arkime_config_str_list(keyfile, "includes", NULL);
    if (includes) {
        arkime_config_load_includes(includes);
        g_strfreev(includes);
    }

    extern char **environ;
    for (int e = 0; environ[e]; e++) {
        if (!g_str_has_prefix(environ[e], "ARKIME_"))
            continue;

        const char *equal = strchr(environ[e] + 7, '=');
        if (!equal)
            continue;

        GString *section = NULL;
        GString *key;

        if (environ[e][7] == '_') {
            key = g_string_new_len(environ[e] + 8, equal - environ[e] - 8);
        } else {
            const char *underunder = strstr(environ[e] + 7, "__");
            if (!underunder)
                continue;

            section = g_string_new_len(environ[e] + 7, underunder - environ[e] - 7);
            key = g_string_new_len(underunder + 2,  equal - underunder - 2);
        }

        g_string_replace(key, "DASH", "-", 0);
        g_string_replace(key, "COLON", ":", 0);
        g_string_replace(key, "DOT", ".", 0);
        g_string_replace(key, "SLASH", "/", 0);

        if (section) {
            g_string_replace(section, "DASH", "-", 0);
            g_string_replace(section, "COLON", ":", 0);
            g_string_replace(section, "DOT", ".", 0);
            g_string_replace(section, "SLASH", "/", 0);
            g_key_file_set_string(keyfile, section->str, key->str, equal + 1);
            g_string_free(section, TRUE);
        } else {
            g_key_file_set_string(keyfile, "default", key->str, equal + 1);
        }
        g_string_free(key, TRUE);
    }

    if (config.dumpConfig) {
        if (config.override) {
            fprintf(stderr, "OVERRIDE:\n");
            g_hash_table_foreach(config.override, arkime_config_override_print, NULL);
        }

        fprintf(stderr, "CONFIG:\n");
        gsize groups_len;
        gchar **groups = g_key_file_get_groups(keyfile, &groups_len);
        qsort(groups, groups_len, sizeof(gchar *), cstring_cmp);
        for (int i = 0; groups[i]; i++) {
            if (i > 0)
                fprintf(stderr, "\n");
            fprintf(stderr, "[%s]\n", groups[i]);

            gsize keys_len;
            gchar **keys = g_key_file_get_keys(keyfile, groups[i], &keys_len, NULL);

            qsort(keys, keys_len, sizeof(gchar *), cstring_cmp);

            for (int j = 0; keys[j]; j++) {
                gchar *value = g_key_file_get_string(keyfile, groups[i], keys[j], NULL);
                fprintf(stderr, "%s=%s\n", keys[j], value);
                g_free(value);
            }
            g_strfreev(keys);
        }
        g_strfreev(groups);
        if (config.regressionTests) {
            exit(0);
        }
    }


    if (config.debug == 0) {
        config.debug = arkime_config_int(keyfile, "debug", 0, 0, 128);
    }

    char *rotateIndex       = arkime_config_str(keyfile, "rotateIndex", "daily");

    if (!rotateIndex) {
        CONFIGEXIT("The rotateIndex= can't be empty in config file (%s)", config.configFile);
    } else if (strcmp(rotateIndex, "hourly") == 0)
        config.rotate = ARKIME_ROTATE_HOURLY;
    else if (strcmp(rotateIndex, "hourly2") == 0)
        config.rotate = ARKIME_ROTATE_HOURLY2;
    else if (strcmp(rotateIndex, "hourly3") == 0)
        config.rotate = ARKIME_ROTATE_HOURLY3;
    else if (strcmp(rotateIndex, "hourly4") == 0)
        config.rotate = ARKIME_ROTATE_HOURLY4;
    else if (strcmp(rotateIndex, "hourly6") == 0)
        config.rotate = ARKIME_ROTATE_HOURLY6;
    else if (strcmp(rotateIndex, "hourly8") == 0)
        config.rotate = ARKIME_ROTATE_HOURLY8;
    else if (strcmp(rotateIndex, "hourly12") == 0)
        config.rotate = ARKIME_ROTATE_HOURLY12;
    else if (strcmp(rotateIndex, "daily") == 0)
        config.rotate = ARKIME_ROTATE_DAILY;
    else if (strcmp(rotateIndex, "weekly") == 0)
        config.rotate = ARKIME_ROTATE_WEEKLY;
    else if (strcmp(rotateIndex, "monthly") == 0)
        config.rotate = ARKIME_ROTATE_MONTHLY;
    else {
        CONFIGEXIT("Unknown rotateIndex '%s' in config file (%s), see https://arkime.com/settings#rotateindex", rotateIndex, config.configFile);
    }
    g_free(rotateIndex);

    config.nodeClass        = arkime_config_str(keyfile, "nodeClass", NULL);
    gchar **tags            = arkime_config_str_list(keyfile, "dontSaveTags", NULL);
    if (tags) {
        for (int i = 0; tags[i]; i++) {
            if (!(*tags[i]))
                continue;
            int num = 1;
            char *colon = strchr(tags[i], ':');
            if (colon) {
                *colon = 0;
                num = atoi(colon + 1);
                if (num < 1)
                    num = 1;
                if (num > 0xffff)
                    num = 0xffff;
            }
            arkime_string_add((ArkimeStringHash_t *)(char *)&config.dontSaveTags, tags[i], (gpointer)(long)num, TRUE);
        }
        g_strfreev(tags);
    }

    config.plugins          = arkime_config_str_list(keyfile, "plugins", NULL);
    config.rootPlugins      = arkime_config_str_list(keyfile, "rootPlugins", NULL);
    config.smtpIpHeaders    = arkime_config_str_list(keyfile, "smtpIpHeaders", NULL);

    if (config.smtpIpHeaders) {
        for (int i = 0; config.smtpIpHeaders[i]; i++) {
            int len = strlen(config.smtpIpHeaders[i]);
            char *lower = g_ascii_strdown(config.smtpIpHeaders[i], len);
            g_free(config.smtpIpHeaders[i]);
            config.smtpIpHeaders[i] = lower;
            if (lower[len - 1] == ':')
                lower[len - 1] = 0;
        }
    }

    config.prefix           = arkime_config_str(keyfile, "prefix", "arkime_");
    int len = strlen(config.prefix);
    if (len > 50)
        CONFIGEXIT("prefix can be at most 50 characters long");
    if (len > 0 && config.prefix[len - 1] != '_') {
        char *tmp  = g_malloc(len + 2);
        memcpy(tmp, config.prefix, len);
        tmp[len] = '_';
        tmp[len + 1] = 0;
        g_free(config.prefix);
        config.prefix = tmp;
    }

    config.elasticsearch    = arkime_config_str(keyfile, "elasticsearch", "localhost:9200");
    config.interface        = arkime_config_str_list(keyfile, "interface", NULL);
    config.pcapDir          = arkime_config_str_list(keyfile, "pcapDir", "/opt/arkime/raw");
    config.bpf              = arkime_config_str(keyfile, "bpf", NULL);
    config.yara             = arkime_config_str(keyfile, "yara", NULL);
    config.rirFile          = arkime_config_str(keyfile, "rirFile", NULL);
    config.ouiFile          = arkime_config_str(keyfile, "ouiFile", NULL);
    config.geoLite2ASN      = arkime_config_str_list(keyfile, "geoLite2ASN", "/var/lib/GeoIP/GeoLite2-ASN.mmdb;/usr/share/GeoIP/GeoLite2-ASN.mmdb;" CONFIG_PREFIX "/etc/GeoLite2-ASN.mmdb");
    config.geoLite2Country  = arkime_config_str_list(keyfile, "geoLite2Country", "/var/lib/GeoIP/GeoLite2-City.mmdb;/var/lib/GeoIP/GeoLite2-Country.mmdb;/usr/share/GeoIP/GeoLite2-City.mmdb;/usr/share/GeoIP/GeoLite2-Country.mmdb;" CONFIG_PREFIX "/etc/GeoLite2-City.mmdb;" CONFIG_PREFIX "/etc/GeoLite2-Country.mmdb");
    config.dropUser         = arkime_config_str(keyfile, "dropUser", NULL);
    config.dropGroup        = arkime_config_str(keyfile, "dropGroup", NULL);
    config.pluginsDir       = arkime_config_str_list(keyfile, "pluginsDir", CONFIG_PREFIX "/plugins ; ./plugins ");
    config.parsersDir       = arkime_config_str_list(keyfile, "parsersDir", CONFIG_PREFIX "/parsers ; ./parsers ");
    config.caTrustFile      = arkime_config_str(keyfile, "caTrustFile", NULL);
    char *offlineRegex      = arkime_config_str(keyfile, "offlineFilenameRegex", "(?i)\\.(pcap|cap)$");

    if (config.interface) {
        for (config.interfaceCnt = 0; config.interfaceCnt < MAX_INTERFACES && config.interface[config.interfaceCnt]; config.interfaceCnt++) {
        }
        if (config.interfaceCnt == MAX_INTERFACES && config.interface[config.interfaceCnt]) {
            CONFIGEXIT("Only support up to %d interfaces", MAX_INTERFACES);
        }
    }

    error = NULL;
    config.offlineRegex     = g_regex_new(offlineRegex, 0, 0, &error);
    if (!config.offlineRegex || error) {
        CONFIGEXIT("Couldn't parse offlineRegex (%s) %s", offlineRegex, (error ? error->message : ""));
    }
    g_free(offlineRegex);

    config.pcapDirTemplate  = arkime_config_str(keyfile, "pcapDirTemplate", NULL);
    if (config.pcapDirTemplate && config.pcapDirTemplate[0] != '/') {
        CONFIGEXIT("pcapDirTemplate MUST start with a / '%s'", config.pcapDirTemplate);
    }

    config.pcapDirAlgorithm = arkime_config_str(keyfile, "pcapDirAlgorithm", "round-robin");
    if (strcmp(config.pcapDirAlgorithm, "round-robin") != 0
        && strcmp(config.pcapDirAlgorithm, "max-free-percent") != 0
        && strcmp(config.pcapDirAlgorithm, "max-free-bytes") != 0) {
        CONFIGEXIT("'%s' is not a valid value for pcapDirAlgorithm.  Supported algorithms are round-robin, max-free-percent, and max-free-bytes.", config.pcapDirAlgorithm);
    }

    config.maxFileSizeG          = arkime_config_double(keyfile, "maxFileSizeG", 12, 0.01, 1024);
    config.maxFileSizeB          = config.maxFileSizeG * 1024LL * 1024LL * 1024LL;
    config.maxFileTimeM          = arkime_config_int(keyfile, "maxFileTimeM", 0, 0, 0xffff);
    config.tcpSaveTimeout        = arkime_config_int(keyfile, "tcpSaveTimeout", 60 * 8, 10, 60 * 120);
    int maxStreams               = arkime_config_int(keyfile, "maxStreams", 1500000, 1, 16777215);
    config.maxPackets            = arkime_config_int(keyfile, "maxPackets", 10000, 1, 0xffff);
    config.maxPacketsInQueue     = arkime_config_int(keyfile, "maxPacketsInQueue", 200000, 10000, 5000000);
    config.dbBulkSize            = arkime_config_int(keyfile, "dbBulkSize", 1000000, 500000, 15000000);
    config.dbFlushTimeout        = arkime_config_int(keyfile, "dbFlushTimeout", 5, 1, 60 * 30);
    config.maxESConns            = arkime_config_int(keyfile, "maxESConns", 20, 3, 500);
    config.maxESRequests         = arkime_config_int(keyfile, "maxESRequests", 500, 10, 2500);
    config.logEveryXPackets      = arkime_config_int(keyfile, "logEveryXPackets", 50000, 1000, 0xffffffff);
    config.pcapBufferSize        = arkime_config_int(keyfile, "pcapBufferSize", 300000000, 100000, 0xffffffff);
    config.pcapWriteSize         = arkime_config_int(keyfile, "pcapWriteSize", 0x40000, 0x10000, 0x8000000);
    config.fragsTimeout          = arkime_config_int(keyfile, "fragsTimeout", 60 * 8, 60, 0xffff);
    config.maxFrags              = arkime_config_int(keyfile, "maxFrags", 10000, 100, 0xffffff);
    config.snapLen               = arkime_config_int(keyfile, "snapLen", 16384, 1, ARKIME_PACKET_MAX_LEN);
    config.maxMemPercentage      = arkime_config_int(keyfile, "maxMemPercentage", 100, 5, 100);
    config.maxReqBody            = arkime_config_int(keyfile, "maxReqBody", 256, 0, 0x7fff);

    config.packetThreads         = arkime_config_int(keyfile, "packetThreads", 1, 1, ARKIME_MAX_PACKET_THREADS);

    config.logUnknownProtocols   = arkime_config_boolean(keyfile, "logUnknownProtocols", config.debug);
    config.logESRequests         = arkime_config_boolean(keyfile, "logESRequests", config.debug);
    config.logFileCreation       = arkime_config_boolean(keyfile, "logFileCreation", config.debug);
    config.logHTTPConnections    = arkime_config_boolean(keyfile, "logHTTPConnections", config.debug || !config.pcapReadOffline);
    config.parseSMTPHeaderAll    = arkime_config_boolean(keyfile, "parseSMTPHeaderAll", FALSE);
    config.ja3Strings            = arkime_config_boolean(keyfile, "ja3Strings", FALSE);
    config.parseQSValue          = arkime_config_boolean(keyfile, "parseQSValue", FALSE);
    config.parseCookieValue      = arkime_config_boolean(keyfile, "parseCookieValue", FALSE);
    config.parseHTTPHeaderRequestAll  = arkime_config_boolean(keyfile, "parseHTTPHeaderRequestAll", FALSE);
    config.parseHTTPHeaderResponseAll = arkime_config_boolean(keyfile, "parseHTTPHeaderResponseAll", FALSE);
    config.supportSha256         = arkime_config_boolean(keyfile, "supportSha256", FALSE);
    config.reqBodyOnlyUtf8       = arkime_config_boolean(keyfile, "reqBodyOnlyUtf8", TRUE);
    config.compressES            = arkime_config_boolean(keyfile, "compressES", TRUE);
    config.readTruncatedPackets  = arkime_config_boolean(keyfile, "readTruncatedPackets", FALSE);
    config.trackESP              = arkime_config_boolean(keyfile, "trackESP", FALSE);
    config.yaraEveryPacket       = arkime_config_boolean(keyfile, "yaraEveryPacket", TRUE);
    char  *autoGenerateId        = arkime_config_str(keyfile, "autoGenerateId", "false");
    if (strcmp(autoGenerateId, "consistent") == 0) {
        config.autoGenerateId = 2;
    } else if (strcmp(autoGenerateId, "true") == 0 || strcmp(autoGenerateId, "1") == 0) {
        config.autoGenerateId = 1;
    } else {
        config.autoGenerateId = 0;
    }
    g_free(autoGenerateId);
    config.enablePacketLen       = arkime_config_boolean(NULL, "enablePacketLen", FALSE);
    config.enablePacketDedup     = arkime_config_boolean(NULL, "enablePacketDedup", TRUE);

    config.maxStreams[SESSION_TCP] = MAX(64, maxStreams / config.packetThreads * 1.25);
    config.maxStreams[SESSION_UDP] = MAX(64, maxStreams / config.packetThreads / 20);
    config.maxStreams[SESSION_SCTP] = MAX(64, maxStreams / config.packetThreads / 20);
    config.maxStreams[SESSION_ICMP] = MAX(64, maxStreams / config.packetThreads / 200);
    config.maxStreams[SESSION_ESP] = MAX(64, maxStreams / config.packetThreads / 200);
    config.maxStreams[SESSION_OTHER] = MAX(64, maxStreams / config.packetThreads / 20);

    gchar **saveUnknownPackets     = arkime_config_str_list(keyfile, "saveUnknownPackets", NULL);
    if (saveUnknownPackets) {
        for (int i = 0; saveUnknownPackets[i]; i++) {
            const char *s = saveUnknownPackets[i];

            if (strcmp(s, "all") == 0) {
                memset(&config.etherSavePcap, 0xff, sizeof(config.etherSavePcap));
                memset(&config.ipSavePcap, 0xff, sizeof(config.ipSavePcap));
            } else if (strcmp(s, "ip:all") == 0) {
                memset(&config.ipSavePcap, 0xff, sizeof(config.ipSavePcap));
            } else if (strcmp(s, "ether:all") == 0) {
                memset(&config.etherSavePcap, 0xff, sizeof(config.etherSavePcap));
            } else if (strncmp(s, "ip:", 3) == 0) {
                int n = atoi(s + 3);
                if (n < 0 || n > 0xff)
                    CONFIGEXIT("Bad saveUnknownPackets ip value: %s", s);
                BIT_SET(n, config.ipSavePcap);
            } else if (strncmp(s, "-ip:", 4) == 0) {
                int n = atoi(s + 4);
                if (n < 0 || n > 0xff)
                    CONFIGEXIT("Bad saveUnknownPackets -ip value: %s", s);
                BIT_CLR(n, config.ipSavePcap);
            } else if (strncmp(s, "ether:", 6) == 0) {
                int n = atoi(s + 6);
                if (n < 0 || n > 0xffff)
                    CONFIGEXIT("Bad saveUnknownPackets ether value: %s", s);
                BIT_SET(n, config.etherSavePcap);
            } else if (strncmp(s, "-ether:", 7) == 0) {
                int n = atoi(s + 7);
                if (n < 0 || n > 0xffff)
                    CONFIGEXIT("Bad saveUnknownPackets -ether value: %s", s);
                BIT_CLR(n, config.etherSavePcap);
            } else if (strcmp(s, "corrupt") == 0) {
                config.corruptSavePcap = 1;
            } else if (strcmp(s, "-corrupt") == 0) {
                config.corruptSavePcap = 0;
            } else {
                CONFIGEXIT("Not sure what saveUnknownPackets %s is", s);
            }
        }
        g_strfreev(saveUnknownPackets);
    }

}
/******************************************************************************/
LOCAL void arkime_config_parse_override_ips(GKeyFile *keyFile)
{
    GError   *error = 0;

    if (!g_key_file_has_group(keyFile, "override-ips"))
        return;

    gsize keys_len;
    gchar **keys = g_key_file_get_keys (keyFile, "override-ips", &keys_len, &error);
    if (error) {
        CONFIGEXIT("Error with override-ips: %s", error->message);
    }

    GRegex *asnRegex = g_regex_new("AS\\d+ .+", 0, 0, &error);
    gsize k, v;
    for (k = 0 ; k < keys_len; k++) {
        gsize values_len;
        gchar **values = g_key_file_get_string_list(keyFile,
                                                    "override-ips",
                                                    keys[k],
                                                    &values_len,
                                                    NULL);
        ArkimeIpInfo_t *ii = ARKIME_TYPE_ALLOC0(ArkimeIpInfo_t);
        for (v = 0; v < values_len; v++) {
            if (strncmp(values[v], "asn:", 4) == 0) {
                if (!g_regex_match(asnRegex, values[v] + 4, 0, NULL)) {
                    CONFIGEXIT("Value for override-ips doesn't match ASN format of /AS\\d+ .*/ '%s'", values[v] + 4);
                }
                char *sp = strchr(values[v] + 6, ' ');
                *sp = 0;
                ii->asNum = atoi(values[v] + 6);
                ii->asn = g_strdup(sp + 1);
                ii->asnLen = strlen(sp + 1);
            } else if (strncmp(values[v], "rir:", 4) == 0) {
                ii->rir = g_strdup(values[v] + 4);
            } else if (strncmp(values[v], "tag:", 4) == 0) {
                if (ii->numtags < 10) {
                    ii->tagsStr[(int)ii->numtags] = g_strdup(values[v] + 4);
                    ii->numtags++;
                }
            } else if (strncmp(values[v], "country:", 8) == 0) {
                ii->country = g_strdup(values[v] + 8);
                ii->countryLen = strlen(ii->country);
            } else if (strncmp(values[v], "region:", 7) == 0) {
                ii->region = g_strdup(values[v] + 7);
                ii->regionLen = strlen(ii->region);
            } else if (strncmp(values[v], "city:", 5) == 0) {
                ii->city = g_strdup(values[v] + 5);
                ii->cityLen = strlen(ii->city);
            } else {
                char *colon = strchr(values[v], ':');
                if (!colon)
                    continue;
                *colon = 0; // remove :
                int pos = arkime_field_by_exp(values[v]);
                if (pos != -1) {
                    if (!ii->ops) {
                        ii->ops = ARKIME_TYPE_ALLOC0(ArkimeFieldOps_t);
                        arkime_field_ops_init(ii->ops, 1, ARKIME_FIELD_OPS_FLAGS_COPY);
                    }
                    arkime_field_ops_add(ii->ops, pos, colon + 1, strlen(colon + 1));
                }
            }
        }
        arkime_db_add_override_ip(keys[k], ii);
        g_strfreev(values);
    }
    g_regex_unref(asnRegex);
    g_strfreev(keys);
}
/******************************************************************************/
void arkime_config_load_override_ips()
{
    GError *error = 0;

    if (g_key_file_has_group(arkimeKeyFile, "override-ips")) {
        arkime_config_parse_override_ips(arkimeKeyFile);
    }

    overrideIpsFiles = arkime_config_str_list(NULL, "overrideIpsFiles", NULL);
    if (overrideIpsFiles) {
        for (int i = 0; overrideIpsFiles[i]; i++) {
            GKeyFile *keyfile = g_key_file_new();
            gboolean status = g_key_file_load_from_file(keyfile, overrideIpsFiles[i], G_KEY_FILE_NONE, &error);
            if (!status || error) {
                if (overrideIpsFiles[i][0] == '-') {
                    if (error)
                        g_error_free(error);
                    continue;
                } else {
                    CONFIGEXIT("Couldn't load overrideIpsFiles file (%s) %s", overrideIpsFiles[i], (error ? error->message : ""));
                }
            }
            arkime_config_parse_override_ips(keyfile);
            g_key_file_free(keyfile);
        }
    }

    arkime_db_install_override_ip();
}
/******************************************************************************/
LOCAL void arkime_config_parse_packet_ips(GKeyFile *keyFile)
{
    GError *error = 0;

    if (!g_key_file_has_group(keyFile, "packet-drop-ips"))
        return;

    gsize keys_len;
    gchar **keys = g_key_file_get_keys (keyFile, "packet-drop-ips", &keys_len, &error);
    if (error) {
        CONFIGEXIT("Error with packet-drop-ips: %s", error->message);
    }

    gsize k, v;
    for (k = 0 ; k < keys_len; k++) {
        gsize values_len;
        gchar **values = g_key_file_get_string_list(keyFile,
                                                    "packet-drop-ips",
                                                    keys[k],
                                                    &values_len,
                                                    NULL);
        int mode = 0;
        for (v = 0; v < values_len; v++) {
            if (strncmp(values[v], "drop", 4) == 0) {

            } else if (strncmp(values[v], "allow", 4) == 0) {
                mode = 1;
            } else {
                CONFIGEXIT("Unknown argument to packet-drop-ips %s %s", keys[k], values[v]);
            }
        }
        arkime_packet_add_packet_ip(keys[k], mode);
        g_strfreev(values);
    }
    g_strfreev(keys);
}
/******************************************************************************/
void arkime_config_load_packet_ips()
{
    GError *error = 0;

    if (g_key_file_has_group(arkimeKeyFile, "packet-ips")) {
        arkime_config_parse_packet_ips(arkimeKeyFile);
    }

    packetDropIpsFiles = arkime_config_str_list(NULL, "packetDropIpsFiles", NULL);
    if (packetDropIpsFiles) {
        for (int i = 0; packetDropIpsFiles[i]; i++) {
            GKeyFile *keyfile = g_key_file_new();
            gboolean status = g_key_file_load_from_file(keyfile, packetDropIpsFiles[i], G_KEY_FILE_NONE, &error);
            if (!status || error) {
                if (packetDropIpsFiles[i][0] == '-') {
                    if (error)
                        g_error_free(error);
                    continue;
                } else {
                    CONFIGEXIT("Couldn't load packetDropIpsFiles file (%s) %s", packetDropIpsFiles[i], (error ? error->message : ""));
                }
            }
            arkime_config_parse_packet_ips(keyfile);
            g_key_file_free(keyfile);
        }
    }

    arkime_packet_install_packet_ip();
}
/******************************************************************************/
void arkime_config_add_header(ArkimeStringHashStd_t *hash, char *key, int pos)
{
    ArkimeString_t *hstring;

    hstring = ARKIME_TYPE_ALLOC0(ArkimeString_t);
    hstring->str = key;
    hstring->len = strlen(key);
    hstring->uw = (gpointer)(long)pos;
    HASH_ADD(s_, *hash, hstring->str, hstring);
}
/******************************************************************************/
void arkime_config_load_header(char *section, char *group, char *helpBase, char *expBase, char *aliasBase, char *dbBase, ArkimeStringHashStd_t *hash, int flags)
{
    GError   *error = 0;
    char      name[100];

    if (!g_key_file_has_group(arkimeKeyFile, section))
        return;

    gsize keys_len;
    gchar **keys = g_key_file_get_keys (arkimeKeyFile, section, &keys_len, &error);
    if (error) {
        CONFIGEXIT("Error with %s: %s", section, error->message);
    }

    gsize k, v;
    for (k = 0 ; k < keys_len; k++) {
        gsize values_len;
        gchar **values = g_key_file_get_string_list(arkimeKeyFile,
                                                    section,
                                                    keys[k],
                                                    &values_len,
                                                    NULL);
        snprintf(name, sizeof(name), "%s", keys[k]);
        int type = 0;
        ArkimeFieldType t = ARKIME_FIELD_TYPE_INT;
        int unique = 1;
        int count  = 0;
        char *kind = 0;
        for (v = 0; v < values_len; v++) {
            if (strcmp(values[v], "type:integer") == 0 ||
                strcmp(values[v], "type:seconds") == 0 ||
                strcmp(values[v], "type:date") == 0) {
                type = 1;
                kind = values[v] + 5; // after type:
            } else if (strcmp(values[v], "type:ip") == 0) {
                type = 2;
            } else if (strcmp(values[v], "unique:false") == 0) {
                unique = 0;
            } else if (strcmp(values[v], "count:true") == 0) {
                count = 1;
            }
        }

        int f = flags;

        if (count)
            f |= ARKIME_FIELD_FLAG_CNT;

        switch (type) {
        case 0:
            kind = "termfield";
            if (unique)
                t = ARKIME_FIELD_TYPE_STR_HASH;
            else
                t = ARKIME_FIELD_TYPE_STR_ARRAY;
            break;
        case 1:
            if (unique)
                t = ARKIME_FIELD_TYPE_INT_GHASH;
            else
                t = ARKIME_FIELD_TYPE_INT_ARRAY;
            break;
        case 2:
            kind = "ip";
            t = ARKIME_FIELD_TYPE_IP_GHASH;
            break;
        }

        ArkimeString_t *hstring;

        HASH_FIND(s_, *hash, keys[k], hstring);
        if (hstring) {
            LOG("WARNING - ignoring field %s for %s", keys[k], section);
            g_strfreev(values);
            continue;
        }

        char expression[100];
        char field[100];
        char help[100];
        char aliases[205];

        snprintf(expression, sizeof(expression), "%s%s", expBase, name);
        if (aliasBase) {
            snprintf(aliases, sizeof(aliases), "[\"%s%s\"]", aliasBase, name);
        }
        snprintf(field, sizeof(field), "%s%s", dbBase, name);
        snprintf(help, sizeof(help), "%s%s", helpBase, name);

        int pos;
        pos = arkime_field_define(group, kind,
                                  expression, expression, field,
                                  help,
                                  t, f, "aliases", aliasBase ? aliases : NULL, (char *)NULL);
        arkime_config_add_header(hash, g_strdup(keys[k]), pos);
        g_strfreev(values);
    }
    g_strfreev(keys);
}

/******************************************************************************/
#define ARKIME_CONFIG_FILES 100
typedef struct {
    char                 *desc;
    int                   num;
    char                 *name[ARKIME_CONFIG_FILES];
    ArkimeFileChange_cb   cb;
    ArkimeFilesChange_cb  cbs;
    off_t                 size[ARKIME_CONFIG_FILES];
    int64_t               modify[ARKIME_CONFIG_FILES];
} ArkimeFileChange_t;

LOCAL GPtrArray          *files;
/******************************************************************************/
void arkime_config_monitor_file_msg(const char *desc, char *name, ArkimeFileChange_cb cb, const char *msg)
{
    struct stat     sb;

    if (stat(name, &sb) != 0) {
        CONFIGEXIT("Couldn't stat %s file '%s' with error '%s' %s", desc, name, strerror(errno), msg);
    }

    ArkimeFileChange_t *file = ARKIME_TYPE_ALLOC0(ArkimeFileChange_t);

    file->name[0] = g_strdup(name);
    file->modify[0] = sb.st_mtime;

    file->desc = g_strdup(desc);
    file->cb = cb;
    file->num = 1;

    g_ptr_array_add(files, file);

    cb(name);
}
/******************************************************************************/
void arkime_config_monitor_file(const char *desc, char *name, ArkimeFileChange_cb cb)
{
    arkime_config_monitor_file_msg(desc, name, cb, "");
}
/******************************************************************************/
void arkime_config_monitor_files(const char *desc, char **names, ArkimeFilesChange_cb cb)
{
    struct stat     sb;
    int             i;

    ArkimeFileChange_t *file = ARKIME_TYPE_ALLOC0(ArkimeFileChange_t);

    for (i = 0; i < ARKIME_CONFIG_FILES && names[i]; i++) {
        if (stat(names[i], &sb) != 0) {
            CONFIGEXIT("Couldn't stat %s file %s error %s", desc, names[i], strerror(errno));
        }

        file->name[i] = g_strdup(names[i]);
        file->modify[i] = sb.st_mtime;
    }

    file->desc = g_strdup(desc);
    file->cbs = cb;
    file->num = i;

    g_ptr_array_add(files, file);
    cb(names);
}
/******************************************************************************/
LOCAL gboolean arkime_config_reload_files (gpointer UNUSED(user_data))
{
    int             f;
    struct stat     sb[ARKIME_CONFIG_FILES];

    for (guint i = 0; i < files->len; i++) {
        int changed = 0;
        ArkimeFileChange_t *file = g_ptr_array_index(files, i);
        for (f = 0; f < file->num; f++) {
            if (stat(file->name[f], &sb[f]) != 0) {
                LOG("Couldn't stat %s file %s error %s", file->desc, file->name[f], strerror(errno));
                changed = 0;
                break;
            }

            if (sb[f].st_size <= 1) { // Ignore tiny files for reloads
                changed = 0;
                break;
            }

            if (sb[f].st_mtime > file->modify[f]) {
                if (file->size[f] != sb[f].st_size) {
                    file->size[f] = sb[f].st_size;
                    changed = 0;
                    break;
                }
                if (config.debug)
                    LOG("Changed %s %s", file->desc, file->name[f]);
                changed = 1;
            }
        }

        // Something was changed
        if (changed) {
            if (file->cbs)
                file->cbs(file->name);
            else
                file->cb(file->name[0]);

            for (f = 0; f < file->num; f++) {
                file->size[f] = 0;
                file->modify[f] = sb[f].st_mtime;
            }
        }
    }

    return G_SOURCE_CONTINUE;
}
/******************************************************************************/
typedef struct {
    char     *name;
    gpointer  var;
    int       typelen;
} ArkimeConfigVar_t;
LOCAL GHashTable        *arkimeConfigVarsHash = NULL;
LOCAL GPtrArray         *arkimeConfigVarsArray;
LOCAL gboolean           arkimeConfigVarsSorted = FALSE;
/******************************************************************************/
void arkime_config_register_cmd_var(const char *name, void *var, size_t typelen)
{
    if (!config.commandSocket && !config.commandList)
        return;

    ArkimeConfigVar_t *acv = ARKIME_TYPE_ALLOC0(ArkimeConfigVar_t);
    acv->name = g_strdup(name);
    acv->var = var;
    acv->typelen = (int)typelen;

    g_hash_table_insert(arkimeConfigVarsHash, acv->name, acv);
    g_ptr_array_add(arkimeConfigVarsArray, acv);
    arkimeConfigVarsSorted = FALSE;
}
/******************************************************************************/
LOCAL int arkime_config_vars_cmp(const void *a, const void *b)
{
    return strcmp((*(ArkimeConfigVar_t **)a)->name, (*(ArkimeConfigVar_t **)b)->name);
}
/******************************************************************************/
LOCAL void arkime_config_cmd_set(int argc, char **argv, gpointer cc)
{
    char buf[10000];
    BSB bsb;
    BSB_INIT(bsb, buf, sizeof(buf));

    if (!arkimeConfigVarsSorted) {
        g_ptr_array_sort(arkimeConfigVarsArray, arkime_config_vars_cmp);
        arkimeConfigVarsSorted = TRUE;
    }

    if (argc == 1) {
        for (guint i = 0; i < arkimeConfigVarsArray->len; i++) {
            const ArkimeConfigVar_t *acv = g_ptr_array_index(arkimeConfigVarsArray, i);
            switch (acv->typelen) {
            case 1:
                BSB_EXPORT_sprintf(bsb, "%s=%d\n", acv->name, *(char *)acv->var);
                break;
            case 2:
                BSB_EXPORT_sprintf(bsb, "%s=%d\n", acv->name, *(short *)acv->var);
                break;
            case 4:
                BSB_EXPORT_sprintf(bsb, "%s=%d\n", acv->name, *(int *)acv->var);
                break;
            case 8:
                BSB_EXPORT_sprintf(bsb, "%s=%" PRId64 "\n", acv->name, *(int64_t *)acv->var);
                break;
            case ARKIME_CONFIG_CMD_VAR_STR_PTR:
                if (!*(char **)acv->var)
                    BSB_EXPORT_sprintf(bsb, "%s=NULL\n", acv->name);
                else
                    BSB_EXPORT_sprintf(bsb, "%s=%s\n", acv->name, *(char **)acv->var);
                break;
            default:
                BSB_EXPORT_sprintf(bsb, "%s=unknown\n", acv->name);
            }
        }
        arkime_command_respond(cc, buf, BSB_LENGTH(bsb));
        return;
    } else if (argc == 2) {
        const ArkimeConfigVar_t *acv = g_hash_table_lookup(arkimeConfigVarsHash, argv[1]);
        if (!acv) {
            BSB_EXPORT_sprintf(bsb, "Unknown variable: %s\n", argv[1]);
            arkime_command_respond(cc, buf, BSB_LENGTH(bsb));
            return;
        }
        switch (acv->typelen) {
        case 1:
            BSB_EXPORT_sprintf(bsb, "%d\n",  *(char *)acv->var);
            break;
        case 2:
            BSB_EXPORT_sprintf(bsb, "%d\n", *(short *)acv->var);
            break;
        case 4:
            BSB_EXPORT_sprintf(bsb, "%d\n", *(int *)acv->var);
            break;
        case 8:
            BSB_EXPORT_sprintf(bsb, "%" PRId64 "\n", *(int64_t *)acv->var);
            break;
        case ARKIME_CONFIG_CMD_VAR_STR_PTR:
            if (!*(char *)acv->var)
                BSB_EXPORT_sprintf(bsb, "NULL\n");
            else
                BSB_EXPORT_sprintf(bsb, "%s\n", *(char **)acv->var);
            break;
        default:
            BSB_EXPORT_sprintf(bsb, "unknown\n");
        }
        arkime_command_respond(cc, buf, BSB_LENGTH(bsb));
        return;
    } else if (argc == 3) {
        ArkimeConfigVar_t *acv = g_hash_table_lookup(arkimeConfigVarsHash, argv[1]);
        if (!acv) {
            BSB_EXPORT_sprintf(bsb, "Unknown variable: %s\n", argv[1]);
            arkime_command_respond(cc, buf, BSB_LENGTH(bsb));
            return;
        }

        switch (acv->typelen) {
        case 1:
            *(char *)acv->var = atoi(argv[2]);
            BSB_EXPORT_sprintf(bsb, "%s=%d\n", acv->name, *(char *)acv->var);
            break;
        case 2:
            *(short *)acv->var = atoi(argv[2]);
            BSB_EXPORT_sprintf(bsb, "%s=%d\n", acv->name, *(short *)acv->var);
            break;
        case 4:
            *(int *)acv->var = atoi(argv[2]);
            BSB_EXPORT_sprintf(bsb, "%s=%d\n", acv->name, *(int *)acv->var);
            break;
        case 8:
            *(int64_t *)acv->var = atoi(argv[2]);
            BSB_EXPORT_sprintf(bsb, "%s=%" PRId64 "\n", acv->name, *(int64_t *)acv->var);
            break;
        case ARKIME_CONFIG_CMD_VAR_STR_PTR:
            g_free(*(char **)acv->var);
            if (strcmp(argv[2], "NULL") == 0) {
                *(char **)acv->var = NULL;
                BSB_EXPORT_sprintf(bsb, "NULL\n");
            } else {
                *(char **)acv->var = g_strdup(argv[2]);
                BSB_EXPORT_sprintf(bsb, "%s\n", *(char **)acv->var);
            }
            break;
        default:
            BSB_EXPORT_sprintf(bsb, "Unknown variable: %s\n", argv[1]);
        }

        arkime_command_respond(cc, buf, BSB_LENGTH(bsb));
    }
}
/******************************************************************************/
#ifdef SUPPORT_CMD_LIST
LOCAL void arkime_config_cmd_override_print(gpointer key, gpointer value, gpointer cc)
{
    char buf[1000];
    snprintf(buf, sizeof(buf), "%s=%s\n", (char *)key, (char *)value);
    arkime_command_respond(cc, buf, -1);
}
/******************************************************************************/
LOCAL void arkime_config_cmd_list(int UNUSED(argc), char UNUSED(**argv), gpointer cc)
{
    if (config.override) {
        arkime_command_respond(cc, "[OVERRIDE]\n", -1);
        g_hash_table_foreach(config.override, arkime_config_cmd_override_print, cc);
        arkime_command_respond(cc, "\n", -1);
    }
    char *data = g_key_file_to_data(arkimeKeyFile, NULL, NULL);
    arkime_command_respond(cc, data, -1);
    g_free(data);
}
#endif
/******************************************************************************/
void arkime_config_check(const char *prefix, ...)
{
    va_list args;
    const char *key;

    // Create a GHashTable without key/value destroy notifiers
    GHashTable *expected_keys = g_hash_table_new(g_str_hash, g_str_equal);

    va_start(args, prefix);
    while ((key = va_arg(args, const char *)) != NULL) {
        g_hash_table_insert(expected_keys, (gpointer)key, NULL);
    }
    va_end(args);

    gchar *sections[3];
    sections[0] = "default";
    sections[1] = config.nodeClass;
    sections[2] = config.nodeName;

    for (int s = 0; s < 3; s++) {
        if (!sections[s])
            continue;

        gsize num_keys;
        gchar **keys;

        keys = g_key_file_get_keys(arkimeKeyFile, sections[s], &num_keys, NULL);

        if (!keys)
            continue;

        for (gsize j = 0; j < num_keys; j++) {
            if (g_str_has_prefix(keys[j], prefix) && !g_hash_table_contains(expected_keys, keys[j])) {
                CONFIGEXIT("In section '%s' unknown key '%s' in config file", sections[s], keys[j]);
            }
        }

        g_strfreev(keys);
    }
    g_hash_table_destroy(expected_keys);
}

/******************************************************************************/
void arkime_config_init()
{
    if (config.commandSocket || config.commandList) {
        arkimeConfigVarsArray = g_ptr_array_new();
        arkimeConfigVarsHash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        arkime_config_register_cmd_var("debug", &config.debug, sizeof(config.debug));
        arkime_config_register_cmd_var("quiet", &config.quiet, sizeof(config.quiet));
        arkime_config_register_cmd_var("recursive", &config.pcapRecursive, sizeof(config.pcapRecursive));
        arkime_config_register_cmd_var("skip", &config.pcapSkip, sizeof(config.pcapSkip));
        arkime_config_register_cmd_var("reprocess", &config.pcapReprocess, sizeof(config.pcapReprocess));
        arkime_config_register_cmd_var("flush", &config.flushBetween, sizeof(config.flushBetween));
        arkime_config_register_cmd_var("ignoreerrors", &config.ignoreErrors, sizeof(config.ignoreErrors));
        arkime_config_register_cmd_var("profile", &config.profile, ARKIME_CONFIG_CMD_VAR_STR_PTR);
    }

    HASH_INIT(s_, config.dontSaveTags, arkime_string_hash, arkime_string_cmp);
    files = g_ptr_array_new();

    arkime_config_load();

    if (config.debug) {
        LOG("maxFileSizeB: %" PRIu64, config.maxFileSizeB);
    }

    if (config.interface && !config.interface[0]) {
        CONFIGEXIT("The interface= is set in the config file (%s), but it is empty. :( You need to fix this before Arkime can continue.", config.configFile);
    }

    if (!config.interface && !config.pcapReadOffline) {
        CONFIGEXIT("Please set interface= in the [default] or [%s] section of the config file (%s) OR on the capture command line use either a pcap file (-r) or pcap directory (-R) switch. You need to fix this before Arkime can continue.", config.nodeName, config.configFile);
    }

    if (!config.pcapDir || !config.pcapDir[0]) {
        CONFIGEXIT("You must set a non empty pcapDir= in the config file(%s) to save files to. You need to fix this before Arkime can continue.", config.configFile);
    }

    if (!config.dryRun) {
        g_timeout_add_seconds( 10, arkime_config_reload_files, 0);
    }

    arkime_command_register("set", arkime_config_cmd_set, "Set/Get a config value - set [<name> [<value>]]");
#ifdef SUPPORT_CMD_LIST
    arkime_command_register("config-list", arkime_config_cmd_list, "List raw config");
#endif
}
/******************************************************************************/
void arkime_config_exit()
{
    g_key_file_free(arkimeKeyFile);

    if (config.nodeClass)
        g_free(config.nodeClass);
    if (config.interface)
        g_strfreev(config.interface);
    if (config.elasticsearch)
        g_free(config.elasticsearch);
    if (config.bpf)
        g_free(config.bpf);
    if (config.yara)
        g_free(config.yara);
    if (config.pcapDir)
        g_strfreev(config.pcapDir);
    if (config.pcapDirTemplate)
        g_free(config.pcapDirTemplate);
    if (config.pluginsDir)
        g_strfreev(config.pluginsDir);
    if (config.parsersDir)
        g_strfreev(config.parsersDir);
    if (config.plugins)
        g_strfreev(config.plugins);
    if (config.rootPlugins)
        g_strfreev(config.rootPlugins);
    if (config.smtpIpHeaders)
        g_strfreev(config.smtpIpHeaders);

    g_strfreev(overrideIpsFiles);
    g_strfreev(packetDropIpsFiles);
}
