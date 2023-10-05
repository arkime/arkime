/* molua.c  -- lua interface
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include "molua.h"
/******************************************************************************/

extern ArkimeConfig_t        config;

lua_State *Ls[ARKIME_MAX_PACKET_THREADS];
ArkimeSession_t moluaFakeSessions[ARKIME_MAX_PACKET_THREADS];

int molua_pluginIndex;

/******************************************************************************/
void molua_stackDump (lua_State *L)
{
  int i;
  int top = lua_gettop(L);
  for (i = 1; i <= top; i++) {  /* repeat for each level */
    int t = lua_type(L, i);
    switch (t) {

      case LUA_TSTRING:  /* strings */
        printf("`%s'", lua_tostring(L, i));
        break;

      case LUA_TBOOLEAN:  /* booleans */
        printf(lua_toboolean(L, i) ? "true" : "false");
        break;

      case LUA_TNUMBER:  /* numbers */
        printf("%g", lua_tonumber(L, i));
        break;

      default:  /* other values */
        printf("%s", lua_typename(L, t));
        break;

    }
    printf("  ");  /* put a separator */
  }
  printf("\n");  /* end the listing */
}
/******************************************************************************/
LOCAL int M_expression_to_fieldId(lua_State *L)
{
    if (lua_gettop(L) != 1 || !lua_isstring(L, 1)) {
        return luaL_error(L, "usage: <field expression>");
    }

    int pos = arkime_field_by_exp(lua_tostring(L, 1));
    lua_pushinteger(L, pos);

    return 1;
}
/******************************************************************************/
void luaopen_arkime(lua_State *L)
{
    static const struct luaL_Reg methods[] = {
        { NULL, NULL }
    };

    static const struct luaL_Reg functions[] = {
        {"expression_to_fieldId", M_expression_to_fieldId},
        { NULL, NULL }
    };

    luaL_newmetatable(L, "Arkime");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, methods, 0);
    luaL_newlib(L, functions);
    lua_setglobal(L, "Arkime");
}
/******************************************************************************/
void molua_session_save(ArkimeSession_t *session, int final)
{
    if (final && session->pluginData[molua_pluginIndex]) {
        MoluaPlugin_t *mp = session->pluginData[molua_pluginIndex];

        if (mp->table) {
            luaL_unref(Ls[session->thread], LUA_REGISTRYINDEX, mp->table);
        }
        ARKIME_TYPE_FREE(MoluaPlugin_t, mp);
        session->pluginData[molua_pluginIndex] = 0;
    }
}
/******************************************************************************/
void arkime_plugin_init()
{
    int thread;
    char **names = arkime_config_str_list(NULL, "luaFiles", "moloch.lua");

    molua_pluginIndex = arkime_plugins_register("lua", TRUE);

    arkime_plugins_set_cb("lua", NULL, NULL, NULL, NULL, molua_session_save, NULL, NULL, NULL);


    for (thread = 0; thread < config.packetThreads; thread++) {
        lua_State *L = Ls[thread] = luaL_newstate();
        luaL_openlibs(L);
        moluaFakeSessions[thread].thread = thread;

        int i;
        for (i = 0; names[i]; i++) {

            luaopen_arkime(L);
            luaopen_arkimehttpservice(L);
            luaopen_arkimesession(L);
            luaopen_arkimedata(L);

            if (luaL_loadfile(L, names[i])) {
                CONFIGEXIT("Error loading %s: %s", names[i], lua_tostring(L, -1));
            }

            if (lua_pcall(L, 0, 0, 0)) {
                CONFIGEXIT("Error initing %s: %s", names[i], lua_tostring(L, -1));
            }
        }
    }
}
