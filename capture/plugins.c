/******************************************************************************/
/* plugins.c  -- Functions dealing with plugins
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
#include <inttypes.h>
#include <errno.h>
#include "gmodule.h"

/******************************************************************************/
extern MolochConfig_t        config;

uint32_t                     pluginsCbs = 0;

/******************************************************************************/
typedef struct moloch_plugin {
    struct moloch_plugin        *p_next, *p_prev;
    char                        *name;
    uint32_t                     p_hash;
    short                        p_bucket;
    short                        p_count;

    int                          num;

    MolochPluginIpFunc           ipFunc;
    MolochPluginUdpFunc          udpFunc;
    MolochPluginTcpFunc          tcpFunc;
    MolochPluginSaveFunc         preSaveFunc;
    MolochPluginSaveFunc         saveFunc;
    MolochPluginNewFunc          newFunc;
    MolochPluginExitFunc         exitFunc;
    MolochPluginReloadFunc       reloadFunc;
    MolochPluginOutstandingFunc  outstandingFunc;

    MolochPluginHttpFunc         on_message_begin;
    MolochPluginHttpDataFunc     on_url;
    MolochPluginHttpDataFunc     on_header_field;
    MolochPluginHttpDataFunc     on_header_value;
    MolochPluginHttpFunc         on_headers_complete;
    MolochPluginHttpDataFunc     on_body;
    MolochPluginHttpFunc         on_message_complete;

    MolochPluginSMTPHeaderFunc   smtp_on_header;
    MolochPluginSMTPFunc         smtp_on_header_complete;
} MolochPlugin_t;

HASH_VAR(p_, plugins, MolochPlugin_t, 11);
/******************************************************************************/
void moloch_plugins_init()
{
    HASH_INIT(p_, plugins, moloch_string_hash, moloch_string_cmp);
}

