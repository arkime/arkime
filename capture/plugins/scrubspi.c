/*
 * Scrub data before sending to ES.
 *
 * 1) Add scrubspi.so to plugins in config
 *
 * 2) Configure the fields to scrub, the first character after the = is used as the delimeter.
 *
 *   Prior to 0.17) a single variable of scrubspi=fieldexp1,fieldexp2=/search regex/replace string/   example: scrubspi=http.uri=/github/foohub/
 *   Since 0.17) create a section called [scrubspi] with each line being one expression
 *     [scrubspi]
 *     http.uri=/github/foohub/
 *     asn.dst=:AOL:EXAMPLE:
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "moloch.h"

extern MolochConfig_t        config;

typedef struct {
    int     pos;
    GRegex *search;
    char   *replace;
} SS_t;

#define MAX_SS 100
LOCAL int                ssLen;
LOCAL SS_t               ss[MAX_SS];


/******************************************************************************/
LOCAL void scrubspi_plugin_save(MolochSession_t *session, int UNUSED(final))
{
    int                    s;
    guint                  i;
    gchar                 *newstr;
    MolochStringHashStd_t *shash;
    MolochString_t        *hstring;

    for (s = 0; s < ssLen; s++) {
        const int pos = ss[s].pos;
        if (!session->fields[pos])
            continue;

        MolochFieldInfo_t *field = config.fields[pos];
        switch (field->type) {
        case MOLOCH_FIELD_TYPE_STR:
            newstr = g_regex_replace(ss[s].search, session->fields[pos]->str, -1, 0, ss[s].replace, 0, NULL);
            if (newstr) {
                g_free(session->fields[pos]->str);
                session->fields[pos]->str = newstr;
            }
            break;
        case MOLOCH_FIELD_TYPE_STR_ARRAY:
            for(i = 0; i < session->fields[pos]->sarray->len; i++) {
                newstr = g_regex_replace(ss[s].search, g_ptr_array_index(session->fields[pos]->sarray, i), -1, 0, ss[s].replace, 0, NULL);
                if (newstr) {
                    g_free(g_ptr_array_index(session->fields[pos]->sarray, i));
                    g_ptr_array_index(session->fields[pos]->sarray, i) = newstr;
                }
            }
            break;
        case MOLOCH_FIELD_TYPE_STR_HASH:
            shash = session->fields[pos]->shash;
            HASH_FORALL(s_, *shash, hstring,
                newstr = g_regex_replace(ss[s].search, hstring->str, -1, 0, ss[s].replace, 0, NULL);
                if (newstr) {
                    g_free(hstring->str);
                    hstring->str = newstr;
                }
            );

            break;
        case MOLOCH_FIELD_TYPE_STR_GHASH:
            LOGEXIT("MOLOCH_FIELD_TYPE_STR_GHASH not supported yet, open an issue");
            break;
        }
    }
}
/******************************************************************************/
LOCAL void scrubspi_add_entry(char *key, char *value)
{
    char spliton[2] = {0, 0};
    spliton[0] = value[0];
    char **values = g_strsplit(value+1, spliton, 0); // Don't free

    if (!values[0] || !values[1])
        LOGEXIT("'%s' bad format, should be '/search pcre/replace literal/', where the '/' can be any char in all three places", value);

    GError *error = NULL;
    GRegex *search = g_regex_new(values[0], G_REGEX_OPTIMIZE, 0, &error);
    if (!search || error)
        LOGEXIT("Couldn't compile %s %s", values[0], error->message);

    char **keys = g_strsplit(key, ",", 0);

    int j;
    for (j = 0; keys[j]; j++) {
        int pos = moloch_field_by_exp(keys[j]);
        if (pos == -1)
            LOGEXIT("Field %s in section [scrubspi] not found", keys[j]);
        if (ssLen >= MAX_SS)
            LOGEXIT("Too many [scrubspi] items, max is %d", MAX_SS);
        MolochFieldInfo_t *field = config.fields[pos];
        if (field->type != MOLOCH_FIELD_TYPE_STR &&
            field->type != MOLOCH_FIELD_TYPE_STR_ARRAY &&
            field->type != MOLOCH_FIELD_TYPE_STR_HASH &&
            field->type != MOLOCH_FIELD_TYPE_STR_GHASH) {
            LOGEXIT("Field %s in [scrubspi] is not of type string", keys[j]);
        }
        ss[ssLen].pos     = pos;
        ss[ssLen].search  = search;
        ss[ssLen].replace = values[1];
        ssLen++;
    }
    g_strfreev(keys);
}
/******************************************************************************/
void moloch_plugin_init()
{
    moloch_plugins_register("scrubspi", FALSE);

    moloch_plugins_set_cb("scrubspi",
      NULL,
      NULL,
      NULL,
      NULL,
      scrubspi_plugin_save,
      NULL,
      NULL,
      NULL
    );

#if MOLOCH_API_VERSION >= 17
    gsize keys_len;
    gchar **keys = moloch_config_section_keys(NULL, "scrubspi", &keys_len);
    if (!keys)
        LOGEXIT("Missing [scrubspi] section in config file");

    if (keys_len == 0)
        LOG("WARNING - [scrubspi] section is empty");

    gsize i;
    for (i = 0; i < keys_len; i++) {
        char *value = moloch_config_section_str(NULL, "scrubspi", keys[i], NULL);
        if (value == NULL)
            LOGEXIT("No value for %s in section [scrubspi]", keys[i]);

        scrubspi_add_entry(keys[i], value);
    }
    g_strfreev(keys);
#else
    gchar *key = moloch_config_str(NULL, "scrubspi", NULL);
    if (!key)
        LOGEXIT("Must set scrubspi variable in config");
    gchar *equal = strchr(key, '=');
    if (!equal)
        LOGEXIT("scrubspi variable missing value");
    *equal = 0;
    scrubspi_add_entry(key, equal+1);
    g_free(key);
#endif
}
