/* lua.c  -- lua capture plugins
 *
 *  Simple plugin that queries the wise service for
 *  ips, domains, email, and md5s which can use various
 *  services to return data.  It caches all the results.
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

#include "moloch.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
/******************************************************************************/

extern MolochConfig_t        config;

static lua_State *Ls[MOLOCH_MAX_PACKET_THREADS];
static MolochSession_t fakeSessions[MOLOCH_MAX_PACKET_THREADS];

typedef struct {
    long ref;
    int  thread;
    int code; 
    unsigned char *data;
    int len;
} LuaHttp_t;

/******************************************************************************/
static void stackDump (lua_State *L) {
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
void lua_classify_cb(MolochSession_t *session, const unsigned char *data, int len, int which, void *uw)
{
    lua_State *L = Ls[session->thread];
    lua_getglobal(L, uw);
    lua_pushlightuserdata(L, session);
    lua_pushlstring(L, (char *)data, len);
    lua_pushnumber(L, which);
    if (lua_pcall(L, 3, 0, 0) != 0) {
       LOG("error running function %s: %s", uw, lua_tostring(L, -1));
       exit(0);
    }
}


/******************************************************************************/
int lua_parsers_cb(MolochSession_t *session, void *uw, const unsigned char *data, int remaining, int which)
{
    lua_State *L = Ls[session->thread];
    lua_rawgeti(L, LUA_REGISTRYINDEX, (long)uw);
    lua_pushlightuserdata(L, session);
    lua_pushlstring(L, (char *)data, remaining);
    lua_pushnumber(L, which);

    if (lua_pcall(L, 3, 1, 0) != 0) {
       LOG("error running parser function %s", lua_tostring(L, -1));
       exit(0);
    }

    int num = lua_tointeger(L, -1);
    if (num == -1)
        moloch_parsers_unregister(session, uw);

    return 0;
}
/******************************************************************************/
void lua_parsers_free_cb(MolochSession_t *session, void *uw)
{
    lua_State *L = Ls[session->thread];
    luaL_unref(L, LUA_REGISTRYINDEX, (long)uw);
}
/******************************************************************************/
static int lua_parsers_classifier_register_tcp(lua_State *L) 
{
    if (lua_gettop(L) != 4 || !lua_isstring(L, 1) || !lua_isinteger(L, 2) || !lua_isstring(L, 3) || !lua_isstring(L, 4)) {
        return luaL_error(L, "usage: <name> <offset> <match> <function>");
    }

    char *name      = g_strdup(lua_tostring(L, 1));
    char  offset    = lua_tonumber(L, 2);
    int   match_len = lua_rawlen(L, 3);
    guchar *match     = g_memdup(lua_tostring(L, 3), match_len);
    char *function  = g_strdup(lua_tostring(L, 4));

    moloch_parsers_classifier_register_tcp(name, function, offset, match, match_len, lua_classify_cb);
    return 0;
}
/******************************************************************************/
static int lua_parsers_classifier_register_udp(lua_State *L) 
{
    if (lua_gettop(L) != 4 || !lua_isstring(L, 1) || !lua_isinteger(L, 2) || !lua_isstring(L, 3) || !lua_isstring(L, 4)) {
        return luaL_error(L, "usage: <name> <offset> <match> <function>");
    }

    char *name      = g_strdup(lua_tostring(L, 1));
    char  offset    = lua_tonumber(L, 2);
    int   match_len = lua_rawlen(L, 3);
    guchar *match     = g_memdup(lua_tostring(L, 3), match_len);
    char *function  = g_strdup(lua_tostring(L, 4));

    moloch_parsers_classifier_register_udp(name, function, offset, match, match_len, lua_classify_cb);
    return 0;
}
/******************************************************************************/
static int lua_parsers_register(lua_State *L) 
{
    if (lua_gettop(L) != 2 || !lua_isuserdata(L, 1) || !lua_isfunction(L, 2)) {
        return luaL_error(L, "usage: <session> <function>");
    }

    MolochSession_t *session = lua_touserdata(L, 1);
    long ref = luaL_ref(L, LUA_REGISTRYINDEX);

    moloch_parsers_register2(session, lua_parsers_cb, (void*)ref, lua_parsers_free_cb, NULL);

    return 0;
}
/******************************************************************************/
static int lua_session_add_tag(lua_State *L) 
{
    if (lua_gettop(L) != 2 || !lua_isuserdata(L, 1) || !lua_isstring(L, 2)) {
        return luaL_error(L, "usage: <session> <tag>");
    }

    MolochSession_t *session = lua_touserdata(L, 1);
    const char      *tag     = lua_tostring(L, 2);
    moloch_session_add_tag(session, tag);

    return 0;
}
/******************************************************************************/
static int lua_session_incr_oustanding(lua_State *L) 
{
    if (lua_gettop(L) != 1 || !lua_isuserdata(L, 1)) {
        return luaL_error(L, "usage: <session>");
    }

    MolochSession_t *session = lua_touserdata(L, 1);
    moloch_session_incr_outstanding(session);

    return 0;
}
/******************************************************************************/
static int lua_session_decr_oustanding(lua_State *L) 
{
    if (lua_gettop(L) != 1 || !lua_isuserdata(L, 1)) {
        return luaL_error(L, "usage: <session>");
    }

    moloch_session_decr_outstanding(lua_touserdata(L, 1));

    return 0;
}
/******************************************************************************/
static int lua_field_string_add(lua_State *L) 
{
    if (config.debug > 2)
        stackDump(L);

    if (lua_gettop(L) != 3 || !lua_isuserdata(L, 1) || !(lua_isstring(L, 2) || lua_isinteger(L, 2)) || !lua_isstring(L, 3)) {
        return luaL_error(L, "usage: <session> <field string or field num(faster)> <string>");
    }

    MolochSession_t *session = lua_touserdata(L, 1);
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
static int lua_field_int_add(lua_State *L) 
{
    if (config.debug > 2)
        stackDump(L);

    if (lua_gettop(L) != 3 || !lua_isuserdata(L, 1) || !(lua_isstring(L, 2) || lua_isinteger(L, 2)) || !lua_isinteger(L, 3)) {
        return luaL_error(L, "usage: <session> <field string or field num(faster)> <integer>");
    }

    MolochSession_t *session = lua_touserdata(L, 1);
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
static int lua_field_by_exp(lua_State *L) 
{
    if (lua_gettop(L) != 1 || !lua_isstring(L, 1)) {
        return luaL_error(L, "usage: <field expression>");
    }

    int pos = moloch_field_by_exp(lua_tostring(L, 1));
    lua_pushinteger(L, pos);

    return 1;
}
/******************************************************************************/
static int lua_http_create_server(lua_State *L) 
{
    if (lua_gettop(L) != 3 || !lua_isstring(L, 1) || !lua_isinteger(L, 2) || !lua_isinteger(L, 3)) {
        return luaL_error(L, "usage: <hosts:ports> <maxConnections> <maxRequests>");
    }

    void *server = moloch_http_create_server(lua_tostring(L, 1), 0, lua_tointeger(L, 2), lua_tointeger(L, 3), 0);
    lua_pushlightuserdata(L, server);
    return 1;
}
/******************************************************************************/
void lua_http_response_cb_process(MolochSession_t *UNUSED(session), gpointer uw1, gpointer UNUSED(uw2))
{
    LuaHttp_t *lhttp = uw1;

    lua_State *L = Ls[lhttp->thread];
    lua_rawgeti(L, LUA_REGISTRYINDEX, lhttp->ref);
    lua_pushinteger(L, lhttp->code);
    lua_pushlstring(L, (char *)lhttp->data, lhttp->len);

    if (lua_pcall(L, 2, 0, 0) != 0) {
       LOG("error running http callback function %s", lua_tostring(L, -1));
       exit(0);
    }

    g_free(lhttp->data);
    MOLOCH_TYPE_FREE(LuaHttp_t, lhttp);
}
/******************************************************************************/
void lua_http_response_cb(int code, unsigned char *data, int len, gpointer uw)
{
    LuaHttp_t *lhttp = uw;
    lhttp->code = code;
    lhttp->len = len;
    lhttp->data = g_memdup(data, len); // Sucks

    moloch_session_add_cmd(&fakeSessions[lhttp->thread], MOLOCH_SES_CMD_FUNC, lhttp, NULL, lua_http_response_cb_process);
}
/******************************************************************************/
static int lua_http_request(lua_State *L) 
{
    if (config.debug > 2)
        stackDump(L);

    if (lua_gettop(L) != 5 || !lua_isuserdata(L, 1) || !lua_isstring(L, 2) || !lua_isstring(L, 3) || !lua_isstring(L, 4) || !lua_isfunction(L, 5)) {
        return luaL_error(L, "usage: <server> <method> <path> <data> <function>");
    }

    void *server = lua_touserdata(L, 1);
    int data_len = lua_rawlen(L, 4);
    char *data;

    if (data_len > 0) {
        data = moloch_http_get_buffer(data_len);
        memcpy(data, lua_tostring(L, 4), data_len);
    } else {
        data = NULL;
    }

    int thread;
    for (thread = 0; thread < config.packetThreads; thread++) {
        if (L == Ls[thread])
            break;
    }

    LuaHttp_t *lhttp = MOLOCH_TYPE_ALLOC(LuaHttp_t);
    lhttp->ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lhttp->thread = thread;

    gboolean result;
    result = moloch_http_send(lua_touserdata(L, 1), // server
                              lua_tostring(L, 2),   // method
                              lua_tostring(L, 3),   // key
                              lua_rawlen(L, 3),     // key_len
                              data,
                              data_len,
                              NULL,
                              0,
                              lua_http_response_cb,
                              lhttp);

    lua_pushboolean(L, result);
    return 1;
}
/******************************************************************************/
static int lua_http_free_server(lua_State *L) 
{
    if (lua_gettop(L) != 1 || !lua_isuserdata(L, 1)) {
        return luaL_error(L, "usage: <server>");
    }

    moloch_http_free_server(lua_touserdata(L, 1));
    return 0;
}
/******************************************************************************/
void moloch_plugin_init()
{
    int thread;
    char **names = moloch_config_str_list(NULL, "luaFiles", "moloch.lua");
    lua_State *L;

    for (thread = 0; thread < config.packetThreads; thread++) {
        L = Ls[thread] = luaL_newstate();
        luaL_openlibs(L);
        fakeSessions[thread].thread = thread;

        for (int i = 0; names[i]; i++) {
            if (luaL_loadfile(L, names[i])) {
                LOG("Error loading %s: %s", names[i], lua_tostring(L, -1));
                exit(0);
            }

            lua_register(L, "moloch_parsers_classifier_register_tcp", lua_parsers_classifier_register_tcp);
            lua_register(L, "moloch_parsers_classifier_register_udp", lua_parsers_classifier_register_udp);
            lua_register(L, "moloch_parsers_register", lua_parsers_register);

            lua_register(L, "moloch_session_add_tag", lua_session_add_tag);
            lua_register(L, "moloch_session_incr_outstanding", lua_session_incr_oustanding);
            lua_register(L, "moloch_session_decr_outstanding", lua_session_decr_oustanding);

            lua_register(L, "moloch_field_add_string", lua_field_string_add);
            lua_register(L, "moloch_field_add_int", lua_field_int_add);
            lua_register(L, "moloch_field_by_exp", lua_field_by_exp);

            lua_register(L, "moloch_http_create_server", lua_http_create_server);
            lua_register(L, "moloch_http_request", lua_http_request);
            lua_register(L, "moloch_http_free_server", lua_http_free_server);


            if (lua_pcall(L, 0, 0, 0)) {
                LOG("Error initing %s: %s", names[i], lua_tostring(L, -1));
                exit(0);
            }
        }
    }
}
