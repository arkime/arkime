/* session.c  -- lua interface to session stuff
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "molua.h"
#include <arpa/inet.h>
/******************************************************************************/

extern lua_State *Ls[ARKIME_MAX_PACKET_THREADS];

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

static const char *http_method_strings[] = {
#define XX(num, name, string) #string,
    HTTP_METHOD_MAP(XX)
#undef XX
    0
};

LOCAL  int                   certsField;

/******************************************************************************/
// Used to keep track of all the callbacks that should be called
LOCAL  char *callbackRefs[MOLUA_REF_SIZE][MOLUA_REF_MAX_CNT];
LOCAL  int   callbackRefsCnt[MOLUA_REF_SIZE];

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
LOCAL void *checkArkimeSession (lua_State *L, int index)
{
    void **pms, *ms;
    luaL_checktype(L, index, LUA_TUSERDATA);
    pms = (void **)luaL_checkudata(L, index, "ArkimeSession");
    if (pms == NULL) {
        luaL_argerror(L, index, lua_pushfstring(L, "ArkimeSession expected, got %s", luaL_typename(L, index)));
        return NULL;
    }
    ms = *pms;
    if (!ms)
        luaL_error(L, "null ArkimeSession");
    return ms;
}
/******************************************************************************/
void *molua_pushArkimeSession (lua_State *L, const ArkimeSession_t *ms)
{
    void **pms = (void **)lua_newuserdata(L, sizeof(void *));
    *pms = (void *)ms;
    luaL_getmetatable(L, "ArkimeSession");
    lua_setmetatable(L, -2);
    return pms;
}

/******************************************************************************/
void molua_classify_cb(ArkimeSession_t *session, const uint8_t *data, int len, int which, void *uw)
{
    lua_State *L = Ls[session->thread];
    lua_getglobal(L, uw);
    molua_pushArkimeSession(L, session);
    lua_pushlstring(L, (char *)data, len);
    lua_pushnumber(L, which);
    if (lua_pcall(L, 3, 0, 0) != 0) {
        LOGEXIT("error running function %s: %s", (char *)uw, lua_tostring(L, -1));
    }
}


