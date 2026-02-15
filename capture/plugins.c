/******************************************************************************/
/* plugins.c  -- Functions dealing with plugins
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <inttypes.h>
#include <errno.h>
#include "gmodule.h"

/******************************************************************************/
extern ArkimeConfig_t        config;

uint32_t                     pluginsCbs = 0;

/******************************************************************************/
typedef struct arkime_plugin {
    struct arkime_plugin        *p_next, *p_prev;
    char                        *name;
    uint32_t                     p_hash;
    short                        p_bucket;
    short                        p_count;

    int                          num;

    ArkimePluginUdpFunc          udpFunc;
    ArkimePluginTcpFunc          tcpFunc;
    ArkimePluginSaveFunc         preSaveFunc;
    ArkimePluginSaveFunc         saveFunc;
    ArkimePluginNewFunc          newFunc;
    ArkimePluginExitFunc         exitFunc;
    ArkimePluginReloadFunc       reloadFunc;
    ArkimePluginOutstandingFunc  outstandingFunc;

    ArkimePluginHttpFunc         on_message_begin;
    ArkimePluginHttpDataFunc     on_url;
    ArkimePluginHttpDataFunc     on_header_field;
    ArkimePluginHttpDataFunc     on_header_field_raw;
    ArkimePluginHttpDataFunc     on_header_value;
    ArkimePluginHttpFunc         on_headers_complete;
    ArkimePluginHttpDataFunc     on_body;
    ArkimePluginHttpFunc         on_message_complete;

    ArkimePluginSMTPHeaderFunc   smtp_on_header;
    ArkimePluginSMTPFunc         smtp_on_header_complete;
} ArkimePlugin_t;

HASH_VAR(p_, plugins, ArkimePlugin_t, 11);

typedef struct {
    const char           *extension;
    ArkimePluginLoadFunc  loadFunc;
} ArkimeExtensions_t;
LOCAL GPtrArray *extensionsArr;

LOCAL uint32_t arkime_plugins_outstanding();
/******************************************************************************/
LOCAL void arkime_plugins_cmd_list(int UNUSED(argc), char UNUSED( * *argv), gpointer cc)
{
    char buf[10000];
    BSB bsb;
    BSB_INIT(bsb, buf, sizeof(buf));

    const ArkimePlugin_t *plugin;

    if (HASH_COUNT(p_, plugins) == 0) {
        BSB_EXPORT_sprintf(bsb, "No plugins loaded\n");
    }
    HASH_FORALL2(p_, plugins, plugin) {
        BSB_EXPORT_sprintf(bsb, "%s\n", plugin->name);
    }
    arkime_command_respond(cc, buf, BSB_LENGTH(bsb));
}

