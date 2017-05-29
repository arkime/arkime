/* session.c  -- lua interface to session stuff
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

extern lua_State *Ls[MOLOCH_MAX_PACKET_THREADS];


/******************************************************************************/
static void *checkMolochSession (lua_State *L, int index)
{
    void **pms, *ms;
    luaL_checktype(L, index, LUA_TUSERDATA);
    pms = (void**)luaL_checkudata(L, index, "MolochSession");
    if (pms == NULL) {
        luaL_argerror(L, index, lua_pushfstring(L, "MolochSession expected, got %s", luaL_typename(L, index)));
        return NULL;
    }
    ms = *pms;
    if (!ms)
        luaL_error(L, "null MolochSession");
    return ms;
}
/******************************************************************************/
void *molua_pushMolochSession (lua_State *L, const MolochSession_t *ms)
{
    void **pms = (void **)lua_newuserdata(L, sizeof(void *));
    *pms = (void*)ms;
    luaL_getmetatable(L, "MolochSession");
    lua_setmetatable(L, -2);
    return pms;
}

/******************************************************************************/
void molua_classify_cb(MolochSession_t *session, const unsigned char *data, int len, int which, void *uw)
{
    lua_State *L = Ls[session->thread];
    lua_getglobal(L, uw);
    molua_pushMolochSession(L, session);
    lua_pushlstring(L, (char *)data, len);
    lua_pushnumber(L, which);
    if (lua_pcall(L, 3, 0, 0) != 0) {
       LOGEXIT("error running function %s: %s", (char *)uw, lua_tostring(L, -1));
    }
}