/******************************************************************************/
int molua_parsers_cb(ArkimeSession_t *session, void *uw, const uint8_t *data, int remaining, int which)
{
    lua_State *L = Ls[session->thread];
    lua_rawgeti(L, LUA_REGISTRYINDEX, (long)uw);
    molua_pushArkimeSession(L, session);
    lua_pushlstring(L, (char *)data, remaining);
    lua_pushnumber(L, which);

    if (lua_pcall(L, 3, 1, 0) != 0) {
        LOGEXIT("error running parser function %s", lua_tostring(L, -1));
    }

    int num = lua_tointeger(L, -1);
    if (num == -1)
        arkime_parsers_unregister(session, uw);
    lua_pop(L, 1);

    return 0;
}
/******************************************************************************/
void molua_parsers_free_cb(ArkimeSession_t *session, void *uw)
{
    lua_State *L = Ls[session->thread];
    luaL_unref(L, LUA_REGISTRYINDEX, (long)uw);
}
/******************************************************************************/
LOCAL int MS_register_tcp_classifier(lua_State *L)
{
    if (L != Ls[0]) // Only do in thread 0
        return 0;

    if (lua_gettop(L) != 4 || !lua_isstring(L, 1) || !lua_isinteger(L, 2) || !lua_isstring(L, 3) || !lua_isstring(L, 4)) {
        return luaL_error(L, "usage: <name> <offset> <match> <function>");
    }

    const char *name = g_strdup(lua_tostring(L, 1));
    char  offset = lua_tonumber(L, 2);
    int   match_len = lua_rawlen(L, 3);
    const uint8_t *match = g_memdup(lua_tostring(L, 3), match_len);
    char *function = g_strdup(lua_tostring(L, 4));

    arkime_parsers_classifier_register_tcp(name, function, offset, match, match_len, molua_classify_cb);
    return 0;
}
/******************************************************************************/
LOCAL int MS_register_udp_classifier(lua_State *L)
{
    if (L != Ls[0]) // Only do in thread 0
        return 0;

    if (lua_gettop(L) != 4 || !lua_isstring(L, 1) || !lua_isinteger(L, 2) || !lua_isstring(L, 3) || !lua_isstring(L, 4)) {
        return luaL_error(L, "usage: <name> <offset> <match> <function>");
    }

    char *name      = g_strdup(lua_tostring(L, 1));
    char  offset    = lua_tonumber(L, 2);
    int   match_len = lua_rawlen(L, 3);
    guchar *match     = g_memdup(lua_tostring(L, 3), match_len);
    char *function  = g_strdup(lua_tostring(L, 4));

    arkime_parsers_classifier_register_udp(name, function, offset, match, match_len, molua_classify_cb);
    return 0;
}
/******************************************************************************/
void molua_http_cb (int callback_type, ArkimeSession_t *session, http_parser *hp, const char *at, size_t length)
{
    MoluaPlugin_t *mp = session->pluginData[molua_pluginIndex];
    lua_State *L = Ls[session->thread];
    int i;
    for (i = 0; i < callbackRefsCnt[callback_type]; i++) {
        if (mp && mp->callbackOff[callback_type] & (1 << i))
            continue;

        uint8_t isArkimeData;

        if (at) {
            molua_pushArkimeData(L, at, length);
            isArkimeData = 1;
        } else {
            lua_pushnil(L);
            isArkimeData = 0;
        }
        lua_getglobal(L, callbackRefs[callback_type][i]);
        molua_pushArkimeSession(L, session);
        lua_pushvalue(L, -3);
        lua_pushnumber(L, hp->type == HTTP_REQUEST ? 0 : 1);

        if (lua_pcall(L, 3, 1, 0) != 0) {
            molua_stackDump(L);
            LOGEXIT("error running http callback function %s type %d", lua_tostring(L, -1), callback_type);
        }

        int num = lua_tointeger(L, -1);
        if (num == -1) {
            if (!mp) {
                mp = session->pluginData[molua_pluginIndex] = ARKIME_TYPE_ALLOC0(MoluaPlugin_t);
            }
            mp->callbackOff[callback_type] |= (1 << i);
        }
        if (isArkimeData) {
            MD_markInvalid(L, -2);
        }
        lua_pop(L, 2);
    }
}
/******************************************************************************/
void molua_http_on_body_cb (ArkimeSession_t *session, http_parser *hp, const char *at, size_t length)
{
    MoluaPlugin_t *mp = session->pluginData[molua_pluginIndex];
    lua_State *L = Ls[session->thread];
    int i;
    for (i = 0; i < callbackRefsCnt[MOLUA_REF_HTTP]; i++) {
        if (mp && mp->callbackOff[MOLUA_REF_HTTP] & (1 << i))
            continue;

        molua_pushArkimeData(L, at, length);
        lua_getglobal(L, callbackRefs[MOLUA_REF_HTTP][i]);
        molua_pushArkimeSession(L, session);
        lua_pushvalue(L, -3);

        if (lua_pcall(L, 2, 1, 0) != 0) {
            molua_stackDump(L);
            LOGEXIT("error running http callback function %s", lua_tostring(L, -1));
        }

        int num = lua_tointeger(L, -1);
        if (num == -1) {
            if (!mp) {
                mp = session->pluginData[molua_pluginIndex] = ARKIME_TYPE_ALLOC0(MoluaPlugin_t);
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
LOCAL void molua_http_on_message_begin(ArkimeSession_t *session, http_parser *hp)
{
    if (hp->type == HTTP_REQUEST) {
        molua_http_cb(MOLUA_REF_HTTP_MESSAGE_BEGIN, session, hp,
                      http_method_strings[hp->method], strlen(http_method_strings[hp->method]));
    } else {
        molua_http_cb(MOLUA_REF_HTTP_MESSAGE_BEGIN, session, hp, NULL, 0);
    }
}
/******************************************************************************/
LOCAL void handle_missing_message_begin(ArkimeSession_t *session, http_parser *hp)
{
    MoluaPlugin_t *mp = session->pluginData[molua_pluginIndex];
    if (!mp || !mp->done_message_begin[hp->type]) {
        if (!mp) {
            mp = session->pluginData[molua_pluginIndex] = ARKIME_TYPE_ALLOC0(MoluaPlugin_t);
        }
        mp->done_message_begin[hp->type] = 1;
        molua_http_on_message_begin(session, hp);
    }
}
/******************************************************************************/
LOCAL void molua_http_on_url(ArkimeSession_t *session, http_parser *hp, const char *at, size_t length)
{
    handle_missing_message_begin(session, hp);
    molua_http_cb(MOLUA_REF_HTTP_URL, session, hp, at, length);
}
/******************************************************************************/
LOCAL void molua_http_on_header_field_raw(ArkimeSession_t *session, http_parser *hp, const char *at, size_t length)
{
    handle_missing_message_begin(session, hp);
    molua_http_cb(MOLUA_REF_HTTP_HEADER_FIELD_RAW, session, hp, at, length);
}
/******************************************************************************/
LOCAL void molua_http_on_header_field(ArkimeSession_t *session, http_parser *hp, const char *at, size_t length)
{
    handle_missing_message_begin(session, hp);
    molua_http_cb(MOLUA_REF_HTTP_HEADER_FIELD, session, hp, at, length);
}
/******************************************************************************/
LOCAL void molua_http_on_header_value(ArkimeSession_t *session, http_parser *hp, const char *at, size_t length)
{
    molua_http_cb(MOLUA_REF_HTTP_HEADER_VALUE, session, hp, at, length);
}
/******************************************************************************/
LOCAL void molua_http_on_headers_complete(ArkimeSession_t *session, http_parser *hp)
{
    handle_missing_message_begin(session, hp);
    molua_http_cb(MOLUA_REF_HTTP_HEADERS_COMPLETE, session, hp, NULL, 0);
}
/******************************************************************************/
LOCAL void molua_http_on_message_complete(ArkimeSession_t *session, http_parser *hp)
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
        arkime_plugins_set_http_ext_cb("lua",
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
    if (L != Ls[0]) // Only do in thread 0
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
    if (L != Ls[0]) // Only do in thread 0
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
LOCAL void molua_pre_save(ArkimeSession_t *session, int final)
{
    const MoluaPlugin_t *mp = session->pluginData[molua_pluginIndex];
    lua_State *L = Ls[session->thread];
    int i;
    for (i = 0; i < callbackRefsCnt[MOLUA_REF_PRE_SAVE]; i++) {
        if (mp && mp->callbackOff[MOLUA_REF_PRE_SAVE] & (1 << i))
            continue;

        lua_getglobal(L, callbackRefs[MOLUA_REF_PRE_SAVE][i]);
        molua_pushArkimeSession(L, session);
        lua_pushboolean(L, final);

        if (lua_pcall(L, 2, 0, 0) != 0) {
            molua_stackDump(L);
            LOGEXIT("error running pre save callback function %s type %d", lua_tostring(L, -1), MOLUA_REF_PRE_SAVE);
        }
    }
}
/******************************************************************************/
LOCAL void molua_save(ArkimeSession_t *session, int final)
{
    const MoluaPlugin_t *mp = session->pluginData[molua_pluginIndex];
    lua_State *L = Ls[session->thread];
    int i;
    for (i = 0; i < callbackRefsCnt[MOLUA_REF_SAVE]; i++) {
        if (mp && mp->callbackOff[MOLUA_REF_SAVE] & (1 << i))
            continue;

        lua_getglobal(L, callbackRefs[MOLUA_REF_SAVE][i]);
        molua_pushArkimeSession(L, session);
        lua_pushboolean(L, final);

        if (lua_pcall(L, 2, 0, 0) != 0) {
            molua_stackDump(L);
            LOGEXIT("error running save callback function %s type %d", lua_tostring(L, -1), MOLUA_REF_SAVE);
        }
    }
}
/******************************************************************************/
LOCAL void MS_register_all_save_cbs()
{
    static char save_cbs_registered;

    if (!save_cbs_registered) {
        save_cbs_registered = 1;
        arkime_plugins_set_cb("lua",
                              NULL,
                              NULL,
                              NULL,
                              molua_pre_save,
                              molua_save,
                              NULL,
                              NULL,
                              NULL);
    }
}
/******************************************************************************/
LOCAL int MS_register_save_cb(lua_State *L)
{
    if (L != Ls[0]) // Only do in thread 0
        return 0;

    if (lua_gettop(L) != 1 || !lua_isstring(L, 1)) {
        return luaL_error(L, "usage: <function name>");
    }

    MS_register_all_save_cbs();

    if (callbackRefsCnt[MOLUA_REF_SAVE] < MOLUA_REF_MAX_CNT) {
        lua_pushvalue(L, 1);
        callbackRefs[MOLUA_REF_SAVE][callbackRefsCnt[MOLUA_REF_SAVE]++] = g_strdup(lua_tostring(L, 1));
    } else {
        return luaL_error(L, "Can't have more then %d callbacks of this type", MOLUA_REF_MAX_CNT);
    }

    return 0;
}
/******************************************************************************/
LOCAL int MS_register_pre_save_cb(lua_State *L)
{
    if (L != Ls[0]) // Only do in thread 0
        return 0;

    if (lua_gettop(L) != 1 || !lua_isstring(L, 1)) {
        return luaL_error(L, "usage: <function name>");
    }

    MS_register_all_save_cbs();

    if (callbackRefsCnt[MOLUA_REF_PRE_SAVE] < MOLUA_REF_MAX_CNT) {
        lua_pushvalue(L, 1);
        callbackRefs[MOLUA_REF_PRE_SAVE][callbackRefsCnt[MOLUA_REF_PRE_SAVE]++] = g_strdup(lua_tostring(L, 1));
    } else {
        return luaL_error(L, "Can't have more then %d callbacks of this type", MOLUA_REF_MAX_CNT);
    }

    return 0;
}
/******************************************************************************/
LOCAL int MS_register_parser(lua_State *L)
{
    if (lua_gettop(L) != 2 || !lua_isuserdata(L, 1) || !lua_isfunction(L, 2)) {
        return luaL_error(L, "usage: <session> <function>");
    }

    ArkimeSession_t *session = checkArkimeSession(L, 1);
    long ref = luaL_ref(L, LUA_REGISTRYINDEX);

    arkime_parsers_register2(session, molua_parsers_cb, (void *)ref, molua_parsers_free_cb, NULL);

    return 0;
}
/******************************************************************************/
LOCAL int MS_add_tag(lua_State *L)
{
    if (lua_gettop(L) != 2 || !lua_isuserdata(L, 1) || !lua_isstring(L, 2)) {
        return luaL_error(L, "usage: <session> <tag>");
    }

    ArkimeSession_t *session = checkArkimeSession(L, 1);
    const char      *tag     = lua_tostring(L, 2);
    arkime_session_add_tag(session, tag);

    return 0;
}
/******************************************************************************/
LOCAL int MS_incr_outstanding(lua_State *L)
{
    if (lua_gettop(L) != 1 || !lua_isuserdata(L, 1)) {
        return luaL_error(L, "usage: <session>");
    }

    ArkimeSession_t *session = checkArkimeSession(L, 1);
    arkime_session_incr_outstanding(session);

    return 0;
}
/******************************************************************************/
LOCAL int MS_decr_outstanding(lua_State *L)
{
    if (lua_gettop(L) != 1 || !lua_isuserdata(L, 1)) {
        return luaL_error(L, "usage: <session>");
    }

    ArkimeSession_t *session = checkArkimeSession(L, 1);
    arkime_session_decr_outstanding(session);

    return 0;
}
/******************************************************************************/
LOCAL int MS_add_protocol(lua_State *L)
{
    if (lua_gettop(L) != 2 || !lua_isuserdata(L, 1) || !lua_isstring(L, 2)) {
        return luaL_error(L, "usage: <session> <protocol>");
    }

    ArkimeSession_t *session = checkArkimeSession(L, 1);
    arkime_session_add_protocol(session, lua_tostring(L, 2));

    return 0;
}
/******************************************************************************/
LOCAL int MS_has_protocol(lua_State *L)
{
    if (lua_gettop(L) != 2 || !lua_isuserdata(L, 1) || !lua_isstring(L, 2)) {
        return luaL_error(L, "usage: <session> <protocol>");
    }

    ArkimeSession_t *session = checkArkimeSession(L, 1);
    gboolean result = arkime_session_has_protocol(session, lua_tostring(L, 2));

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

    ArkimeSession_t *session = checkArkimeSession(L, 1);
    const char      *string  = lua_tostring(L, 3);
    int              len     = lua_rawlen(L, 3);
    int              pos;
    if (lua_isinteger(L, 2)) {
        pos = lua_tointeger(L, 2);
    } else {
        pos = arkime_field_by_exp(lua_tostring(L, 2));
    }
    gboolean result;
    result = arkime_field_string_add(pos, session, string, len, TRUE) != NULL;
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

    ArkimeSession_t *session = checkArkimeSession(L, 1);
    int              value   = lua_tointeger(L, 3);
    int              pos;
    if (lua_isinteger(L, 2)) {
        pos = lua_tointeger(L, 2);
    } else {
        pos = arkime_field_by_exp(lua_tostring(L, 2));
    }
    gboolean result;
    result = arkime_field_int_add(pos, session, value);
    lua_pushboolean(L, result);

    return 1;
}
/******************************************************************************/
LOCAL int MSP_get_addr1(lua_State *L)
{
    ArkimeSession_t *session = checkArkimeSession(L, 1);

    char addrbuf[INET6_ADDRSTRLEN];
    const char *result;
    if (ARKIME_SESSION_v6(session)) {
        result = inet_ntop(AF_INET6, &session->addr1, addrbuf, INET6_ADDRSTRLEN);
    } else {
        result = inet_ntop(AF_INET, &ARKIME_V6_TO_V4(session->addr1), addrbuf, INET6_ADDRSTRLEN);
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
    ArkimeSession_t *session = checkArkimeSession(L, 1);
    lua_pushnumber(L, session->port1);
    return 1;
}
/******************************************************************************/
LOCAL int MSP_get_addr2(lua_State *L)
{
    ArkimeSession_t *session = checkArkimeSession(L, 1);

    char addrbuf[INET6_ADDRSTRLEN];
    const char *result;
    if (ARKIME_SESSION_v6(session)) {
        result = inet_ntop(AF_INET6, &session->addr2, addrbuf, INET6_ADDRSTRLEN);
    } else {
        result = inet_ntop(AF_INET, &ARKIME_V6_TO_V4(session->addr2), addrbuf, INET6_ADDRSTRLEN);
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
    ArkimeSession_t *session = checkArkimeSession(L, 1);
    lua_pushnumber(L, session->port2);
    return 1;
}
/******************************************************************************/
LOCAL int MS_get(lua_State *L)
{
    if (config.debug > 2)
        molua_stackDump(L);

    if (lua_gettop(L) != 2 || !lua_isuserdata(L, 1) || !(lua_isstring(L, 2) || lua_isinteger(L, 2))) {
        return luaL_error(L, "usage: <session> <field string or field num(faster)>");
    }

    ArkimeSession_t *session = checkArkimeSession(L, 1);
    int              pos;
    if (lua_isinteger(L, 2)) {
        pos = lua_tointeger(L, 2);
    } else {
        const char *exp = lua_tostring(L, 2);
        switch (exp[0]) {
        case 'd':
            if (strcmp(exp, "databytes.src") == 0) {
                lua_pushinteger(L, session->databytes[0]);
                return 1;
            }
            if (strcmp(exp, "databytes.dst") == 0) {
                lua_pushinteger(L, session->databytes[1]);
                return 1;
            }
            break;
        case 'i':
            if (strcmp(exp, "ip.src") == 0)
                return MSP_get_addr1(L);
            if (strcmp(exp, "ip.dst") == 0)
                return MSP_get_addr2(L);
            break;
        case 'p':
            if (strcmp(exp, "port.src") == 0) {
                lua_pushinteger(L, session->port1);
                return 1;
            }
            if (strcmp(exp, "port.dst") == 0) {
                lua_pushinteger(L, session->port2);
                return 1;
            }
            if (strcmp(exp, "packets.src") == 0) {
                lua_pushinteger(L, session->packets[0]);
                return 1;
            }
            if (strcmp(exp, "packets.dst") == 0) {
                lua_pushinteger(L, session->packets[1]);
                return 1;
            }
            break;
        case 't':
            if (strncmp(exp, "tcpflags.", 9) != 0)
                break;
            if (strcmp(exp + 9, "syn") == 0)
                lua_pushinteger(L, session->tcpFlagCnt[ARKIME_TCPFLAG_SYN]);
            else if (strcmp(exp + 9, "syn-ack") == 0)
                lua_pushinteger(L, session->tcpFlagCnt[ARKIME_TCPFLAG_SYN_ACK]);
            else if (strcmp(exp + 9, "ack") == 0)
                lua_pushinteger(L, session->tcpFlagCnt[ARKIME_TCPFLAG_ACK]);
            else if (strcmp(exp + 9, "psh") == 0)
                lua_pushinteger(L, session->tcpFlagCnt[ARKIME_TCPFLAG_PSH]);
            else if (strcmp(exp + 9, "rst") == 0)
                lua_pushinteger(L, session->tcpFlagCnt[ARKIME_TCPFLAG_RST]);
            else if (strcmp(exp + 9, "FIN") == 0)
                lua_pushinteger(L, session->tcpFlagCnt[ARKIME_TCPFLAG_FIN]);
            else if (strcmp(exp + 9, "URG") == 0)
                lua_pushinteger(L, session->tcpFlagCnt[ARKIME_TCPFLAG_URG]);
            else
                break;
            return 1;
        }


        pos = arkime_field_by_exp(lua_tostring(L, 2));
    }

    if (pos > session->maxFields || !session->fields[pos]) {
        lua_pushnil(L);
        return 1;
    }

    guint                  i;
    ArkimeField_t         *field = session->fields[pos];
    ArkimeString_t        *hstring;
    ArkimeInt_t           *hint;
    const ArkimeStringHashStd_t *shash;
    const ArkimeIntHashStd_t    *ihash;
    GHashTable            *ghash;
    GHashTableIter         iter;
    gpointer               ikey;
    char                   addrbuf[INET6_ADDRSTRLEN];
    switch (config.fields[pos]->type) {
    case ARKIME_FIELD_TYPE_INT:
        lua_pushinteger(L, field->i);
        break;
    case ARKIME_FIELD_TYPE_INT_HASH:
        ihash = field->ihash;
        i = 0;
        lua_newtable(L);
        HASH_FORALL2(i_, *ihash, hint) {
            lua_pushinteger(L, i + 1);
            lua_pushinteger(L, hint->i_hash);
            lua_settable(L, -3);
            i++;
        }
        break;
    case ARKIME_FIELD_TYPE_INT_GHASH:
        ghash = field->ghash;
        i = 0;
        lua_newtable(L);
        g_hash_table_iter_init (&iter, ghash);
        while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
            lua_pushinteger(L, i + 1);
            lua_pushinteger(L, (unsigned int)(long)ikey);
            lua_settable(L, -3);
            i++;
        }
        break;
    case ARKIME_FIELD_TYPE_STR:
        lua_pushstring(L, field->str);
        break;
    case ARKIME_FIELD_TYPE_STR_ARRAY:
        lua_newtable(L);
        for (i = 0; i < field->sarray->len; i++) {
            lua_pushinteger(L, i + 1);
            lua_pushstring(L, g_ptr_array_index(field->sarray, i));
            lua_settable(L, -3);
        }
        break;
    case ARKIME_FIELD_TYPE_STR_HASH:
        shash = field->shash;
        i = 0;
        lua_newtable(L);
        HASH_FORALL2(s_, *shash, hstring) {
            lua_pushinteger(L, i + 1);
            lua_pushstring(L, hstring->str);
            lua_settable(L, -3);
            i++;
        }
        break;
    case ARKIME_FIELD_TYPE_STR_GHASH:
        ghash = field->ghash;
        i = 0;
        lua_newtable(L);
        g_hash_table_iter_init (&iter, ghash);
        while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
            lua_pushinteger(L, i + 1);
            lua_pushstring(L, ikey);
            lua_settable(L, -3);
            i++;
        }
        break;
    case ARKIME_FIELD_TYPE_IP:
        ikey = field->ip;
        if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)ikey)) {
            inet_ntop(AF_INET6, ikey, addrbuf, INET6_ADDRSTRLEN);
        } else {
            inet_ntop(AF_INET, &ARKIME_V6_TO_V4(*(struct in6_addr *)ikey), addrbuf, INET6_ADDRSTRLEN);
        }
        lua_pushstring(L, addrbuf);
        break;
    case ARKIME_FIELD_TYPE_IP_GHASH:
        ghash = field->ghash;
        i = 0;
        lua_newtable(L);
        g_hash_table_iter_init (&iter, ghash);
        while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
            lua_pushinteger(L, i + 1);
            if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)ikey)) {
                inet_ntop(AF_INET6, ikey, addrbuf, INET6_ADDRSTRLEN);
            } else {
                inet_ntop(AF_INET, &ARKIME_V6_TO_V4(*(struct in6_addr *)ikey), addrbuf, INET6_ADDRSTRLEN);
            }
            lua_pushstring(L, addrbuf);
            lua_settable(L, -3);
            i++;
        }
        break;
    default:
        LOGEXIT("Unsupported lua get");
    }

    return 1;
}
/******************************************************************************/
LOCAL int MS_tostring(lua_State *L)
{
    ArkimeSession_t *session = checkArkimeSession(L, 1);
    lua_pushfstring(L, "ArkimeSession: %p", session);
    return 1;
}

