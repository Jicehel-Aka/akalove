// lua_compat.h — pont entre les habitudes de l'API C de Lua 5.2+/5.4 et Lua 5.1.5.
#pragma once
extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

#ifndef LUA_OK
#define LUA_OK 0
#endif

static inline void luaL_setmetatable(lua_State* L, const char* t) {
    luaL_getmetatable(L, t);
    lua_setmetatable(L, -2);
}

static inline void luaL_setfuncs(lua_State* L, const luaL_Reg* l, int /*nup*/) {
    for (; l->name; ++l) {
        lua_pushcfunction(L, l->func);
        lua_setfield(L, -2, l->name);
    }
}

static inline const char* luaL_tolstring(lua_State* L, int idx, size_t* len) {
    if (!luaL_callmeta(L, idx, "__tostring")) {
        switch (lua_type(L, idx)) {
            case LUA_TNUMBER:
            case LUA_TSTRING:
                lua_pushvalue(L, idx);
                break;
            case LUA_TBOOLEAN:
                lua_pushstring(L, lua_toboolean(L, idx) ? "true" : "false");
                break;
            case LUA_TNIL:
                lua_pushliteral(L, "nil");
                break;
            default:
                lua_pushfstring(L, "%s: %p", luaL_typename(L, idx), lua_topointer(L, idx));
        }
    }
    return lua_tolstring(L, -1, len);
}

// Handler d'erreur : message + traceback (remplace luaL_traceback)
static inline int aka_traceback(lua_State* L) {
    lua_getglobal(L, "debug");
    lua_getfield(L, -1, "traceback");
    lua_pushvalue(L, 1);
    lua_pushinteger(L, 2);
    lua_call(L, 2, 1);
    return 1;
}
