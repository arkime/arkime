#ifndef _MOLOCH_LUA_
#define _MOLOCH_LUA_
#include "moloch.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

extern MolochConfig_t        config;

void luaopen_molochhttpservice(lua_State *L);
void luaopen_molochsession(lua_State *L);
void luaopen_molochstring(lua_State *L);

void molua_stackDump (lua_State *L);

typedef struct {
    const char   *str;
    int           len;
    uint8_t       needFree;
} MS_t;
MS_t *molua_pushMolochString (lua_State *L, const char *str, int len);

#endif