/******************************************************************************/
void arkime_plugins_load(char **plugins)
{

    if (!config.pluginsDir)
        return;

    if (!plugins)
        return;

    arkime_add_can_quit((ArkimeCanQuitFunc)arkime_plugins_outstanding, "plugins outstanding");

    for (int i = 0; plugins[i]; i++) {
        const char *name = plugins[i];

        guint e;
        for (e = 0; e < extensionsArr->len; e++) {
            if (g_str_has_suffix(name, ((ArkimeExtensions_t *)g_ptr_array_index(extensionsArr, e))->extension)) {
                break;
            }
        }

        if (e == extensionsArr->len) {
            LOG("WARNING - plugin '%s' has unknown extension", name);
            continue;
        }

        gchar   *path;
        int      loaded = 0;
        for (int d = 0; config.pluginsDir[d]; d++) {
            path = g_build_filename (config.pluginsDir[d], name, NULL);

            if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
                g_free (path);
                continue;
            }

            int rc = ((ArkimeExtensions_t *)g_ptr_array_index(extensionsArr, e))->loadFunc(path);

            if (rc == 0) {
                loaded = 1;
                break;
            }
        }

        if (!loaded) {
            LOG("WARNING - plugin '%s' not found", name);
            continue;
        }

        if (config.debug)
            LOG("Loaded %s", path);

        g_free (path);
    }
}
/******************************************************************************/
LOCAL int arkime_plugins_load_so(const char *path)
{
    GModule *plugin = g_module_open (path, 0); /*G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);*/

    if (!plugin) {
        LOG("ERROR - Couldn't load plugin from '%s'\n%s", path, g_module_error());
        return 1;
    }

    ArkimePluginInitFunc plugin_init;

    if (!g_module_symbol(plugin, "arkime_plugin_init", (gpointer *)(char *)&plugin_init) || plugin_init == NULL) {
        LOG("ERROR - Module %s doesn't have a arkime_plugin_init", path);
        return 1;
    }

    plugin_init();
    return 0;
}
/******************************************************************************/
void arkime_plugins_register_load_extension(const char *extension, ArkimeParserLoadFunc loadFunc)
{
    if (extension[0] != '.') {
        LOGEXIT("ERROR - Extension '%s' must start with a .", extension);
    }

    if (!extensionsArr) {
        extensionsArr = g_ptr_array_new_full(4, NULL);
    }
    ArkimeExtensions_t *ext = ARKIME_TYPE_ALLOC0(ArkimeExtensions_t);
    ext->extension = extension;
    ext->loadFunc = loadFunc;
    g_ptr_array_add(extensionsArr, ext);
}
/******************************************************************************/
int arkime_plugins_register_internal(const char             *name,
                                     gboolean                storeData,
                                     size_t                  sessionsize,
                                     int                     apiversion)
{
    ArkimePlugin_t *plugin;

    if (sizeof(ArkimeSession_t) != sessionsize) {
        CONFIGEXIT("Plugin '%s' built with different version of arkime.h", name);
    }

    if (ARKIME_API_VERSION != apiversion) {
        CONFIGEXIT("Plugin '%s' built with different version of arkime.h", name);
    }

    HASH_FIND(p_, plugins, name, plugin);
    if (plugin) {
        CONFIGEXIT("Plugin %s is already registered", name);
    }

    plugin = ARKIME_TYPE_ALLOC0(ArkimePlugin_t);
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
void arkime_plugins_set_cb(const char             *name,
                           ArkimePluginIpFunc      ipFunc,
                           ArkimePluginUdpFunc     udpFunc,
                           ArkimePluginTcpFunc     tcpFunc,
                           ArkimePluginSaveFunc    preSaveFunc,
                           ArkimePluginSaveFunc    saveFunc,
                           ArkimePluginNewFunc     newFunc,
                           ArkimePluginExitFunc    exitFunc,
                           ArkimePluginReloadFunc  reloadFunc)
{
    ArkimePlugin_t *plugin;

    HASH_FIND(p_, plugins, name, plugin);
    if (!plugin) {
        LOG("Can't find plugin with name %s", name);
        return;
    }

    if (ipFunc) {
        LOGEXIT("EXIT - Plugin %s registered for IP callbacks which is no longer supported", name);
    }

    plugin->udpFunc = udpFunc;
    if (udpFunc)
        pluginsCbs |= ARKIME_PLUGIN_UDP;

    plugin->tcpFunc = tcpFunc;
    if (tcpFunc)
        pluginsCbs |= ARKIME_PLUGIN_TCP;

    plugin->preSaveFunc = preSaveFunc;
    if (preSaveFunc)
        pluginsCbs |= ARKIME_PLUGIN_PRE_SAVE;

    plugin->saveFunc = saveFunc;
    if (saveFunc)
        pluginsCbs |= ARKIME_PLUGIN_SAVE;

    plugin->newFunc = newFunc;
    if (newFunc)
        pluginsCbs |= ARKIME_PLUGIN_NEW;

    plugin->exitFunc = exitFunc;
    if (exitFunc)
        pluginsCbs |= ARKIME_PLUGIN_EXIT;

    plugin->reloadFunc = reloadFunc;
    if (reloadFunc)
        pluginsCbs |= ARKIME_PLUGIN_RELOAD;
}
/******************************************************************************/
void arkime_plugins_set_http_cb(const char              *name,
                                ArkimePluginHttpFunc     on_message_begin,
                                ArkimePluginHttpDataFunc on_url,
                                ArkimePluginHttpDataFunc on_header_field,
                                ArkimePluginHttpDataFunc on_header_value,
                                ArkimePluginHttpFunc     on_headers_complete,
                                ArkimePluginHttpDataFunc on_body,
                                ArkimePluginHttpFunc     on_message_complete)
{
    arkime_plugins_set_http_ext_cb(name,
                                   on_message_begin,
                                   on_url,
                                   on_header_field,
                                   NULL,
                                   on_header_value,
                                   on_headers_complete,
                                   on_body,
                                   on_message_complete);
}
/******************************************************************************/
void arkime_plugins_set_http_ext_cb(const char              *name,
                                    ArkimePluginHttpFunc     on_message_begin,
                                    ArkimePluginHttpDataFunc on_url,
                                    ArkimePluginHttpDataFunc on_header_field,
                                    ArkimePluginHttpDataFunc on_header_field_raw,
                                    ArkimePluginHttpDataFunc on_header_value,
                                    ArkimePluginHttpFunc     on_headers_complete,
                                    ArkimePluginHttpDataFunc on_body,
                                    ArkimePluginHttpFunc     on_message_complete)
{
    ArkimePlugin_t *plugin;

    HASH_FIND(p_, plugins, name, plugin);
    if (!plugin) {
        LOG("Can't find plugin with name %s", name);
        return;
    }

    plugin->on_message_begin = on_message_begin;
    if (on_message_begin)
        pluginsCbs |= ARKIME_PLUGIN_HP_OMB;

    plugin->on_url = on_url;
    if (on_url)
        pluginsCbs |= ARKIME_PLUGIN_HP_OU;

    plugin->on_header_field = on_header_field;
    if (on_header_field)
        pluginsCbs |= ARKIME_PLUGIN_HP_OHF;

    plugin->on_header_field_raw = on_header_field_raw;
    if (on_header_field_raw)
        pluginsCbs |= ARKIME_PLUGIN_HP_OHFR;

    plugin->on_header_value = on_header_value;
    if (on_header_value)
        pluginsCbs |= ARKIME_PLUGIN_HP_OHV;

    plugin->on_headers_complete = on_headers_complete;
    if (on_headers_complete)
        pluginsCbs |= ARKIME_PLUGIN_HP_OHC;

    plugin->on_body = on_body;
    if (on_body)
        pluginsCbs |= ARKIME_PLUGIN_HP_OB;

    plugin->on_message_complete = on_message_complete;
    if (on_message_complete)
        pluginsCbs |= ARKIME_PLUGIN_HP_OMC;

}
/******************************************************************************/
void arkime_plugins_set_smtp_cb(const char                 *name,
                                ArkimePluginSMTPHeaderFunc  on_header,
                                ArkimePluginSMTPFunc        on_header_complete)
{
    ArkimePlugin_t *plugin;

    HASH_FIND(p_, plugins, name, plugin);
    if (!plugin) {
        LOG("Can't find plugin with name %s", name);
        return;
    }

    plugin->smtp_on_header = on_header;
    if (on_header)
        pluginsCbs |= ARKIME_PLUGIN_SMTP_OH;

    plugin->smtp_on_header_complete = on_header_complete;
    if (on_header_complete)
        pluginsCbs |= ARKIME_PLUGIN_SMTP_OHC;
}
/******************************************************************************/
void arkime_plugins_set_outstanding_cb(const char                 *name,
                                       ArkimePluginOutstandingFunc        outstandingFunc)
{
    ArkimePlugin_t *plugin;

    HASH_FIND(p_, plugins, name, plugin);
    if (!plugin) {
        LOG("Can't find plugin with name %s", name);
        return;
    }

    plugin->outstandingFunc = outstandingFunc;
}
/******************************************************************************/
void arkime_plugins_cb_pre_save(ArkimeSession_t *session, int final)
{
    ArkimePlugin_t *plugin;

    HASH_FORALL2(p_, plugins, plugin) {
        if (plugin->preSaveFunc)
            plugin->preSaveFunc(session, final);
    }
}
/******************************************************************************/
void arkime_plugins_cb_save(ArkimeSession_t *session, int final)
{
    ArkimePlugin_t *plugin;

    HASH_FORALL2(p_, plugins, plugin) {
        if (plugin->saveFunc)
            plugin->saveFunc(session, final);
    }
}
/******************************************************************************/
void arkime_plugins_cb_new(ArkimeSession_t *session)
{
    ArkimePlugin_t *plugin;

    HASH_FORALL2(p_, plugins, plugin) {
        if (plugin->newFunc)
            plugin->newFunc(session);
    }
}
/******************************************************************************/
void arkime_plugins_cb_tcp(ArkimeSession_t *session, const uint8_t *data, int len, int which)
{
    ArkimePlugin_t *plugin;

    HASH_FORALL2(p_, plugins, plugin) {
        if (plugin->tcpFunc)
            plugin->tcpFunc(session, data, len, which);
    }
}
/******************************************************************************/
void arkime_plugins_cb_udp(ArkimeSession_t *session, const uint8_t *data, int len, int which)
{
    ArkimePlugin_t *plugin;

    HASH_FORALL2(p_, plugins, plugin) {
        if (plugin->udpFunc)
            plugin->udpFunc(session, data, len, which);
    }
}
/******************************************************************************/
void arkime_plugins_cb_hp_omb(ArkimeSession_t *session, http_parser *parser)
{
    ArkimePlugin_t *plugin;

    HASH_FORALL2(p_, plugins, plugin) {
        if (plugin->on_message_begin)
            plugin->on_message_begin(session, parser);
    }
}
/******************************************************************************/
void arkime_plugins_cb_hp_ou(ArkimeSession_t *session, http_parser *parser, const char *at, size_t length)
{
    ArkimePlugin_t *plugin;

    HASH_FORALL2(p_, plugins, plugin) {
        if (plugin->on_url)
            plugin->on_url(session, parser, at, length);
    }
}
/******************************************************************************/
void arkime_plugins_cb_hp_ohf(ArkimeSession_t *session, http_parser *parser, const char *at, size_t length)
{
    ArkimePlugin_t *plugin;

    HASH_FORALL2(p_, plugins, plugin) {
        if (plugin->on_header_field)
            plugin->on_header_field(session, parser, at, length);
    }
}
/******************************************************************************/
void arkime_plugins_cb_hp_ohfr(ArkimeSession_t *session, http_parser *parser, const char *at, size_t length)
{
    ArkimePlugin_t *plugin;

    HASH_FORALL2(p_, plugins, plugin) {
        if (plugin->on_header_field_raw)
            plugin->on_header_field_raw(session, parser, at, length);
    }
}
/******************************************************************************/
void arkime_plugins_cb_hp_ohv(ArkimeSession_t *session, http_parser *parser, const char *at, size_t length)
{
    ArkimePlugin_t *plugin;

    HASH_FORALL2(p_, plugins, plugin) {
        if (plugin->on_header_value)
            plugin->on_header_value(session, parser, at, length);
    }
}
/******************************************************************************/
void arkime_plugins_cb_hp_ohc(ArkimeSession_t *session, http_parser *parser)
{
    ArkimePlugin_t *plugin;

    HASH_FORALL2(p_, plugins, plugin) {
        if (plugin->on_headers_complete)
            plugin->on_headers_complete(session, parser);
    }
}
/******************************************************************************/
void arkime_plugins_cb_hp_ob(ArkimeSession_t *session, http_parser *parser, const char *at, size_t length)
{
    ArkimePlugin_t *plugin;

    HASH_FORALL2(p_, plugins, plugin) {
        if (plugin->on_body)
            plugin->on_body(session, parser, at, length);
    }
}
/******************************************************************************/
void arkime_plugins_cb_hp_omc(ArkimeSession_t *session, http_parser *parser)
{
    ArkimePlugin_t *plugin;

    HASH_FORALL2(p_, plugins, plugin) {
        if (plugin->on_message_complete)
            plugin->on_message_complete(session, parser);
    }
}
/******************************************************************************/
void arkime_plugins_cb_smtp_oh(ArkimeSession_t *session, const char *field, size_t field_len, const char *value, size_t value_len)
{
    ArkimePlugin_t *plugin;

    HASH_FORALL2(p_, plugins, plugin) {
        if (plugin->smtp_on_header)
            plugin->smtp_on_header(session, field, field_len, value, value_len);
    }
}
/******************************************************************************/
void arkime_plugins_cb_smtp_ohc(ArkimeSession_t *session)
{
    ArkimePlugin_t *plugin;

    HASH_FORALL2(p_, plugins, plugin) {
        if (plugin->smtp_on_header_complete)
            plugin->smtp_on_header_complete(session);
    }
}
/******************************************************************************/
void arkime_plugins_exit()
{
    ArkimePlugin_t *plugin;

    HASH_FORALL2(p_, plugins, plugin) {
        if (plugin->exitFunc)
            plugin->exitFunc();
    }

    HASH_FORALL_POP_HEAD2(p_, plugins, plugin) {
        free(plugin->name);
        ARKIME_TYPE_FREE(ArkimePlugin_t, plugin);
    }
}
/******************************************************************************/
void arkime_plugins_reload()
{
    ArkimePlugin_t *plugin;

    HASH_FORALL2(p_, plugins, plugin) {
        if (plugin->reloadFunc)
            plugin->reloadFunc();
    }
}
/******************************************************************************/
LOCAL uint32_t arkime_plugins_outstanding()
{
    ArkimePlugin_t *plugin;
    uint32_t        outstanding = 0;

    HASH_FORALL2(p_, plugins, plugin) {
        if (plugin->outstandingFunc)
            outstanding += plugin->outstandingFunc();
    }
    return outstanding;
}
/******************************************************************************/
void arkime_plugins_init()
{
    HASH_INIT(p_, plugins, arkime_string_hash, arkime_string_cmp);
    arkime_command_register("plugins-list", arkime_plugins_cmd_list, "List loaded plugins");
    arkime_plugins_register_load_extension(".so", arkime_plugins_load_so);
}
