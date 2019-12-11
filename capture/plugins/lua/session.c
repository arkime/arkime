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
#include <arpa/inet.h>
/******************************************************************************/

extern lua_State *Ls[MOLOCH_MAX_PACKET_THREADS];

#define HTTP_CONSTANT(x) { MOLUA_REF_HTTP_ ##x, #x }

static struct {
    int value;
    const char *name;
} http_constants[] = {
    HTTP_CONSTANT(MESSAGE_BEGIN),
    HTTP_CONSTANT(URL),
    HTTP_CONSTANT(HEADER_FIELD),
    HTTP_CONSTANT(HEADER_FIELD_RAW),
    HTTP_CONSTANT(HEADER_VALUE),
    HTTP_CONSTANT(HEADERS_COMPLETE),
    HTTP_CONSTANT(BODY),
    HTTP_CONSTANT(MESSAGE_COMPLETE)
};

static const char *http_method_strings[] =
    {
#define XX(num, name, string) #string,
    HTTP_METHOD_MAP(XX)
#undef XX
    0
    };

/******************************************************************************/
LOCAL void register_http_constants(lua_State *L)
{
    lua_pushstring(L, "HTTP");
    lua_newtable(L);

    for (size_t i = 0; i < sizeof(http_constants) / sizeof(http_constants[0]); i++) {
        lua_pushstring(L, http_constants[i].name);
        lua_pushnumber(L, http_constants[i].value);
        lua_settable(L, -3);
    }
    lua_settable(L, -3);
}
/******************************************************************************/
LOCAL void *checkMolochSession (lua_State *L, int index)
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
LOCAL int MS_register_tcp_classifier(lua_State *L)
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
LOCAL int MS_register_udp_classifier(lua_State *L)
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
LOCAL  char *callbackRefs[MOLUA_REF_SIZE][MOLUA_REF_MAX_CNT];
LOCAL  int   callbackRefsCnt[MOLUA_REF_SIZE];
/******************************************************************************/
void molua_http_cb (int callback_type, MolochSession_t *session, http_parser *hp, const char *at, size_t length)
{
    MoluaPlugin_t *mp = session->pluginData[molua_pluginIndex];
    lua_State *L = Ls[session->thread];
    int i;
    for (i = 0; i < callbackRefsCnt[callback_type]; i++) {
        if (mp && mp->callbackOff[callback_type] & (1 << i))
            continue;

        uint8_t isMolochData;

        if (at) {
            molua_pushMolochData(L, at, length);
            isMolochData = 1;
        } else {
            lua_pushnil(L);
            isMolochData = 0;
        }
        lua_getglobal(L, callbackRefs[callback_type][i]);
        molua_pushMolochSession(L, session);
        lua_pushvalue(L, -3);
        lua_pushnumber(L, hp->type == HTTP_REQUEST ? 0 : 1);

        if (lua_pcall(L, 3, 1, 0) != 0) {
            molua_stackDump(L);
            LOGEXIT("error running http callback function %s type %d", lua_tostring(L, -1), callback_type);
        }

        int num = lua_tointeger(L, -1);
        if (num == -1) {
            if (!mp) {
                mp = session->pluginData[molua_pluginIndex] = MOLOCH_TYPE_ALLOC0(MoluaPlugin_t);
            }
            mp->callbackOff[callback_type] |= (1 << i);
        }
        if (isMolochData) {
            MD_markInvalid(L, -2);
        }
        lua_pop(L, 2);
    }
}
/******************************************************************************/
void molua_http_on_body_cb (MolochSession_t *session, http_parser *hp, const char *at, size_t length)
{
    MoluaPlugin_t *mp = session->pluginData[molua_pluginIndex];
    lua_State *L = Ls[session->thread];
    int i;
    for (i = 0; i < callbackRefsCnt[MOLUA_REF_HTTP]; i++) {
        if (mp && mp->callbackOff[MOLUA_REF_HTTP] & (1 << i))
            continue;

        molua_pushMolochData(L, at, length);
        lua_getglobal(L, callbackRefs[MOLUA_REF_HTTP][i]);
        molua_pushMolochSession(L, session);
        lua_pushvalue(L, -3);

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
        MD_markInvalid(L, -2);
        lua_pop(L, 2);
    }

    // We have two ways of doing this callback
    molua_http_cb(MOLUA_REF_HTTP_BODY, session, hp, at, length);
}
/******************************************************************************/
LOCAL void molua_http_on_message_begin(MolochSession_t *session, http_parser *hp)
{
    if (hp->type == HTTP_REQUEST) {
        molua_http_cb(MOLUA_REF_HTTP_MESSAGE_BEGIN, session, hp,
                      http_method_strings[hp->method], strlen(http_method_strings[hp->method]));
    } else {
        molua_http_cb(MOLUA_REF_HTTP_MESSAGE_BEGIN, session, hp, NULL, 0);
    }
}
/******************************************************************************/
LOCAL void handle_missing_message_begin(MolochSession_t *session, http_parser *hp)
{
    MoluaPlugin_t *mp = session->pluginData[molua_pluginIndex];
    if (!mp || !mp->done_message_begin[hp->type]) {
        if (!mp) {
            mp = session->pluginData[molua_pluginIndex] = MOLOCH_TYPE_ALLOC0(MoluaPlugin_t);
        }
        mp->done_message_begin[hp->type] = 1;
        molua_http_on_message_begin(session, hp);
    }
}
/******************************************************************************/
LOCAL void molua_http_on_url(MolochSession_t *session, http_parser *hp, const char *at, size_t length)
{
    handle_missing_message_begin(session, hp);
    molua_http_cb(MOLUA_REF_HTTP_URL, session, hp, at, length);
}
/******************************************************************************/
LOCAL void molua_http_on_header_field_raw(MolochSession_t *session, http_parser *hp, const char *at, size_t length)
{
    handle_missing_message_begin(session, hp);
    molua_http_cb(MOLUA_REF_HTTP_HEADER_FIELD_RAW, session, hp, at, length);
}
/******************************************************************************/
LOCAL void molua_http_on_header_field(MolochSession_t *session, http_parser *hp, const char *at, size_t length)
{
    handle_missing_message_begin(session, hp);
    molua_http_cb(MOLUA_REF_HTTP_HEADER_FIELD, session, hp, at, length);
}
/******************************************************************************/
LOCAL void molua_http_on_header_value(MolochSession_t *session, http_parser *hp, const char *at, size_t length)
{
    molua_http_cb(MOLUA_REF_HTTP_HEADER_VALUE, session, hp, at, length);
}
/******************************************************************************/
LOCAL void molua_http_on_headers_complete(MolochSession_t *session, http_parser *hp)
{
    handle_missing_message_begin(session, hp);
    molua_http_cb(MOLUA_REF_HTTP_HEADERS_COMPLETE, session, hp, NULL, 0);
}
/******************************************************************************/
LOCAL void molua_http_on_message_complete(MolochSession_t *session, http_parser *hp)
{
    handle_missing_message_begin(session, hp);
    molua_http_cb(MOLUA_REF_HTTP_MESSAGE_COMPLETE, session, hp, NULL, 0);
}
/******************************************************************************/
LOCAL void MS_register_all_http_cbs()
{
    static char http_cbs_registered;

    if (!http_cbs_registered) {
        http_cbs_registered = 1;
        moloch_plugins_set_http_ext_cb("lua",
            NULL,
            molua_http_on_url,
            molua_http_on_header_field,
            molua_http_on_header_field_raw,
            molua_http_on_header_value,
            molua_http_on_headers_complete,
            molua_http_on_body_cb,
            molua_http_on_message_complete);
    }
}
/******************************************************************************/
LOCAL int MS_register_http_cb(lua_State *L)
{
    if (L != Ls[0]) // Only do once
        return 0;

    if (lua_gettop(L) != 2 || !lua_isnumber(L, 1) || !lua_isstring(L, 2)) {
        return luaL_error(L, "usage: <cb_type> <function name>");
    }

    int num = lua_tointeger(L, 1);
    if (num < MOLUA_REF_HTTP_CB_FIRST || num > MOLUA_REF_HTTP_CB_LAST) {
        return luaL_error(L, "invalid callback type");
    }

    MS_register_all_http_cbs();

    if (callbackRefsCnt[num] < MOLUA_REF_MAX_CNT) {
        lua_pushvalue(L, 2);
        callbackRefs[num][callbackRefsCnt[num]++] = g_strdup(lua_tostring(L, 2));
    } else {
        return luaL_error(L, "Can't have more then %d callbacks of this type", MOLUA_REF_MAX_CNT);
    }

    return 0;
}
/******************************************************************************/
LOCAL int MS_register_body_feed(lua_State *L)
{
    if (L != Ls[0]) // Only do once
        return 0;

    if (lua_gettop(L) != 2 || !lua_isstring(L, 1) || !lua_isstring(L, 2)) {
        return luaL_error(L, "usage: <type> <functionName>");
    }

    const char *type = lua_tostring(L, 1);

    if (strcmp(type, "http") == 0) {
        MS_register_all_http_cbs();
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
LOCAL int MS_register_parser(lua_State *L)
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
LOCAL int MS_add_tag(lua_State *L)
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
LOCAL int MS_incr_outstanding(lua_State *L)
{
    if (lua_gettop(L) != 1 || !lua_isuserdata(L, 1)) {
        return luaL_error(L, "usage: <session>");
    }

    MolochSession_t *session = checkMolochSession(L, 1);
    moloch_session_incr_outstanding(session);

    return 0;
}
/******************************************************************************/
LOCAL int MS_decr_outstanding(lua_State *L)
{
    if (lua_gettop(L) != 1 || !lua_isuserdata(L, 1)) {
        return luaL_error(L, "usage: <session>");
    }

    MolochSession_t *session = checkMolochSession(L, 1);
    moloch_session_decr_outstanding(session);

    return 0;
}
/******************************************************************************/
LOCAL int MS_add_protocol(lua_State *L)
{
    if (lua_gettop(L) != 2 || !lua_isuserdata(L, 1) || !lua_isstring(L, 2)) {
        return luaL_error(L, "usage: <session> <protocol>");
    }

    MolochSession_t *session = checkMolochSession(L, 1);
    moloch_session_add_protocol(session, lua_tostring(L, 2));

    return 0;
}
/******************************************************************************/
LOCAL int MS_has_protocol(lua_State *L)
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
LOCAL int MS_add_string(lua_State *L)
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
    result = moloch_field_string_add(pos, session, string, len, TRUE) != NULL;
    lua_pushboolean(L, result);

    return 1;
}
/******************************************************************************/
LOCAL int MS_add_int(lua_State *L)
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
LOCAL int MS_tostring(lua_State *L)
{
    MolochSession_t *session = checkMolochSession(L, 1);
    lua_pushfstring(L, "MolochSession: %p", session);
    return 1;
}

/******************************************************************************/
LOCAL int MS_table(lua_State *L)
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
LOCAL int MSP_get_addr1(lua_State *L)
{
    MolochSession_t *session = checkMolochSession(L, 1);

    char addrbuf[INET6_ADDRSTRLEN];
    const char *result;
    if (MOLOCH_SESSION_v6(session)) {
        result = inet_ntop(AF_INET6, &session->addr1, addrbuf, INET6_ADDRSTRLEN);
    } else {
        result = inet_ntop(AF_INET, &MOLOCH_V6_TO_V4(session->addr1), addrbuf, INET6_ADDRSTRLEN);
    }
    if (!result) {
        return luaL_error(L, "Failed to convert IP address to text");
    }
    lua_pushstring(L, result);
    return 1;
}
/******************************************************************************/
LOCAL int MSP_get_port1(lua_State *L)
{
    MolochSession_t *session = checkMolochSession(L, 1);
    lua_pushnumber(L, session->port1);
    return 1;
}
/******************************************************************************/
LOCAL int MSP_get_addr2(lua_State *L)
{
    MolochSession_t *session = checkMolochSession(L, 1);

    char addrbuf[INET6_ADDRSTRLEN];
    const char *result;
    if (MOLOCH_SESSION_v6(session)) {
        result = inet_ntop(AF_INET6, &session->addr2, addrbuf, INET6_ADDRSTRLEN);
    } else {
        result = inet_ntop(AF_INET, &MOLOCH_V6_TO_V4(session->addr2), addrbuf, INET6_ADDRSTRLEN);
    }
    if (!result) {
        return luaL_error(L, "Failed to convert IP address to text");
    }
    lua_pushstring(L, result);
    return 1;
}
/******************************************************************************/
LOCAL int MSP_get_port2(lua_State *L)
{
    MolochSession_t *session = checkMolochSession(L, 1);
    lua_pushnumber(L, session->port2);
    return 1;
}
/******************************************************************************/
LOCAL int MSP_get_protocol(lua_State *L)
{
    MolochSession_t *session = checkMolochSession(L, 1);
    char pnum[16];
    const char *protocol = pnum;
    switch (session->protocol) {
        case 6:
            protocol = "tcp";
            break;
        default:
            sprintf(pnum, "%d", session->protocol);
            break;
    }
    lua_pushstring(L, protocol);
    return 1;
}
/******************************************************************************/
LOCAL int MSP__index(lua_State *L)
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

    static const struct luaL_Reg fields[] = {
        {"addr1", MSP_get_addr1},
        {"addr2", MSP_get_addr2},
        {"port1", MSP_get_port1},
        {"port2", MSP_get_port2},
        {"protocol", MSP_get_protocol},
        { NULL, NULL }
    };
    const char *field = lua_tostring(L, 2);

    for (int i = 0; fields[i].name; i++) {
        if (strcmp(fields[i].name, field) == 0) {
            return fields[i].func(L);
        }
    }

    for (int i = 0; methods[i].name; i++) {
        if (strcmp(methods[i].name, field) == 0) {
            lua_pushcfunction(L, methods[i].func);
            return 1;
        }
    }

    return 0;
}
/******************************************************************************/
void luaopen_molochsession(lua_State *L)
{
    static const struct luaL_Reg methods[] = {
        {"__index", MSP__index},
        { NULL, NULL }
    };
    static const struct luaL_Reg functions[] = {
        { "http_on", MS_register_http_cb},
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
    register_http_constants(L);
    lua_setglobal(L, "MolochSession");
}