/******************************************************************************/
LOCAL int MS_table(lua_State *L)
{
    ArkimeSession_t *session = checkArkimeSession(L, 1);
    MoluaPlugin_t *mp = session->pluginData[molua_pluginIndex];
    if (!mp) {
        mp = session->pluginData[molua_pluginIndex] = ARKIME_TYPE_ALLOC0(MoluaPlugin_t);
    }

    if (!mp->table) {
        lua_newtable(L);
        mp->table = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, mp->table);
    return 1;
}
/******************************************************************************/
LOCAL int MSP_get_protocol(lua_State *L)
{
    const ArkimeSession_t *session = checkArkimeSession(L, 1);
    char pnum[16];
    const char *protocol = pnum;
    switch (session->ipProtocol) {
    case 1:
        protocol = "icmp";
        break;
    case 6:
        protocol = "tcp";
        break;
    case 17:
        protocol = "udp";
        break;
    case 58:
        protocol = "icmpv6";
        break;
    case 132:
        protocol = "sctp";
        break;
    default:
        snprintf(pnum, sizeof(pnum), "%d", session->ipProtocol);
        break;
    }
    lua_pushstring(L, protocol);
    return 1;
}
/******************************************************************************/
LOCAL int MSP__index(lua_State *L)
{
    static const struct luaL_Reg methods[] = {
        {"__tostring",       MS_tostring},
        {"register_parser",  MS_register_parser},
        {"add_tag",          MS_add_tag},
        {"add_protocol",     MS_add_protocol},
        {"has_protocol",     MS_has_protocol},
        {"add_int",          MS_add_int},
        {"add_string",       MS_add_string},
        {"get",              MS_get},
        {"incr_outstanding", MS_incr_outstanding},
        {"decr_outstanding", MS_decr_outstanding},
        {"table",            MS_table},
        { NULL, NULL }
    };

    static const struct luaL_Reg fields[] = {
        {"addr1",      MSP_get_addr1},
        {"srcIp",      MSP_get_addr1},
        {"addr2",      MSP_get_addr2},
        {"dstIp",      MSP_get_addr1},
        {"port1",      MSP_get_port1},
        {"srcPort",    MSP_get_port1},
        {"port2",      MSP_get_port2},
        {"dstPort",    MSP_get_port2},
        {"protocol",   MSP_get_protocol},
        {"ipProtocol", MSP_get_protocol},
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
void luaopen_arkimesession(lua_State *L)
{
    static const struct luaL_Reg methods[] = {
        {"__index", MSP__index},
        { NULL, NULL }
    };
    static const struct luaL_Reg functions[] = {
        { "http_on", MS_register_http_cb},
        { "register_save", MS_register_save_cb},
        { "register_pre_save", MS_register_pre_save_cb},
        { "register_body_feed", MS_register_body_feed},
        { "register_tcp_classifier", MS_register_tcp_classifier},
        { "register_udp_classifier", MS_register_udp_classifier},
        { NULL, NULL }
    };

    luaL_newmetatable(L, "ArkimeSession");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, methods, 0);
    luaL_newlib(L, functions);
    register_http_constants(L);
    lua_setglobal(L, "ArkimeSession");

    if (certsField == 0)
        certsField = arkime_field_by_exp("cert");
}
