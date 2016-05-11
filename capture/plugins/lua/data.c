/* data.c  -- lua interface to string stuff
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
static MD_t *checkMolochData (lua_State *L, int index)
{
    MD_t *ms;
    luaL_checktype(L, index, LUA_TUSERDATA);
    ms = (MD_t*)luaL_checkudata(L, index, "MolochData");
    if (ms == NULL) {
        luaL_argerror(L, index, lua_pushfstring(L, "MolochData expected, got %s", luaL_typename(L, index)));
        return NULL;
    }
    return ms;
}
/******************************************************************************/
MD_t *molua_pushMolochData (lua_State *L, const char *str, int len)
{
    MD_t *ms = (MD_t *)lua_newuserdata(L, sizeof(MD_t));
    ms->str = str;
    ms->len = len;
    ms->needFree = 0;
    luaL_getmetatable(L, "MolochData");
    lua_setmetatable(L, -2);
    return ms;
}
/******************************************************************************/
int MD_tostring(lua_State *L)
{
    MD_t *ms = checkMolochData(L, 1);
    lua_pushlstring(L, ms->str, ms->len);
    return 1;
}
/******************************************************************************/
int MD_gc(lua_State *L)
{
    MD_t *ms = checkMolochData(L, 1);
    if (ms->needFree) {
        g_free((gpointer)ms->str);
    }
    return 0;
}
/******************************************************************************/
int MD_memmem(lua_State *L)
{
    MD_t *ms = checkMolochData(L, 1);
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
int MD_copy(lua_State *L)
{
    MD_t *ms = checkMolochData(L, 1);
    MD_t *nms = molua_pushMolochData(L, ms->str, ms->len);
    nms->str = g_memdup(ms->str, ms->len);
    nms->needFree = 1;
    return 1;
}
/******************************************************************************/
int MD_new(lua_State *L)
{
    size_t len;
    const char *str = luaL_checklstring(L, 1, &len);
    MD_t *ms = molua_pushMolochData(L, str, len);
    ms->str = g_memdup(str, len);
    ms->needFree = 1;
    return 1;
}
/******************************************************************************/
int MD_pcre_create(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    GRegex     *regex = g_regex_new(str, 0, 0, 0);
    lua_pushlightuserdata(L, regex);
    return 1;
}
/******************************************************************************/
int MD_pcre_ismatch(lua_State *L)
{
    MD_t *data = checkMolochData(L, 1);
    GRegex *pattern = lua_touserdata(L, 2);
    gboolean result = g_regex_match_full (pattern, data->str, data->len, 0, 0, NULL, NULL);
    lua_pushboolean(L, result);
    return 1;
}
/******************************************************************************/
int MD_pcre_match(lua_State *L)
{
    MD_t *data = checkMolochData(L, 1);
    GRegex *pattern = lua_touserdata(L, 2);
    GMatchInfo *match_info;
    gboolean result = g_regex_match_full (pattern, data->str, data->len, 0, 0, &match_info, NULL);
    lua_pushboolean(L, result);

    int i;
    int cnt = g_match_info_get_match_count(match_info);
    lua_checkstack(L, cnt+1);
    for (i = 0; i < cnt; i++) {
        gint start, end;
        g_match_info_fetch_pos(match_info, i, &start, &end);
        lua_pushlstring(L, data->str + start, end-start);
    }
    g_match_info_free(match_info);
    return cnt+1;
}
/******************************************************************************/
int MD_pattern_create(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    GPatternSpec *pattern = g_pattern_spec_new(str);
    lua_pushlightuserdata(L, pattern);
    return 1;
}
/******************************************************************************/
int MD_pattern_ismatch(lua_State *L)
{
    MD_t *data = checkMolochData(L, 1);
    GPatternSpec *pattern = lua_touserdata(L, 2);
    lua_pushboolean(L, g_pattern_match(pattern, data->len, data->str, NULL));
    return 1;
}

/******************************************************************************/
void luaopen_molochdata(lua_State *L)
{
    static const struct luaL_Reg methods[] = {
        {"__tostring", MD_tostring},
        {"__gc", MD_gc},
        {"memmem", MD_memmem},
        {"pattern_ismatch", MD_pattern_ismatch},
        {"pcre_ismatch", MD_pcre_ismatch},
        {"pcre_match", MD_pcre_match},
        {"get", MD_tostring},
        {"copy", MD_copy},
        { NULL, NULL }
    };
    static const struct luaL_Reg functions[] = {
        {"pcre_create", MD_pcre_create},
        {"pattern_create", MD_pattern_create},
        {"new", MD_new},
        { NULL, NULL }
    };

    luaL_newmetatable(L, "MolochData");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, methods, 0);
    luaL_newlib(L, functions);
    lua_setglobal(L, "MolochData");
}
