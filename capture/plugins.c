/******************************************************************************/
/* plugins.c  -- Functions dealing with plugins
 *
 * Copyright 2012 AOL Inc. All rights reserved.
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <string.h>
#include <netdb.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/statvfs.h>
#include <ctype.h>
#include "glib.h"
#include "gmodule.h"
#include "moloch.h"

/******************************************************************************/
extern MolochConfig_t        config;
extern gboolean              dryRun;
extern gboolean              debug;

uint32_t                     pluginsCbs = 0;
int                          numPlugins = 0;

/******************************************************************************/
typedef struct moloch_plugin {
    struct moloch_plugin    *p_next, *p_prev;
    char                    *name;
    short                    p_bucket;
    short                    p_count;

    int                      num;

    MolochPluginIpFunc       ipFunc;
    MolochPluginUdpFunc      udpFunc;
    MolochPluginTcpFunc      tcpFunc;
    MolochPluginSaveFunc     saveFunc;
    MolochPluginNewFunc      newFunc;
    MolochPluginExitFunc     exitFunc;

    MolochPluginHttpFunc     on_message_begin;
    MolochPluginHttpDataFunc on_url;
    MolochPluginHttpDataFunc on_header_field;
    MolochPluginHttpDataFunc on_header_value;
    MolochPluginHttpFunc     on_headers_complete;
    MolochPluginHttpDataFunc on_body;
    MolochPluginHttpFunc     on_message_complete;
} MolochPlugin_t;

HASH_VAR(p_, plugins, MolochPlugin_t, 11);
/******************************************************************************/
void moloch_plugins_init()
{
    HASH_INIT(p_, plugins, moloch_string_hash, moloch_string_cmp);

    if (!config.pluginsDir)
        return;

    if (!config.plugins)
        return;

    if (!g_module_supported ()) {
        LOG("ERROR - glib compiled without module support");
        return;
    }

    int         i;

    for (i = 0; config.plugins[i]; i++) {
        const char *name = config.plugins[i];
        while (isspace(*name))
            name++;
        g_strchomp((char *)name);

        gchar   *path = g_build_filename (config.pluginsDir, name, NULL);
        GModule *plugin = g_module_open (path, 0); /*G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);*/

        if (!plugin) {
            LOG("ERROR - Couldn't load plugin %s from '%s'\n%s", name, path, g_module_error());
            g_free (path);
            continue;
        }
        g_free (path);

        MolochPluginInitFunc plugin_init;

        if (!g_module_symbol(plugin, "moloch_plugin_init", (gpointer *)&plugin_init) || plugin_init == NULL) {
            LOG("ERROR - Module %s doesn't have a moloch_plugin_init", name);
            continue;
        }

        plugin_init();
    }
}
/******************************************************************************/
int moloch_plugins_register(const char *            name,
                            gboolean                storeData)
{
    MolochPlugin_t *plugin;

    HASH_FIND(p_, plugins, name, plugin);
    if (plugin) {
        LOG("Plugin %s is already registered", name);
        exit(-1);
    }
    
    plugin = malloc(sizeof(MolochPlugin_t));
    memset(plugin, 0, sizeof(*plugin));
    plugin->name = strdup(name);
    if (storeData) {
        plugin->num  = numPlugins++;
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
                           MolochPluginSaveFunc    saveFunc,
                           MolochPluginNewFunc     newFunc,
                           MolochPluginExitFunc    exitFunc)
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

    plugin->saveFunc = saveFunc;
    if (saveFunc)
        pluginsCbs |= MOLOCH_PLUGIN_SAVE;

    plugin->newFunc = newFunc;
    if (newFunc)
        pluginsCbs |= MOLOCH_PLUGIN_NEW;

    plugin->exitFunc = exitFunc;
    if (exitFunc)
        pluginsCbs |= MOLOCH_PLUGIN_EXIT;
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
void moloch_plugins_cb_tcp(MolochSession_t *session, struct tcp_stream *a_tcp)
{
    MolochPlugin_t *plugin;

    HASH_FORALL(p_, plugins, plugin, 
        if (plugin->tcpFunc)
            plugin->tcpFunc(session, a_tcp);
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
void moloch_plugins_cb_ip(MolochSession_t *session, struct ip *packet, int len)
{
    MolochPlugin_t *plugin;

    HASH_FORALL(p_, plugins, plugin, 
        if (plugin->ipFunc)
            plugin->ipFunc(session, packet, len);
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
void moloch_plugins_exit()
{
    MolochPlugin_t *plugin;

    HASH_FORALL(p_, plugins, plugin, 
        if (plugin->exitFunc)
            plugin->exitFunc();
    );

    HASH_FORALL_POP_HEAD(p_, plugins, plugin, 
        free(plugin->name);
        free(plugin);
    );
}
