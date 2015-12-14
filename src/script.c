#include "script.h"
#include <stdlib.h>
#include <string.h>

void
lua_loadlib(lua_State *L, const char *libname, lua_CFunction luafunc) {
    lua_pushcfunction(L, luafunc);
    lua_pushstring(L, libname);
    lua_call(L, 1, 0); 
}

lua_State *
script_init(const char *filename)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dofile(L, filename);

    return L;
}

void
script_release(lua_State *L)
{
    lua_close(L);
}

int
script_check_func_exists(lua_State * L, const char *func_name) {
    int ret;

    lua_getglobal(L, func_name);
    ret = lua_isfunction(L,lua_gettop(L));
    lua_pop(L,-1);

    return ret;
}

void
script_pushtableinteger(lua_State* L , char* key , double value)
{
    lua_pushstring(L, key);
    lua_pushinteger(L, value);
    lua_settable(L, -3);
}

void
script_pushtablestring(lua_State* L , char* key , char* value)
{
    lua_pushstring(L, key);
    lua_pushstring(L, value);
    lua_settable(L, -3);
}
