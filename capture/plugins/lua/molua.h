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
    uint8_t       invalid;              // True if the pointer is no longer valid
} MD_t;

#define MOLUA_REF_MAX_CNT 32

#define MOLUA_REF_HTTP 0
#define MOLUA_REF_SMTP 1
#define MOLUA_REF_HTTP_CB_FIRST 2
#define MOLUA_REF_HTTP_MESSAGE_BEGIN 2
#define MOLUA_REF_HTTP_URL 3
#define MOLUA_REF_HTTP_HEADER_FIELD 4
#define MOLUA_REF_HTTP_HEADER_VALUE 5
#define MOLUA_REF_HTTP_HEADERS_COMPLETE 6
#define MOLUA_REF_HTTP_BODY 7
#define MOLUA_REF_HTTP_MESSAGE_COMPLETE 8
#define MOLUA_REF_HTTP_HEADER_FIELD_RAW 9
#define MOLUA_REF_HTTP_CB_LAST 9
#define MOLUA_REF_PRE_SAVE 10
#define MOLUA_REF_SAVE 11
#define MOLUA_REF_SIZE 12

typedef struct {
    uint32_t callbackOff[MOLUA_REF_SIZE];
    long     table;
    char     done_message_begin[2];     // 0 for REQ, 1 for RESP
} MoluaPlugin_t;

MD_t *molua_pushMolochData (lua_State *L, const char *str, int len);
void *molua_pushMolochSession (lua_State *L, const MolochSession_t *session);
void MD_markInvalid(lua_State *L, int index);

extern int molua_pluginIndex;

#endif
