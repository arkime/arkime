/* molua.c  -- lua interface
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


#include "molua.h"
/******************************************************************************/

extern MolochConfig_t        config;

lua_State *Ls[MOLOCH_MAX_PACKET_THREADS];
MolochSession_t moluaFakeSessions[MOLOCH_MAX_PACKET_THREADS];

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
long refs[1];
/******************************************************************************/
void lua_http_on_body_cb (MolochSession_t *session, http_parser *UNUSED(hp), const char *at, size_t length)
{
    lua_State *L = Ls[session->thread];
    lua_rawgeti(L, LUA_REGISTRYINDEX, refs[0]);
    molua_pushMolochSession(L, session);
    molua_pushMolochData(L, at, length);

    if (lua_pcall(L, 2, 0, 0) != 0) {
       LOGEXIT("error running http callback function %s", lua_tostring(L, -1));
    }
}
/******************************************************************************/
static int M_expression_to_fieldId(lua_State *L)
{
    if (lua_gettop(L) != 1 || !lua_isstring(L, 1)) {
        return luaL_error(L, "usage: <field expression>");
    }

    int pos = moloch_field_by_exp(lua_tostring(L, 1));
    lua_pushinteger(L, pos);

    return 1;
}
/******************************************************************************/
void luaopen_moloch(lua_State *L)
{
    static const struct luaL_Reg methods[] = {
        { NULL, NULL }
    };

    static const struct luaL_Reg functions[] = {
        {"expression_to_fieldId", M_expression_to_fieldId},
        { NULL, NULL }
    };

    luaL_newmetatable(L, "Moloch");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, methods, 0);
    luaL_newlib(L, functions);
    lua_setglobal(L, "Moloch");
}
/******************************************************************************/
void molua_session_save(MolochSession_t *session, int final)
{
    if (final && session->pluginData[molua_pluginIndex]) {
        MoluaPlugin_t *mp = session->pluginData[molua_pluginIndex];

        if (mp->table) {
            luaL_unref(Ls[session->thread], LUA_REGISTRYINDEX, mp->table);
        }
        MOLOCH_TYPE_FREE(MoluaPlugin_t, mp);
        session->pluginData[molua_pluginIndex] = 0;
    }
}
/******************************************************************************/
void moloch_plugin_init()
{
    int thread;
    char **names = moloch_config_str_list(NULL, "luaFiles", "moloch.lua");
    lua_State *L;

    molua_pluginIndex = moloch_plugins_register("lua", TRUE);

    moloch_plugins_set_cb("lua", NULL, NULL, NULL, NULL, molua_session_save, NULL, NULL, NULL);


    for (thread = 0; thread < config.packetThreads; thread++) {
        L = Ls[thread] = luaL_newstate();
        luaL_openlibs(L);
        moluaFakeSessions[thread].thread = thread;

        int i;
        for (i = 0; names[i]; i++) {

            luaopen_moloch(L);
            luaopen_molochhttpservice(L);
            luaopen_molochsession(L);
            luaopen_molochdata(L);

            if (luaL_loadfile(L, names[i])) {
                LOGEXIT("Error loading %s: %s", names[i], lua_tostring(L, -1));
            }

            if (lua_pcall(L, 0, 0, 0)) {
                LOGEXIT("Error initing %s: %s", names[i], lua_tostring(L, -1));
            }
        }
    }
}