/******************************************************************************/
int molua_parsers_cb(MolochSession_t *session, void *uw, const unsigned char *data, int remaining, int which)
{
    lua_State *L = Ls[session->thread];
    lua_rawgeti(L, LUA_REGISTRYINDEX, (long)uw);
    molua_pushMolochSession(L, session);
    lua_pushlstring(L, (char *)data, remaining);
    lua_pushnumber(L, which);

    if (lua_pcall(L, 3, 1, 0) != 0) {
       LOGEXIT("error running parser function %s", lua_tostring(L, -1));
    }

    int num = lua_tointeger(L, -1);
    if (num == -1)
        moloch_parsers_unregister(session, uw);
    lua_pop(L, 1);

    return 0;
}
/******************************************************************************/
void molua_parsers_free_cb(MolochSession_t *session, void *uw)
{
    lua_State *L = Ls[session->thread];
    luaL_unref(L, LUA_REGISTRYINDEX, (long)uw);
}
/******************************************************************************/
static int MS_register_tcp_classifier(lua_State *L)
{
    if (L != Ls[0]) // Only do once
        return 0;

    if (lua_gettop(L) != 4 || !lua_isstring(L, 1) || !lua_isinteger(L, 2) || !lua_isstring(L, 3) || !lua_isstring(L, 4)) {
        return luaL_error(L, "usage: <name> <offset> <match> <function>");
    }

    char *name      = g_strdup(lua_tostring(L, 1));
    char  offset    = lua_tonumber(L, 2);
    int   match_len = lua_rawlen(L, 3);
    guchar *match     = g_memdup(lua_tostring(L, 3), match_len);
    char *function  = g_strdup(lua_tostring(L, 4));

    moloch_parsers_classifier_register_tcp(name, function, offset, match, match_len, molua_classify_cb);
    return 0;
}
/******************************************************************************/
static int MS_register_udp_classifier(lua_State *L)
{
    if (L != Ls[0]) // Only do once
        return 0;

    if (lua_gettop(L) != 4 || !lua_isstring(L, 1) || !lua_isinteger(L, 2) || !lua_isstring(L, 3) || !lua_isstring(L, 4)) {
        return luaL_error(L, "usage: <name> <offset> <match> <function>");
    }

    char *name      = g_strdup(lua_tostring(L, 1));
    char  offset    = lua_tonumber(L, 2);
    int   match_len = lua_rawlen(L, 3);
    guchar *match     = g_memdup(lua_tostring(L, 3), match_len);
    char *function  = g_strdup(lua_tostring(L, 4));

    moloch_parsers_classifier_register_udp(name, function, offset, match, match_len, molua_classify_cb);
    return 0;
}
static char *callbackRefs[MOLUA_REF_SIZE][MOLUA_REF_MAX_CNT];
static int   callbackRefsCnt[MOLUA_REF_SIZE];
/******************************************************************************/
void molua_http_on_body_cb (MolochSession_t *session, http_parser *UNUSED(hp), const char *at, size_t length)
{
    MoluaPlugin_t *mp = session->pluginData[molua_pluginIndex];
    lua_State *L = Ls[session->thread];
    int i;
    for (i = 0; i < callbackRefsCnt[MOLUA_REF_HTTP]; i++) {
        if (mp && mp->callbackOff[MOLUA_REF_HTTP] & (1 << i))
            continue;

        lua_getglobal(L, callbackRefs[MOLUA_REF_HTTP][i]);
        molua_pushMolochSession(L, session);
        molua_pushMolochData(L, at, length);

        if (lua_pcall(L, 2, 1, 0) != 0) {
            molua_stackDump(L);
            LOGEXIT("error running http callback function %s", lua_tostring(L, -1));
        }

        int num = lua_tointeger(L, -1);
        if (num == -1) {
            if (!mp) {
                mp = session->pluginData[molua_pluginIndex] = MOLOCH_TYPE_ALLOC0(MoluaPlugin_t);
            }
            mp->callbackOff[MOLUA_REF_HTTP] |= (1 << i);
        }
        lua_pop(L, 1);
    }
}
/******************************************************************************/
static int MS_register_body_feed(lua_State *L)
{
    if (L != Ls[0]) // Only do once
        return 0;

    if (lua_gettop(L) != 2 || !lua_isstring(L, 1) || !lua_isstring(L, 2)) {
        return luaL_error(L, "usage: <type> <functionName>");
    }

    const char *type = lua_tostring(L, 1);

    if (strcmp(type, "http") == 0) {
        moloch_plugins_set_http_cb("lua", NULL, NULL, NULL, NULL, NULL, molua_http_on_body_cb, NULL);
        if (callbackRefsCnt[MOLUA_REF_HTTP] < MOLUA_REF_MAX_CNT) {
            callbackRefs[MOLUA_REF_HTTP][callbackRefsCnt[MOLUA_REF_HTTP]++] = g_strdup(lua_tostring(L, 2));
        } else {
            return luaL_error(L, "Can't have more then %d %s callbacks", MOLUA_REF_MAX_CNT, type);
        }
    } else {
        return luaL_error(L, "Unknown type: %s", type);
    }

    return 0;
}
/******************************************************************************/
static int MS_register_parser(lua_State *L)
{
    if (lua_gettop(L) != 2 || !lua_isuserdata(L, 1) || !lua_isfunction(L, 2)) {
        return luaL_error(L, "usage: <session> <function>");
    }

    MolochSession_t *session = checkMolochSession(L, 1);
    long ref = luaL_ref(L, LUA_REGISTRYINDEX);

    moloch_parsers_register2(session, molua_parsers_cb, (void*)ref, molua_parsers_free_cb, NULL);

    return 0;
}
/******************************************************************************/
static int MS_add_tag(lua_State *L)
{
    if (lua_gettop(L) != 2 || !lua_isuserdata(L, 1) || !lua_isstring(L, 2)) {
        return luaL_error(L, "usage: <session> <tag>");
    }

    MolochSession_t *session = checkMolochSession(L, 1);
    const char      *tag     = lua_tostring(L, 2);
    moloch_session_add_tag(session, tag);

    return 0;
}
/******************************************************************************/
static int MS_incr_outstanding(lua_State *L)
{
    if (lua_gettop(L) != 1 || !lua_isuserdata(L, 1)) {
        return luaL_error(L, "usage: <session>");
    }

    MolochSession_t *session = checkMolochSession(L, 1);
    moloch_session_incr_outstanding(session);

    return 0;
}
/******************************************************************************/
static int MS_decr_outstanding(lua_State *L)
{
    if (lua_gettop(L) != 1 || !lua_isuserdata(L, 1)) {
        return luaL_error(L, "usage: <session>");
    }

    MolochSession_t *session = checkMolochSession(L, 1);
    moloch_session_decr_outstanding(session);

    return 0;
}
/******************************************************************************/
static int MS_add_protocol(lua_State *L)
{
    if (lua_gettop(L) != 2 || !lua_isuserdata(L, 1) || !lua_isstring(L, 2)) {
        return luaL_error(L, "usage: <session> <protocol>");
    }

    MolochSession_t *session = checkMolochSession(L, 1);
    moloch_session_add_protocol(session, lua_tostring(L, 2));

    return 0;
}
/******************************************************************************/
static int MS_has_protocol(lua_State *L)
{
    if (lua_gettop(L) != 2 || !lua_isuserdata(L, 1) || !lua_isstring(L, 2)) {
        return luaL_error(L, "usage: <session> <protocol>");
    }

    MolochSession_t *session = checkMolochSession(L, 1);
    gboolean result = moloch_session_has_protocol(session, lua_tostring(L, 2));

    lua_pushboolean(L, result);
    return 1;
}
/******************************************************************************/
static int MS_add_string(lua_State *L)
{
    if (config.debug > 2)
        molua_stackDump(L);

    if (lua_gettop(L) != 3 || !lua_isuserdata(L, 1) || !(lua_isstring(L, 2) || lua_isinteger(L, 2)) || !lua_isstring(L, 3)) {
        return luaL_error(L, "usage: <session> <field string or field num(faster)> <string>");
    }

    MolochSession_t *session = checkMolochSession(L, 1);
    const char      *string  = lua_tostring(L, 3);
    int              len     = lua_rawlen(L, 3);
    int              pos;
    if (lua_isinteger(L, 2)) {
        pos = lua_tointeger(L, 2);
    } else {
        pos = moloch_field_by_exp(lua_tostring(L, 2));
    }
    gboolean result;
    result = moloch_field_string_add(pos, session, string, len, TRUE);
    lua_pushboolean(L, result);

    return 1;
}
/******************************************************************************/
static int MS_add_int(lua_State *L)
{
    if (config.debug > 2)
        molua_stackDump(L);

    if (lua_gettop(L) != 3 || !lua_isuserdata(L, 1) || !(lua_isstring(L, 2) || lua_isinteger(L, 2)) || !lua_isinteger(L, 3)) {
        return luaL_error(L, "usage: <session> <field string or field num(faster)> <integer>");
    }

    MolochSession_t *session = checkMolochSession(L, 1);
    int              value   = lua_tointeger(L, 3);
    int              pos;
    if (lua_isinteger(L, 2)) {
        pos = lua_tointeger(L, 2);
    } else {
        pos = moloch_field_by_exp(lua_tostring(L, 2));
    }
    gboolean result;
    result = moloch_field_int_add(pos, session, value);
    lua_pushboolean(L, result);

    return 1;
}
/******************************************************************************/
static int MS_tostring(lua_State *L)
{
    MolochSession_t *session = checkMolochSession(L, 1);
    lua_pushfstring(L, "MolochSession: %p", session);
    return 1;
}

