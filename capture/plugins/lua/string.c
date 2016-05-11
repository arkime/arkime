/* string.c  -- lua interface to string stuff
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
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
static MS_t *checkMolochString (lua_State *L, int index)
{
    MS_t *ms;
    luaL_checktype(L, index, LUA_TUSERDATA);
    ms = (MS_t*)luaL_checkudata(L, index, "MolochString");
    if (ms == NULL) {
        luaL_argerror(L, index, lua_pushfstring(L, "MolochString expected, got %s", luaL_typename(L, index)));
        return NULL;
    }
    return ms;
}
/******************************************************************************/
MS_t *molua_pushMolochString (lua_State *L, const char *str, int len)
{
    MS_t *ms = (MS_t *)lua_newuserdata(L, sizeof(MS_t));
    ms->str = str;
    ms->len = len;
    ms->needFree = 0;
    luaL_getmetatable(L, "MolochString");
    lua_setmetatable(L, -2);
    return ms;
}
/******************************************************************************/
int MS_tostring(lua_State *L)
{
    MS_t *ms = checkMolochString(L, 1);
    lua_pushlstring(L, ms->str, ms->len);
    return 1;
}
/******************************************************************************/
int MS_gc(lua_State *L)
{
    MS_t *ms = checkMolochString(L, 1);
    if (ms->needFree) {
        g_free((gpointer)ms->str);
    }
    return 0;
}
/******************************************************************************/
int MS_memmem(lua_State *L)
{
    MS_t *ms = checkMolochString(L, 1);
    size_t len;
    const char *needle = luaL_checklstring(L, 2, &len);
    const char *match = moloch_memstr(ms->str, ms->len, needle, len);
    if (match) {
        lua_pushinteger(L, match - ms->str);
    } else {
        lua_pushinteger(L, -1);
    }
    return 1;
}
/******************************************************************************/
int MS_copy(lua_State *L)
{
    MS_t *ms = checkMolochString(L, 1);
    MS_t *nms = molua_pushMolochString(L, ms->str, ms->len);
    nms->str = g_memdup(ms->str, ms->len);
    nms->needFree = 1;
    return 1;
}
/******************************************************************************/
int MS_new(lua_State *L)
{
    size_t len;
    const char *str = luaL_checklstring(L, 1, &len);
    MS_t *ms = molua_pushMolochString(L, str, len);
    ms->str = g_memdup(str, len);
    ms->needFree = 1;
    return 1;
}

/******************************************************************************/
void luaopen_molochstring(lua_State *L)
{
    static const struct luaL_Reg methods[] = {
        {"__tostring", MS_tostring},
        {"__gc", MS_gc},
        {"memmem", MS_memmem},
        {"get", MS_tostring},
        {"copy", MS_copy},
        { NULL, NULL }
    };
    static const struct luaL_Reg functions[] = {
        {"new", MS_new},
        { NULL, NULL }
    };

    luaL_newmetatable(L, "MolochString");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, methods, 0);
    luaL_newlib(L, functions);
    lua_setglobal(L, "MolochString");
}