/******************************************************************************/
void moloch_plugins_load(char **plugins) {

    if (!config.pluginsDir)
        return;

    if (!plugins)
        return;

    if (!g_module_supported ()) {
        LOG("ERROR - glib compiled without module support");
        return;
    }

    moloch_add_can_quit((MolochCanQuitFunc)moloch_plugins_outstanding, "plugin outstanding");

    int         i;

    for (i = 0; plugins[i]; i++) {
        const char *name = plugins[i];

        int d;
        GModule *plugin = 0;
        for (d = 0; config.pluginsDir[d]; d++) {
            gchar   *path = g_build_filename (config.pluginsDir[d], name, NULL);

            if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
                g_free (path);
                continue;
            }

            plugin = g_module_open (path, 0); /*G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);*/

            if (!plugin) {
                LOG("ERROR - Couldn't load plugin %s from '%s'\n%s", name, path, g_module_error());
                g_free (path);
                continue;
            }

            g_free (path);
            break;
        }

        if (!plugin) {
            LOG("WARNING - plugin '%s' not found", name);
            continue;
        }

        MolochPluginInitFunc plugin_init;

        if (!g_module_symbol(plugin, "moloch_plugin_init", (gpointer *)(char*)&plugin_init) || plugin_init == NULL) {
            LOG("ERROR - Module %s doesn't have a moloch_plugin_init", name);
            continue;
        }

        plugin_init();
    }
}
/******************************************************************************/
int moloch_plugins_register_internal(const char *            name,
                                     gboolean                storeData,
                                     size_t                  sessionsize,
                                     int                     apiversion)
{
    MolochPlugin_t *plugin;

    if (sizeof(MolochSession_t) != sessionsize) {
        LOGEXIT("Plugin '%s' built with different version of moloch.h", name);
    }

    if (MOLOCH_API_VERSION != apiversion) {
        LOGEXIT("Plugin '%s' built with different version of moloch.h", name);
    }

    HASH_FIND(p_, plugins, name, plugin);
    if (plugin) {
        LOGEXIT("Plugin %s is already registered", name);
    }

    plugin = MOLOCH_TYPE_ALLOC0(MolochPlugin_t);
    plugin->name = strdup(name);
    if (storeData) {
        plugin->num  = config.numPlugins++;
    } else {
        plugin->num  = -1;
    }
    HASH_ADD(p_, plugins, name, plugin);
    return plugin->num;
}
/******************************************************************************/
void moloch_plugins_set_cb(const char *            name,
                           MolochPluginIpFunc      ipFunc,
                           MolochPluginUdpFunc     udpFunc,
                           MolochPluginTcpFunc     tcpFunc,
                           MolochPluginSaveFunc    preSaveFunc,
                           MolochPluginSaveFunc    saveFunc,
                           MolochPluginNewFunc     newFunc,
                           MolochPluginExitFunc    exitFunc,
                           MolochPluginReloadFunc  reloadFunc)
{
    MolochPlugin_t *plugin;

    HASH_FIND(p_, plugins, name, plugin);
    if (!plugin) {
        LOG("Can't find plugin with name %s", name);
        return;
    }

    plugin->ipFunc = ipFunc;
    if (ipFunc)
        pluginsCbs |= MOLOCH_PLUGIN_IP;

    plugin->udpFunc = udpFunc;
    if (udpFunc)
        pluginsCbs |= MOLOCH_PLUGIN_UDP;

    plugin->tcpFunc = tcpFunc;
    if (tcpFunc)
        pluginsCbs |= MOLOCH_PLUGIN_TCP;

    plugin->preSaveFunc = preSaveFunc;
    if (preSaveFunc)
        pluginsCbs |= MOLOCH_PLUGIN_PRE_SAVE;

    plugin->saveFunc = saveFunc;
    if (saveFunc)
        pluginsCbs |= MOLOCH_PLUGIN_SAVE;

    plugin->newFunc = newFunc;
    if (newFunc)
        pluginsCbs |= MOLOCH_PLUGIN_NEW;

    plugin->exitFunc = exitFunc;
    if (exitFunc)
        pluginsCbs |= MOLOCH_PLUGIN_EXIT;

    plugin->reloadFunc = reloadFunc;
    if (reloadFunc)
        pluginsCbs |= MOLOCH_PLUGIN_RELOAD;
}
/******************************************************************************/
void moloch_plugins_set_http_cb(const char *             name,
                                MolochPluginHttpFunc     on_message_begin,
                                MolochPluginHttpDataFunc on_url,
                                MolochPluginHttpDataFunc on_header_field,
                                MolochPluginHttpDataFunc on_header_value,
                                MolochPluginHttpFunc     on_headers_complete,
                                MolochPluginHttpDataFunc on_body,
                                MolochPluginHttpFunc     on_message_complete)
{
    MolochPlugin_t *plugin;

    HASH_FIND(p_, plugins, name, plugin);
    if (!plugin) {
        LOG("Can't find plugin with name %s", name);
        return;
    }

    plugin->on_message_begin = on_message_begin;
    if (on_message_begin)
        pluginsCbs |= MOLOCH_PLUGIN_HP_OMB;

    plugin->on_url = on_url;
    if (on_url)
        pluginsCbs |= MOLOCH_PLUGIN_HP_OU;

    plugin->on_header_field = on_header_field;
    if (on_header_field)
        pluginsCbs |= MOLOCH_PLUGIN_HP_OHF;

    plugin->on_header_value = on_header_value;
    if (on_header_value)
        pluginsCbs |= MOLOCH_PLUGIN_HP_OHV;

    plugin->on_headers_complete = on_headers_complete;
    if (on_headers_complete)
        pluginsCbs |= MOLOCH_PLUGIN_HP_OHC;

    plugin->on_body = on_body;
    if (on_body)
        pluginsCbs |= MOLOCH_PLUGIN_HP_OB;

    plugin->on_message_complete = on_message_complete;
    if (on_message_complete)
        pluginsCbs |= MOLOCH_PLUGIN_HP_OMC;

}
/******************************************************************************/
void moloch_plugins_set_smtp_cb(const char *                name,
                                MolochPluginSMTPHeaderFunc  on_header,
                                MolochPluginSMTPFunc        on_header_complete)
{
    MolochPlugin_t *plugin;

    HASH_FIND(p_, plugins, name, plugin);
    if (!plugin) {
        LOG("Can't find plugin with name %s", name);
        return;
    }

    plugin->smtp_on_header = on_header;
    if (on_header)
        pluginsCbs |= MOLOCH_PLUGIN_SMTP_OH;

    plugin->smtp_on_header_complete = on_header_complete;
    if (on_header_complete)
        pluginsCbs |= MOLOCH_PLUGIN_SMTP_OHC;
}
/******************************************************************************/
void moloch_plugins_set_outstanding_cb(const char *                name,
                                MolochPluginOutstandingFunc        outstanding)
{
    MolochPlugin_t *plugin;

    HASH_FIND(p_, plugins, name, plugin);
    if (!plugin) {
        LOG("Can't find plugin with name %s", name);
        return;
    }

    plugin->outstandingFunc = outstanding;
}
/******************************************************************************/
void moloch_plugins_cb_pre_save(MolochSession_t *session, int final)
{
    MolochPlugin_t *plugin;

    HASH_FORALL(p_, plugins, plugin,
        if (plugin->preSaveFunc)
            plugin->preSaveFunc(session, final);
    );
}
/******************************************************************************/
void moloch_plugins_cb_save(MolochSession_t *session, int final)
{
    MolochPlugin_t *plugin;

    HASH_FORALL(p_, plugins, plugin,
        if (plugin->saveFunc)
            plugin->saveFunc(session, final);
    );
}
/******************************************************************************/
void moloch_plugins_cb_new(MolochSession_t *session)
{
    MolochPlugin_t *plugin;

    HASH_FORALL(p_, plugins, plugin,
        if (plugin->newFunc)
            plugin->newFunc(session);
    );
}
/******************************************************************************/
void moloch_plugins_cb_tcp(MolochSession_t *session, unsigned char *data, int len)
{
    MolochPlugin_t *plugin;

    HASH_FORALL(p_, plugins, plugin,
        if (plugin->tcpFunc)
            plugin->tcpFunc(session, data, len);
    );
}
/******************************************************************************/
void moloch_plugins_cb_udp(MolochSession_t *session, struct udphdr *udphdr, unsigned char *data, int len)
{
    MolochPlugin_t *plugin;

    HASH_FORALL(p_, plugins, plugin,
        if (plugin->udpFunc)
            plugin->udpFunc(session, udphdr, data, len);
    );
}
/******************************************************************************/
void moloch_plugins_cb_hp_omb(MolochSession_t *session, http_parser *parser)
{
    MolochPlugin_t *plugin;

    HASH_FORALL(p_, plugins, plugin,
        if (plugin->on_message_begin)
            plugin->on_message_begin(session, parser);
    );
}
/******************************************************************************/
void moloch_plugins_cb_hp_ou(MolochSession_t *session, http_parser *parser, const char *at, size_t length)
{
    MolochPlugin_t *plugin;

    HASH_FORALL(p_, plugins, plugin,
        if (plugin->on_url)
            plugin->on_url(session, parser, at, length);
    );
}
/******************************************************************************/
void moloch_plugins_cb_hp_ohf(MolochSession_t *session, http_parser *parser, const char *at, size_t length)
{
    MolochPlugin_t *plugin;

    HASH_FORALL(p_, plugins, plugin,
        if (plugin->on_header_field)
            plugin->on_header_field(session, parser, at, length);
    );
}
/******************************************************************************/
void moloch_plugins_cb_hp_ohv(MolochSession_t *session, http_parser *parser, const char *at, size_t length)
{
    MolochPlugin_t *plugin;

    HASH_FORALL(p_, plugins, plugin,
        if (plugin->on_header_value)
            plugin->on_header_value(session, parser, at, length);
    );
}
/******************************************************************************/
void moloch_plugins_cb_hp_ohc(MolochSession_t *session, http_parser *parser)
{
    MolochPlugin_t *plugin;

    HASH_FORALL(p_, plugins, plugin,
        if (plugin->on_headers_complete)
            plugin->on_headers_complete(session, parser);
    );
}
/******************************************************************************/
void moloch_plugins_cb_hp_ob(MolochSession_t *session, http_parser *parser, const char *at, size_t length)
{
    MolochPlugin_t *plugin;

    HASH_FORALL(p_, plugins, plugin,
        if (plugin->on_body)
            plugin->on_body(session, parser, at, length);
    );
}
/******************************************************************************/
void moloch_plugins_cb_hp_omc(MolochSession_t *session, http_parser *parser)
{
    MolochPlugin_t *plugin;

    HASH_FORALL(p_, plugins, plugin,
        if (plugin->on_message_complete)
            plugin->on_message_complete(session, parser);
    );
}
/******************************************************************************/
void moloch_plugins_cb_smtp_oh(MolochSession_t *session, const char *field, size_t field_len, const char *value, size_t value_len)
{
    MolochPlugin_t *plugin;

    HASH_FORALL(p_, plugins, plugin,
        if (plugin->smtp_on_header)
            plugin->smtp_on_header(session, field, field_len, value, value_len);
    );
}
/******************************************************************************/
void moloch_plugins_cb_smtp_ohc(MolochSession_t *session)
{
    MolochPlugin_t *plugin;

    HASH_FORALL(p_, plugins, plugin,
        if (plugin->smtp_on_header_complete)
            plugin->smtp_on_header_complete(session);
    );
}
/******************************************************************************/
void moloch_plugins_exit()
{
    MolochPlugin_t *plugin;

    HASH_FORALL(p_, plugins, plugin,
        if (plugin->exitFunc)
            plugin->exitFunc();
    );

    HASH_FORALL_POP_HEAD(p_, plugins, plugin,
        free(plugin->name);
        MOLOCH_TYPE_FREE(MolochPlugin_t, plugin);
    );
}
/******************************************************************************/
void moloch_plugins_reload()
{
    MolochPlugin_t *plugin;

    HASH_FORALL(p_, plugins, plugin,
        if (plugin->reloadFunc)
            plugin->reloadFunc();
    );
}
/******************************************************************************/
uint32_t moloch_plugins_outstanding()
{
    MolochPlugin_t *plugin;
    uint32_t        outstanding = 0;

    HASH_FORALL(p_, plugins, plugin,
        if (plugin->outstandingFunc)
            outstanding += plugin->outstandingFunc();
    );
    return outstanding;
}