/******************************************************************************/
static int MS_table(lua_State *L)
{
    MolochSession_t *session = checkMolochSession(L, 1);
    MoluaPlugin_t *mp = session->pluginData[molua_pluginIndex];
    if (!mp) {
        mp = session->pluginData[molua_pluginIndex] = MOLOCH_TYPE_ALLOC0(MoluaPlugin_t);
    }

    if (!mp->table) {
        lua_newtable(L);
        mp->table = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, mp->table);
    return 1;
}
/******************************************************************************/
void luaopen_molochsession(lua_State *L)
{
    static const struct luaL_Reg methods[] = {
        {"__tostring", MS_tostring},
        {"register_parser", MS_register_parser},
        {"add_tag", MS_add_tag},
        {"add_protocol", MS_add_protocol},
        {"has_protocol", MS_has_protocol},
        {"add_int", MS_add_int},
        {"add_string", MS_add_string},
        {"incr_outstanding", MS_incr_outstanding},
        {"decr_outstanding", MS_decr_outstanding},
        {"table", MS_table},
        { NULL, NULL }
    };
    static const struct luaL_Reg functions[] = {
        { "register_body_feed", MS_register_body_feed},
        { "register_tcp_classifier", MS_register_tcp_classifier},
        { "register_udp_classifier", MS_register_udp_classifier},
        { NULL, NULL }
    };

    luaL_newmetatable(L, "MolochSession");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, methods, 0);
    luaL_newlib(L, functions);
    lua_setglobal(L, "MolochSession");
}
