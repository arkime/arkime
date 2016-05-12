#ifndef _MOLOCH_LUA_
#define _MOLOCH_LUA_
#include "moloch.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

extern MolochConfig_t        config;

void luaopen_molochhttpservice(lua_State *L);
void luaopen_molochsession(lua_State *L);
void luaopen_molochdata(lua_State *L);

void molua_stackDump (lua_State *L);

typedef struct {
    const char   *str;
    int           len;
    uint8_t       needFree;
} MD_t;

#define MOLUA_REF_MAX_CNT 32

#define MOLUA_REF_HTTP 0
#define MOLUA_REF_SMTP 1
#define MOLUA_REF_SIZE 2

typedef struct {
    uint32_t callbackOff[MOLUA_REF_SIZE];
    long     table;
} MoluaPlugin_t;

MD_t *molua_pushMolochData (lua_State *L, const char *str, int len);
void *molua_pushMolochSession (lua_State *L, const MolochSession_t *session);

extern int molua_pluginIndex;

#endif
