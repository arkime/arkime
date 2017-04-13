/* httpService.c  -- lua interface to http.c
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


extern lua_State *Ls[MOLOCH_MAX_PACKET_THREADS];
extern MolochSession_t moluaFakeSessions[MOLOCH_MAX_PACKET_THREADS];

typedef struct {
    long ref;
    int  thread;
    int code;
    unsigned char *data;
    int len;
} LuaHttp_t;

/******************************************************************************/
static void *checkMHS (lua_State *L, int index)
{
    void **pmhs, *mhs;
    luaL_checktype(L, index, LUA_TUSERDATA);
    pmhs = (void**)luaL_checkudata(L, index, "MolochHttpService");
    if (pmhs == NULL) {
        luaL_argerror(L, index, lua_pushfstring(L, "MolochHttpService expected, got %s", luaL_typename(L, index)));
        return NULL;
    }
    mhs = *pmhs;
    if (!mhs)
        luaL_error(L, "null MolochHttpService");
    return mhs;
}
/******************************************************************************/
static void *pushMHS (lua_State *L, void *mhs)
{
    void **pmhs = (void **)lua_newuserdata(L, sizeof(void *));
    *pmhs = mhs;
    luaL_getmetatable(L, "MolochHttpService");
    lua_setmetatable(L, -2);
    return pmhs;
}

/******************************************************************************/
static int MHS_new(lua_State *L)
{
    if (lua_gettop(L) != 3 || !lua_isstring(L, 1) || !lua_isinteger(L, 2) || !lua_isinteger(L, 3)) {
        return luaL_error(L, "usage: <hosts:ports> <maxConnections> <maxRequests>");
    }

    void *server = moloch_http_create_server(lua_tostring(L, 1), lua_tointeger(L, 2), lua_tointeger(L, 3), 0);
    pushMHS(L, server);
    return 1;
}
/******************************************************************************/
static void mhs_http_response_cb_process(MolochSession_t *UNUSED(session), gpointer uw1, gpointer UNUSED(uw2))
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
void mhs_http_response_cb(int code, unsigned char *data, int len, gpointer uw)
{
    LuaHttp_t *lhttp = uw;
    lhttp->code = code;
    lhttp->len = len;
    lhttp->data = g_memdup(data, len); // Sucks

    moloch_session_add_cmd(&moluaFakeSessions[lhttp->thread], MOLOCH_SES_CMD_FUNC, lhttp, NULL, mhs_http_response_cb_process);
}
/******************************************************************************/
static int MHS_request(lua_State *L)
{
    if (config.debug > 2)
        molua_stackDump(L);

    if (lua_gettop(L) != 5 || !lua_isuserdata(L, 1) || !lua_isstring(L, 2) || !lua_isstring(L, 3) || !lua_isstring(L, 4) || !lua_isfunction(L, 5)) {
        return luaL_error(L, "usage: <server> <method> <path> <data> <function>");
    }

    void *server = checkMHS(L, 1);

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
    result = moloch_http_send(server,
                              lua_tostring(L, 2),   // method
                              lua_tostring(L, 3),   // key
                              lua_rawlen(L, 3),     // key_len
                              data,
                              data_len,
                              NULL,
                              0,
                              mhs_http_response_cb,
                              lhttp);

    lua_pushboolean(L, result);
    return 1;
}
/******************************************************************************/
static int MHS_gc(lua_State *L)
{
    if (lua_gettop(L) != 1 || !lua_isuserdata(L, 1)) {
        return luaL_error(L, "usage: <server>");
    }

    void *server = checkMHS(L, 1);

    moloch_http_free_server(server);
    return 0;
}
/******************************************************************************/
static int MHS_tostring (lua_State *L)
{
    lua_pushfstring(L, "MolochHttpService: %p", lua_touserdata(L, 1));
    return 1;
}

/******************************************************************************/
void luaopen_molochhttpservice(lua_State *L)
{
    static const struct luaL_Reg methods[] = {
        {"__tostring", MHS_tostring},
        {"__gc", MHS_gc},
        {"request", MHS_request},
        { NULL, NULL }
    };

    static const struct luaL_Reg functions[] = {
        {"new", MHS_new},
        { NULL, NULL }
    };

    luaL_newmetatable(L, "MolochHttpService");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, methods, 0);
    luaL_newlib(L, functions);
    lua_setglobal(L, "MolochHttpService");
}
