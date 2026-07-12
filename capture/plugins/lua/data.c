/* data.c  -- lua interface to string stuff
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "molua.h"

/******************************************************************************/
LOCAL MD_t *checkArkimeData(lua_State *L, int index)
{
    MD_t *md;
    luaL_checktype(L, index, LUA_TUSERDATA);
    md = (MD_t *)luaL_checkudata(L, index, "ArkimeData");
    if (md == NULL) {
        luaL_argerror(L, index, lua_pushfstring(L, "ArkimeData expected, got %s", luaL_typename(L, index)));
        return NULL;
    }
    if (md->invalid) {
        luaL_error(L, "ArkimeData does not contain valid data");
        return NULL;
    }
    return md;
}
/******************************************************************************/
MD_t *molua_pushArkimeData(lua_State *L, const char *str, int len)
{
    MD_t *md = (MD_t *)lua_newuserdata(L, sizeof(MD_t));
    md->str = str;
    md->len = len;
    md->needFree = 0;
    md->invalid = 0;
    luaL_getmetatable(L, "ArkimeData");
    lua_setmetatable(L, -2);
    return md;
}
/******************************************************************************/
LOCAL int MD_tostring(lua_State *L)
{
    MD_t *md = checkArkimeData(L, 1);
    lua_pushlstring(L, md->str, md->len);
    return 1;
}
/******************************************************************************/
void MD_markInvalid(lua_State *L, int index)
{
    MD_t *md = checkArkimeData(L, index);
    md->invalid = 1;
}
/******************************************************************************/
LOCAL int MD_gc(lua_State *L)
{
    // Do this directly as we don't want the 'invalid' check
    MD_t *md = (MD_t *)luaL_checkudata(L, 1, "ArkimeData");
    if (md->needFree) {
        g_free((gpointer)md->str);
    }
    return 0;
}
/******************************************************************************/
LOCAL int MD_memmem(lua_State *L)
{
    const MD_t *md = checkArkimeData(L, 1);
    size_t len;
    const char *needle = luaL_checklstring(L, 2, &len);
    const char *match = arkime_memstr(md->str, md->len, needle, len);
    if (match) {
        lua_pushinteger(L, match - md->str);
    } else {
        lua_pushinteger(L, -1);
    }
    return 1;
}
/******************************************************************************/
LOCAL int MD_copy(lua_State *L)
{
    const MD_t *md = checkArkimeData(L, 1);
    MD_t *nmd = molua_pushArkimeData(L, md->str, md->len);
    nmd->str = g_memdup(md->str, md->len);
    nmd->needFree = 1;
    return 1;
}
/******************************************************************************/
LOCAL int MD_new(lua_State *L)
{
    size_t len;
    const char *str = luaL_checklstring(L, 1, &len);
    MD_t *md = molua_pushArkimeData(L, str, len);
    md->str = g_memdup(str, len);
    md->needFree = 1;
    return 1;
}
/******************************************************************************/
LOCAL int MD_pcre_create(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    GError     *error = NULL;
    GRegex     *regex = g_regex_new(str, G_REGEX_RAW, 0, &error);
    if (!regex) {
        lua_pushfstring(L, "pcre_create: %s", error ? error->message : "compile failed");
        g_clear_error(&error);
        return lua_error(L);
    }
    GRegex **ud = (GRegex **)lua_newuserdata(L, sizeof(GRegex *));
    *ud = regex;
    luaL_getmetatable(L, "ArkimeRegex");
    lua_setmetatable(L, -2);
    return 1;
}
/******************************************************************************/
LOCAL int MD_pcre_gc(lua_State *L)
{
    GRegex **ud = (GRegex **)luaL_checkudata(L, 1, "ArkimeRegex");
    g_regex_unref(*ud);
    return 0;
}
/******************************************************************************/
LOCAL int MD_pcre_ismatch(lua_State *L)
{
    const MD_t *data = checkArkimeData(L, 1);
    const GRegex *pattern = *(GRegex **)luaL_checkudata(L, 2, "ArkimeRegex");
    gboolean result = g_regex_match_full(pattern, data->str, data->len, 0, 0, NULL, NULL);
    lua_pushboolean(L, result);
    return 1;
}
/******************************************************************************/
LOCAL int MD_pcre_match(lua_State *L)
{
    MD_t *data = checkArkimeData(L, 1);
    const GRegex *pattern = *(GRegex **)luaL_checkudata(L, 2, "ArkimeRegex");
    GMatchInfo *match_info;
    gboolean result = g_regex_match_full(pattern, data->str, data->len, 0, 0, &match_info, NULL);
    lua_pushboolean(L, result);

    int i;
    int cnt = g_match_info_get_match_count(match_info);
    if (!lua_checkstack(L, cnt + 2)) {
        LOG("ERROR - Failed to increase stack from %d by %d", lua_gettop(L), cnt + 2);
    }
    for (i = 0; i < cnt; i++) {
        gint start, end;
        if (g_match_info_fetch_pos(match_info, i, &start, &end) && start >= 0 && end >= start) {
            lua_pushlstring(L, data->str + start, end - start);
        } else {
            lua_pushnil(L);
        }
    }
    g_match_info_free(match_info);
    return cnt + 1;
}
/******************************************************************************/
LOCAL int MD_pattern_create(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    GPatternSpec *pattern = g_pattern_spec_new(str);
    GPatternSpec **ud = (GPatternSpec **)lua_newuserdata(L, sizeof(GPatternSpec *));
    *ud = pattern;
    luaL_getmetatable(L, "ArkimePattern");
    lua_setmetatable(L, -2);
    return 1;
}
/******************************************************************************/
LOCAL int MD_pattern_gc(lua_State *L)
{
    GPatternSpec **ud = (GPatternSpec **)luaL_checkudata(L, 1, "ArkimePattern");
    g_pattern_spec_free(*ud);
    return 0;
}
/******************************************************************************/
LOCAL int MD_pattern_ismatch(lua_State *L)
{
    const MD_t *data = checkArkimeData(L, 1);
    GPatternSpec *pattern = *(GPatternSpec **)luaL_checkudata(L, 2, "ArkimePattern");
    lua_pushboolean(L, g_pattern_match(pattern, data->len, data->str, NULL));
    return 1;
}

/******************************************************************************/
void luaopen_arkimedata(lua_State *L)
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

    luaL_newmetatable(L, "ArkimeData");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, methods, 0);

    luaL_newmetatable(L, "ArkimeRegex");
    lua_pushcfunction(L, MD_pcre_gc);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    luaL_newmetatable(L, "ArkimePattern");
    lua_pushcfunction(L, MD_pattern_gc);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    luaL_newlib(L, functions);
    lua_setglobal(L, "ArkimeData");
}
